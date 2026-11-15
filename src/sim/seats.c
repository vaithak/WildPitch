/* seats.c - which player each human seat controls (switching) and the pass in flight.
 *  - Never switch away from the ball carrier (control()/body rules assume the owner is a seat's player).
 *  - Targeted pass: control hands over to the receiver on the CONTACT tick (a whiff doesn't switch).
 *  - While that pass is in flight there is no auto-switch (the passer would otherwise be nearest and win it back);
 *    the receiver auto-runs to meet the ball until the human moves the stick away from where it was at the pass.
 *  - Loose ball: auto-switch to the outfield team-mate nearest the ball, with hysteresis + cooldown.
 *  - SWITCH button: nearest team-mate to the ball other than the current one; locks auto-switch for a while. */
#include "sim/sim_internal.h"

static int seat_owns(const SimState *s, int player) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == player) return 1;
    return 0;
}

static void do_switch(SimState *s, int seat, int to, uint8_t cool) {
    WpPlayer *old = &s->players[s->seat_player[seat]];
    old->kick_charge = 0;
    old->charge_kind = 0xFF;
    s->seat_player[seat] = (int8_t)to;
    s->switch_cool[seat] = cool;
}

/* nearest outfield team-mate of `team` to the ball, skipping `skip` and players other seats hold */
static int nearest_to_ball(const SimState *s, int team, int skip, fx *out_d) {
    int best = -1; fx bd = 0;
    for (int j = team * s->n_per_side; j < (team + 1) * s->n_per_side; j++) {
        const WpPlayer *p = &s->players[j];
        if (j == skip || p->role == WP_ROLE_KEEPER || seat_owns(s, j)) continue;
        fx d = fv2_len(fv2_sub(s->ball.pos, p->pos));
        if (best < 0 || d < bd) { best = j; bd = d; }
    }
    if (out_d) *out_d = bd;
    return best;
}

void seats_pass_handoff(SimState *s, int kicker, int receiver, const WpSeatInput *in) {
    int seat = -1;
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == kicker) seat = k;
    s->pass_target = (int8_t)receiver;
    s->pass_from = (int8_t)kicker;
    s->pass_ticks = 0;
    if (seat < 0 || seat_owns(s, receiver)) { s->pass_stick[0] = s->pass_stick[1] = -128; return; }
    s->pass_stick[0] = in->move_x;
    s->pass_stick[1] = in->move_y;
    do_switch(s, seat, receiver, T_SWITCH_COOL);
}

void seats_update(SimState *s, const WpInput *in) {
    const WpBall *b = &s->ball;
    /* pass in flight bookkeeping */
    if (s->pass_target >= 0) {
        s->pass_ticks++;
        int ends = s->phase != WP_PHASE_PLAY || b->owner >= 0 || b->last_touch != s->pass_from ||
                   s->pass_ticks > T_PASS_FLIGHT_MAX || (b->z == 0 && fv2_len(b->vel) < FX(1.0));
        if (ends) s->pass_target = -1;
    }
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        int me = s->seat_player[k];
        if (me < 0) continue;
        const WpSeatInput *si = &in->seat[k];
        if (s->switch_cool[k]) s->switch_cool[k]--;
        if (s->pass_target == me && s->pass_stick[0] != -128) {      /* has the human taken over the receiver? */
            int dx = si->move_x - s->pass_stick[0], dy = si->move_y - s->pass_stick[1];
            if (dx * dx + dy * dy > 48 * 48) s->pass_stick[0] = s->pass_stick[1] = -128;
        }
        const WpPlayer *p = &s->players[me];
        if (s->phase != WP_PHASE_PLAY || b->owner == me || (p->action == WP_ACT_KICK && !p->kick.done)) continue;
        int pressed = (si->buttons & WP_BTN_SWITCH) && !(s->prev_buttons[k] & WP_BTN_SWITCH);
        if (pressed) {
            int to = nearest_to_ball(s, p->team, me, 0);
            if (to >= 0) do_switch(s, k, to, T_SWITCH_LOCK_MANUAL);
            continue;
        }
        if (s->switch_cool[k] || s->pass_target >= 0 || b->owner >= 0) continue;
        fx dn;
        int to = nearest_to_ball(s, p->team, me, &dn);
        fx dme = fv2_len(fv2_sub(b->pos, p->pos));
        if (to >= 0 && dme - dn > T_SWITCH_HYST) do_switch(s, k, to, T_SWITCH_COOL);
    }
}

/* Where should player p meet the ball? Steps a cheap prediction (gravity + air drag in flight, rolling decel on
 * the grass, bounces ignored) every 50 ms for up to 4 s; among the points at head height or below that he can run
 * to in time (~6.5 m/s + 0.15 s to react) it takes the one CLOSEST to him: step across to a pass and let it come
 * (soft first touch), wait where a through ball slows, get under a lofted ball. Fallback: where it ends up. */
static fv2 meet_point(const SimState *s, const WpPlayer *p, int through) {
    const WpBall *b = &s->ball;
    fv2 pos = b->pos, v = b->vel;
    fx z = b->z, vz = b->vz, dt = FX(0.05), best_d = -1;
    fv2 best = b->pos;
    for (int k = 1; k <= 80; k++) {
        fx hs = fv2_len(v);
        if (z > 0 || vz > 0) {
            fx kd = fx_min(fx_mul(fx_mul(T_AIR_DRAG, fv2_len(fv2_make(hs, vz))), dt), FX_ONE);
            v = fv2_sub(v, fv2_scale(v, kd));
            vz -= fx_mul(vz, kd) + fx_mul(T_GRAVITY, dt);
            z += fx_mul(vz, dt);
            if (z <= 0) { z = 0; vz = 0; }
        } else if (hs > 0) {
            fx dec = fx_mul(ball_roll_decel(hs), dt);
            v = hs <= dec ? fv2_make(0, 0) : fv2_scale(v, fx_div(hs - dec, hs));
        }
        pos = fv2_add(pos, fv2_scale(v, dt));
        if (z > FX(1.9)) continue;
        if (through && fv2_len(v) <= T_THROUGH_ARRIVE + FX(0.5)) return pos;   /* run onto it where it slows */
        fx d = fv2_len(fv2_sub(pos, p->pos)), t = dt * k, reach = fx_div(d, FX(6.5)) + FX(0.15);
        if (reach <= t && (best_d < 0 || d < best_d)) { best_d = d; best = pos; }
    }
    return best_d >= 0 ? best : pos;
}

/* Assisted receive: the seat's player is the pass target and the human hasn't moved the stick since the pass ->
 * run to where he can first meet the ball (onto a through ball, under a lofted one). Buttons kept. */
void seats_assist_receive(const SimState *s, int seat, WpSeatInput *io) {
    int me = s->seat_player[seat];
    if (me < 0 || s->pass_target != me || s->pass_stick[0] == -128) return;
    const WpBall *b = &s->ball;
    const WpPlayer *p = &s->players[me];
    (void)b;
    fv2 meet = meet_point(s, p, s->pass_from >= 0 && s->players[s->pass_from].kick.kind == WP_KICK_THROUGH);
    uint16_t buttons = io->buttons;
    team_steer(p, meet, FX(0.3), FX(1.5), io);
    io->buttons = buttons;
}
