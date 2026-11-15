/* shots.c - live shot outcomes: our striker gets the ball 7-16 m from goal, charges (8/20/32 ticks) toward goal, releases.
 * usage: shots [seven=0]. Tallies goal / keeper catch / keeper body block / defender block / defender control / out. */
/* live: our striker gets the ball at (x,y), charges a shot (stick toward goal) and releases; tally outcomes */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"
static SimState s;
static uint16_t st(WpSeatInput a) { WpInput in; memset(&in, 0, sizeof in); in.seat[0] = a; sim_step(&s, &in); return s.events; }
int main(int argc, char **argv) {
    int seven = argc > 1 && atoi(argv[1]);
    WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
    int kc[3] = {0}, kb[3] = {0}, gl[3] = {0}, goal = 0, keeper = 0, body = 0, ctrl = 0, wide = 0, tackled = 0, other = 0, n = 0;
    double spd_ctrl = 0;
    for (int xi = 0; xi < 4; xi++) for (int yi = 0; yi < 5; yi++) for (int ch = 0; ch < 3; ch++) for (int set = 0; set < 3; set++) {
        sim_init(&s, &cfg);
        int me = s.seat_player[0];
        for (int t = 0; t < 30 + set * 40; t++) st(NOIN);            /* let the AI settle into defending positions */
        s.players[me].pos = fv2_make(s.pitch.length / 2 - FX_INT(16 - xi * 3), FX_INT(-6 + yi * 3));   /* 4..13 m from goal */
        s.ball.pos = fv2_add(s.players[me].pos, fv2_make(FX(0.4), 0)); s.ball.vel = fv2_make(0, 0); s.ball.owner = (int8_t)me;
        s.ball.last_touch = (int8_t)me;
        int charge = 8 + ch * 12;
        uint16_t ev = 0;
        for (int t = 0; t < charge; t++) ev |= st((WpSeatInput){127, 0, WP_BTN_SHOOT});
        if (s.ball.owner != me) { tackled++; n++; continue; }
        ev = st((WpSeatInput){127, 0, 0});
        int kicked = 0, res = -1;
        for (int t = 0; t < 120 && res < 0; t++) {
            uint16_t e = st(NOIN);
            if (!kicked) { if (e & WP_EV_KICK) kicked = 1; else if (s.ball.owner != me) { res = 5; break; } continue; }
            if (e & WP_EV_GOAL) res = 0;
            else if (s.ball.owner >= 0) {
                int o = s.ball.owner;
                if (s.players[o].role == WP_ROLE_KEEPER && s.players[o].team == 1) res = 1;
                else if (s.players[o].team == 1) { res = 3; }
                else res = 6;
            } else if (e & WP_EV_OUT) res = 4;
            else if ((e & WP_EV_BODY) && s.ball.last_touch >= 0 && s.players[s.ball.last_touch].team == 1 && s.players[s.ball.last_touch].role != WP_ROLE_KEEPER) res = 2;
            else if ((e & WP_EV_BODY) && s.ball.last_touch >= 0 && s.players[s.ball.last_touch].team == 1) res = 7;
        }
        n++;
        if (res == 1) kc[ch]++;
        if (res == 7) { kb[ch]++; res = 1; }
        if (res == 0) gl[ch]++;
        if (res == 0) goal++; else if (res == 1) keeper++; else if (res == 2) body++; else if (res == 3) ctrl++;
        else if (res == 4) wide++; else if (res == 5) tackled++; else other++;
    }
    for (int c = 0; c < 3; c++) printf("charge %d: goals %d, keeper catches %d, keeper body blocks %d\n", 8 + c * 12, gl[c], kc[c], kb[c]);
    printf("%d live shots: goal %d, keeper %d, blocked by a defender's body %d, defender controlled it %d, wide/out %d, "
           "tackled before the strike %d, other %d\n", n, goal, keeper, body, ctrl, wide, tackled, other);
}
