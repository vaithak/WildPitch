/* avatar.c - procedural skeleton footballers. See avatar.h and DECISIONS D-009. */
#include "game/avatar.h"
#include "sim/sim.h"
#include <math.h>

#define PI_F 3.14159265f

const WpAvatarStyle wp_avatar_styles[WP_AVATAR_STYLE_COUNT] = {
    {"Modern",      1.15f, 1.10f, 1.3f, 1},
    {"Arcade",      1.45f, 1.30f, 1.7f, 1},
    {"Retro pixel", 1.40f, 1.30f, 1.0f, 3},
};

/* ------------------------------------------------------------------ soft primitives */

SDL_Texture *wp_soft_disc_create(SDL_Renderer *r) {
    enum { N = 64 };
    static Uint32 px[N * N];
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float dx = ((float)x + 0.5f) / (N / 2) - 1.0f, dy = ((float)y + 0.5f) / (N / 2) - 1.0f;
            float d = sqrtf(dx * dx + dy * dy);
            float a = (1.0f - d) / 0.09f;           /* ~9% edge falloff -> ~1 px at typical sizes */
            a = a < 0 ? 0 : (a > 1 ? 1 : a);
            Uint32 A = (Uint32)(a * 255.0f + 0.5f);
            px[y * N + x] = (A << 24) | 0x00FFFFFFu; /* RGBA32 on little-endian = ABGR word */
        }
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, N, N);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, px, N * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    return t;
}

void wp_soft_disc(SDL_Renderer *r, SDL_Texture *soft, float cx, float cy, float rx, float ry, SDL_FColor c) {
    SDL_Vertex v[4] = {
        {{cx - rx, cy - ry}, c, {0, 0}}, {{cx + rx, cy - ry}, c, {1, 0}},
        {{cx + rx, cy + ry}, c, {1, 1}}, {{cx - rx, cy + ry}, c, {0, 1}},
    };
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, soft, v, 4, idx, 6);
}

/* Capsule = soft quad along the segment (texture row v=0.5 gives a soft profile across the width) + 2 caps. */
void wp_soft_capsule(SDL_Renderer *r, SDL_Texture *soft, float ax, float ay, float ar,
                     float bx, float by, float br, SDL_FColor c) {
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    if (len > 0.01f) {
        float nx = -dy / len, ny = dx / len;
        SDL_Vertex v[4] = {
            {{ax + nx * ar, ay + ny * ar}, c, {0, 0.5f}}, {{ax - nx * ar, ay - ny * ar}, c, {1, 0.5f}},
            {{bx - nx * br, by - ny * br}, c, {1, 0.5f}}, {{bx + nx * br, by + ny * br}, c, {0, 0.5f}},
        };
        static const int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(r, soft, v, 4, idx, 6);
    }
    wp_soft_disc(r, soft, ax, ay, ar, ar, c);
    wp_soft_disc(r, soft, bx, by, br, br, c);
}

/* ------------------------------------------------------------------ appearance */

static uint64_t mix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static float hrand(uint64_t *h) { *h = mix64(*h + 0x9e3779b97f4a7c15ULL); return (float)(*h >> 40) / 16777216.0f; }
static SDL_FColor rgb(float r, float g, float b) { return (SDL_FColor){r, g, b, 1}; }

void wp_avatar_make_look(WpAvatarLook *L, uint64_t seed, int player_index, int team, bool keeper) {
    static const SDL_FColor skins[7] = {
        {0.98f, 0.84f, 0.72f, 1}, {0.93f, 0.76f, 0.62f, 1}, {0.84f, 0.64f, 0.48f, 1}, {0.72f, 0.52f, 0.37f, 1},
        {0.58f, 0.40f, 0.28f, 1}, {0.45f, 0.30f, 0.21f, 1}, {0.34f, 0.23f, 0.17f, 1},
    };
    static const SDL_FColor hairs[6] = {
        {0.08f, 0.07f, 0.07f, 1}, {0.22f, 0.14f, 0.09f, 1}, {0.42f, 0.27f, 0.15f, 1},
        {0.86f, 0.70f, 0.40f, 1}, {0.66f, 0.30f, 0.14f, 1}, {0.62f, 0.62f, 0.60f, 1},
    };
    uint64_t h = mix64(seed * 1315423911ULL + (uint64_t)player_index * 2654435761ULL + 17);
    L->skin = skins[(int)(hrand(&h) * 7) % 7];
    int hc = (int)(hrand(&h) * 6) % 6;
    if (hc == 5 && hrand(&h) < 0.7f) hc = 0;           /* grey hair is rare */
    L->hair = hairs[hc];
    L->hair_style = (uint8_t)((int)(hrand(&h) * 6) % 6);
    L->height = 1.68f + hrand(&h) * 0.24f;
    L->build = 0.92f + hrand(&h) * 0.18f;
    if (team == 0) {
        L->shirt = rgb(0.86f, 0.22f, 0.18f); L->shorts = rgb(0.96f, 0.96f, 0.94f); L->socks = rgb(0.86f, 0.22f, 0.18f);
    } else {
        L->shirt = rgb(0.18f, 0.40f, 0.86f); L->shorts = rgb(0.10f, 0.14f, 0.32f); L->socks = rgb(0.96f, 0.96f, 0.94f);
    }
    L->sleeve = L->shirt;
    L->boots = rgb(0.10f, 0.10f, 0.11f);
    L->gloves = L->skin;
    if (keeper) {
        L->shirt = team == 0 ? rgb(0.98f, 0.82f, 0.12f) : rgb(0.35f, 0.82f, 0.30f);
        L->sleeve = L->shirt;
        L->shorts = rgb(0.12f, 0.12f, 0.13f);
        L->socks = L->shirt;
        L->gloves = rgb(0.95f, 0.95f, 0.95f);
        L->height += 0.06f;
    }
}

/* ------------------------------------------------------------------ animation state */

static float wrap_pi(float a) {
    while (a > PI_F) a -= 2 * PI_F;
    while (a < -PI_F) a += 2 * PI_F;
    return a;
}

void wp_avatar_anim_update(WpAvatarAnim *a, float x, float y, float sim_facing, float speed, float dt) {
    if (!a->init) { a->init = true; a->last_x = x; a->last_y = y; a->facing = sim_facing; a->phase = 0; }
    float dx = x - a->last_x, dy = y - a->last_y, dist = sqrtf(dx * dx + dy * dy);
    if (dist > 3.0f) dist = 0;                         /* teleport (kickoff reset) */
    float cycle = 1.1f + 0.22f * speed;                /* metres per full stride cycle (2 steps) */
    a->phase = fmodf(a->phase + dist / cycle * 2 * PI_F, 2 * PI_F);
    a->last_x = x; a->last_y = y;
    float diff = wrap_pi(sim_facing - a->facing), maxturn = 11.0f * dt;
    a->facing = wrap_pi(a->facing + (diff > maxturn ? maxturn : (diff < -maxturn ? -maxturn : diff)));
}

/* ------------------------------------------------------------------ skeleton */

typedef struct { float x, y, z; } V3;
static V3 v3(float x, float y, float z) { V3 v = {x, y, z}; return v; }
static V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }

enum { J_PELVIS, J_CHEST, J_HEAD, J_HIP_L, J_KNEE_L, J_ANK_L, J_TOE_L, J_HIP_R, J_KNEE_R, J_ANK_R, J_TOE_R,
       J_SH_L, J_ELB_L, J_HAND_L, J_SH_R, J_ELB_R, J_HAND_R, J_COUNT };

typedef struct {             /* floats only: blended field-by-field */
    float lean;                 /* torso pitch forward (rad) */
    float hip[2], knee[2];      /* leg swing (fwd +), knee bend (>=0); [0]=left [1]=right */
    float leg_out[2];           /* lateral splay (rad) */
    float sh[2], elb[2], ab[2]; /* arm swing fwd, elbow bend, abduction */
    float nod;                  /* extra head pitch */
    float crouch;               /* extra pelvis drop (m) */
} Pose;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

static void build_pose(const WpAvatarPose *P, Pose *o) {
    float amp = clampf(P->speed / 7.0f, 0, 1);
    float run = smooth01(P->speed / 1.2f);           /* 0 idle .. 1 moving */
    for (int s = 0; s < 2; s++) {
        float p = P->phase + (float)s * PI_F;
        o->hip[s] = run * (0.18f + 0.50f * amp) * sinf(p);
        o->knee[s] = 0.12f + run * (0.15f + (0.55f + 0.75f * amp) * fmaxf(0.0f, cosf(p)));
        o->leg_out[s] = 0.05f;
        o->sh[s] = -run * (0.25f + 0.55f * amp) * sinf(p);
        o->elb[s] = 0.25f + run * (0.45f + 0.75f * amp);
        o->ab[s] = 0.10f + 0.04f * (1 - run);
    }
    o->lean = 0.04f + 0.012f * sinf(P->time * 2.1f) * (1 - run) + run * (0.06f + 0.22f * amp);
    o->nod = 0; o->crouch = 0;

    if (P->keeper && P->action == WP_ACT_NONE && P->speed < 1.5f) {      /* ready stance */
        for (int s = 0; s < 2; s++) {
            o->hip[s] = 0.45f; o->knee[s] = 0.75f; o->leg_out[s] = 0.16f;
            o->sh[s] = 0.75f; o->elb[s] = 0.7f; o->ab[s] = 0.45f;
        }
        o->lean = 0.38f;
    }
    if (P->action == WP_ACT_NONE) return;
    /* Action poses are computed separately and blended over the locomotion pose with a weight that ramps in
     * (~0.06 s) and out before the sim clears the action, so nothing pops. */
    Pose a = *o;
    float t = P->action_t, w = 0;
    switch (P->action) {
    case WP_ACT_KICK: {          /* right-footed strike; sim clears the action after 0.5 s */
        float back = smooth01(t / 0.16f), strike = smooth01((t - 0.16f) / 0.11f), rec = smooth01((t - 0.32f) / 0.12f);
        a.hip[1] = (-0.8f * back + 2.1f * strike) * (1 - rec) + 0.1f * rec;
        a.knee[1] = (0.15f + 1.35f * back * (1 - strike) + 0.1f * strike) * (1 - rec) + 0.2f * rec;
        a.hip[0] = 0.25f; a.knee[0] = 0.35f;
        a.sh[0] = 0.5f; a.ab[0] = 0.9f; a.elb[0] = 0.5f;
        a.sh[1] = -0.6f * strike; a.ab[1] = 0.6f;
        a.lean = 0.10f - 0.12f * strike * (1 - rec);
        w = smooth01(t / 0.06f) * (1 - smooth01((t - 0.36f) / 0.12f));
        break;
    }
    case WP_ACT_JUMP_HEADER:
    case WP_ACT_STAND_HEADER: {  /* lasts until landing (~0.69 s for a full jump) */
        for (int s = 0; s < 2; s++) {
            a.hip[s] = s ? 0.55f : -0.15f;
            a.knee[s] = s ? 1.1f : 1.4f;
            a.sh[s] = 1.1f; a.ab[s] = 1.0f; a.elb[s] = 0.95f;
        }
        float nod = t < 0.28f ? -0.35f * smooth01(t / 0.2f) : -0.35f + 0.95f * smooth01((t - 0.28f) / 0.08f);
        a.lean = 0.05f + nod * 0.8f;
        a.nod = nod * 0.6f;
        w = smooth01(t / 0.06f) * (1 - smooth01((t - 0.50f) / 0.17f));
        break;
    }
    default: break;
    }
    float *dst = (float *)o;
    const float *src = (const float *)&a;
    for (size_t i = 0; i < sizeof(Pose) / sizeof(float); i++) dst[i] += (src[i] - dst[i]) * w;
}

static void solve(const WpAvatarPose *P, const WpAvatarLook *L, const WpAvatarStyle *S, float x, float y,
                  V3 *J, float *head_r) {
    Pose o;
    build_pose(P, &o);
    float k = L->height / 1.80f, w = L->build;
    float cf = cosf(P->facing), sf = sinf(P->facing);
    V3 F = v3(cf, sf, 0), Lt = v3(-sf, cf, 0), U = v3(0, 0, 1);
    float hip_w = 0.10f * w * k, sh_w = 0.20f * w * k;
    float thigh = 0.45f * k, shin = 0.44f * k, foot = 0.19f * k, torso = 0.50f * k;
    float uarm = 0.28f * k, farm = 0.26f * k, neck = 0.08f * k;
    *head_r = 0.115f * k * S->head_scale;

    V3 pelvis = v3(x, y, 0.95f * k - o.crouch);
    J[J_PELVIS] = pelvis;
    V3 tdir = add(mul(F, sinf(o.lean)), mul(U, cosf(o.lean)));
    J[J_CHEST] = add(pelvis, mul(tdir, torso));
    float hp = o.lean + o.nod;
    J[J_HEAD] = add(J[J_CHEST], mul(add(mul(F, sinf(hp)), mul(U, cosf(hp))), neck + *head_r * 1.05f));
    for (int s = 0; s < 2; s++) {
        float side = s == 0 ? 1.0f : -1.0f;             /* left = +Lt */
        int jh = s ? J_HIP_R : J_HIP_L;
        V3 hip = add(pelvis, mul(Lt, side * hip_w));
        float a = o.hip[s], b = a - o.knee[s], so = o.leg_out[s] * side;
        V3 d1 = add(add(mul(F, sinf(a)), mul(U, -cosf(a))), mul(Lt, sinf(so)));
        V3 d2 = add(add(mul(F, sinf(b)), mul(U, -cosf(b))), mul(Lt, sinf(so) * 0.5f));
        J[jh] = hip;
        J[jh + 1] = add(hip, mul(d1, thigh));
        J[jh + 2] = add(J[jh + 1], mul(d2, shin));
        float fa = b + 1.45f;                               /* foot roughly perpendicular to shin */
        J[jh + 3] = add(J[jh + 2], mul(add(mul(F, sinf(fa)), mul(U, -cosf(fa))), foot));
        int js = s ? J_SH_R : J_SH_L;
        V3 sh = add(add(J[J_CHEST], mul(Lt, side * sh_w)), mul(tdir, -0.05f * k));
        float aa = o.sh[s], ab = o.ab[s], ae = aa + o.elb[s];
        V3 a1 = add(add(mul(F, sinf(aa) * cosf(ab)), mul(U, -cosf(aa) * cosf(ab))), mul(Lt, side * sinf(ab)));
        V3 a2 = add(add(mul(F, sinf(ae) * cosf(ab)), mul(U, -cosf(ae) * cosf(ab))), mul(Lt, side * sinf(ab) * 0.6f));
        J[js] = sh;
        J[js + 1] = add(sh, mul(a1, uarm));
        J[js + 2] = add(J[js + 1], mul(a2, farm));
    }
    /* ground contact: lowest foot point sits on the grass (+ jump height). Produces natural run bob. */
    float lowest = 1e9f;
    int feet[4] = {J_ANK_L, J_TOE_L, J_ANK_R, J_TOE_R};
    for (int i = 0; i < 4; i++) lowest = fminf(lowest, J[feet[i]].z);
    float shift = (0.05f * k + P->z) - lowest;
    for (int i = 0; i < J_COUNT; i++) J[i].z += shift;
}

/* ------------------------------------------------------------------ drawing */

typedef struct {
    int a, b;            /* joint indices; b = -1 for a disc */
    float ra, rb;        /* radii in metres */
    float t0, t1;        /* sub-segment (0..1) of a->b, for kit splits */
    SDL_FColor col;
    float depth;
    bool limb;
} Part;

static V3 lerp3(V3 a, V3 b, float t) { return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

void wp_avatar_draw(SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam, float x, float y,
                    const WpAvatarPose *P, const WpAvatarLook *L, const WpAvatarStyle *S) {
    V3 J[J_COUNT + 4];
    float head_r;
    solve(P, L, S, x, y, J, &head_r);
    float k = L->height / 1.80f, ls = S->limb_scale * L->build;

    Part parts[40];
    int n = 0;
#define SEG(A, B, RA, RB, T0, T1, C, LIMB) parts[n++] = (Part){A, B, (RA), (RB), T0, T1, C, 0, LIMB}
    for (int s = 0; s < 2; s++) {
        int h = s ? J_HIP_R : J_HIP_L, sh = s ? J_SH_R : J_SH_L;
        SEG(h, h + 1, 0.085f * ls * k, 0.07f * ls * k, 0.0f, 0.55f, L->shorts, true);   /* shorts */
        SEG(h, h + 1, 0.072f * ls * k, 0.062f * ls * k, 0.5f, 1.0f, L->skin, true);    /* thigh */
        SEG(h + 1, h + 2, 0.062f * ls * k, 0.05f * ls * k, 0.0f, 0.18f, L->skin, true); /* knee */
        SEG(h + 1, h + 2, 0.064f * ls * k, 0.052f * ls * k, 0.15f, 1.0f, L->socks, true);/* sock */
        SEG(h + 2, h + 3, 0.055f * ls * k, 0.045f * ls * k, 0.0f, 1.0f, L->boots, true);/* boot */
        bool longsleeve = P->keeper;
        SEG(sh, sh + 1, 0.062f * ls * k, 0.055f * ls * k, 0.0f, longsleeve ? 1.0f : 0.6f, L->sleeve, true);
        if (!longsleeve) SEG(sh, sh + 1, 0.05f * ls * k, 0.047f * ls * k, 0.55f, 1.0f, L->skin, true);
        SEG(sh + 1, sh + 2, 0.047f * ls * k, 0.042f * ls * k, 0.0f, 1.0f, longsleeve ? L->sleeve : L->skin, true);
        parts[n++] = (Part){sh + 2, -1, 0.055f * ls * k, 0, 0, 0, P->keeper ? L->gloves : L->skin, 0, true};
    }
    /* torso: shirt mass from hips to shoulders (two capsules give width that turns with the body) */
    SEG(J_HIP_L, J_SH_L, 0.12f * ls * k, 0.12f * ls * k, 0.0f, 1.0f, L->shirt, false);
    SEG(J_HIP_R, J_SH_R, 0.12f * ls * k, 0.12f * ls * k, 0.0f, 1.0f, L->shirt, false);
    SEG(J_PELVIS, J_CHEST, 0.15f * ls * k, 0.16f * ls * k, 0.05f, 0.92f, L->shirt, false);
    SEG(J_HIP_L, J_HIP_R, 0.11f * ls * k, 0.11f * ls * k, 0.0f, 1.0f, L->shorts, false);
    SEG(J_CHEST, J_HEAD, 0.05f * ls * k, 0.05f * ls * k, 0.0f, 0.5f, L->skin, false);        /* neck */
#undef SEG
    int torso_first = n - 5;
    int neck_idx = n - 1;
    /* shirt number patch on the back: makes front vs back readable at match scale */
    float cf0 = cosf(P->facing), sf0 = sinf(P->facing);
    if (sf0 > 0.15f) {
        int extra = J_COUNT;                         /* virtual joints appended after the skeleton */
        V3 back = add(J[J_CHEST], v3(-cf0 * 0.15f * k * ls, -sf0 * 0.15f * k * ls, -0.08f * k));
        V3 lat = v3(-sf0 * 0.04f * k, cf0 * 0.04f * k, 0);
        J[extra] = add(back, lat);      J[extra + 1] = add(J[extra], v3(0, 0, -0.16f * k));
        J[extra + 2] = add(back, mul(lat, -1)); J[extra + 3] = add(J[extra + 2], v3(0, 0, -0.16f * k));
        SDL_FColor num = P->keeper ? (SDL_FColor){0.1f, 0.1f, 0.1f, 1} : (SDL_FColor){0.97f, 0.97f, 0.95f, 1};
        parts[n++] = (Part){extra, extra + 1, 0.022f * k, 0.022f * k, 0, 1, num, 0, false};
        parts[n++] = (Part){extra + 2, extra + 3, 0.022f * k, 0.022f * k, 0, 1, num, 0, false};
        parts[n - 1].depth = parts[n - 2].depth = -1;   /* marker: always just above the shirt (set below) */
    } else if (sf0 < -0.15f) {                          /* club crest on the front-left chest */
        int extra = J_COUNT;
        V3 lat = v3(-sf0, cf0, 0);
        J[extra] = add(add(J[J_CHEST], v3(cf0 * 0.15f * k * ls, sf0 * 0.15f * k * ls, -0.13f * k)), mul(lat, 0.07f * k));
        SDL_FColor crest = P->keeper ? (SDL_FColor){0.1f, 0.1f, 0.1f, 1} : (SDL_FColor){0.98f, 0.86f, 0.25f, 1};
        parts[n++] = (Part){extra, -1, 0.035f * k, 0, 0, 0, crest, 0, false};
    }

    /* depth along the camera's view direction: farther = larger. Raised points are nearer the camera. */
    float body_depth = 0;
    for (int i = 0; i < n; i++) {
        Part *p = &parts[i];
        V3 a = J[p->a], b = p->b >= 0 ? J[p->b] : a;
        V3 m = lerp3(a, b, (p->t0 + p->t1) * 0.5f);
        p->depth = m.y - cam->tilt * m.z;
        if (i >= torso_first && i <= neck_idx) body_depth += p->depth / 5.0f;
    }
    parts[neck_idx].depth += 1.0f;   /* neck always behind shirt and head (else it shows as a strip on the chest) */
    for (int i = neck_idx + 1; i < n; i++) parts[i].depth = body_depth - 0.2f;   /* number patch: on top of shirt */
    for (int i = 1; i < n; i++) {                     /* far first */
        Part t = parts[i]; int j = i - 1;
        while (j >= 0 && parts[j].depth < t.depth) { parts[j + 1] = parts[j]; j--; }
        parts[j + 1] = t;
    }

    WpScreenPt base = wp_project(cam, x, y, 0);
    float px_per_m = cam->ppm * base.s;
    float ol = S->outline_px;
    SDL_FColor ink = {0.07f, 0.07f, 0.09f, 1};

    /* pass 1: silhouette outline (union of all parts, slightly fattened) */
    if (ol > 0)
        for (int i = 0; i < n; i++) {
            Part *p = &parts[i];
            V3 a = J[p->a], b = p->b >= 0 ? J[p->b] : a;
            V3 a2 = lerp3(a, b, p->t0), b2 = lerp3(a, b, p->t1);
            WpScreenPt sa = wp_project(cam, a2.x, a2.y, a2.z), sb = wp_project(cam, b2.x, b2.y, b2.z);
            wp_soft_capsule(r, soft, sa.x, sa.y, p->ra * px_per_m + ol, sb.x, sb.y, p->rb * px_per_m + ol, ink);
        }
    /* head outline */
    WpScreenPt hs = wp_project(cam, J[J_HEAD].x, J[J_HEAD].y, J[J_HEAD].z);
    float hr = head_r * px_per_m;
    float hair_big = L->hair_style == 3 ? 1.22f : 1.0f;
    float head_depth = J[J_HEAD].y - cam->tilt * J[J_HEAD].z;
    if (ol > 0) wp_soft_disc(r, soft, hs.x, hs.y, hr * hair_big + ol, hr * hair_big + ol, ink);

    /* pass 2: fills far -> near; limbs in front of the body get their own outline for separation */
    bool head_drawn = false;
    for (int i = 0; i <= n; i++) {
        if (!head_drawn && (i == n || parts[i].depth < head_depth - 0.02f)) {
            head_drawn = true;
            /* head: hair cap, then face offset toward where the player looks */
            float cf = cosf(P->facing), sf = sinf(P->facing);
            float toward = -sf;                          /* 1 = facing the camera (-y) */
            if (L->hair_style == 2 || L->hair_style == 5) {   /* long hair / bun hangs behind */
                WpScreenPt hb = wp_project(cam, J[J_HEAD].x - cf * head_r * 0.5f, J[J_HEAD].y - sf * head_r * 0.5f,
                                           J[J_HEAD].z - (L->hair_style == 2 ? head_r * 0.6f : -head_r * 0.7f));
                float rr = hr * (L->hair_style == 2 ? 0.8f : 0.55f);
                if (ol > 0) wp_soft_disc(r, soft, hb.x, hb.y, rr + ol, rr + ol, ink);
                wp_soft_disc(r, soft, hb.x, hb.y, rr, rr, L->hair);
            }
            SDL_FColor cap = L->hair_style == 4 ? L->skin : L->hair;
            if (L->hair_style == 1) cap = (SDL_FColor){(L->hair.r + L->skin.r) * 0.5f, (L->hair.g + L->skin.g) * 0.5f,
                                                       (L->hair.b + L->skin.b) * 0.5f, 1};
            wp_soft_disc(r, soft, hs.x, hs.y, hr * hair_big, hr * hair_big, cap);
            if (toward > -0.35f) {
                float vis = clampf((toward + 0.35f) / 0.8f, 0, 1);
                float fx_ = hs.x + cf * hr * 0.32f, fy_ = hs.y + hr * (0.22f - 0.05f * vis);
                float fr = hr * (0.62f + 0.22f * vis);
                wp_soft_disc(r, soft, fx_, fy_, fr, fr * (0.95f + 0.05f * vis), L->skin);
                if (hr > 5.5f && vis > 0.5f) {           /* eyes only when big enough to read */
                    float ex = hr * 0.28f, ey = fy_ - fr * 0.12f, er = fmaxf(0.8f, hr * 0.09f);
                    SDL_FColor eye = {0.1f, 0.08f, 0.08f, 1};
                    wp_soft_disc(r, soft, fx_ - ex + cf * hr * 0.1f, ey, er, er, eye);
                    wp_soft_disc(r, soft, fx_ + ex + cf * hr * 0.1f, ey, er, er, eye);
                }
            }
        }
        if (i == n) break;
        Part *p = &parts[i];
        V3 a = J[p->a], b = p->b >= 0 ? J[p->b] : a;
        V3 a2 = lerp3(a, b, p->t0), b2 = lerp3(a, b, p->t1);
        WpScreenPt sa = wp_project(cam, a2.x, a2.y, a2.z), sb = wp_project(cam, b2.x, b2.y, b2.z);
        if (ol > 0 && p->limb && p->depth < body_depth - 0.04f && p->t0 == 0.0f)
            wp_soft_capsule(r, soft, sa.x, sa.y, p->ra * px_per_m + ol * 0.8f, sb.x, sb.y, p->rb * px_per_m + ol * 0.8f, ink);
        wp_soft_capsule(r, soft, sa.x, sa.y, p->ra * px_per_m, sb.x, sb.y, p->rb * px_per_m, p->col);
    }
}
