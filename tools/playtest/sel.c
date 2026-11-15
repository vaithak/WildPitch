/* sel.c - pass receiver selection sweep (rebuilt from the audit's method, research_notes/Pass receiver selection/
 * wildpitch_audit.md). Real 5v5 (team.c moves everyone), seat 0 on carrier c in {1,3,4}, teleported to a grid of
 * start points with the ball, settles, then dribbles along 4 paths; every 10 owned ticks, for each of the 8 keyboard
 * directions: pass_pick() + a simulated tap (J 3 ticks, release, keep the key) on a copy of the state.
 * usage: sel [opp_on=1] [settle=60]
 * build: cc -std=c17 -O2 -Isrc -Itests -Itools/playtest tools/playtest/sel.c build/libwp_sim.a -lm -o /tmp/sel */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

static const int8_t KEYS[8][2] = {{127,0},{127,127},{0,127},{-127,127},{-127,0},{-127,-127},{0,-127},{127,-127}};
static int opp_on = 1, settle = 60;

static void park_opp(SimState *s) {
    if (opp_on) return;
    for (int j = s->n_per_side; j < 2 * s->n_per_side; j++) s->players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
}
static void step(SimState *s, WpSeatInput in) { WpInput w; memset(&w, 0, sizeof w); w.seat[0] = in; sim_step(s, &w); park_opp(s); }
static double deg(int32_t a) { return fabs((double)a * 360.0 / 65536.0); }

int main(int argc, char **argv) {
    if (argc > 1) opp_on = atoi(argv[1]);
    if (argc > 2) settle = atoi(argv[2]);
    static SimState s, c;
    long presses = 0, picks = 0, untargeted = 0, unt_out = 0, tgt = 0, tgt_recv = 0, tgt_opp = 0;
    long reach_n = 0, reach_ok = 0, vis_n = 0, vis_ok = 0, flick_n = 0, flick_ch = 0, prev_n = 0, prev_ch = 0, raw_ch = 0;
    static const int carriers[3] = {1, 3, 4};
    for (int ci = 0; ci < 3; ci++)
    for (int xi = 0; xi < 5; xi++) for (int yi = 0; yi < 3; yi++) for (int path = 0; path < 4; path++) {
        WpMatchConfig cfg = sim_default_config();
        sim_init(&s, &cfg);
        int me = carriers[ci];
        s.seat_player[0] = (int8_t)me;
        s.players[me].pos = fv2_make(FX_INT(-14 + xi * 6), FX_INT(-8 + yi * 8));
        s.ball.pos = fv2_add(s.players[me].pos, fv2_make(FX(0.4), 0));
        park_opp(&s);
        for (int t = 0; t < settle; t++) step(&s, (WpSeatInput){0, 0, 0});
        int prev_pick[8]; for (int k = 0; k < 8; k++) prev_pick[k] = -9;
        int last_prev = -9, last_raw = -9;
        for (int t = 0; t < 150; t++) {
            int8_t mx = 127, my = 0;
            if (path == 1) { mx = 0; my = 127; } else if (path == 2) { mx = 0; my = -127; }
            else if (path == 3) { my = (int8_t)(((t / 50) & 1) ? 127 : -127); }
            step(&s, (WpSeatInput){mx, my, 0});
            if (s.ball.owner != me || s.phase != WP_PHASE_PLAY) break;
            {   /* the preview ring (sticky) vs a stateless pick, with the dribble stick held */
                int pr = s.pick_recv[0], raw = pass_pick(&s, me, s.pick_dir[0]);
                if (last_prev != -9) { prev_n++; prev_ch += pr != last_prev; raw_ch += raw != last_raw; }
                last_prev = pr; last_raw = raw;
            }
            for (int k = 0; k < 8; k++) {          /* per-tick stability with the key held */
                wp_angle a = fx_atan2(FX_INT(KEYS[k][1]), FX_INT(KEYS[k][0]));
                int pk = pass_pick(&s, me, a);
                if (prev_pick[k] != -9) { flick_n++; flick_ch += pk != prev_pick[k]; }
                prev_pick[k] = pk;
            }
            if (t % 10) continue;
            /* reachability: every outfield mate 2..26 m away; nearest key -> him? analogue at visible bearing -> him? */
            for (int j = 0; j < s.n_per_side; j++) {
                if (j == me || s.players[j].role == WP_ROLE_KEEPER) continue;
                fv2 d = fv2_sub(s.players[j].pos, s.players[me].pos);
                double dist = sqrt((double)d.x * d.x + (double)d.y * d.y) / 65536.0;
                if (dist < 2 || dist > 26) continue;
                wp_angle b = fx_atan2(d.y, d.x);
                int kb = (int)(((uint32_t)b + 4096) / 8192) & 7;
                reach_n++; reach_ok += pass_pick(&s, me, fx_atan2(FX_INT(KEYS[kb][1]), FX_INT(KEYS[kb][0]))) == j;
                vis_n++; vis_ok += pass_pick(&s, me, b) == j;
            }
            for (int k = 0; k < 8; k++) {
                presses++;
                wp_angle a = fx_atan2(FX_INT(KEYS[k][1]), FX_INT(KEYS[k][0]));
                int pk = pass_pick(&s, me, a);
                picks += pk >= 0;
                c = s;                              /* simulate the real tap on a copy */
                for (int u = 0; u < 3; u++) { WpInput w; memset(&w, 0, sizeof w); w.seat[0] = (WpSeatInput){KEYS[k][0], KEYS[k][1], WP_BTN_PASS}; sim_step(&c, &w); }
                int kicked = 0, to = -1, out = 0, got = -1;
                for (int u = 0; u < 300; u++) {
                    WpInput w; memset(&w, 0, sizeof w); w.seat[0] = (WpSeatInput){KEYS[k][0], KEYS[k][1], 0};
                    sim_step(&c, &w);
                    if (!opp_on) for (int j = c.n_per_side; j < 2 * c.n_per_side; j++) c.players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
                    if (!kicked && (c.events & WP_EV_KICK)) { kicked = 1; to = c.players[me].kick.to - 1; continue; }
                    if (!kicked) continue;
                    if (c.events & (WP_EV_OUT | WP_EV_GOAL)) { out = 1; break; }
                    if (c.ball.owner >= 0) { got = c.ball.owner; break; }
                }
                if (!kicked) continue;
                if (to < 0) { untargeted++; unt_out += out; }
                else { tgt++; tgt_recv += got == to; tgt_opp += got >= s.n_per_side; }
            }
        }
    }
    printf("sel opp_on=%d settle=%d: presses %ld\n", opp_on, settle, presses);
    printf("  keyboard pick rate            %5.1f%%\n", 100.0 * picks / presses);
    printf("  untargeted taps out of play   %5.1f%% (of %ld)\n", 100.0 * unt_out / (untargeted ? untargeted : 1), untargeted);
    printf("  targeted taps received        %5.1f%%, intercepted %4.1f%% (of %ld)\n", 100.0 * tgt_recv / (tgt ? tgt : 1), 100.0 * tgt_opp / (tgt ? tgt : 1), tgt);
    printf("  nearest key reaches mate      %5.1f%% (of %ld)\n", 100.0 * reach_ok / reach_n, reach_n);
    printf("  analogue at visible bearing   %5.1f%%\n", 100.0 * vis_ok / vis_n);
    printf("  pick change per tick (key held) %.2f%%  (~x12 = within a tap)\n", 100.0 * flick_ch / flick_n);
    printf("  preview ring change per tick while dribbling %.2f%% (stateless pick %.2f%%)\n", 100.0 * prev_ch / prev_n, 100.0 * raw_ch / prev_n);
    (void)deg;
    return 0;
}
