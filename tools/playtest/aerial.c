/* aerial.c - AI vs AI aerial counters (CP0.5a): lob / cross completion, first aerial contact by height band and its
 * control success, header passes, headers per minute. Definitions: tests/aerial_metrics.h (shared with the tests).
 * Same match setup as aimatch.c (harness-only start nudges). Read-only: the golden hash is untouched.
 * usage: aerial [matches=20] [minutes=5] [seven=1]      (WP_STATS / WP_RULES as in aimatch.c)
 * build: cc -std=c17 -O2 -Isrc -Itests -Itools/playtest tools/playtest/aerial.c build/libwp_sim.a -lm -o /tmp/aerial */
#include <stdio.h>
#include <stdlib.h>
#include "sim_test_helpers.h"
#include "aerial_metrics.h"

int main(int argc, char **argv) {
    int matches = argc > 1 ? atoi(argv[1]) : 20, minutes = argc > 2 ? atoi(argv[2]) : 5, seven = argc > 3 ? atoi(argv[3]) : 1;
    static SimState s;
    AerialStats a; memset(&a, 0, sizeof a);
    long goals = 0, shots = 0;
    for (int m = 0; m < matches; m++) {
        WpMatchConfig cfg = seven ? sim_config_7v7() : sim_default_config();
        if (getenv("WP_STATS")) cfg.stats_mode = (uint8_t)atoi(getenv("WP_STATS"));
        if (getenv("WP_RULES")) cfg.rules = (uint8_t)atoi(getenv("WP_RULES"));
        cfg.seed = (uint64_t)(m + 1);
        sim_init(&s, &cfg);
        for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
        s.kickoff_team = (uint8_t)(m & 1);
        s.ball.pos = fv2_make(FX_INT((m % 5) - 2), FX_INT((m % 3) - 1));   /* nudge the start (as aimatch.c) */
        WpInput in; memset(&in, 0, sizeof in);
        WpRng hr; wp_rng_seed(&hr, (uint64_t)(1000 + m), 7);
        AerialTrack tr; aerial_reset(&tr, &s);
        for (long t = 0; t < 60L * 60 * minutes; t++) {
            sim_step(&s, &in);
            if (s.events & WP_EV_RESET) {
                for (int i = 0; i < 2 * s.n_per_side; i++) {
                    fx dx = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                    fx dy = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                    s.players[i].pos = fv2_add(s.players[i].pos, fv2_make(dx, dy));
                }
            }
            aerial_update(&a, &tr, &s);
            if (s.events & WP_EV_GOAL) goals++;
            if ((s.events & WP_EV_KICK) && s.ball.last_touch >= 0 && s.players[s.ball.last_touch].kick.kind == WP_KICK_SHOT &&
                !(s.events & WP_EV_HEADER)) shots++;
        }
    }
    double mins = (double)a.ticks / 3600.0;
    printf("AI vs AI aerial: %d matches x %d min (%s)\n", matches, minutes, seven ? "7v7" : "5v5 40 m");
    printf("  goals %ld (%.2f/min), shots %ld (%.2f/min)\n", goals, goals / mins, shots, shots / mins);
    aerial_print(&a);
    return 0;
}
