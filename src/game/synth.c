/* synth.c - see synth.h. Building blocks: a xorshift noise source, RBJ biquads (band / low / high pass), a pitch-
 * swept sine "thump" with a filtered noise click (every ball contact), inharmonic partials (the post), an FM-trilled
 * tone (the pea whistle) and slowly modulated band-limited noise (crowds). Each sound is built into its slice of one
 * static pool, then DC-blocked, peak-normalised to its level and faded at the ends (loops: ends cross-faded). */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "game/synth.h"

#define R ((float)WP_AUDIO_RATE)
static const float TAU = 6.2831853f;

/* lengths in seconds */
static const float LEN_S[WP_SND_COUNT] = {0.18f, 0.30f, 0.26f, 0.20f, 0.12f, 1.20f, 0.65f, 0.34f, 1.10f, 0.32f,
                                          6.0f, 4.0f, 3.6f, 1.9f};
/* peak level after normalising (the mix balance between sounds) */
static const float LEVEL[WP_SND_COUNT] = {0.55f, 0.85f, 0.55f, 0.50f, 0.32f, 0.55f, 0.45f, 0.30f, 0.30f, 0.35f,
                                          0.40f, 0.45f, 0.70f, 0.50f};
static const char *NAME[WP_SND_COUNT] = {"kick_pass", "kick_shot", "kick_lob", "header", "bounce", "post", "net",
                                         "whistle_short", "whistle_long", "tackle", "crowd_bed", "crowd_excite",
                                         "crowd_roar", "crowd_groan"};

#define POOL_SAMPLES (22 * WP_AUDIO_RATE)     /* sum of LEN_S (20.2 s) + two 0.5 s loop cross-fade tails */
static float   pool[POOL_SAMPLES];
static WpSound sounds[WP_SND_COUNT];
static bool    built;

/* ---- building blocks ---- */
static uint32_t rng_state = 0x9E3779B9u;
static float noise(void) {
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return (float)(x >> 8) / 8388608.0f - 1.0f;
}

typedef struct { float b0, b1, b2, a1, a2, x1, x2, y1, y2; } Biquad;
enum { BQ_LP, BQ_HP, BQ_BP };
static void bq_set(Biquad *f, int type, float hz, float q) {
    float w = TAU * hz / R, cw = cosf(w), al = sinf(w) / (2.0f * q), a0 = 1.0f + al;
    if (type == BQ_LP) { f->b0 = (1 - cw) * 0.5f; f->b1 = 1 - cw; f->b2 = (1 - cw) * 0.5f; }
    else if (type == BQ_HP) { f->b0 = (1 + cw) * 0.5f; f->b1 = -(1 + cw); f->b2 = (1 + cw) * 0.5f; }
    else { f->b0 = al; f->b1 = 0; f->b2 = -al; }
    f->a1 = -2.0f * cw; f->a2 = 1.0f - al;
    f->b0 /= a0; f->b1 /= a0; f->b2 /= a0; f->a1 /= a0; f->a2 /= a0;
}
static void bq_init(Biquad *f, int type, float hz, float q) { memset(f, 0, sizeof *f); bq_set(f, type, hz, q); }
static float bq(Biquad *f, float x) {
    float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = x; f->y2 = f->y1; f->y1 = y;
    return y;
}

/* slow random modulation in [0, 1]: noise -> two one-pole low passes at `hz` */
typedef struct { float a, s1, s2; } Lfo;
static void lfo_init(Lfo *l, float hz) { l->a = 1.0f - expf(-TAU * hz / R); l->s1 = l->s2 = 0.5f; }
static float lfo(Lfo *l) {
    l->s1 += l->a * (noise() * 3.0f + 0.5f - l->s1);
    l->s2 += l->a * (l->s1 - l->s2);
    return l->s2 < 0 ? 0 : l->s2 > 1 ? 1 : l->s2;
}

/* ball contact: a sine swept f0 -> f1 decaying over `decay` s, plus a noise click low-passed at click_hz */
static void thump(float *o, int n, float f0, float f1, float decay, float body, float click, float click_hz, float click_decay) {
    Biquad lp;
    bq_init(&lp, BQ_LP, click_hz, 0.7f);
    float ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / R, f = f1 + (f0 - f1) * expf(-t / 0.022f);
        ph += TAU * f / R;
        o[i] += body * sinf(ph) * expf(-t / decay) + click * bq(&lp, noise()) * expf(-t / click_decay);
    }
}

/* band-passed noise with an envelope callback-free shape: attack a s, then exp decay d s (d <= 0: hold) */
static void noise_band(float *o, int n, float hz, float q, float gain, float a, float d) {
    Biquad bp;
    bq_init(&bp, BQ_BP, hz, q);
    for (int i = 0; i < n; i++) {
        float t = (float)i / R, env = t < a ? t / a : (d > 0 ? expf(-(t - a) / d) : 1.0f);
        o[i] += gain * env * bq(&bp, noise());
    }
}

static void whistle(float *o, int n) {
    Biquad bp;
    bq_init(&bp, BQ_BP, 2900.0f, 6.0f);
    float ph = 0, ph2 = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / R, tr = sinf(TAU * 31.0f * t);           /* the pea rattling: ~31 Hz trill */
        float f = 2750.0f + 180.0f * (1.0f - expf(-t / 0.03f)) + 70.0f * tr;
        ph += TAU * f / R; ph2 += TAU * 2.0f * f / R;
        float am = 0.78f + 0.22f * tr;
        o[i] += am * (sinf(ph) + 0.18f * sinf(ph2)) + 0.25f * bq(&bp, noise());
    }
}

static void post(float *o, int n) {
    static const float P[6][3] = {{780, 0.30f, 0.55f}, {1185, 0.50f, 0.45f}, {1193, 0.25f, 0.40f},
                                  {2950, 0.33f, 0.28f}, {4330, 0.22f, 0.20f}, {6110, 0.14f, 0.12f}};
    for (int k = 0; k < 6; k++)
        for (int i = 0; i < n; i++) {
            float t = (float)i / R;
            o[i] += P[k][1] * sinf(TAU * P[k][0] * t) * expf(-t / P[k][2]);
        }
    thump(o, n, 220, 110, 0.03f, 0.6f, 0.4f, 5000, 0.004f);
}

/* crowd: band-limited noise in two bands, each swelling with its own slow modulation (many voices) */
static void crowd(float *o, int n, float f1, float f2, float q, float mod_hz, float depth, float bright) {
    Biquad b1, b2, b3;
    bq_init(&b1, BQ_BP, f1, q); bq_init(&b2, BQ_BP, f2, q * 1.4f); bq_init(&b3, BQ_BP, f2 * 2.3f, 1.2f);
    Lfo l1, l2, l3;
    lfo_init(&l1, mod_hz); lfo_init(&l2, mod_hz * 1.7f); lfo_init(&l3, mod_hz * 3.1f);
    for (int i = 0; i < n; i++) {
        float m1 = 1.0f - depth + depth * lfo(&l1), m2 = 1.0f - depth + depth * lfo(&l2), m3 = lfo(&l3);
        o[i] += m1 * bq(&b1, noise()) + 0.6f * m2 * bq(&b2, noise()) + bright * m3 * bq(&b3, noise());
    }
}

static void roar(float *o, int n) {
    Biquad b1, b2;
    bq_init(&b1, BQ_BP, 600, 0.6f); bq_init(&b2, BQ_BP, 1500, 0.8f);
    Lfo l;
    lfo_init(&l, 4.0f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / R;
        if ((i & 63) == 0) {                                  /* the voices rise as it swells */
            float rise = 1.0f - expf(-t / 0.35f);
            bq_set(&b1, BQ_BP, 500.0f + 450.0f * rise, 0.6f);
            bq_set(&b2, BQ_BP, 1200.0f + 700.0f * rise, 0.8f);
        }
        float att = t / 0.30f, env = att < 1 ? att * att * (3 - 2 * att) : (t < 2.0f ? 1.0f : expf(-(t - 2.0f) / 0.5f));
        o[i] += env * (0.85f + 0.15f * lfo(&l)) * (bq(&b1, noise()) + 0.8f * bq(&b2, noise()));
    }
}

static void groan(float *o, int n) {
    Biquad f1, f2;
    bq_init(&f1, BQ_BP, 650, 3.0f); bq_init(&f2, BQ_BP, 1050, 4.0f);
    Lfo l;
    lfo_init(&l, 3.0f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / R;
        if ((i & 63) == 0) {                                  /* "oooh" falling: formants slide down */
            float s = fminf(t / 1.2f, 1.0f);
            bq_set(&f1, BQ_BP, 650.0f - 260.0f * s, 3.0f);
            bq_set(&f2, BQ_BP, 1050.0f - 250.0f * s, 4.0f);
        }
        float env = t < 0.18f ? t / 0.18f : expf(-(t - 0.18f) / 0.55f);
        o[i] += env * (0.8f + 0.2f * lfo(&l)) * (bq(&f1, noise()) + 0.5f * bq(&f2, noise()));
    }
}

/* ---- finishing: DC block, normalise, end fades / loop cross-fade ---- */
static void dc_block(float *o, int n) {
    float x1 = 0, y1 = 0;
    for (int i = 0; i < n; i++) { float y = o[i] - x1 + 0.9995f * y1; x1 = o[i]; y1 = y; o[i] = y; }
}
static void normalise(float *o, int n, float level) {
    float pk = 0;
    for (int i = 0; i < n; i++) pk = fmaxf(pk, fabsf(o[i]));
    if (pk > 1e-9f) for (int i = 0; i < n; i++) o[i] *= level / pk;
}
static void fade_ends(float *o, int n, float in_s, float out_s) {
    int a = (int)(in_s * R), b = (int)(out_s * R);
    for (int i = 0; i < a && i < n; i++) o[i] *= (float)i / (float)a;
    for (int i = 0; i < b && i < n; i++) o[n - 1 - i] *= (float)i / (float)b;
}
/* the loop is o[0..n); o[n..n+x) was rendered past its end and is blended into its start (equal power) */
static void loop_crossfade(float *o, int n, int x) {
    for (int i = 0; i < x; i++) {
        float w = (float)i / (float)x;
        o[i] = o[i] * sinf(w * 1.5707963f) + o[n + i] * cosf(w * 1.5707963f);
    }
}

void wp_synth_build(void) {
    if (built) return;
    memset(pool, 0, sizeof pool);
    int at = 0;
    const int XF = (int)(0.5f * R);                           /* loop cross-fade */
    for (int id = 0; id < WP_SND_COUNT; id++) {
        int n = (int)(LEN_S[id] * R);
        bool loop = id == WP_SND_CROWD_BED || id == WP_SND_CROWD_EXCITE;
        float *o = &pool[at];
        int m = loop ? n + XF : n;                            /* loops render a tail to blend into their start */
        if (at + m > POOL_SAMPLES) break;                     /* (sized above; never happens) */
        rng_state = 0x9E3779B9u + (uint32_t)id * 7919u;
        switch (id) {
        case WP_SND_KICK_PASS: thump(o, m, 260, 95, 0.045f, 1.0f, 0.55f, 3500, 0.005f); break;
        case WP_SND_KICK_SHOT:
            thump(o, m, 330, 70, 0.075f, 1.0f, 0.9f, 7000, 0.006f);
            noise_band(o, m, 1800, 1.2f, 0.5f, 0.001f, 0.012f);            /* leather slap */
            break;
        case WP_SND_KICK_LOB:
            thump(o, m, 210, 85, 0.06f, 1.0f, 0.35f, 2500, 0.005f);
            noise_band(o, m, 900, 0.7f, 0.25f, 0.04f, 0.08f);              /* scooped air */
            break;
        case WP_SND_HEADER: thump(o, m, 165, 85, 0.05f, 1.0f, 0.3f, 1100, 0.008f); break;
        case WP_SND_BOUNCE: thump(o, m, 140, 72, 0.035f, 1.0f, 0.2f, 1500, 0.004f); break;
        case WP_SND_POST: post(o, m); break;
        case WP_SND_NET: {
            noise_band(o, m, 2600, 0.6f, 1.0f, 0.008f, 0.13f);
            Lfo l; lfo_init(&l, 25.0f);                                     /* rustle */
            for (int i = 0; i < m; i++) o[i] *= 0.55f + 0.9f * lfo(&l);
            thump(o, m, 120, 70, 0.05f, 0.35f, 0.0f, 1000, 0.01f);
            break;
        }
        case WP_SND_WHISTLE_SHORT: case WP_SND_WHISTLE_LONG: whistle(o, m); break;
        case WP_SND_TACKLE: {
            noise_band(o, m, 700, 0.8f, 1.0f, 0.02f, 0.09f);
            Lfo l; lfo_init(&l, 40.0f);                                     /* grainy grass */
            for (int i = 0; i < m; i++) o[i] *= 0.3f + 1.4f * lfo(&l);
            thump(o, m, 120, 70, 0.04f, 0.4f, 0.0f, 1000, 0.01f);
            break;
        }
        case WP_SND_CROWD_BED: crowd(o, m, 420, 1100, 0.55f, 0.35f, 0.55f, 0.15f); break;
        case WP_SND_CROWD_EXCITE: crowd(o, m, 700, 1700, 0.5f, 0.9f, 0.45f, 0.45f); break;
        case WP_SND_CROWD_ROAR: roar(o, m); break;
        case WP_SND_CROWD_GROAN: groan(o, m); break;
        default: break;
        }
        dc_block(o, m);
        if (loop) loop_crossfade(o, n, XF);
        normalise(o, n, LEVEL[id]);
        if (!loop) {
            bool sharp = id <= WP_SND_POST || id == WP_SND_TACKLE;          /* contacts: a 0.5 ms attack */
            fade_ends(o, n, sharp ? 0.0005f : 0.01f, id == WP_SND_WHISTLE_SHORT || id == WP_SND_WHISTLE_LONG ? 0.04f : 0.02f);
        }
        sounds[id] = (WpSound){o, n, loop, NAME[id]};
        at += m;
        for (int i = n; i < m; i++) pool[at - m + i] = 0;                   /* the tail is not part of the sound */
    }
    built = true;
}

const WpSound *wp_sound(WpSoundId id) { return &sounds[id]; }
