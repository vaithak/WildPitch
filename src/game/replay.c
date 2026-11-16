/* replay.c - see replay.h. */
#include <stdio.h>
#include <string.h>
#include "game/replay.h"

void wp_replay_reset(WpReplayLog *log) { log->n = 0; }

void wp_replay_record(WpReplayLog *log, const SimState *s, const WpInput *in) {
    uint32_t t = log->n;
    if (t % WP_REPLAY_SNAP_EVERY == 0) log->snap[(t / WP_REPLAY_SNAP_EVERY) % WP_REPLAY_SNAPS] = *s;
    log->in[t % WP_REPLAY_TICKS] = *in;
    log->n = t + 1;
}

bool wp_replay_start(WpReplay *r, const WpReplayLog *log) {
    uint32_t n = log->n;
    if (n == 0) return false;
    /* newest snapshot at least BACK ticks old; a young match starts from its first snapshot */
    uint32_t k = n >= WP_REPLAY_BACK_TICKS ? (n - WP_REPLAY_BACK_TICKS) / WP_REPLAY_SNAP_EVERY * WP_REPLAY_SNAP_EVERY : 0;
    r->cur = log->snap[(k / WP_REPLAY_SNAP_EVERY) % WP_REPLAY_SNAPS];
    r->prev = r->cur;
    r->pos = k;
    r->end = n;
    r->sub = 0;
    r->slow = false;
    r->active = true;
    return true;
}

bool wp_replay_step(WpReplay *r, const WpReplayLog *log) {
    if (!r->active) return false;
    if (r->pos >= r->end) { r->active = false; return false; }
    r->prev = r->cur;
    sim_step(&r->cur, &log->in[r->pos % WP_REPLAY_TICKS]);
    r->pos++;
    return true;
}

void wp_replay_stop(WpReplay *r) { r->active = false; }

/* ---- --check-replay: 30 s of a 7v7 match with scripted seat-0 input (stick + every button, changing often so an
 * off-by-one in the input ring shows), then replays from the snapshot and compares sim_hash() every tick with the
 * live run. Also checks a young match (< 10 s) and that the live state is untouched. ---- */
static WpInput scripted(uint32_t t) {
    WpInput in;
    memset(&in, 0, sizeof in);
    uint32_t h = (t / 7) * 2654435761u;                          /* new stick/buttons every 7 ticks */
    in.seat[0].move_x = (int8_t)((int)((h >> 8) % 255u) - 127);     /* -127..127 */
    in.seat[0].move_y = (int8_t)((int)((h >> 16) % 255u) - 127);
    in.seat[0].buttons = (uint16_t)((h >> 24) & (WP_BTN_PASS | WP_BTN_SHOOT | WP_BTN_SPRINT | WP_BTN_SWITCH |
                                                 WP_BTN_CHIP | WP_BTN_LOB));
    if ((t / 60) % 3 == 0) in.seat[0].buttons &= (uint16_t)~(WP_BTN_SHOOT | WP_BTN_CHIP);   /* let some play happen */
    return in;
}

static int run_case(uint32_t total) {
    static SimState live;
    static WpReplayLog log;
    static WpReplay rp;
    static uint64_t hash_after[WP_REPLAY_TICKS];                 /* live hash after logged tick t, ring */
    WpMatchConfig cfg = sim_config_7v7();
    sim_init(&live, &cfg);
    sim_kickoff_setpiece(&live);
    wp_replay_reset(&log);
    for (uint32_t t = 0; t < total; t++) {
        WpInput in = scripted(t);
        wp_replay_record(&log, &live, &in);
        sim_step(&live, &in);
        hash_after[t % WP_REPLAY_TICKS] = sim_hash(&live);
    }
    uint64_t live_hash = sim_hash(&live);
    if (!wp_replay_start(&rp, &log)) { printf("replay check FAIL: nothing to replay\n"); return 1; }
    uint32_t from = rp.pos, steps = 0;
    while (wp_replay_step(&rp, &log)) {
        steps++;
        uint32_t t = rp.pos - 1;
        if (sim_hash(&rp.cur) != hash_after[t % WP_REPLAY_TICKS]) {
            printf("replay check FAIL: diverged at logged tick %u (replay from %u, live ran %u)\n", t, from, total);
            return 1;
        }
    }
    int bad = 0;
    if (sim_hash(&rp.cur) != live_hash) { printf("replay check FAIL: end hash differs\n"); bad = 1; }
    if (sim_hash(&live) != live_hash) { printf("replay check FAIL: live state changed\n"); bad = 1; }
    if (total >= WP_REPLAY_BACK_TICKS && (steps < WP_REPLAY_BACK_TICKS || steps >= WP_REPLAY_BACK_TICKS + WP_REPLAY_SNAP_EVERY)) {
        printf("replay check FAIL: replayed %u ticks\n", steps); bad = 1;
    }
    if (!bad) printf("replay %u ticks: replayed %u ticks from logged tick %u, hashes match (%016llx)\n", total, steps, from,
                     (unsigned long long)live_hash);
    return bad;
}

int wp_replay_check(void) {
    int bad = 0;
    bad += run_case(30 * WP_TICK_HZ);        /* both rings wrapped */
    bad += run_case(30 * WP_TICK_HZ + 77);   /* not on a snapshot boundary */
    bad += run_case(5 * WP_TICK_HZ);         /* young match: from the first snapshot */
    printf("replay check: %s (memory %u bytes: %d snapshots + %d inputs)\n", bad ? "FAIL" : "OK",
           (unsigned)sizeof(WpReplayLog), WP_REPLAY_SNAPS, WP_REPLAY_TICKS);
    return bad ? 1 : 0;
}
