/* avatar_stance.c - aerial contact stances (D-052, CP0.5a stage 5). Presentation only.
 * The sim plans an aerial touch (WpPlan: stance, contact tick, the ball's point and height) and marks the touch it
 * made (WpPlayer.last_touch_kind / tick); stance_vis.c turns that into WpAvatarPose.stance / stance_t / stance_tgt.
 * Here each stance is a pose family with a few free parameters, fitted (a small coordinate search over the real
 * skeleton) so that the limb's contact centre - chest / thigh top / forehead plus a ball radius, or the instep - is on
 * the ball's centre at the contact tick. The family ramps in over ~0.3 s (peaking exactly on the contact tick, the
 * procedural "strike frame") and out over ~0.3 s after, with a cushion (control) or a follow-through (volleys).
 *  - chest / lay-off: arms out, lean back (or over a low ball), knees give;   head control: arms out, a little hop
 *  - thigh: knee raised under the ball, standing leg bent;                    instep: the foot lifted to it
 *  - volley / half-volley: side-on, body tilted away from the striking leg, backswing -> contact -> follow-through
 *  - header (incl. the header pass's glance): the header action's own pose, nudged (lean / rise / a step) so the
 *    forehead meets the ball on the contact tick (avatar_header_reach) */
#include "game/avatar_internal.h"
#include "sim/sim.h"

enum { F_NONE = 0, F_CHEST, F_HEAD, F_THIGH, F_FOOT, F_HEADER, F_N };
static int family(uint8_t st) {
    return st == WP_LT_HEADER ? F_HEADER : st == WP_LT_CHEST || st == WP_LT_LAYOFF ? F_CHEST : st == WP_LT_HEADCTL ? F_HEAD : st == WP_LT_THIGH ? F_THIGH
         : st == WP_LT_INSTEP || st == WP_LT_VOLLEY || st == WP_LT_HALFVOLLEY ? F_FOOT : F_NONE;
}

#define NP 5
/* parameter ranges and starting values per family:
 *  chest / head: lean, knee give, rise, shift forward, shift left
 *  thigh: hip of the raised leg, its splay, standing-leg give, shift forward, shift left
 *  foot: hip, knee, splay of the striking leg, shift forward, shift left */
static const float LO[F_N][NP] = {{0}, {-0.45f, 0, 0, -0.30f, -0.35f}, {-0.35f, 0, 0, -0.30f, -0.35f},
                                  {0.20f, -0.35f, 0, -0.30f, -0.35f}, {-0.30f, 0.05f, -0.40f, -0.30f, -0.35f},
                                  {-0.30f, 0, -0.10f, -0.35f, -0.35f}};
static const float HI[F_N][NP] = {{0}, {0.55f, 0.70f, 0.15f, 0.35f, 0.35f}, {0.45f, 0.50f, 0.35f, 0.35f, 0.35f},
                                  {1.70f, 0.60f, 0.50f, 0.35f, 0.35f}, {1.80f, 1.80f, 1.30f, 0.35f, 0.35f},
                                  {0.70f, 0, 0.30f, 0.35f, 0.35f}};
static const float Q0[F_N][NP] = {{0}, {-0.15f, 0.10f, 0, 0, 0}, {0.0f, 0.05f, 0, 0, 0},
                                  {1.2f, 0.10f, 0.10f, 0, 0}, {0.6f, 0.6f, 0.10f, 0, 0}, {0, 0, 0, 0, 0}};
/* the fit prefers the limb to the lunge: cost per unit away from Q0 (m of miss per rad / m) */
static const float RW[F_N][NP] = {{0}, {0.02f, 0.02f, 0.10f, 0.12f, 0.12f}, {0.02f, 0.02f, 0.05f, 0.12f, 0.12f},
                                  {0.08f, 0.02f, 0.02f, 0.12f, 0.12f}, {0.01f, 0.01f, 0.02f, 0.12f, 0.12f},
                                  {0.03f, 0, 0.05f, 0.12f, 0.12f}};

/* the family's pose at the contact itself (weight 1) for parameters q */
static void family_pose(const WpAvatarPose *P, const float *q, Pose *a) {
    int f = family(P->stance), s = P->stance_leg ? 1 : 0, o = 1 - s;
    a->shift_f = q[3]; a->shift_l = q[4];
    a->roll = 0; a->rise = 0; a->crouch = 0; a->nod = 0;
    if (f == F_CHEST || f == F_HEAD) {
        float give = q[1];
        for (int k = 0; k < 2; k++) {                      /* knees give (the body sinks to a low ball) */
            a->hip[k] = (k ? -0.10f : 0.12f) + 0.9f * give;
            a->knee[k] = 0.25f + 1.9f * give;
            a->leg_out[k] = 0.08f;
            a->sh[k] = f == F_CHEST ? 0.45f : 0.6f; a->ab[k] = f == F_CHEST ? 1.15f : 0.9f; a->elb[k] = 0.55f;
        }
        a->lean = q[0];
        a->rise = q[2];
        a->nod = f == F_CHEST ? -0.20f : -0.10f;           /* eyes on the ball */
    } else if (f == F_THIGH) {
        a->hip[s] = q[0]; a->knee[s] = q[0]; a->leg_out[s] = q[1];   /* thigh up, shin hanging */
        a->hip[o] = 0.05f + 0.4f * q[2]; a->knee[o] = 0.15f + 0.8f * q[2]; a->leg_out[o] = 0.06f;
        a->lean = 0.10f;
        for (int k = 0; k < 2; k++) { a->sh[k] = 0.30f; a->ab[k] = 0.80f; a->elb[k] = 0.60f; }
    } else if (f == F_FOOT) {
        int volley = P->stance == WP_LT_VOLLEY || P->stance == WP_LT_HALFVOLLEY;
        a->hip[s] = q[0]; a->knee[s] = q[1]; a->leg_out[s] = q[2];
        a->hip[o] = 0.10f; a->knee[o] = 0.30f; a->leg_out[o] = 0.08f;
        a->lean = volley ? -0.12f : 0.12f;
        a->roll = volley ? (s ? 0.25f : -0.25f) : 0;       /* side-on: tilted away from the striking leg */
        for (int k = 0; k < 2; k++) { a->sh[k] = 0.40f; a->ab[k] = 0.90f; a->elb[k] = 0.50f; }
    }
}

void avatar_header_reach(const WpAvatarPose *P, Pose *o) {
    if (family(P->stance) != F_HEADER) return;
    float t = P->stance_t, w = t < 0 ? smooth01((t + 0.20f) / 0.16f) : 1 - smooth01((t - 0.06f) / 0.20f);
    o->lean += P->stance_prm[0] * w;
    o->rise += P->stance_prm[2] * w;
    o->shift_f += P->stance_prm[3] * w;
    o->shift_l += P->stance_prm[4] * w;
}

float avatar_stance_pose(const WpAvatarPose *P, Pose *a) {
    int f = family(P->stance), s = P->stance_leg ? 1 : 0;
    if (f == F_NONE || f == F_HEADER) return 0;
    family_pose(P, P->stance_prm, a);
    float t = P->stance_t;
    if (t < 0) {
        if (P->stance == WP_LT_VOLLEY || P->stance == WP_LT_HALFVOLLEY) {   /* backswing, gone by the contact */
            float b = smooth01((t + 0.30f) / 0.12f) * (1 - smooth01((t + 0.08f) / 0.08f));
            a->hip[s] -= 1.1f * b; a->knee[s] += 0.9f * b;
        }
        return smooth01((t + 0.34f) / 0.30f);             /* 1 from 0.04 s before the contact */
    }
    float c = smooth01(t / 0.18f);
    switch (P->stance) {                                  /* after it: cushion / follow-through */
    case WP_LT_CHEST:      a->lean -= 0.12f * c; break;
    case WP_LT_LAYOFF:     a->lean += 0.15f * c; break;
    case WP_LT_HEADCTL:    a->nod += 0.25f * c; a->rise *= 1 - c; break;
    case WP_LT_THIGH:      a->hip[s] -= 0.35f * c; a->knee[s] -= 0.35f * c; break;
    case WP_LT_INSTEP:     a->hip[s] -= 0.25f * c; a->knee[s] += 0.20f * c; break;
    case WP_LT_VOLLEY: case WP_LT_HALFVOLLEY: {
        float g = smooth01(t / 0.12f);
        a->hip[s] += 0.7f * g; a->knee[s] *= 1 - 0.7f * g;
        break;
    }
    default: break;
    }
    return 1 - smooth01((t - 0.10f) / 0.30f);
}

/* the limb's contact centre for a solved skeleton (origin = his position, facing F) */
static V3 contact_centre(const WpAvatarPose *P, const V3 *J, float head_r, float k, V3 tgt) {
    int f = family(P->stance), s = P->stance_leg ? 1 : 0;
    V3 F = v3(cosf(P->facing), sinf(P->facing), 0), U = v3(0, 0, 1);
    const float BR = 0.11f;                                /* the ball's radius */
    if (f == F_CHEST) {
        V3 d = add(J[J_CHEST], mul(J[J_PELVIS], -1));
        float dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        d = mul(d, 1.0f / fmaxf(dl, 1e-4f));
        float fd = F.x * d.x + F.y * d.y + F.z * d.z;
        V3 n = add(F, mul(d, -fd));                         /* the chest's front, square to the spine */
        float nl = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        n = mul(n, 1.0f / fmaxf(nl, 1e-4f));
        V3 sternum = add(J[J_PELVIS], mul(add(J[J_CHEST], mul(J[J_PELVIS], -1)), 0.85f));
        return add(sternum, mul(n, 0.13f * k + BR));
    }
    if (f == F_HEAD || f == F_HEADER) {                   /* anywhere on the head: a glancing header too */
        V3 d = add(tgt, mul(J[J_HEAD], -1));
        float dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        return add(J[J_HEAD], dl > 1e-4f ? mul(d, (head_r + BR) / dl) : mul(F, head_r + BR));
    }
    int h = s ? J_HIP_R : J_HIP_L;
    if (f == F_THIGH) {
        V3 d = add(J[h + 1], mul(J[h], -1));
        float dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        d = mul(d, 1.0f / fmaxf(dl, 1e-4f));
        float ud = d.z;
        V3 n = add(U, mul(d, -ud));                         /* the thigh's top, square to it */
        float nl = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        n = mul(n, 1.0f / fmaxf(nl, 1e-4f));
        return add(add(J[h], mul(add(J[h + 1], mul(J[h], -1)), 0.7f)), mul(n, 0.07f * k + BR));
    }
    return mul(add(J[h + 2], J[h + 3]), 0.5f);              /* the instep */
}

static float miss(const WpAvatarPose *P, const WpAvatarLook *L, const WpAvatarStyle *S, V3 tgt, V3 *at) {
    V3 J[J_COUNT + 4];
    float hr;
    avatar_solve(P, L, S, 0, 0, J, &hr);
    V3 c = contact_centre(P, J, hr, L->height / 1.80f, tgt);
    if (at) *at = c;
    V3 d = add(c, mul(tgt, -1));
    return sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
}

static float cost(const WpAvatarPose *Q, int f, const WpAvatarLook *L, const WpAvatarStyle *S, V3 tgt) {
    float c = miss(Q, L, S, tgt, 0);
    for (int k = 0; k < NP; k++) c += RW[f][k] * fabsf(Q->stance_prm[k] - Q0[f][k]);
    return c;
}

/* the target in world terms around the origin, from his frame (forward, left, up) */
static V3 target_world(const WpAvatarPose *P) {
    float cf = cosf(P->facing), sf = sinf(P->facing);
    return v3(cf * P->stance_tgt[0] - sf * P->stance_tgt[1], sf * P->stance_tgt[0] + cf * P->stance_tgt[1], P->stance_tgt[2]);
}

void avatar_stance_fit(WpAvatarPose *P, const WpAvatarLook *L, const WpAvatarStyle *S) {
    int f = family(P->stance);
    if (f == F_NONE) { P->stance = 0; return; }
    WpAvatarPose Q = *P;                                  /* fitted at the contact itself (a header: in his */
    Q.stance_t = 0;                                       /* jump, as drawn; the others standing) */
    if (f != F_HEADER) { Q.speed = 0; Q.z = 0; Q.action = WP_ACT_NONE; Q.charge01 = 0; Q.touch01 = 0; Q.holding = false; Q.throwin_hold = false; }
    V3 tgt = target_world(&Q);
    for (int k = 0; k < NP; k++) Q.stance_prm[k] = Q0[f][k];
    float best = cost(&Q, f, L, S, tgt);
    for (int round = 0; round < 10; round++) {
        for (int k = 0; k < NP; k++) {
            float step = (HI[f][k] - LO[f][k]) * 0.25f / (float)(1 << round);
            for (int dir = -1; dir <= 1; dir += 2) {
                float keep = Q.stance_prm[k];
                Q.stance_prm[k] = clampf(keep + (float)dir * step, LO[f][k], HI[f][k]);
                float m = cost(&Q, f, L, S, tgt);
                if (m + 1e-4f < best) { best = m; break; }
                Q.stance_prm[k] = keep;
            }
        }
    }
    for (int k = 0; k < NP; k++) P->stance_prm[k] = Q.stance_prm[k];
}

void wp_avatar_stance_point(const WpAvatarPose *pose, const WpAvatarLook *look, const WpAvatarStyle *style,
                            float x, float y, float out[3]) {
    WpAvatarPose Q = *pose;
    if (!family(Q.stance)) { out[0] = x; out[1] = y; out[2] = 0; return; }
    avatar_stance_fit(&Q, look, style);
    V3 J[J_COUNT + 4];
    float hr;
    avatar_solve(&Q, look, style, x, y, J, &hr);
    V3 tw = target_world(&Q);
    V3 c = contact_centre(&Q, J, hr, look->height / 1.80f, v3(tw.x + x, tw.y + y, tw.z));
    out[0] = c.x; out[1] = c.y; out[2] = c.z;
}
