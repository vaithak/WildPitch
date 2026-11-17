/* stance_vis.c - see stance_vis.h (D-052). */
#include "game/stance_vis.h"
#include <math.h>

#define LOOK_TICKS 24            /* a planned contact shows from this many ticks before it ... */
#define HOLD_TICKS 27            /* ... and the touch's pose plays out for this many after */

static float tof(fx v) { return (float)v / 65536.0f; }

static int seat_of(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return k;
    return -1;
}

/* what the planned touch will be: the stance, or the first-time strike the buffered press / AI intent makes of it */
static uint8_t planned_kind(const SimState *s, int i) {
    const WpPlayer *p = &s->players[i];
    int seat = seat_of(s, i), intent = seat >= 0 ? (s->intent[seat] == 0xFF ? -1 : s->intent[seat])
                                                 : (p->ai_intent ? p->ai_intent - 1 : -1);
    int band = p->plan.band;
    if ((intent == WP_KICK_SHOT || intent == WP_KICK_LOB) && (band == WP_CB_LOW || band == WP_CB_MID)) return WP_LT_VOLLEY;
    if (intent == WP_KICK_PASS && band == WP_CB_LOW) return WP_LT_HALFVOLLEY;
    if (intent == WP_KICK_PASS && band == WP_CB_CHEST) return WP_LT_LAYOFF;
    return p->plan.kind;                                  /* (the control stances share WP_CK_* values) */
}

void wp_stance_fill(const SimState *prev, const SimState *cur, int i, float alpha, float px, float py,
                    WpStanceVis *c, WpAvatarPose *pose) {
    const WpPlayer *p = &cur->players[i];
    uint16_t step = (uint16_t)(cur->tick - 1);            /* the tick `cur` completed (drawn from alpha 0) */
    pose->stance = 0;
    pose->head_turn = 0;
    /* header pass: the head glances toward where it is sent (after the contact) */
    if ((p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER) && p->kick.kind == WP_KICK_HEADER &&
        p->kick.done && p->kick.foot != 1) {
        float d = (float)(int16_t)(uint16_t)(p->kick.dir - p->facing) * (6.2831853f / 65536.0f);
        pose->head_turn = d == 0.0f ? 0.001f : d;
    }
    if (p->role == WP_ROLE_KEEPER) { c->valid = false; return; }
    int lt = p->last_touch_kind;
    int since = (int16_t)(uint16_t)(step - p->last_touch_tick);
    int post = ((lt >= WP_LT_INSTEP && lt <= WP_LT_LAYOFF) || lt == WP_LT_HEADER) && since >= 0 && since <= HOLD_TICKS;
    int heading = (p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER) && p->kick.kind == WP_KICK_HEADER;
    if (post) {
        if (since == 0 || !c->valid || c->tick != p->last_touch_tick) {
            if (since != 0) { c->valid = false; return; }  /* (missed the frame of the touch: no pose) */
            c->x = tof(prev->ball.pos.x); c->y = tof(prev->ball.pos.y); c->z = tof(prev->ball.z);   /* where it was met */
            c->tick = p->last_touch_tick;
            c->valid = true;
        }
        c->kind = (uint8_t)lt;
        if (p->action == WP_ACT_KICK && (lt == WP_LT_VOLLEY || lt == WP_LT_HALFVOLLEY || lt == WP_LT_LAYOFF))
            pose->action = WP_ACT_NONE;                   /* the strike is the stance's follow-through */
    } else if (heading && !p->kick.done) {               /* a header coming: where it will meet his head */
        const WpBall *b = &cur->ball;
        float vx = tof(b->vel.x), vy = tof(b->vel.y), v2 = vx * vx + vy * vy;
        float hx = tof(p->pos.x) + cosf(pose->facing) * 0.12f, hy = tof(p->pos.y) + sinf(pose->facing) * 0.12f;
        float t = v2 > 0.25f ? ((hx - tof(b->pos.x)) * vx + (hy - tof(b->pos.y)) * vy) / v2 : 0;
        if (t < 0 || t * (float)WP_TICK_HZ > (float)LOOK_TICKS) { c->valid = false; return; }
        c->kind = WP_LT_HEADER;
        c->tick = (uint16_t)(step + (uint16_t)(t * (float)WP_TICK_HZ + 0.5f));
        c->x = tof(b->pos.x) + vx * t; c->y = tof(b->pos.y) + vy * t;
        c->z = tof(b->z) + tof(b->vz) * t - 0.5f * 9.81f * t * t;
        c->valid = true;
    } else {
        int left = (int16_t)(uint16_t)(p->plan.tick - step);
        if (p->plan.kind < WP_CK_INSTEP || p->plan.kind > WP_CK_HEADCTL || left < 0 || left > LOOK_TICKS ||
            p->action != WP_ACT_NONE || p->z > 0) { c->valid = false; return; }
        c->kind = planned_kind(cur, i);
        c->tick = p->plan.tick;
        c->x = tof(p->plan.at.x); c->y = tof(p->plan.at.y); c->z = tof(p->plan.z);
        c->valid = true;
    }
    if (pose->action != WP_ACT_NONE && c->kind != WP_LT_HEADER) return;
    float dx = c->x - px, dy = c->y - py, cf = cosf(pose->facing), sf = sinf(pose->facing);
    pose->stance = c->kind;
    pose->stance_t = ((float)(int16_t)(uint16_t)(step - c->tick) + alpha) / (float)WP_TICK_HZ;
    pose->stance_tgt[0] = dx * cf + dy * sf;
    pose->stance_tgt[1] = -dx * sf + dy * cf;
    pose->stance_tgt[2] = c->z;
    pose->stance_leg = pose->stance_tgt[1] > 0.05f ? 0 : 1;   /* the leg on the ball's side (right when central) */
}
