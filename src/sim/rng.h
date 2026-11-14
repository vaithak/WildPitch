/* rng.h - deterministic PCG32. State lives inside SimState so replays/lockstep stay exact. */
#ifndef WP_RNG_H
#define WP_RNG_H
#include <stdint.h>
#include "sim/fixed.h"

typedef struct { uint64_t state, inc; } WpRng;

void     wp_rng_seed(WpRng *r, uint64_t seed, uint64_t stream);
uint32_t wp_rng_u32(WpRng *r);
uint32_t wp_rng_range(WpRng *r, uint32_t n);      /* [0, n) unbiased */
fx       wp_rng_fx01(WpRng *r);                   /* [0, 1) in Q16.16 */
fx       wp_rng_fx_signed(WpRng *r, fx mag);      /* [-mag, mag) */

#endif
