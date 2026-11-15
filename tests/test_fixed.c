/* Accuracy of fixed-point maths against libm (libm is fine in tests, never in src/sim). */
#include <math.h>
#include "test_util.h"
#define WP_PI 3.14159265358979323846
#include "sim/fixed.h"
#include "sim/rng.h"

static double tod(fx a) { return (double)a / 65536.0; }

int main(void) {
    CHECK(fx_mul(FX(1.5), FX(2.0)) == FX(3.0));
    CHECK(fx_div(FX(3.0), FX(2.0)) == FX(1.5));
    CHECK(fx_mul(FX(-1.5), FX(2.0)) == FX(-3.0));
    for (double v = 0.0; v < 30000.0; v = v * 1.7 + 0.013) {
        double got = tod(fx_sqrt((fx)(v * 65536.0)));
        CHECKF(fabs(got - sqrt(v)) < 0.0005 + sqrt(v) * 1e-5, "sqrt(%f)=%f", v, got);
    }
    double max_sin_err = 0, max_atan_err = 0;
    for (int a = 0; a < 65536; a += 7) {
        double rad = a * 2.0 * WP_PI / 65536.0;
        double e = fabs(tod(fx_sin((wp_angle)a)) - sin(rad));
        if (e > max_sin_err) max_sin_err = e;
        e = fabs(tod(fx_cos((wp_angle)a)) - cos(rad));
        if (e > max_sin_err) max_sin_err = e;
        fx x = (fx)(cos(rad) * 65536.0 * 10), y = (fx)(sin(rad) * 65536.0 * 10);
        double ang = fx_atan2(y, x) * 2.0 * WP_PI / 65536.0;
        double d = fabs(remainder(ang - rad, 2 * WP_PI));
        if (d > max_atan_err) max_atan_err = d;
    }
    CHECKF(max_sin_err < 0.0001, "max sin err %f", max_sin_err);
    CHECKF(max_atan_err < 0.005, "max atan2 err %f rad", max_atan_err);
    CHECK(fx_atan2(0, FX(1)) == 0);
    CHECK(fx_atan2(FX(1), 0) == 16384);
    CHECK(fx_atan2(0, -FX(1)) == 32768);
    CHECK(fv2_len(fv2_make(FX(3), FX(4))) == FX(5));
    CHECK(fv2_len(fv2_make(FX(3000), FX(4000))) == FX(5000)); /* no overflow on long vectors */
    fv2 n = fv2_norm(fv2_make(FX(10), 0));
    CHECK(n.x == FX_ONE && n.y == 0);

    WpRng r; wp_rng_seed(&r, 42, 1);
    int buckets[10] = {0};
    for (int i = 0; i < 100000; i++) buckets[wp_rng_range(&r, 10)]++;
    for (int i = 0; i < 10; i++) CHECKF(buckets[i] > 9500 && buckets[i] < 10500, "bucket %d=%d", i, buckets[i]);
    /* exact isqrt (clz start) == the reference digit-by-digit loop, bit for bit, over a wide sweep */
    {
        uint64_t x = 0x9E3779B97F4A7C15ULL; int bad = 0;
        for (int k = 0; k < 200000; k++) {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            int32_t a = (int32_t)(x >> (33 + (k % 31))) ;
            fv2 v = fv2_make(a, (int32_t)(x & 0xFFFFFF));
            uint64_t nn = (uint64_t)((int64_t)v.x * v.x + (int64_t)v.y * v.y);
            uint64_t res = 0, bit = (uint64_t)1 << 62;                 /* reference */
            while (bit > nn) bit >>= 2;
            while (bit) { if (nn >= res + bit) { nn -= res + bit; res = (res >> 1) + bit; } else res >>= 1; bit >>= 2; }
            bad += (uint64_t)fv2_len(v) != res;
        }
        CHECKF(bad == 0, "isqrt mismatches: %d", bad);
    }
    return TEST_DONE();
}
