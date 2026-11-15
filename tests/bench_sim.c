#define _POSIX_C_SOURCE 200809L
/* bench_sim - headless simulation speed (ticks/second). Training in Phase 1 depends on this being
 * hundreds of times faster than real time. Usage: ./build/bench_sim [seconds_of_match=600] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "scripted_match.h"

int main(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 600;
    uint32_t ticks = (uint32_t)secs * WP_TICK_HZ;
    static SimState s;
    WpMatchConfig cfg = sim_default_config();
    sim_init(&s, &cfg);
    scripted_setup(&s);
    WpRng script; wp_rng_seed(&script, 777, 3);
    WpInput in;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint32_t t = 0; t < ticks; t++) { scripted_input(&script, t, &s, &in); scripted_poke(&script, t, &s); sim_step(&s, &in); }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double el = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    printf("%u ticks in %.3f s = %.0f ticks/s = %.0fx real time (%.2f us/tick), hash %016llx\n",
           ticks, el, ticks / el, (ticks / el) / WP_TICK_HZ, el * 1e6 / ticks, (unsigned long long)sim_hash(&s));
    return 0;
}
