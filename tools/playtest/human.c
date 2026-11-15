/* human.c - scripted human vs AI: dispossession standing/jogging/sprinting, who gets a won tackle, kickoff race.
 * usage: human [seven=0] */
/* scripted human vs AI: (a) carry the ball jogging/sprinting/standing - how often does blue win it? who gets it
 * after a won tackle? (b) kickoff race (c) keeper high catch */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"
static SimState s;
static uint16_t st(WpSeatInput a) { WpInput in; memset(&in, 0, sizeof in); in.seat[0] = a; sim_step(&s, &in); return s.events; }
int main(int argc, char **argv) {
    int seven = argc > 1 && atoi(argv[1]);
    WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
    for (int mode = 0; mode < 3; mode++) {       /* 0 stand still with it, 1 jog, 2 sprint */
        int turnovers = 0, tackles = 0, after_tackle_us = 0, after_tackle_them = 0, owned = 0;
        for (int run = 0; run < 10; run++) {
            sim_init(&s, &cfg);
            int me = s.seat_player[0];
            s.players[me].pos = fv2_make(-FX_INT(run), FX_INT(run % 5 - 2)); s.ball.pos = fv2_add(s.players[me].pos, fv2_make(FX(0.4), 0));
            s.ball.last_touch = (int8_t)me;
            int pending = -1;
            for (int t = 0; t < 60 * 6; t++) {
                me = s.seat_player[0];
                int8_t mx = 0, my = 0; uint16_t b = 0;
                if (s.ball.owner == me && mode) { mx = 127; my = (int8_t)(((t / 40) & 1) ? 60 : -60); if (mode == 2) b = WP_BTN_SPRINT; }
                else if (s.ball.owner != me) { fv2 d = fv2_sub(s.ball.pos, s.players[me].pos);
                    mx = (int8_t)(d.x > FX(0.3) ? 127 : d.x < -FX(0.3) ? -127 : 0); my = (int8_t)(d.y > FX(0.3) ? 127 : d.y < -FX(0.3) ? -127 : 0); }
                int before = s.ball.owner;
                uint16_t ev = st((WpSeatInput){mx, my, b});
                if (s.ball.owner >= 0 && s.players[s.ball.owner].team == 0) owned++;
                if (ev & WP_EV_TACKLE) { tackles++; pending = 40; }
                if (pending > 0 && s.ball.owner >= 0) { if (s.players[s.ball.owner].team == 0) after_tackle_us++; else after_tackle_them++; pending = -1; }
                else if (pending > 0) pending--;
                if (before >= 0 && s.players[before].team == 0 && s.ball.owner >= 0 && s.players[s.ball.owner].team == 1) turnovers++;
                if (s.phase != WP_PHASE_PLAY) break;
            }
        }
        printf("%s with the ball (10 x 6 s): blue won it %d times (%d by tackle; after a tackle: us %d / them %d), we owned %.0f%%\n",
               mode == 0 ? "standing" : mode == 1 ? "jogging" : "sprinting", turnovers, tackles, after_tackle_us, after_tackle_them, owned / 36.0);
    }
    /* kickoff race: who touches it first if the human reacts after 0.3 s */
    int us = 0, them = 0;
    for (int k = 0; k < 2; k++) {
        sim_init(&s, &cfg);
        s.kickoff_team = (uint8_t)k; /* placement already done for team 0; just test team-0 kickoff */
        for (int t = 0; t < 120 && s.ball.last_touch < 0; t++) {
            int me = s.seat_player[0];
            fv2 d = fv2_sub(s.ball.pos, s.players[me].pos);
            WpSeatInput in = t < 18 ? NOIN : (WpSeatInput){(int8_t)(d.x > 0 ? 127 : -127), 0, 0};
            st(in);
        }
        if (s.ball.last_touch >= 0) { if (s.players[s.ball.last_touch].team == 0) us++; else them++; }
    }
    printf("our kickoff, human reacts after 0.3 s: first touch us %d / them %d\n", us, them);
    return 0;
}
