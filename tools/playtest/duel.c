/* duel.c - scripted cross duels (CP0.5a stage 4, D-051): an AI crosser on the wing lofts the ball to a team-mate in
 * the box (as the brain's cross does), with one marker placed on a grid around the receiver (3 receiver spots x 24
 * offsets of 0.5 m steps); everyone else is far away. Counts who makes the first contact: the attackers ("won") or
 * the marker ("lost"). A clean, low-noise measure of the aerial duel (match harnesses mix it with everything else).
 * usage: duel [v]     (v = one line per setup)
 * build: cc -std=c17 -O2 -Isrc -Itests -Itools/playtest tools/playtest/duel.c build/libwp_sim.a -lm -o /tmp/duel */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

int main(int argc, char **argv) {
    int verbose = argc > 1;
    (void)argv;
    static SimState s;
    int won = 0, lost = 0, other = 0;
    for (int ry = -2; ry <= 2; ry += 2)
        for (int dx = -2; dx <= 2; dx++)
            for (int dy = -2; dy <= 2; dy++) {
                if (!dx && !dy) continue;
                WpMatchConfig cfg = sim_config_7v7();
                cfg.seed = 7;
                sim_init(&s, &cfg);
                for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
                s.phase = WP_PHASE_PLAY;
                s.restart_kind = 0;
                int n = s.n_per_side, sg = team_dir(&s, 0);
                fx hl = s.pitch.length / 2, hw = s.pitch.width / 2;
                for (int i = 0; i < 2 * n; i++) {                      /* everyone far away ... */
                    s.players[i].pos = fv2_make(-sg * (hl - FX(12.0)) + FX_INT(i), FX_INT(i - 7));
                    s.players[i].vel = fv2_make(0, 0);
                }
                s.players[n].pos = fv2_make(sg * (hl - FX(1.0)), 0);   /* ... but the keepers */
                s.players[0].pos = fv2_make(-sg * (hl - FX(1.0)), 0);
                int cr = 1, rc = 2, mk = n + 1;
                s.players[cr].pos = fv2_make(sg * (hl - FX(8.0)), hw - FX(3.0));
                s.players[rc].pos = fv2_make(sg * (hl - FX(9.0)), FX_INT(ry));
                s.players[mk].pos = fv2_add(s.players[rc].pos, fv2_make(sg * dx * FX(0.5), dy * FX(0.5)));
                s.ball.pos = fv2_add(s.players[cr].pos, fv2_make(0, -FX(0.4)));
                s.ball.vel = fv2_make(0, 0);
                s.ball.z = s.ball.vz = 0;
                s.ball.owner = (int8_t)cr;
                s.ball.held = 0;
                fv2 d = fv2_sub(s.players[rc].pos, s.ball.pos);
                s.players[cr].facing = fx_atan2(d.y, d.x);
                WpKick k = {0};
                k.kind = WP_KICK_LOB; k.dir = fx_atan2(d.y, d.x); k.to = (uint8_t)(rc + 1); k.foot = 1; k.windup = T_KICK_WINDUP_CHIP;
                sim_player_kick(&s, cr, &k);
                WpInput in; memset(&in, 0, sizeof in);
                int kicked = 0, res = -1;
                const char *how = "none";
                for (int t = 0; t < 240; t++) {
                    int po = s.ball.owner;
                    sim_step(&s, &in);
                    if ((s.events & WP_EV_KICK) && !kicked) { kicked = 1; continue; }
                    if (!kicked) continue;
                    if (s.ball.owner >= 0 && s.ball.owner != po && s.ball.owner != cr) { res = s.ball.owner; how = "gain"; break; }
                    if (s.events & WP_EV_HEADER) { res = s.ball.last_touch; how = "head"; break; }
                    if ((s.events & WP_EV_BODY) && s.ball.last_touch != cr) { res = s.ball.last_touch; how = "body"; break; }
                    if (s.events & (WP_EV_OUT | WP_EV_GOAL)) break;
                }
                if (res >= 0 && s.players[res].team == 0) won++;
                else if (res == mk) lost++;
                else other++;
                if (verbose) printf("receiver y %+d, marker %+.1f / %+.1f m: %s by %d\n", ry, dx * 0.5, dy * 0.5, how, res);
            }
    printf("cross duels: attackers won %d, marker won %d, other %d\n", won, lost, other);
    return 0;
}
