/* D-054 (owner: "goalkeeper facing in random direction sometimes, like facing its own goal"): an AI keeper who isn't
 * diving, catching or on the ball keeps facing the ball - he backpedals and shuffles. Measured before the fix: 30% of
 * his ticks faced away from the ball (he faced where he was walking). */
#include <stdio.h>
#include <string.h>
#include "test_util.h"
#include "sim/sim.h"
#include "sim/sim_internal.h"

static SimState s;

int main(void) {
    long away = 0, tot = 0;
    for (int m = 0; m < 2; m++) {
        WpMatchConfig c = sim_config_7v7();
        c.seed = (uint32_t)(m + 1);
        sim_init(&s, &c);
        sim_kickoff_setpiece(&s);
        for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
        WpInput in;
        memset(&in, 0, sizeof in);
        for (int t = 0; t < 60 * 120; t++) {
            sim_step(&s, &in);
            for (int tm = 0; tm < 2; tm++) {
                int g = tm * s.n_per_side;
                const WpPlayer *p = &s.players[g];
                if (p->action != WP_ACT_NONE || s.ball.owner == g || s.phase != WP_PHASE_PLAY) continue;
                tot++;
                if (fv2_dot(fv2_sub(s.ball.pos, p->pos), fv2_from_angle(p->facing, FX_ONE)) < 0) away++;
            }
        }
    }
    printf("keeper ticks in play %ld, facing away from the ball %ld\n", tot, away);
    CHECK(tot > 1000);
    CHECK(away * 100 < tot);                              /* < 1% (turning after a switch of play, a dive's get-up) */
    return TEST_DONE();
}
