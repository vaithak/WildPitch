/* sim_test_helpers.h - shared helpers for behavioural sim tests. */
#ifndef WP_SIM_TEST_HELPERS_H
#define WP_SIM_TEST_HELPERS_H
#include <string.h>
#include "sim/sim.h"
#include "sim/tuning.h"

static inline double tod(fx v) { return (double)v / 65536.0; }

/* fresh 5v5 match with every player parked far off the pitch (so they don't block), except `keep` */
static inline void setup_empty(SimState *s, int keep) {
    WpMatchConfig cfg = sim_default_config();
    sim_init(s, &cfg);
    s->flags |= WP_SIMF_STATIC_OTHERS;
    for (int i = 0; i < 2 * s->n_per_side; i++)
        if (i != keep) s->players[i].pos = fv2_make(FX(-15.0) + i * FX(2.0), FX(60.0));
    s->events = 0;
}

/* step n ticks with seat 0 input; returns OR of events */
static inline uint16_t run_seat(SimState *s, WpSeatInput in, int n) {
    WpInput all; memset(&all, 0, sizeof all); all.seat[0] = in;
    uint16_t ev = 0;
    for (int i = 0; i < n; i++) { sim_step(s, &all); ev |= s->events; }
    return ev;
}
static const WpSeatInput NOIN = {0, 0, 0};
#endif
