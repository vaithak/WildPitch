/* mirror.c - symmetry check: a state and its exact mirror (teams swapped, pitch rotated 180 deg) stepped side by side;
 * reports the first tick/player where they diverge. Divergence early + different actions = an order/index bias.
 * usage: mirror [seven=0] */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"
static SimState A, B;
static int mi(int i, int n) { return i < n ? i + n : i - n; }
static fv2 neg(fv2 v) { return fv2_make(-v.x, -v.y); }
int main(int argc, char **argv) {
    int seven = argc > 1 && atoi(argv[1]);
    WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
    sim_init(&A, &cfg);
    for (int k = 0; k < WP_MAX_SEATS; k++) A.seat_player[k] = -1;
    A.ball.pos = fv2_make(FX(1.0), FX(0.5));
    B = A;
    int n = A.n_per_side;
    for (int i = 0; i < 2 * n; i++) {
        WpPlayer p = A.players[i];
        p.pos = neg(p.pos); p.vel = neg(p.vel); p.facing = (wp_angle)(p.facing + 32768);
        p.team = (uint8_t)(1 - p.team);
        B.players[mi(i, n)] = p;
    }
    B.ball.pos = neg(A.ball.pos);
    WpInput in; memset(&in, 0, sizeof in);
    for (int t = 0; t < 3000; t++) {
        sim_step(&A, &in); sim_step(&B, &in);
        fx worst = 0; int wi = -1;
        for (int i = 0; i < 2 * n; i++) {
            fx d = fv2_len(fv2_sub(A.players[i].pos, neg(B.players[mi(i, n)].pos)));
            if (d > worst) { worst = d; wi = i; }
        }
        fx db = fv2_len(fv2_sub(A.ball.pos, neg(B.ball.pos)));
        if (worst > FX(0.01) || db > FX(0.01)) {
            printf("diverged at tick %d: player %d by %.3f (act A %d B %d), ball by %.3f, owners %d/%d\n", t, wi, tod(worst),
                   A.players[wi].ai_act, B.players[mi(wi, n)].ai_act, tod(db), A.ball.owner, B.ball.owner);
            return 0;
        }
    }
    printf("no divergence in 3000 ticks\n");
}
