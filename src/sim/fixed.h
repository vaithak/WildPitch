/* fixed.h - Q16.16 fixed-point maths for the deterministic simulation.
 *
 * RULES (see docs/ARCHITECTURE.md "Determinism"):
 *  - The sim never uses float/double at runtime and never calls libm.
 *  - FX(literal) converts a numeric *literal* at compile time; never pass a variable.
 *  - Units: 1.0 = 1 metre, 1.0 = 1 second. Angles are binary angles (uint16, 65536 = full turn).
 *  - Right shift of negative values is arithmetic on every compiler we support (gcc/clang);
 *    the sim is compiled with -fwrapv so overflow wraps instead of being UB.
 */
#ifndef WP_FIXED_H
#define WP_FIXED_H

#include <stdint.h>

typedef int32_t fx;          /* Q16.16 */
typedef uint16_t wp_angle;   /* binary angle: 0..65535 = 0..360 deg */

#define FX_SHIFT 16
#define FX_ONE   ((fx)1 << FX_SHIFT)
#define FX_HALF  ((fx)1 << (FX_SHIFT - 1))
#define FX(lit)  ((fx)((lit) * 65536.0 + ((lit) >= 0 ? 0.5 : -0.5))) /* compile-time literals only */
#define FX_INT(i) ((fx)((i) * FX_ONE))

#define WP_ANGLE_DEG(d) ((wp_angle)(((d) * 65536L) / 360))

static inline fx fx_mul(fx a, fx b) { return (fx)(((int64_t)a * b) >> FX_SHIFT); }
static inline fx fx_div(fx a, fx b) {
    if (b == 0) return a >= 0 ? INT32_MAX : -INT32_MAX;
    return (fx)(((int64_t)a * FX_ONE) / b);
}
static inline fx fx_abs(fx a) { return a < 0 ? -a : a; }
static inline fx fx_min(fx a, fx b) { return a < b ? a : b; }
static inline fx fx_max(fx a, fx b) { return a > b ? a : b; }
static inline fx fx_clamp(fx v, fx lo, fx hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int fx_to_int(fx a) { return (int)(a >> FX_SHIFT); }   /* floor */
static inline fx fx_lerp(fx a, fx b, fx t) { return a + fx_mul(b - a, t); }

fx fx_sqrt(fx a);                 /* a >= 0; returns 0 for a <= 0 */
fx fx_sin(wp_angle a);
fx fx_cos(wp_angle a);
wp_angle fx_atan2(fx y, fx x);    /* angle of vector (x,y); 0 = +x, 16384 = +y */

/* 2D vectors ------------------------------------------------------------ */
typedef struct { fx x, y; } fv2;

static inline fv2 fv2_make(fx x, fx y) { fv2 v = {x, y}; return v; }
static inline fv2 fv2_add(fv2 a, fv2 b) { return fv2_make(a.x + b.x, a.y + b.y); }
static inline fv2 fv2_sub(fv2 a, fv2 b) { return fv2_make(a.x - b.x, a.y - b.y); }
static inline fv2 fv2_scale(fv2 a, fx s) { return fv2_make(fx_mul(a.x, s), fx_mul(a.y, s)); }
static inline fx  fv2_dot(fv2 a, fv2 b) { return fx_mul(a.x, b.x) + fx_mul(a.y, b.y); }
/* Squared length in 64-bit Q32.32 to avoid overflow for long vectors. */
static inline int64_t fv2_len2_wide(fv2 a) { return (int64_t)a.x * a.x + (int64_t)a.y * a.y; }
fx  fv2_len(fv2 a);
fv2 fv2_norm(fv2 a);              /* zero vector stays zero */
fv2 fv2_clamp_len(fv2 a, fx max_len);
static inline fv2 fv2_from_angle(wp_angle a, fx len) { return fv2_make(fx_mul(fx_cos(a), len), fx_mul(fx_sin(a), len)); }

#endif
