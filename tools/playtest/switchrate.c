/* switchrate.c - how often control of the human seat changes hands without being asked (CP0.5b, D-050 acceptance).
 * 7v7 (sim_config_7v7: assist Normal, the game's switching mode), seat 0 on team 0, everyone else AI, two scripts:
 *   R = stick released (he only walks onto a still loose ball within 5 m, and taps PASS 0.25 s after getting it),
 *   H = stick held toward the ball (full 8-way keys; on the ball: toward their goal, tap PASS likewise).
 * Reasons are inferred from the state around the tick (works on any version of the sim):
 *   restart (not in open play) / pass (hand-off to the receiver of his pass) / gain (the new man has the ball) /
 *   deflect (our pass ended loose this tick) / keeper (the keeper handed back) / unprompted (everything else: the
 *   loose-ball and defender auto-switches). A->B->A = back to the same man within 60 ticks.
 * Usage: cc -std=c17 -O2 -fwrapv -Isrc -Itests tools/playtest/switchrate.c build/libwp_sim.a -lm -o /tmp/sr
 *        /tmp/sr [seeds=3] [minutes=5]        env WP_SWITCH=1|2|3 (Manual/Events/Assisted; needs D-050), WP_STATS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sim/sim_internal.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { R_RESTART, R_PASS, R_GAIN, R_DEFLECT, R_KEEPER, R_UNPROMPTED, R_N };
static const char *RN[R_N] = {"restart", "pass", "gain", "deflect", "keeper", "unprompted"};

static WpSeatInput script(const SimState *s, int held, int *own_ticks) {
    WpSeatInput in = {0, 0, 0};
    int me = s->seat_player[0];
    if (me < 0) return in;
    const WpPlayer *p = &s->players[me];
    if (s->ball.owner == me) {
        (*own_ticks)++;
        if (held) in.move_x = (int8_t)(team_dir(s, p->team) > 0 ? 127 : -127);
        if (*own_ticks % 30 == 15) in.buttons = WP_BTN_PASS;   /* a tap (pressed one tick); again every 0.5 s */
        return in;
    }
    *own_ticks = 0;
    if (!held) {                                      /* released - but a still loose ball at his feet he walks onto */
        fv2 d = fv2_sub(s->ball.pos, p->pos);
        if (s->ball.owner < 0 && !s->ball.held && fv2_len(s->ball.vel) < FX(0.5) && fv2_len(d) < FX(5.0) && fv2_len(d) > FX(0.2)) {
            d = fv2_norm(d);
            in.move_x = (int8_t)(d.x * 127 / FX_ONE); in.move_y = (int8_t)(d.y * 127 / FX_ONE);
        }
        return in;
    }
    double dx = (double)(s->ball.pos.x - p->pos.x) / 65536.0, dy = (double)(s->ball.pos.y - p->pos.y) / 65536.0;
    if (dx * dx + dy * dy < 0.04) return in;
    double a = atan2(dy, dx);
    int o = (int)lround(a / (M_PI / 4));
    double x = cos(o * M_PI / 4), y = sin(o * M_PI / 4);
    in.move_x = (int8_t)(fabs(x) < 0.1 ? 0 : (x > 0 ? 127 : -127));
    in.move_y = (int8_t)(fabs(y) < 0.1 ? 0 : (y > 0 ? 127 : -127));
    return in;
}

int main(int argc, char **argv) {
    int seeds = argc > 1 ? atoi(argv[1]) : 3, minutes = argc > 2 ? atoi(argv[2]) : 5;
    static SimState s;
    for (int held = 0; held < 2; held++) {
        long sim_reason[16] = {0}, nx_changes = 0, nx_aba = 0, q_changes = 0;
        long cnt[R_N] = {0}, aba_un = 0, aba_any = 0, ticks = 0, aba_un_gain = 0, ctx[4] = {0};
        for (int seed = 1; seed <= seeds; seed++) {
            WpMatchConfig cfg = sim_config_7v7();
            cfg.seed = (uint64_t)seed;
            cfg.half_ticks = 0;                       /* endless: no half time in the window */
            if (getenv("WP_STATS")) cfg.stats_mode = (uint8_t)atoi(getenv("WP_STATS"));
#ifdef WP_HAVE_SWITCH_MODE
            if (getenv("WP_SWITCH")) cfg.switch_mode = (uint8_t)atoi(getenv("WP_SWITCH"));
#endif
            sim_init(&s, &cfg);
            sim_kickoff_setpiece(&s);
            WpInput in; memset(&in, 0, sizeof in);
            int own = 0;
            int nx_prev = -1, nx_prev2 = -1, q_prev = -1; long nx_t = -1000;
            /* history of switches: (tick, from, to, reason) - the last two are enough for A->B->A */
            long h_tick[2] = {-1000, -1000}; int h_from[2] = {-1, -1}, h_to[2] = {-1, -1}, h_r[2] = {0, 0};
            for (long t = 0; t < 60L * 60 * minutes; t++) {
                in.seat[0] = script(&s, held, &own);
                int before = s.seat_player[0], phase0 = s.phase, ptgt0 = s.pass_target, pfrom0 = s.pass_from;
                int ours0 = ptgt0 >= 0 && s.players[pfrom0].team == 0;
                /* the ball before the step: 0 their carrier, 1 their pass in flight, 2 our pass in flight, 3 loose */
                int bctx = s.ball.owner >= 0 ? (s.players[s.ball.owner].team ? 0 : 3)
                         : ptgt0 >= 0 ? (ours0 ? 2 : 1) : 3;
                sim_step(&s, &in);
                ticks++;
#ifdef WP_HAVE_SWITCH_MODE
                int o = s.ball.owner, shown = s.phase == WP_PHASE_PLAY && s.seat_player[0] == before &&
                    !(o >= 0 && s.players[o].team == 0) && !(s.pass_target >= 0 && s.players[s.pass_from].team == 0);
                if (shown) {                          /* the indicator, while it is drawn (render.c's rule) */
                    int nx = s.sw_next[0];
                    if (nx != nx_prev && nx >= 0 && nx_prev >= 0) {
                        nx_changes++;
                        if (nx == nx_prev2 && t - nx_t <= 30) { nx_aba++; if (getenv("WP_NX_TRACE")) printf("nx ABA t%ld %d->%d->%d (prev change t%ld) age %d seat %d owner %d ptgt %d\n", t, nx_prev2, nx_prev, nx, nx_t, s.sw_next_age[0], s.seat_player[0], s.ball.owner, s.pass_target); }
                        nx_prev2 = nx_prev; nx_t = t;
                    }
                    if (nx >= 0) nx_prev = nx;
                    if (s.sw_q[0] != q_prev && s.sw_q[0] >= 0 && q_prev >= 0) q_changes++;
                    if (s.sw_q[0] >= 0) q_prev = s.sw_q[0];
                } else { nx_prev = nx_prev2 = q_prev = -1; }
#endif
                int now = s.seat_player[0];
                if (now == before || before < 0 || now < 0) continue;
                int r;
                if (phase0 != WP_PHASE_PLAY || s.phase != WP_PHASE_PLAY) r = R_RESTART;
                else if (s.pass_target == now && s.pass_from == before) r = R_PASS;
                else if (s.ball.owner == now) r = R_GAIN;
                else if (s.players[before].role == WP_ROLE_KEEPER) r = R_KEEPER;
                else if (ours0 && s.pass_target < 0 && s.ball.owner < 0) r = R_DEFLECT;
                else r = R_UNPROMPTED;
                cnt[r]++;
#ifdef WP_HAVE_SWITCH_MODE
                sim_reason[s.sw_reason[0] < 16 ? s.sw_reason[0] : 15]++;
                if (getenv("WP_SW_TRACE") && (t - h_tick[1] <= 60 || atoi(getenv("WP_SW_TRACE")) > 1))
                    printf("t%ld %d->%d r%d simr%d (prev %d->%d r%d at t%ld) owner %d ptgt %d\n", t, before, now, r, s.sw_reason[0],
                           h_from[1], h_to[1], h_r[1], h_tick[1], s.ball.owner, s.pass_target);
#endif
                if (r == R_UNPROMPTED) ctx[bctx]++;
                /* A->B->A: this switch goes back to the man the previous one left, within 60 ticks */
                if (h_to[1] == before && h_from[1] == now && t - h_tick[1] <= 60) {
                    if (r != R_PASS && r != R_RESTART && h_r[1] != R_PASS && h_r[1] != R_RESTART) aba_any++;
                    if (r == R_UNPROMPTED && h_r[1] == R_UNPROMPTED) aba_un++;
                    if ((r == R_UNPROMPTED || r == R_GAIN) && h_r[1] == R_UNPROMPTED) aba_un_gain++;
                }
                h_tick[0] = h_tick[1]; h_from[0] = h_from[1]; h_to[0] = h_to[1]; h_r[0] = h_r[1];
                h_tick[1] = t; h_from[1] = before; h_to[1] = now; h_r[1] = r;
            }
        }
        double mins = (double)ticks / 3600.0;
        printf("script %s (%d seeds x %d min, 7v7):", held ? "H stick held toward ball" : "R stick released", seeds, minutes);
        long tot = 0;
        for (int r = 0; r < R_N; r++) tot += cnt[r];
        printf(" switches %ld (%.1f/min)\n   ", tot, tot / mins);
        for (int r = 0; r < R_N; r++) printf(" %s %ld (%.2f/min)", RN[r], cnt[r], cnt[r] / mins);
        printf("\n    unprompted by ball state before it: their carrier %ld, their pass %ld, our pass %ld, loose %ld",
               ctx[0], ctx[1], ctx[2], ctx[3]);
#ifdef WP_HAVE_SWITCH_MODE
        printf("\n    sim reasons (WP_SW_*): pass %ld gain %ld deflect %ld keeper %ld restart %ld manual %ld/%ld event %ld override %ld",
               sim_reason[WP_SW_PASS], sim_reason[WP_SW_GAIN], sim_reason[WP_SW_DEFLECT], sim_reason[WP_SW_KEEPER],
               sim_reason[WP_SW_RESTART], sim_reason[WP_SW_MANUAL], sim_reason[WP_SW_MANUAL_DIR], sim_reason[WP_SW_EVENT],
               sim_reason[WP_SW_OVERRIDE]);
        printf("\n    indicator: oracle (sw_next) changes %.1f/min, A->B->A within 30 ticks %ld; Q target (sw_q) changes %.1f/min",
               nx_changes / mins, nx_aba, q_changes / mins);
#endif
        printf("\n    A->B->A within 60 ticks: unprompted both %ld, unprompted then gain %ld, any non-pass %ld\n",
               aba_un, aba_un_gain, aba_any);
    }
    return 0;
}
