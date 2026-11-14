/* fixed.c - deterministic integer implementations of sqrt/trig. */
#include "sim/fixed.h"
#include "sim/trig_table.h"

/* Integer square root of a 64-bit value (floor). */
static uint64_t isqrt64(uint64_t n) {
    uint64_t res = 0, bit = (uint64_t)1 << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) { n -= res + bit; res = (res >> 1) + bit; }
        else res >>= 1;
        bit >>= 2;
    }
    return res;
}

fx fx_sqrt(fx a) {
    if (a <= 0) return 0;
    /* sqrt(a / 2^16) * 2^16 = sqrt(a * 2^16) */
    return (fx)isqrt64((uint64_t)a << FX_SHIFT);
}

fx fx_sin(wp_angle a) {
    /* 65536 per turn, quarter = 16384 units, table has 4096 steps -> 4 units per step. */
    uint32_t q = a >> 14;            /* quadrant 0..3 */
    uint32_t r = a & 0x3FFF;         /* 0..16383 within quadrant */
    uint32_t idx = r >> 2, frac = r & 3;
    int32_t v;
    if (q & 1) { /* descending quadrant: sin(90 - r) */
        uint32_t ri = 16384 - r; idx = ri >> 2; frac = ri & 3;
    }
    v = wp_sin_quarter[idx];
    if (frac && idx < WP_SIN_QUARTER_N)
        v += (int32_t)(((int64_t)(wp_sin_quarter[idx + 1] - v) * frac) >> 2);
    return (q & 2) ? -v : v;
}

fx fx_cos(wp_angle a) { return fx_sin((wp_angle)(a + 16384)); }

/* atan2 via octant reduction + atan(t) for t in [0,1] using a rational approx:
 * atan(t) ~= t*(pi/4) + 0.273*t*(1-t)  (max err ~0.0038 rad = 0.22 deg) in binary angles. */
wp_angle fx_atan2(fx y, fx x) {
    if (x == 0 && y == 0) return 0;
    int64_t ax = x < 0 ? -(int64_t)x : x, ay = y < 0 ? -(int64_t)y : y;
    int swap = ay > ax;
    int64_t num = swap ? ax : ay, den = swap ? ay : ax;
    int64_t t = (num << 16) / den;                      /* Q16 in [0,1] */
    /* pi/4 rad = 8192 binary units; 0.273 rad = 2847 units (0.273/(2pi)*65536) */
    int64_t ang = (t * 8192 + ((t * (65536 - t)) >> 16) * 2847) >> 16;
    if (swap) ang = 16384 - ang;
    if (x < 0) ang = 32768 - ang;
    if (y < 0) ang = 65536 - ang;
    return (wp_angle)(ang & 0xFFFF);
}

fx fv2_len(fv2 a) {
    return (fx)isqrt64((uint64_t)fv2_len2_wide(a));
}

fv2 fv2_norm(fv2 a) {
    fx l = fv2_len(a);
    if (l == 0) return a;
    return fv2_make(fx_div(a.x, l), fx_div(a.y, l));
}

fv2 fv2_clamp_len(fv2 a, fx max_len) {
    fx l = fv2_len(a);
    if (l <= max_len || l == 0) return a;
    return fv2_scale(a, fx_div(max_len, l));
}
