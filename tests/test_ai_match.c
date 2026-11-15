/* CP0.3 instinct AI: AI vs AI matches (no humans). Identical teams must come out balanced (a big imbalance = a
 * code-order or mirror bug), the game must look like football (shots, goals, passes that mostly arrive, tackles),
 * and nobody may get stuck. Start states are nudged after every reset (harness only) so self-play doesn't replay. */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

static int run_format(int seven, int matches);
int main(void) {
    run_format(0, 12);
    run_format(1, 16);
    return TEST_DONE();
}

static int run_format(int seven, int matches) {
    static SimState s;
    long goals[2] = {0}, shots[2] = {0}, passes[2] = {0}, done[2] = {0}, tackles[2] = {0}, poss[2] = {0};
    long stuck = 0, ticks = 0;
    for (int m = 0; m < matches; m++) {
        WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
        sim_init(&s, &cfg);
        for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
        s.ball.pos = fv2_make(FX_INT((m % 5) - 2), FX_INT((m % 3) - 1));
        WpRng hr; wp_rng_seed(&hr, (uint64_t)(1000 + m), 7);
        WpInput in; memset(&in, 0, sizeof in);
        int np = 2 * s.n_per_side;
        int last_kicker = -1, last_kind = 0, still[WP_MAX_PLAYERS] = {0};
        fv2 last_pos[WP_MAX_PLAYERS];
        for (int i = 0; i < np; i++) last_pos[i] = s.players[i].pos;
        for (long t = 0; t < 60L * 60 * 3; t++) {
            sim_step(&s, &in);
            ticks++;
            if (s.events & WP_EV_RESET)
                for (int i = 0; i < np; i++) {
                    fx dx = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                    fx dy = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                    s.players[i].pos = fv2_add(s.players[i].pos, fv2_make(dx, dy));
                }
            if (s.ball.owner >= 0) poss[s.players[s.ball.owner].team]++;
            if (s.events & WP_EV_GOAL) goals[s.players[s.ball.last_touch >= 0 ? s.ball.last_touch : 0].team]++;
            if (s.events & WP_EV_TACKLE) tackles[s.players[s.ball.last_touch].team]++;
            if (s.events & WP_EV_KICK) {
                last_kicker = s.ball.last_touch; last_kind = s.players[last_kicker].kick.kind;
                int tm = s.players[last_kicker].team;
                if (last_kind == WP_KICK_SHOT) shots[tm]++;
                if (last_kind == WP_KICK_PASS || last_kind == WP_KICK_THROUGH) passes[tm]++;
            }
            if (last_kicker >= 0 && s.ball.owner >= 0 && s.ball.owner != last_kicker) {
                if ((last_kind == WP_KICK_PASS || last_kind == WP_KICK_THROUGH) &&
                    s.players[s.ball.owner].team == s.players[last_kicker].team) done[s.players[last_kicker].team]++;
                last_kicker = -1;
            }
            if (t % 60 == 0) for (int i = 0; i < np; i++) {
                if (s.players[i].role == WP_ROLE_KEEPER) continue;
                /* stuck = not moving for 10 s while > 3 m from where he is trying to go (a marker holding his spot is fine) */
                if (fv2_len(fv2_sub(s.players[i].pos, last_pos[i])) < FX(0.3) &&
                    fv2_len(fv2_sub(s.players[i].pos, s.players[i].ai_goal)) > FX(3.0)) { if (++still[i] == 10) { stuck++;
                    printf("  stuck: match %d t%ld player %d act %d at (%.1f,%.1f) goal (%.1f,%.1f) ball (%.1f,%.1f) owner %d\n", m, t, i,
                           s.players[i].ai_act, tod(s.players[i].pos.x), tod(s.players[i].pos.y), tod(s.players[i].ai_goal.x),
                           tod(s.players[i].ai_goal.y), tod(s.ball.pos.x), tod(s.ball.pos.y), s.ball.owner); } }
                else still[i] = 0;
                last_pos[i] = s.players[i].pos;
            }
        }
    }
    double mins = (double)ticks / 3600.0;
    long G = goals[0] + goals[1], S = shots[0] + shots[1], P = passes[0] + passes[1], D = done[0] + done[1];
    printf("AI vs AI %s, %d x 3 min: goals %ld-%ld (%.2f/min), shots %ld-%ld, passes %ld (%.0f%% arrive), tackles %ld-%ld, "
           "possession %.0f%%-%.0f%%, stuck %ld\n", seven ? "7v7" : "5v5", matches, goals[0], goals[1], (double)G / mins, shots[0], shots[1], P, 100.0 * (double)D / (double)(P ? P : 1),
           tackles[0], tackles[1], 100.0 * (double)poss[0] / (double)(poss[0] + poss[1]), 100.0 * (double)poss[1] / (double)(poss[0] + poss[1]), stuck);
    CHECKF((double)G / mins > 0.25 && (double)G / mins < 2.0, "goal rate %.2f/min", (double)G / mins);
    CHECKF((double)S / mins > 1.0, "shot rate %.2f/min", (double)S / mins);
    CHECKF(D * 100 > P * 50, "only %ld/%ld passes arrive", D, P);
    CHECKF(stuck == 0, "%ld stuck players", stuck);
    long ps = poss[0] + poss[1];
    CHECKF(poss[0] * 100 > ps * 40 && poss[1] * 100 > ps * 40, "possession imbalance");
    /* identical teams: the shot difference must be within ~2.5 sigma of chance (|a-b| <= 2.5 sqrt(a+b) + 4), a
     * real order/mirror bias (CP0.3a: 75-192) is far outside it; small samples stay meaningful */
    long dshots = shots[0] > shots[1] ? shots[0] - shots[1] : shots[1] - shots[0], sum = shots[0] + shots[1];
    CHECKF((double)dshots <= 2.5 * sqrt((double)sum) + 4.0, "shot imbalance %ld-%ld", shots[0], shots[1]);
    return 0;
}
