#include "sim/rng.h"

uint32_t wp_rng_u32(WpRng *r) {
    uint64_t old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

void wp_rng_seed(WpRng *r, uint64_t seed, uint64_t stream) {
    r->state = 0u;
    r->inc = (stream << 1u) | 1u;
    wp_rng_u32(r);
    r->state += seed;
    wp_rng_u32(r);
}

uint32_t wp_rng_range(WpRng *r, uint32_t n) {
    if (n == 0) return 0;
    uint32_t threshold = (0u - n) % n;
    for (;;) {
        uint32_t x = wp_rng_u32(r);
        if (x >= threshold) return x % n;
    }
}

fx wp_rng_fx01(WpRng *r) { return (fx)(wp_rng_u32(r) >> 16); }

fx wp_rng_fx_signed(WpRng *r, fx mag) {
    return fx_mul(wp_rng_fx01(r) * 2 - FX_ONE, mag);
}
