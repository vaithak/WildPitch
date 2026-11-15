/* Golden-hash determinism test.
 * Runs a scripted 10-minute 5v5 match and compares the final state hash with tests/golden/determinism.txt.
 * If the sim's behaviour changes INTENTIONALLY, regenerate with:  WP_UPDATE_GOLDEN=1 ./build/test_determinism
 * and say so in the commit message. Also checks: same run twice -> same hash; snapshot/resume -> same hash. */
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "test_util.h"
#include "scripted_match.h"

#define TICKS (60 * 60 * 10)

typedef struct { SimState sim; WpRng script; } Snapshot;

static int ev_count[16], switches, bad_owner;
static void advance(SimState *s, WpRng *script, uint32_t from, uint32_t to) {
    WpInput in;
    for (uint32_t t = from; t < to; t++) {
        scripted_input(script, t, s, &in);
        scripted_poke(script, t, s);
        int8_t seat0 = s->seat_player[0];
        sim_step(s, &in);
        for (int b = 0; b < 16; b++) if (s->events & (1 << b)) ev_count[b]++;
        if (s->seat_player[0] != seat0) switches++;
        /* invariant: the ball is loose or owned by a seat's player (control/body rules assume it) */
        int ok = s->ball.owner < 0;
        for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] >= 0 && s->seat_player[k] == s->ball.owner) ok = 1;
        if (!ok) bad_owner++;
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
    int counts[16];
    memcpy(counts, ev_count, sizeof counts);
    uint64_t hb = run(&b, NULL);
    CHECK(ha == hb);
    /* resume from a mid-match snapshot (struct copy) must reach the identical final state.
     * Note: scripted_input keeps held stick state in a static that is identical at the snapshot point
     * because intents change on tick%30 boundaries and TICKS/2 is one. */
    advance(&snap.sim, &snap.script, TICKS / 2, TICKS);
    CHECK(sim_hash(&snap.sim) == ha);
    /* the scripted match must exercise the gameplay code, or the golden hash is vacuous */
    printf("events in scripted match: kick %d touch %d bounce %d post %d net %d goal %d out %d reset %d body %d; score %d-%d\n",
           counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], counts[7], counts[8], a.score[0], a.score[1]);
    CHECK(counts[0] >= 20);   /* kicks */
    CHECK(counts[1] >= 50);   /* touches */
    CHECK(counts[5] + counts[6] >= 3);   /* goals + outs */
    CHECK(counts[7] >= 2);    /* resets */
    printf("seat 0 switched player %d times; bad-owner ticks %d; headers %d\n", switches, bad_owner, counts[9]);
    CHECK(counts[9] >= 3);    /* head contacts */
    CHECK(switches >= 20);    /* switching (manual + auto + pass hand-off) is exercised */
    CHECK(bad_owner == 0);

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
