/* scripted_match.h - deterministic pseudo-random input script shared by the determinism test
 * and the benchmark. Uses its own RNG (not the sim's) so the script doesn't depend on sim internals. */
#ifndef WP_SCRIPTED_MATCH_H
#define WP_SCRIPTED_MATCH_H
#include <string.h>
#include "sim/sim.h"
static void scripted_input(WpRng *r, uint32_t tick, WpInput *in) {
    memset(in, 0, sizeof *in);
    /* change intent every ~0.5 s, like a human would */
    static WpSeatInput held[WP_MAX_SEATS];
    if (tick % 30 == 0)
        for (int k = 0; k < 2; k++) {
            held[k].move_x = (int8_t)((int)wp_rng_range(r, 255) - 127);
            held[k].move_y = (int8_t)((int)wp_rng_range(r, 255) - 127);
            held[k].buttons = (uint16_t)(wp_rng_u32(r) & 0x3F);
        }
    for (int k = 0; k < 2; k++) in->seat[k] = held[k];
}
#endif
