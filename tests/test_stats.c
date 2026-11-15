/* Player stats (D-034): generation, the neutral guarantee, pace, stamina, kick-off survival. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

static SimState s;

/* sprint player 4 along +x for n ticks (his own ball next to him so control never switches away); his speed */
static fx sprint_speed(uint8_t pace, fx energy, int n) {
    setup_empty(&s, 4);
    s.stats_mode = WP_STATS_MIRRORED;
    memset(s.stats, 50, sizeof s.stats);
    s.stats[4][WP_STAT_PACE] = pace;
    s.seat_player[0] = 4;
    s.players[4].stamina = energy;
    s.players[4].pos = fv2_make(-FX(22.0), -FX(3.0));
    s.ball.pos = fv2_make(-FX(19.5), -FX(12.0));
    run_seat(&s, (WpSeatInput){127, 0, WP_BTN_SPRINT}, n);
    return fv2_len(s.players[4].vel);
}

int main(void) {
    /* generation: in range, deterministic per seed, equal squad totals (OWN), identical squads (MIRRORED) */
    for (int mode = WP_STATS_MIRRORED; mode <= WP_STATS_OWN; mode++)
        for (int seed = 1; seed <= 20; seed++) {
            WpMatchConfig cfg = sim_config_7v7();
            cfg.stats_mode = (uint8_t)mode; cfg.seed = (uint64_t)seed;
            sim_init(&s, &cfg);
            static SimState again;
            sim_init(&again, &cfg);
            CHECK(memcmp(s.stats, again.stats, sizeof s.stats) == 0);
            int n = s.n_per_side, varied = 0;
            for (int st = 0; st < WP_STAT_N; st++) {
                int t0 = 0, t1 = 0;
                for (int k = 0; k < n; k++) {
                    CHECK(s.stats[k][st] >= 1 && s.stats[k][st] <= 99);
                    t0 += s.stats[k][st]; t1 += s.stats[n + k][st];
                    if (s.stats[k][st] != 50) varied = 1;
                    if (mode == WP_STATS_MIRRORED) CHECK(s.stats[k][st] == s.stats[n + k][st]);
                }
                CHECKF(t0 == t1, "mode %d seed %d stat %d: squad totals %d vs %d", mode, seed, st, t0, t1);
            }
            CHECK(varied);
            /* keeper has the keeping, the striker the shooting */
            CHECK(s.stats[0][WP_STAT_KEEPING] > s.stats[n - 1][WP_STAT_KEEPING]);
            CHECK(s.stats[n - 1][WP_STAT_SHOOTING] > s.stats[1][WP_STAT_SHOOTING]);
        }
    /* neutral: everyone 50, the RNG untouched (same state as a fresh seed) */
    {
        WpMatchConfig cfg = sim_default_config();
        sim_init(&s, &cfg);
        WpRng r; wp_rng_seed(&r, cfg.seed, 0x5157u);
        CHECK(s.stats_mode == WP_STATS_NEUTRAL && s.stats[3][WP_STAT_PACE] == 50);
        CHECK(s.rng.state == r.state && s.rng.inc == r.inc);
    }
    /* pace: top sprint scales +-7% per (stat-50)/50 */
    fx slow = sprint_speed(20, FX_ONE, 150), mid = sprint_speed(50, FX_ONE, 150), fast = sprint_speed(85, FX_ONE, 150);
    printf("sprint speed: pace 20 %.2f, 50 %.2f, 85 %.2f m/s\n", tod(slow), tod(mid), tod(fast));
    CHECK(fx_abs(mid - FX(8.0)) < FX(0.05));
    CHECK(slow < mid - FX(0.25) && fast > mid + FX(0.25));
    /* stamina: a tired player's sprint fades; sprinting drains energy, jogging brings it back */
    fx tired = sprint_speed(50, FX(0.05), 20);
    printf("tired sprint: %.2f m/s (energy %.2f after)\n", tod(tired), tod(s.players[4].stamina));
    CHECK(tired < FX(7.2));
    s.players[4].stamina = FX_ONE;                       /* drain / recovery, at the rule level */
    for (int t = 0; t < 600; t++) { s.players[4].vel = fv2_make(FX(8.0), 0); stamina_step(&s, 4); }
    fx after = s.players[4].stamina;
    printf("10 s flat out: energy %.2f\n", tod(after));
    CHECK(after < FX(0.75) && after > FX(0.45));
    for (int t = 0; t < 600; t++) { s.players[4].vel = fv2_make(0, 0); stamina_step(&s, 4); }
    printf("then 10 s standing: energy %.2f\n", tod(s.players[4].stamina));
    CHECK(s.players[4].stamina > after + FX(0.3));
    s.stats_mode = WP_STATS_NEUTRAL;                     /* neutral: no drain at all */
    s.players[4].stamina = FX_ONE;
    for (int t = 0; t < 600; t++) { s.players[4].vel = fv2_make(FX(8.0), 0); stamina_step(&s, 4); }
    CHECK(s.players[4].stamina == FX_ONE);
    /* kick-offs (memset of players) keep the stats */
    {
        WpMatchConfig cfg = sim_config_7v7();
        sim_init(&s, &cfg);
        uint8_t keep[WP_MAX_PLAYERS][WP_STAT_N];
        memcpy(keep, s.stats, sizeof keep);
        match_place_kickoff(&s);
        CHECK(memcmp(keep, s.stats, sizeof keep) == 0);
    }
    return TEST_DONE();
}
