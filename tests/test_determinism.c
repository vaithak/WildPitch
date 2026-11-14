/* Golden-hash determinism test.
 * Runs a scripted 10-minute 5v5 match and compares the final state hash with tests/golden/determinism.txt.
 * If the sim's behaviour changes INTENTIONALLY, regenerate with:  WP_UPDATE_GOLDEN=1 ./build/test_determinism
 * and say so in the commit message. Also checks: same run twice -> same hash; snapshot/resume -> same hash. */
#include <stdlib.h>
#include <inttypes.h>
#include "test_util.h"
#include "scripted_match.h"

#define TICKS (60 * 60 * 10)

static uint64_t run(SimState *s, uint32_t ticks, uint64_t *mid_hash, SimState *mid_snap) {
    WpMatchConfig cfg = sim_default_config();
    cfg.seed = 12345;
    sim_init(s, &cfg);
    WpRng script; wp_rng_seed(&script, 777, 3);
    WpInput in;
    for (uint32_t t = 0; t < ticks; t++) {
        if (t == ticks / 2) { if (mid_hash) *mid_hash = sim_hash(s); if (mid_snap) *mid_snap = *s; }
        scripted_input(&script, t, &in);
        sim_step(s, &in);
    }
    return sim_hash(s);
}

int main(void) {
    static SimState a, b, snap;
    uint64_t mid = 0;
    uint64_t ha = run(&a, TICKS, &mid, &snap);
    uint64_t hb = run(&b, TICKS, NULL, NULL);
    CHECK(ha == hb);
    CHECK(sim_hash(&snap) == mid);  /* struct copy preserves hash */

    /* resume from snapshot: replay second half of the script on the copy */
    {
        WpRng script; wp_rng_seed(&script, 777, 3);
        WpInput in;
        for (uint32_t t = 0; t < TICKS; t++) { scripted_input(&script, t, &in); if (t >= TICKS / 2) sim_step(&snap, &in); }
        CHECK(sim_hash(&snap) == ha);
    }

    const char *path = "tests/golden/determinism.txt";
    if (getenv("WP_UPDATE_GOLDEN")) {
        FILE *f = fopen(path, "w");
        CHECK(f != NULL);
        if (f) { fprintf(f, "%016" PRIx64 "\n", ha); fclose(f); printf("golden updated: %016" PRIx64 "\n", ha); }
    } else {
        FILE *f = fopen(path, "r");
        uint64_t g = 0;
        CHECKF(f != NULL, "missing %s (run with WP_UPDATE_GOLDEN=1)", path);
        if (f) { CHECK(fscanf(f, "%" SCNx64, &g) == 1); fclose(f); }
        CHECKF(g == ha, "hash %016" PRIx64 " != golden %016" PRIx64, ha, g);
    }
    printf("final hash %016" PRIx64 "\n", ha);
    return TEST_DONE();
}
