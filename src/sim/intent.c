/* intent.c - the intent buffer per seat (D-048, CP0.5a stage 3). PASS / SHOOT / LOB pressed while the ball is still
 * coming is not dropped: the newest press is kept (with its gauge: the ticks it was held) and fires when the ball
 * gets there, in the fallback order
 *   1. first time: a header (header.c) if it comes in high, a volley / half-volley / lay-off at the aerial contact
 *      (contact.c), or the kick itself the moment he has it at his feet;
 *   2. otherwise he controls it and the intent stays buffered, so the chain fires on the next touch (chest -> pass when
 *      it drops to his feet, chest -> volley as it drops through 1 m).
 * Cancelled 90 ticks after the release, when the ball's predicted arrival slips by more than 42 ticks, when anyone
 * else gets the ball, on a switch, and when play stops. A buffered LOB never becomes a slide (that needs a fresh
 * press). The AI has no buffer (stage 4). */
#include "sim/sim_internal.h"

static int seat_of_p(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return k;
    return -1;
}

/* the predicted contact tick for player i (his plan, else the nearest reachable meeting point), 0xFFFF = none */
static uint16_t eta_now(const SimState *s, int i) {
    const WpPlayer *p = &s->players[i];
    if (p->plan.kind) return p->plan.tick;
    Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
    if (!m.found) return 0xFFFF;
    return (uint16_t)(s->tick + (uint32_t)((m.t * WP_TICK_HZ) / FX_ONE));
}

void intent_clear_seat(SimState *s, int seat) {
    s->intent[seat] = 0xFF;
    s->intent_held[seat] = 0;
    s->intent_gauge[seat] = 0;
    s->intent_age[seat] = 0;
    s->intent_eta[seat] = 0xFFFF;
}

void intent_clear_player(SimState *s, int i) {
    int seat = seat_of_p(s, i);
    if (seat >= 0 && s->intent[seat] != 0xFF) intent_clear_seat(s, seat);
}

void intent_press(SimState *s, int i, uint8_t kind) {
    int seat = seat_of_p(s, i);
    if (seat < 0) return;
    s->intent[seat] = kind;                               /* the newest press wins */
    s->intent_held[seat] = 1;
    s->intent_gauge[seat] = 0;
    s->intent_age[seat] = 0;
    s->intent_eta[seat] = eta_now(s, i);
    s->intent_dir[seat] = s->pick_dir[seat];              /* the ring's direction (stick, latched, memory, facing) */
}

/* the aim of the buffered press, as a stick, when the real stick has been let go */
static WpSeatInput aimed(const SimState *s, int seat, const WpSeatInput *in) {
    WpSeatInput a = *in;
    if (in->move_x || in->move_y) return a;
    fv2 d = fv2_from_angle(s->intent_dir[seat], FX(1.0));
    a.move_x = (int8_t)fx_clamp(d.x * 127 / FX_ONE, -127, 127);
    a.move_y = (int8_t)fx_clamp(d.y * 127 / FX_ONE, -127, 127);
    return a;
}

/* the seat's stick for a strike by player i: his stick, else his buffered press's aim (no intent: as it is) */
WpSeatInput intent_aim(const SimState *s, int i, const WpSeatInput *in) {
    int seat = seat_of_p(s, i);
    if (seat < 0 || s->intent[seat] == 0xFF) return *in;
    return aimed(s, seat, in);
}

void intent_release(SimState *s, int i, uint8_t kind, uint16_t gauge) {
    int seat = seat_of_p(s, i);
    if (seat < 0 || s->intent[seat] != kind) return;
    s->intent_held[seat] = 0;
    s->intent_gauge[seat] = gauge;
}

int intent_of(const SimState *s, int i, uint16_t *gauge) {
    int seat = seat_of_p(s, i);
    if (seat < 0 || s->intent[seat] == 0xFF) return -1;
    *gauge = s->intent_gauge[seat];
    return s->intent[seat];
}

/* the button may still be down when the buffered kick fires: that press is used up */
static void consume(SimState *s, int seat, int i) {
    WpPlayer *p = &s->players[i];
    if (s->intent_held[seat] && p->charge_kind == s->intent[seat]) { p->charge_kind = 0xFF; p->kick_charge = 0; }
    intent_clear_seat(s, seat);
}

void intent_step(SimState *s, int i, const WpSeatInput *in) {
    int seat = seat_of_p(s, i);
    if (seat < 0 || s->intent[seat] == 0xFF) return;
    WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    if (s->intent_held[seat]) {
        if (p->charge_kind == s->intent[seat]) s->intent_gauge[seat] = p->kick_charge;
        else s->intent_held[seat] = 0;
    } else if (b->owner != i && ++s->intent_age[seat] > T_INTENT_MAX_AGE) { intent_clear_seat(s, seat); return; }
    /* (while he has it - a control touch settling - the chain waits: it fires on the next touch) */
    if (s->phase != WP_PHASE_PLAY || (b->owner >= 0 && b->owner != i)) { intent_clear_seat(s, seat); return; }
    if (b->owner < 0 && s->intent_eta[seat] != 0xFFFF) {
        uint16_t e = eta_now(s, i);
        if (e != 0xFFFF && (int16_t)(uint16_t)(e - s->intent_eta[seat]) > T_INTENT_ETA_SLIP) { intent_clear_seat(s, seat); return; }
    }
    if (p->action != WP_ACT_NONE || p->z > 0) return;
    uint8_t kind = s->intent[seat];
    uint16_t gauge = s->intent_gauge[seat];
    int jump;
    WpSeatInput a = aimed(s, seat, in);
    if (b->owner < 0 && header_wanted_at(s, i, &jump, T_INTENT_HEAD_Z)) {   /* HEAD band: head it (pass/shot/lofted) */
        wp_angle dir = a.move_x || a.move_y ? fx_atan2((fx)a.move_y, (fx)a.move_x) : p->facing;
        header_start(s, i, kind == WP_KICK_SHOT ? 1 : kind == WP_KICK_LOB ? 2 : 0, jump);
        p->kick.dir = dir;                                /* aimed at the press: header_contact uses it if the */
        p->kick.power = FX_ONE;                           /* stick is let go by then */
        consume(s, seat, i);
        return;
    }
    if (b->owner == i && !b->held && b->z <= T_CB_GROUND) {   /* at his feet: the kick (first time / after control) */
        if (kind != WP_KICK_SHOT && !in->move_x && !in->move_y) {   /* the ring as it was aimed at the press */
            s->pick_dir[seat] = s->intent_dir[seat];
            s->pick_recv[seat] = (int8_t)pass_pick(s, i, s->intent_dir[seat]);
        }
        consume(s, seat, i);
        player_fire(s, i, kind, gauge, &a);
        return;
    }
    if (b->owner == i && kind == WP_KICK_SHOT && b->vz < 0 && b->z <= T_CB_MID &&   /* chest -> volley on the drop */
        fv2_len(fv2_sub(b->pos, p->pos)) <= T_VOLLEY_REACH) {
        contact_strike(s, i, &a, WP_KICK_SHOT, gauge);
        consume(s, seat, i);
    }
}
