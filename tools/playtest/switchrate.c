/* switchrate.c - how often control of the human seat changes hands without being asked (CP0.5b, D-050 acceptance).
 * 7v7 (sim_config_7v7: assist Normal, the game's switching mode), seat 0 on team 0, everyone else AI, two scripts:
 *   R = stick released (the human only taps PASS when he has the ball, ~0.25 s after getting it),
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
        if (*own_ticks == 15) in.buttons = WP_BTN_PASS;   /* a tap: pressed one tick, released the next */
        return in;
    }
    *own_ticks = 0;
    if (!held) return in;
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
        printf("\n    A->B->A within 60 ticks: unprompted both %ld, unprompted then gain %ld, any non-pass %ld\n",
               aba_un, aba_un_gain, aba_any);
    }
    return 0;
}
