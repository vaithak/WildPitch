/* aimatch.c - AI vs AI matches (no human seats) to judge the instinct brain: goals, shots, passes, tackles,
 * possession per team (the two teams are identical: a big imbalance = a bias bug), outs, stuck players.
 * Start states are varied per match (harness-only nudges) so self-play doesn't replay one sequence.
 * usage: aimatch [matches=6] [minutes=5] [seven=0]
 * build: cc -std=c17 -O2 -Isrc -Itests -Itools/playtest tools/playtest/aimatch.c build/libwp_sim.a -lm -o /tmp/aimatch */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

int main(int argc, char **argv) {
    int matches = argc > 1 ? atoi(argv[1]) : 6, minutes = argc > 2 ? atoi(argv[2]) : 5, seven = argc > 3 && atoi(argv[3]);
    static SimState s;
    long goals[2] = {0}, shots[2] = {0}, passes[2] = {0}, completed[2] = {0}, tackles[2] = {0}, poss[2] = {0};
    long outs = 0, ticks = 0, stuck = 0, kicks[2] = {0}, out_kind[8] = {0}, third[2] = {0};
    for (int m = 0; m < matches; m++) {
        WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
    if (getenv("WP_STATS")) cfg.stats_mode = (uint8_t)atoi(getenv("WP_STATS"));   /* 0 neutral, 1 mirrored, 2 own */
        cfg.seed = (uint64_t)(m + 1);
        sim_init(&s, &cfg);
        for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
        s.kickoff_team = (uint8_t)(m & 1);
        s.ball.pos = fv2_make(FX_INT((m % 5) - 2), FX_INT((m % 3) - 1));   /* nudge the start */
        WpInput in; memset(&in, 0, sizeof in);
        WpRng hr; wp_rng_seed(&hr, (uint64_t)(1000 + m), 7);     /* harness-only nudges after every reset */
        int np = 2 * s.n_per_side;
        int last_kicker = -1, last_kind = 0; fv2 last_pos[WP_MAX_PLAYERS]; int still[WP_MAX_PLAYERS] = {0};
        for (int i = 0; i < np; i++) last_pos[i] = s.players[i].pos;
        for (long t = 0; t < 60L * 60 * minutes; t++) {
            sim_step(&s, &in);
            ticks++;
            if (s.events & WP_EV_RESET) {
                for (int i = 0; i < 2 * s.n_per_side; i++) {
                    fx dx = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                    fx dy = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                    s.players[i].pos = fv2_add(s.players[i].pos, fv2_make(dx, dy));
                }
            }
            if (s.ball.owner >= 0) poss[s.players[s.ball.owner].team]++;
            if (s.events & WP_EV_GOAL) goals[s.ball.last_touch >= 0 ? s.players[s.ball.last_touch].team : 0]++;
            if (s.events & WP_EV_OUT) { outs++; out_kind[last_kicker >= 0 ? last_kind + 1 : 0]++; }
            if (s.ball.owner >= 0) { int tm = s.players[s.ball.owner].team; if (s.ball.pos.x * (tm ? -1 : 1) > FX(7.0)) third[tm]++; }
            if (s.events & WP_EV_TACKLE) tackles[s.players[s.ball.last_touch].team]++;
            if (s.events & WP_EV_KICK) {
                int k = s.ball.last_touch, tm = s.players[k].team;
                kicks[tm]++;
                last_kicker = k; last_kind = s.players[k].kick.kind;
                if (last_kind == WP_KICK_SHOT) shots[tm]++;
                if (last_kind == WP_KICK_PASS || last_kind == WP_KICK_THROUGH) passes[tm]++;
            }
            if (last_kicker >= 0 && s.ball.owner >= 0 && s.ball.owner != last_kicker) {
                if ((last_kind == WP_KICK_PASS || last_kind == WP_KICK_THROUGH) &&
                    s.players[s.ball.owner].team == s.players[last_kicker].team) completed[s.players[last_kicker].team]++;
                last_kicker = -1;
            }
            if (t % 60 == 0) for (int i = 0; i < np; i++) {   /* outfield player not moving for 10 s = stuck */
                if (s.players[i].role == WP_ROLE_KEEPER) continue;
                if (fv2_len(fv2_sub(s.players[i].pos, last_pos[i])) < FX(0.3)) { if (++still[i] == 10) stuck++; }
                else still[i] = 0;
                last_pos[i] = s.players[i].pos;
            }
        }
    }
    double mins = ticks / 3600.0;
    printf("AI vs AI: %d matches x %d min\n", matches, minutes);
    for (int t = 0; t < 2; t++)
        printf("  team %d: goals %ld (%.2f/min) shots %ld kicks %ld passes %ld completed %ld (%.0f%%) tackles %ld possession %.0f%%\n",
               t, goals[t], goals[t] / mins, shots[t], kicks[t], passes[t], completed[t],
               passes[t] ? 100.0 * completed[t] / passes[t] : 0.0, tackles[t], 100.0 * poss[t] / (poss[0] + poss[1] + 1));
    printf("  outs %ld (%.1f/min: after dribble/touch %ld, pass %ld, shot %ld, through %ld), stuck-player events %ld\n",
           outs, outs / mins, out_kind[0], out_kind[1], out_kind[2], out_kind[5], stuck);
    printf("  time on the ball in the attacking third: team0 %.0f%%, team1 %.0f%%\n", 100.0 * third[0] / (poss[0] + 1), 100.0 * third[1] / (poss[1] + 1));
    return 0;
}
