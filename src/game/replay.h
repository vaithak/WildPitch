/* replay.h - instant replay of the last ~10 s (CP0.4), free thanks to determinism (sim.h: sim_step is a pure
 * function of (state, input)). The live loop logs the WpInput of every tick plus a SimState snapshot (a struct copy)
 * every 2 s; a replay copies the newest snapshot that is at least 10 s old and re-steps it with the logged inputs.
 * The replay runs on its own copy: the live SimState is never touched and resumes exactly where it was.
 * Fixed-size rings, no allocation. Game layer only (uses the sim's public API, nothing else). */
#ifndef WP_REPLAY_H
#define WP_REPLAY_H
#include <stdbool.h>
#include <stdint.h>
#include "sim/sim.h"

#define WP_REPLAY_BACK_TICKS  (10 * WP_TICK_HZ)   /* replay at least this far back */
#define WP_REPLAY_SNAP_EVERY  (2 * WP_TICK_HZ)    /* snapshot period */
/* the snapshot used can be up to BACK + SNAP_EVERY - 1 ticks old: keep that many inputs and snapshots (+1 spare) */
#define WP_REPLAY_SNAPS       ((WP_REPLAY_BACK_TICKS + WP_REPLAY_SNAP_EVERY) / WP_REPLAY_SNAP_EVERY + 1)
#define WP_REPLAY_TICKS       (WP_REPLAY_SNAPS * WP_REPLAY_SNAP_EVERY)

typedef struct {
    uint32_t n;                              /* ticks logged since the last reset (own counter: sim ticks restart) */
    WpInput  in[WP_REPLAY_TICKS];            /* in[t % TICKS] = input stepped at logged tick t */
    SimState snap[WP_REPLAY_SNAPS];          /* snap[(t / SNAP_EVERY) % SNAPS] = state before logged tick t */
} WpReplayLog;

typedef struct {
    bool     active;
    bool     slow;                           /* half speed */
    uint32_t pos, end;                       /* next logged tick to re-step; the live tick count at the start */
    uint32_t sub;                            /* slow motion: frames-of-ticks counter */
    SimState prev, cur;                      /* the replay's own copy (render these while active) */
} WpReplay;

void wp_replay_reset(WpReplayLog *log);                                     /* new match / restart */
/* call immediately before sim_step(s, in) on the live state */
void wp_replay_record(WpReplayLog *log, const SimState *s, const WpInput *in);
bool wp_replay_start(WpReplay *r, const WpReplayLog *log);                 /* false if nothing logged */
/* advance one sim tick of the replay; returns false (and deactivates) when it has caught up with the live tick */
bool wp_replay_step(WpReplay *r, const WpReplayLog *log);
void wp_replay_stop(WpReplay *r);

int  wp_replay_check(void);   /* --check-replay: replay reproduces the live hashes tick for tick */
#endif
