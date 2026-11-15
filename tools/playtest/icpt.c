/* icpt.c - live human taps to the ringed receiver: received / intercepted / out, launch speed.
 * usage: icpt [clear_lanes_only=0] [seven=0] */
/* live human taps to the previewed receiver (ring) when the ring shows someone: speed, flight, outcome */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"
static SimState s, c;
int main(int argc, char **argv) {
    int clear_only = argc > 1 ? atoi(argv[1]) : 0, seven = argc > 2 && atoi(argv[2]);
    WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
    long n = 0, ok = 0, opp = 0, out = 0, lane_ok_n = 0, lane_ok_opp = 0; double v0s = 0, flight = 0;
    static const int8_t K[8][2] = {{127,0},{127,127},{0,127},{-127,127},{-127,0},{-127,-127},{0,-127},{127,-127}};
    for (int g = 0; g < 30; g++) {
        sim_init(&s, &cfg);
        WpRng r; wp_rng_seed(&r, (uint64_t)g + 5, 3);
        WpSeatInput in = {127, 0, 0};
        for (int t = 0; t < 60 * 60; t++) {
            int me = s.seat_player[0];
            if (s.ball.owner != me) { fv2 d = fv2_sub(s.ball.pos, s.players[me].pos);
                in.move_x = (int8_t)(d.x > FX(0.3) ? 127 : d.x < -FX(0.3) ? -127 : 0); in.move_y = (int8_t)(d.y > FX(0.3) ? 127 : d.y < -FX(0.3) ? -127 : 0); }
            else if (t % 30 == 0) { uint32_t k = wp_rng_range(&r, 8); in.move_x = K[k][0]; in.move_y = K[k][1]; }
            WpInput w; memset(&w, 0, sizeof w); w.seat[0] = in; sim_step(&s, &w);
            if (s.ball.owner == me && s.pick_recv[0] >= 0 && t % 20 == 0 && s.phase == WP_PHASE_PLAY) {
                c = s;
                int to = c.pick_recv[0];
                fx lane = pass_lane_clear(&c, 0, c.ball.pos, c.players[to].pos);
                if (clear_only && lane < FX(1.5)) continue;
                WpInput a; memset(&a, 0, sizeof a); a.seat[0] = in; a.seat[0].buttons = WP_BTN_PASS;
                sim_step(&c, &a); sim_step(&c, &a);
                a.seat[0].buttons = 0;
                int kicked = -1, got = -1; fx v0 = 0;
                for (int u = 0; u < 300; u++) {
                    sim_step(&c, &a);
                    if (kicked < 0 && (c.events & WP_EV_KICK)) { kicked = u; v0 = fv2_len(c.ball.vel); continue; }
                    if (kicked < 0) continue;
                    if (c.events & (WP_EV_OUT | WP_EV_GOAL)) { out++; break; }
                    if (c.ball.owner >= 0) { got = c.ball.owner; break; }
                }
                if (kicked < 0) continue;
                n++; v0s += tod(v0);
                if (got == to) { ok++; flight += 0; }
                if (got >= 0 && c.players[got].team == 1) opp++;
                if (lane >= FX(1.5)) { lane_ok_n++; lane_ok_opp += got >= 0 && c.players[got].team == 1; }
            }
        }
    }
    printf("live taps to the ringed mate: %ld; received by him %.0f%%, intercepted %.0f%% (lanes >= 1.5 m clear: %.0f%% of %ld), out %ld; mean launch %.1f m/s\n",
           n, 100.0 * ok / n, 100.0 * opp / n, 100.0 * lane_ok_opp / (lane_ok_n ? lane_ok_n : 1), lane_ok_n, out, v0s / n);
}
