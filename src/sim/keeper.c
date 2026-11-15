/* keeper.c - goalkeeper dives (CP0.3b). The AI keeper (brain.c) decides to dive; this resolves it. A dive throws him
 * sideways along his goal line; while he's in the air his outstretched body is a SEGMENT from his centre toward the
 * dive side (T_DIVE_REACH long). A ball crossing within T_DIVE_HANDS of that segment, low enough, is caught if it's
 * slow enough at body height, otherwise parried out from goal. Then he is down for a moment (T_DIVE_TICKS). No dice:
 * whether he gets there is timing and geometry. */
#include "sim/sim_internal.h"

void keeper_dive_start(SimState *s, int i, int side) {
    WpPlayer *p = &s->players[i];
    p->action = WP_ACT_KEEPER_DIVE;
    p->action_timer = 0;
    p->kick.done = 0;
    p->kick.foot = (uint8_t)(side > 0);               /* dive side for the pose: 1 = +y, 0 = -y */
    p->vel = fv2_make(fx_mul(p->vel.x, FX(0.3)), side > 0 ? T_DIVE_SPEED : -T_DIVE_SPEED);
    p->vz = T_DIVE_HOP;
}

void keeper_step(SimState *s, int i) {
    WpPlayer *p = &s->players[i];
    WpBall *b = &s->ball;
    if (p->action != WP_ACT_KEEPER_DIVE || p->kick.done || s->phase != WP_PHASE_PLAY) return;
    if (p->action_timer > T_DIVE_LIVE) { p->kick.done = 1; return; }   /* landed: no more saves from this dive */
    if (b->owner >= 0 || b->z > T_DIVE_TOP) return;
    fv2 a = p->pos, dir = fv2_make(0, p->kick.foot ? FX_ONE : -FX_ONE);
    fv2 rel = fv2_sub(b->pos, a);
    /* the body stretches out over the first T_DIVE_EXTEND ticks (no full reach the instant he goes) */
    fx reach = fx_mul(T_DIVE_REACH, fx_min(FX_INT(p->action_timer) / T_DIVE_EXTEND, FX_ONE));
    fx u = fx_clamp(fv2_dot(rel, dir), 0, reach);
    fx d = fv2_len(fv2_sub(rel, fv2_scale(dir, u)));
    if (d > T_DIVE_HANDS) return;
    fx speed = fv2_len(fv2_sub(b->vel, p->vel));
    p->kick.done = 1;
    b->last_touch = (int8_t)i;
    if (speed <= T_DIVE_CATCH_REL && b->z <= WP_BAND_BODY_MAX) {          /* held */
        b->vel = p->vel;
        b->vz = 0;
        if (b->z > WP_BAND_FEET_MAX) b->z = WP_BAND_FEET_MAX;
        b->owner = (int8_t)i;
        p->touch_cooldown = 0;
        s->events |= WP_EV_TOUCH;
        return;
    }
    /* parried: pushed back out from his goal and on round the side he dived */
    fx out = p->pos.x > 0 ? -FX_ONE : FX_ONE;
    fx bs = fv2_len(b->vel);
    b->vel = fv2_make(fx_mul(out, fx_mul(bs, FX(0.35))), fx_mul(dir.y, fx_mul(bs, FX(0.25))));
    b->vz = FX(1.5);
    b->spin = 0;
    s->events |= WP_EV_BODY;
}
