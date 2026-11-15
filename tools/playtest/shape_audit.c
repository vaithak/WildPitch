/* shape.c - team-shape audit harness (7v7). usage: shape [matches=10] [minutes=5] [human=0]
 * human=1: seat 0 drives a team-0 player that always runs at the ball (and dribbles to goal / shoots). */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define M_PI 3.14159265358979323846
#include <string.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

enum { A_NONE, A_SHAPE, A_SUPPORT, A_RUN, A_CHASE, A_PRESS, A_COVER, A_MARK, A_RECEIVE, A_DRIBBLE, A_PASS, A_THROUGH,
       A_SHOOT, A_KEEP, A_CROSS, A_HEAD, A_N, A_HUMAN = A_N, A_NN };
static const char *AN[A_NN] = {"none","SHAPE","SUPPORT","RUN","CHASE","PRESS","COVER","MARK","RECEIVE","DRIBBLE","PASS",
  "THROUGH","SHOOT","KEEP","CROSS","HEAD","HUMAN"};
enum { P_IN, P_OUT, P_LOOSE, P_N };
static const char *PN[P_N] = {"IN possession", "OUT of possession", "LOOSE ball"};
static const char *RN[4] = {"keeper","defender(k1,2)","midfield(k3-5)","striker(k6)"};
static int rolecls(int k) { return k == 0 ? 0 : k <= 2 ? 1 : k <= 5 ? 2 : 3; }

typedef struct { double n, s, s2; } St;
static void add(St *a, double v) { a->n++; a->s += v; a->s2 += v * v; }
static double mean(const St *a) { return a->n ? a->s / a->n : 0; }
static double sd(const St *a) { double m = mean(a); return a->n > 1 ? sqrt(fmax(a->s2 / a->n - m * m, 0)) : 0; }
typedef struct { double sxx, sxy, syy, sx, sy, n; } Reg;
static void radd(Reg *r, double x, double y) { r->sx += x; r->sy += y; r->sxx += x*x; r->sxy += x*y; r->syy += y*y; r->n++; }
static void rpr(const char *t, Reg *r) {
  double n = r->n, cx = r->sxx - r->sx*r->sx/n, cxy = r->sxy - r->sx*r->sy/n, cy = r->syy - r->sy*r->sy/n;
  printf("    %-44s slope %.2f  r %.2f\n", t, cxy / cx, cxy / sqrt(cx * cy)); }

static double cross(double ox, double oy, double ax, double ay, double bx, double by) { return (ax-ox)*(by-oy)-(ay-oy)*(bx-ox); }
static double hull_area(double *x, double *y, int n) {           /* monotone chain */
  int idx[16]; for (int i = 0; i < n; i++) idx[i] = i;
  for (int i = 0; i < n; i++) for (int j = i+1; j < n; j++)
    if (x[idx[j]] < x[idx[i]] || (x[idx[j]] == x[idx[i]] && y[idx[j]] < y[idx[i]])) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
  int h[32], k = 0;
  for (int i = 0; i < n; i++) { while (k >= 2 && cross(x[h[k-2]],y[h[k-2]],x[h[k-1]],y[h[k-1]],x[idx[i]],y[idx[i]]) <= 0) k--; h[k++] = idx[i]; }
  for (int i = n-2, t = k+1; i >= 0; i--) { while (k >= t && cross(x[h[k-2]],y[h[k-2]],x[h[k-1]],y[h[k-1]],x[idx[i]],y[idx[i]]) <= 0) k--; h[k++] = idx[i]; }
  double a = 0; for (int i = 0; i < k-1; i++) a += x[h[i]]*y[h[i+1]] - x[h[i+1]]*y[h[i]];
  return fabs(a) / 2; }

/* per team x phase accumulators */
typedef struct {
  long samples;
  long near5[8], near10[8];           /* histogram of outfield count within 5 / 10 m of the ball */
  long engage[8];                      /* # outfield in CHASE|PRESS */
  long ballacts[8];                    /* # outfield in CHASE|PRESS|COVER */
  long act[4][A_NN];                   /* role class x action */
  St len, wid, area, cdist, gap_dm, gap_ms, drift[4], fdrift[4];
  long def_far10;                      /* defender >10 m from his dynamic home spot (per defender sample) */
  long def_samples, def_ahead_ball;    /* defender ahead of the ball (attack x) */
  /* in possession */
  St opts15, opts15_open, ahead, behind5, sectors; long wideL, wideR, wideBoth, nobody_ahead, no_rest;
  /* out of possession */
  St press_d, second_d, goalside, within3; long has_second;
  Reg cy, cx;
  long nonfirst_chase, chase_total, nonfirst_press, press_total;
  St support_from_home, support_to_ball, mark_to_ball;
} Acc;
static Acc A[2][P_N];

static int human_seat;
static WpSeatInput human_input(const SimState *s) {
  WpSeatInput si = {0, 0, 0};
  int me = s->seat_player[0]; if (me < 0) return si;
  const WpPlayer *p = &s->players[me];
  fv2 tgt = s->ball.pos; static int hold = 0;
  if (s->ball.owner == me) {
    tgt = fv2_make(s->pitch.length / 2 * (p->team ? -1 : 1), 0);
    double dg = tod(fv2_len(fv2_sub(tgt, p->pos)));
    if (dg < 14.0 || hold) { if (hold < 12) { si.buttons = WP_BTN_SHOOT; hold++; } else hold = 0; }
    else si.buttons = WP_BTN_SPRINT;
  } else { hold = 0; si.buttons = WP_BTN_SPRINT; }
  fv2 d = fv2_sub(tgt, p->pos); double x = tod(d.x), y = tod(d.y), l = sqrt(x*x+y*y);
  if (l > 0.2) { si.move_x = (int8_t)lround(x/l*127); si.move_y = (int8_t)lround(y/l*127); }
  return si;
}

int main(int argc, char **argv) {
  int matches = argc > 1 ? atoi(argv[1]) : 10, minutes = argc > 2 ? atoi(argv[2]) : 5; human_seat = argc > 3 && atoi(argv[3]);
  static SimState s; WpInput in; memset(&in, 0, sizeof in);
  srand(12345);
  long goals[2] = {0};
  for (int m = 0; m < matches; m++) {
    WpMatchConfig cfg = sim_config_7v7(); cfg.seed = (uint64_t)(m + 1);
    sim_init(&s, &cfg);
    for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
    if (human_seat) s.seat_player[0] = 6;
    int n = s.n_per_side; double hl = tod(s.pitch.length) / 2, hw = tod(s.pitch.width) / 2;
    for (long t = 0; t < 60L * 60 * minutes; t++) {
      if (human_seat) in.seat[0] = human_input(&s);
      sim_step(&s, &in);
      if (s.events & WP_EV_RESET) for (int i = 0; i < 2*n; i++) {
        s.players[i].pos.x += (fx)(rand() % 65536) - FX(0.5); s.players[i].pos.y += (fx)(rand() % 65536) - FX(0.5); }
      if (s.events & WP_EV_GOAL) goals[s.ball.last_touch >= 0 ? s.players[s.ball.last_touch].team : 0]++;
      if (s.phase != WP_PHASE_PLAY || t % 6) continue;
      if (s.restart_kind) continue;                     /* skip set-piece setups */
      int owner = s.ball.owner;
      int possteam = owner >= 0 ? s.players[owner].team : (s.pass_target >= 0 ? s.players[s.pass_from].team : -1);
      for (int tm = 0; tm < 2; tm++) {
        int sg = tm == 0 ? 1 : -1;
        int ph = possteam < 0 ? P_LOOSE : possteam == tm ? P_IN : P_OUT;
        Acc *a = &A[tm][ph]; a->samples++;
        double bx = tod(s.ball.pos.x) * sg, by = tod(s.ball.pos.y) * sg;     /* attack terms */
        /* ball rank replica (ball predicted 0.4 s) */
        double pbx = tod(s.ball.pos.x + fx_mul(s.ball.vel.x, FX(0.4))), pby = tod(s.ball.pos.y + fx_mul(s.ball.vel.y, FX(0.4)));
        double X[8], Y[8]; int no = 0, n5 = 0, n10 = 0, eng = 0, bact = 0;
        double minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9, cxs = 0, cys = 0, lx[4] = {0}, ln[4] = {0};
        double dball[8]; int idx[8];
        for (int k = 1; k < n; k++) {
          int i = tm * n + k; const WpPlayer *p = &s.players[i];
          double x = tod(p->pos.x) * sg, y = tod(p->pos.y) * sg;
          X[no] = x; Y[no] = y; idx[no] = i; dball[no] = hypot(x - bx, y - by); no++;
          if (dball[no-1] < 5) n5++; if (dball[no-1] < 10) n10++;
          minx = fmin(minx, x); maxx = fmax(maxx, x); miny = fmin(miny, y); maxy = fmax(maxy, y); cxs += x; cys += y;
          int rc = rolecls(k); lx[rc] += x; ln[rc]++;
          int ishuman = human_seat && s.seat_player[0] == i;
          int act = ishuman ? A_HUMAN : p->ai_act;
          a->act[rc][act]++;
          if (act == A_CHASE || act == A_PRESS) eng++;
          if (act == A_CHASE || act == A_PRESS || act == A_COVER) bact++;
          /* rank */
          double mine = hypot(tod(p->pos.x) - pbx, tod(p->pos.y) - pby); int r = 0;
          for (int j = tm*n+1; j < (tm+1)*n; j++) { if (j == i) continue;
            double d = hypot(tod(s.players[j].pos.x) - pbx, tod(s.players[j].pos.y) - pby); if (d < mine || (d == mine && j < i)) r++; }
          if (act == A_CHASE) { a->chase_total++; if (r) a->nonfirst_chase++; }
          if (act == A_PRESS) { a->press_total++; if (r) a->nonfirst_press++; }
          /* home spot replica */
          fx fxl, fyl; formation_frac(n, k, &fxl, &fyl);
          double hx = tod(fxl) * hl + bx * 0.55 + (ph == P_IN ? 4.0 : -1.0), hy = tod(fyl) * hw + by * 0.3;
          hx = fmax(-(hl-1.5), fmin(hl-1.5, hx)); hy = fmax(-(hw-1.5), fmin(hw-1.5, hy));
          double dr = hypot(x - hx, y - hy); add(&a->drift[rc], dr);
          add(&a->fdrift[rc], hypot(x - tod(fxl)*hl, y - tod(fyl)*hw));
          if (rc == 1) { a->def_samples++; if (dr > 10) a->def_far10++; if (x > bx + 1) a->def_ahead_ball++; }
          if (!ishuman && p->ai_act) {
            double gx = tod(p->ai_goal.x) * sg, gy = tod(p->ai_goal.y) * sg;
            if (act == A_SUPPORT) { add(&a->support_from_home, hypot(gx-hx, gy-hy)); add(&a->support_to_ball, hypot(gx-bx, gy-by)); }
            if (act == A_MARK) add(&a->mark_to_ball, hypot(gx-bx, gy-by));
          }
        }
        a->near5[n5]++; a->near10[n10]++; a->engage[eng]++; a->ballacts[bact]++;
        add(&a->len, maxx - minx); add(&a->wid, maxy - miny); add(&a->area, hull_area(X, Y, no));
        double cx = cxs / no, cy = cys / no; add(&a->cdist, hypot(cx - bx, cy - by));
        add(&a->gap_dm, lx[2]/ln[2] - lx[1]/ln[1]); add(&a->gap_ms, lx[3]/ln[3] - lx[2]/ln[2]);
        radd(&a->cy, by, cy); radd(&a->cx, bx, cx);
        if (ph == P_IN) {
          int o15 = 0, o15o = 0, ahead = 0, beh = 0, wl = 0, wr = 0, sec[4] = {0};
          for (int q = 0; q < no; q++) {
            if (idx[q] == owner) continue;
            if (X[q] > bx + 3) ahead++;
            if (X[q] < bx - 5) beh++;
            if (Y[q] > hw - 6) wl++; if (Y[q] < -(hw - 6)) wr++;
            if (dball[q] < 15 && dball[q] > 3) { o15++;
              double lane = tod(pass_lane_clear(&s, tm, s.ball.pos, s.players[idx[q]].pos));
              if (lane >= 1.5) { o15o++;
                double ang = atan2(Y[q] - by, X[q] - bx) * 180 / M_PI;
                int sc = fabs(ang) < 45 ? 0 : fabs(ang) > 135 ? 3 : ang > 0 ? 1 : 2; sec[sc] = 1; } }
          }
          add(&a->opts15, o15); add(&a->opts15_open, o15o); add(&a->ahead, ahead); add(&a->behind5, beh);
          add(&a->sectors, sec[0]+sec[1]+sec[2]+sec[3]);
          if (wl) a->wideL++; if (wr) a->wideR++; if (wl && wr) a->wideBoth++; if (!ahead) a->nobody_ahead++; if (!beh) a->no_rest++;
        }
        if (ph == P_OUT && owner >= 0) {
          double d1 = 1e9, d2 = 1e9; int gs = 0, w3 = 0;
          for (int q = 0; q < no; q++) { double d = dball[q];
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
            if (X[q] < bx) gs++; if (d < 3) w3++; }
          add(&a->press_d, d1); add(&a->second_d, d2); add(&a->goalside, gs); add(&a->within3, w3);
          if (d2 < 8) a->has_second++;
        }
      }
    }
  }
  printf("7v7 %s: %d matches x %d min, goals %ld-%ld, samples every 0.1 s in open play\n",
         human_seat ? "HUMAN(seat0 chases ball, team0) vs AI" : "AI vs AI", matches, minutes, goals[0], goals[1]);
  for (int tm = 0; tm < 2; tm++) for (int ph = 0; ph < P_N; ph++) {
    Acc *a = &A[tm][ph]; if (!a->samples) continue;
    printf("\n=== team %d, %s (%ld samples) ===\n", tm, PN[ph], a->samples);
    printf("  outfield within 5 m of ball (0..6):  "); for (int k = 0; k <= 6; k++) printf("%5.1f%% ", 100.0*a->near5[k]/a->samples);
    printf("\n  outfield within 10 m of ball (0..6): "); for (int k = 0; k <= 6; k++) printf("%5.1f%% ", 100.0*a->near10[k]/a->samples);
    double m5 = 0, m10 = 0; for (int k = 0; k <= 6; k++) { m5 += k*a->near5[k]; m10 += k*a->near10[k]; }
    printf("\n  mean within 5 m %.2f, within 10 m %.2f\n", m5/a->samples, m10/a->samples);
    printf("  # in CHASE|PRESS (0..6): "); for (int k = 0; k <= 6; k++) printf("%5.1f%% ", 100.0*a->engage[k]/a->samples);
    printf("\n  # in CHASE|PRESS|COVER : "); for (int k = 0; k <= 6; k++) printf("%5.1f%% ", 100.0*a->ballacts[k]/a->samples);
    printf("\n  CHASE by non-first man: %ld/%ld (%.0f%%), PRESS by non-first: %ld/%ld (%.0f%%)\n", a->nonfirst_chase, a->chase_total,
           100.0*a->nonfirst_chase/(a->chase_total+1e-9), a->nonfirst_press, a->press_total, 100.0*a->nonfirst_press/(a->press_total+1e-9));
    printf("  length %.1f+-%.1f m, width %.1f+-%.1f m, hull area %.0f+-%.0f m2, centroid->ball %.1f m\n",
           mean(&a->len), sd(&a->len), mean(&a->wid), sd(&a->wid), mean(&a->area), sd(&a->area), mean(&a->cdist));
    printf("  line gaps (attack x): def->mid %.1f m, mid->striker %.1f m\n", mean(&a->gap_dm), mean(&a->gap_ms));
    printf("  drift from dynamic home spot: def %.1f, mid %.1f, str %.1f m | from static kickoff spot: def %.1f mid %.1f str %.1f\n",
           mean(&a->drift[1]), mean(&a->drift[2]), mean(&a->drift[3]), mean(&a->fdrift[1]), mean(&a->fdrift[2]), mean(&a->fdrift[3]));
    printf("  defenders >10 m from home spot %.0f%%, defenders ahead of ball (+1 m) %.0f%%\n",
           100.0*a->def_far10/a->def_samples, 100.0*a->def_ahead_ball/a->def_samples);
    printf("  SUPPORT target: %.1f m from home spot, %.1f m from ball; MARK target %.1f m from ball\n",
           mean(&a->support_from_home), mean(&a->support_to_ball), mean(&a->mark_to_ball));
    rpr("centroid y vs ball y", &a->cy); rpr("centroid x vs ball x", &a->cx);
    for (int rc = 1; rc < 4; rc++) { long tot = 0; for (int k = 0; k < A_NN; k++) tot += a->act[rc][k];
      printf("  actions %-15s", RN[rc]); for (int k = 1; k < A_NN; k++) if (a->act[rc][k]*1000 > tot) printf(" %s %.0f%%", AN[k], 100.0*a->act[rc][k]/tot); printf("\n"); }
    if (ph == P_IN) {
      printf("  options 3-15 m from ball %.2f (open lane>=1.5 m: %.2f); sectors covered by open options (fwd/left/right/back, of 4) %.2f\n",
             mean(&a->opts15), mean(&a->opts15_open), mean(&a->sectors));
      printf("  team-mates ahead of ball (+3 m) %.2f (none: %.0f%%); behind ball (-5 m) %.2f (none: %.0f%%)\n",
             mean(&a->ahead), 100.0*a->nobody_ahead/a->samples, mean(&a->behind5), 100.0*a->no_rest/a->samples);
      printf("  someone within 6 m of left touchline %.0f%%, right %.0f%%, both %.0f%%\n",
             100.0*a->wideL/a->samples, 100.0*a->wideR/a->samples, 100.0*a->wideBoth/a->samples);
    }
    if (ph == P_OUT && a->press_d.n) {
      printf("  nearest defender to carrier %.1f m, 2nd nearest %.1f m (2nd within 8 m: %.0f%%), # within 3 m %.2f, # goal-side of ball %.2f/6\n",
             mean(&a->press_d), mean(&a->second_d), 100.0*a->has_second/a->press_d.n, mean(&a->within3), mean(&a->goalside));
    }
  }
  return 0;
}
