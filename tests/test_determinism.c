/* Golden-hash determinism test.
 * Runs a scripted 10-minute 5v5 match and compares the final state hash with tests/golden/determinism.txt.
 * If the sim's behaviour changes INTENTIONALLY, regenerate with:  WP_UPDATE_GOLDEN=1 ./build/test_determinism
 * and say so in the commit message. Also checks: same run twice -> same hash; snapshot/resume -> same hash. */
#include <stdlib.h>
#include <inttypes.h>
#include "test_util.h"
#include "scripted_match.h"

#define TICKS (60 * 60 * 10)

typedef struct { SimState sim; WpRng script; } Snapshot;

static void advance(SimState *s, WpRng *script, uint32_t from, uint32_t to) {
    WpInput in;
    for (uint32_t t = from; t < to; t++) {
        scripted_input(script, t, &in);
        scripted_poke(script, t, s);
        sim_step(s, &in);
    }
}

static uint64_t run(SimState *s, Snapshot *mid) {
    WpMatchConfig cfg = sim_default_config();
    cfg.seed = 12345;
    sim_init(s, &cfg);
    scripted_setup(s);
    WpRng script; wp_rng_seed(&script, 777, 3);
    advance(s, &script, 0, TICKS / 2);
    if (mid) { mid->sim = *s; mid->script = script; }
    advance(s, &script, TICKS / 2, TICKS);
    return sim_hash(s);
}

int main(void) {
    static SimState a, b;
    static Snapshot snap;
    uint64_t ha = run(&a, &snap);
    uint64_t hb = run(&b, NULL);
    CHECK(ha == hb);
    /* resume from a mid-match snapshot (struct copy) must reach the identical final state.
     * Note: scripted_input keeps held stick state in a static that is identical at the snapshot point
     * because intents change on tick%30 boundaries and TICKS/2 is one. */
    advance(&snap.sim, &snap.script, TICKS / 2, TICKS);
    CHECK(sim_hash(&snap.sim) == ha);
    /* the ball must actually have moved (guards against a vacuous golden hash) */
    CHECK(a.ball.pos.x != 0 || a.ball.pos.y != 0);

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
