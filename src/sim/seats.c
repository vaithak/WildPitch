/* seats.c - which player each human seat controls (switching), the pass in flight, and the assisted receiver.
 *  - Never switch away from the ball carrier (control()/body rules assume the owner is a seat's player).
 *  - Targeted pass: control hands over to the receiver on the CONTACT tick (a whiff doesn't switch).
 *  - While that pass is in flight there is no auto-switch (the passer would otherwise be nearest and win it back).
 *  - Assisted receive (D-020): the receiver plays the pass by itself — runners run THROUGH the planned meeting point
 *    at just-in-time speed, standing/checking receivers step to the closest reachable point. The human's stick never
 *    cancels this: released or unchanged = no opinion; any other direction PICKS among the reachable meeting points
 *    (along the ball = meet it later on the move, toward it = attack it, sideways = line up that side). Full manual
 *    control only after the stick points > 120 deg away from the meet for 250 ms, a SWITCH press, the ball having
 *    gone (no reachable point for 6 ticks) or the pass ending. A standing receiver (meet at his feet) is taken
 *    over by holding any direction that isn't toward the ball for 250 ms (to make a run while it travels).
 *  - Deflection off anyone: switch at once to the team-mate with the best time to the ball.
 *  - Loose ball: auto-switch to the outfield team-mate nearest the ball (hysteresis + cooldown).
 *  - After any auto-switch the stick the human was holding is ignored (the new man chases the ball) until it
 *    changes or is released, at most T_SWITCH_LATCH_TICKS (FIFA's "auto switching move assistance").
 *  - SWITCH button: nearest team-mate to the ball other than the current one; locks auto-switch for a while. */
#include "sim/sim_internal.h"

static int seat_owns(const SimState *s, int player) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == player) return 1;
    return 0;
}

static void do_switch(SimState *s, int seat, int to, uint8_t cool, const WpSeatInput *si, int latch) {
    WpPlayer *old = &s->players[s->seat_player[seat]];
    old->kick_charge = 0;
    old->charge_kind = 0xFF;
    s->seat_player[seat] = (int8_t)to;
    s->switch_cool[seat] = cool;
    pass_pick_reset(s, seat);                         /* stick memory belongs to the old player */
    s->latch_ticks[seat] = 0;
    if (latch && si && (si->move_x || si->move_y)) {
        s->latch_stick[seat][0] = si->move_x;
        s->latch_stick[seat][1] = si->move_y;
        s->latch_ticks[seat] = T_SWITCH_LATCH_TICKS;
    }
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


/* Where can player p meet the ball? Cheap prediction every 50 ms for up to 4 s (gravity + air drag in flight,
 * rolling decel on the grass, bounces ignored). Candidates: points at head height or below that he can run to in
 * time (~6.5 m/s + 0.15 s to react). No preference -> the candidate CLOSEST to him (step across, let it come).
 * With a stick preference (unit vector) -> min cost t/2 + 0.3*(1 - cos(stick, direction to candidate)) (GRF's
 * 0.6*angle/pi weighting). Not found -> the last predicted point. */
Meet meet_choose(const SimState *s, const WpPlayer *p, fv2 pref, int has_pref) {
    const WpBall *b = &s->ball;
    fv2 pos = b->pos, v = b->vel;
    fx z = b->z, vz = b->vz, dt = FX(0.05), best_cost = 0;
    fx reach_v = fx_mul(player_top_speed(s, (int)(p - s->players), 1), FX(0.8125));   /* 6.5 m/s for an 8 m/s sprinter */
    Meet m = {b->pos, 0, 0};
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
        fv2 to = fv2_sub(pos, p->pos);
        fx d = fv2_len(to), t = dt * k;
        if (fx_div(d, reach_v) + FX(0.15) > t) continue;
        fx cost = d;
        if (has_pref) {
            fx c = d < FX(0.3) ? FX_ONE : fv2_dot(pref, fv2_scale(to, fx_div(FX_ONE, d)));
            cost = t / 2 + fx_mul(FX(0.3), FX_ONE - c);
        }
        if (!m.found || cost < best_cost) { m.pos = pos; m.t = t; m.found = 1; best_cost = cost; }
    }
    if (!m.found) m.pos = pos;
    return m;
}

void seats_pass_handoff(SimState *s, int kicker, int receiver, const WpSeatInput *in, fv2 meet, fx t_meet,
                        uint8_t plan) {
    int seat = -1;
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == kicker) seat = k;
    s->pass_target = (int8_t)receiver;
    s->pass_from = (int8_t)kicker;
    s->pass_ticks = 0;
    s->pass_meet = meet;
    s->pass_meet_tick = (uint16_t)(s->tick + (uint32_t)((t_meet * WP_TICK_HZ) / FX_ONE));
    s->pass_plan = plan;
    s->recv_away = s->recv_gone = 0;
    s->recv_goal = meet;
    s->recv_speed = 0;
    if (seat < 0 || seat_owns(s, receiver)) { s->pass_stick[0] = s->pass_stick[1] = -128; return; }
    s->pass_stick[0] = in->move_x;
    s->pass_stick[1] = in->move_y;
    /* a pass to the keeper: the stick the passer was holding (toward his own goal) must not carry over */
    do_switch(s, seat, receiver, T_SWITCH_COOL, in, s->players[receiver].role == WP_ROLE_KEEPER);
}

/* the assisted receiver's goal + speed for this tick, and the takeover rule */
static void recv_update(SimState *s, int me, const WpSeatInput *si) {
    const WpPlayer *p = &s->players[me];
    const WpBall *b = &s->ball;
    int dx = si->move_x - s->pass_stick[0], dy = si->move_y - s->pass_stick[1];
    int has = si->move_x || si->move_y;
    int opinion = has && dx * dx + dy * dy > T_RECV_STICK_TOL * T_RECV_STICK_TOL;
    fv2 pref = opinion ? fv2_norm(fv2_make((fx)si->move_x * (FX_ONE / 127), (fx)si->move_y * (FX_ONE / 127)))
                       : fv2_make(0, 0);
    int32_t left = (int32_t)(int16_t)(uint16_t)(s->pass_meet_tick - (uint16_t)s->tick);   /* ticks to the plan */
    fv2 goal; fx speed;
    if (!opinion && s->pass_plan != WP_PLAN_CLOSEST && left > 3) {
        goal = s->pass_meet;                          /* run THROUGH the plan, arriving just in time */
        speed = fx_div(fv2_len(fv2_sub(goal, p->pos)), FX_INT(left) / WP_TICK_HZ);
        speed = fx_clamp(speed, s->pass_plan == WP_PLAN_STRIDE ? FX(4.5) : 0, player_top_speed(s, (int)(p - s->players), 1));
        s->recv_gone = 0;
    } else {
        Meet m = meet_choose(s, p, pref, opinion);
        if (m.found) s->recv_gone = 0; else if (s->recv_gone < 255) s->recv_gone++;
        goal = m.pos;
        speed = fx_min(fx_mul(fv2_len(fv2_sub(goal, p->pos)), FX(4.0)), player_top_speed(s, (int)(p - s->players), 1));   /* arrive, don't overrun */
    }
    /* head-on in the ball's last 0.5 s: don't sprint into it (closing speed stays controllable) */
    fv2 to_ball = fv2_sub(b->pos, p->pos);
    fx bspd = fv2_len(b->vel), dball = fv2_len(to_ball);
    if (bspd > FX(1.0) && fv2_dot(b->vel, to_ball) < 0 && dball < fx_mul(bspd, FX(0.5)) &&
        fv2_dot(fv2_norm(fv2_sub(goal, p->pos)), fv2_norm(to_ball)) > FX(0.7))
        speed = fx_min(speed, T_RECV_HEADON_MS);
    s->recv_goal = goal;
    s->recv_speed = speed;
    /* takeover: stick held well away from the meeting point, or the ball has gone */
    fv2 dg = fv2_sub(goal, p->pos);
    /* away from the meet (> 120 deg), or - for a standing receive, where the meet is at his feet - any direction
     * that isn't toward the ball: the human wants to make a run (owner decision, CP0.2c) */
    int away = fv2_len(dg) > FX(0.5) ? fv2_dot(pref, fv2_norm(dg)) < -FX(0.5)
                                     : fv2_dot(pref, fv2_norm(to_ball)) < FX(0.3);
    if (opinion && away) {
        if (s->recv_away < 255) s->recv_away++;
    } else {
        s->recv_away = 0;
    }
    if (s->recv_away >= T_RECV_TAKEOVER_TICKS || s->recv_gone >= T_RECV_GONE_TICKS)
        s->pass_stick[0] = s->pass_stick[1] = -128;
}

/* a deflection mid-pass: give each seat of the passing team the team-mate with the best time to the ball */
static void deflect_switch(SimState *s, const WpInput *in, int team) {
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        int me = s->seat_player[k];
        if (me < 0 || s->players[me].team != team) continue;
        int best = -1; fx bt = 0;
        for (int j = team * s->n_per_side; j < (team + 1) * s->n_per_side; j++) {
            const WpPlayer *q = &s->players[j];
            if (q->role == WP_ROLE_KEEPER || (j != me && seat_owns(s, j))) continue;
            Meet m = meet_choose(s, q, fv2_make(0, 0), 0);
            fx t = m.found ? m.t : FX(9.0);
            if (best < 0 || t < bt) { best = j; bt = t; }
        }
        if (best >= 0 && best != me) do_switch(s, k, best, T_SWITCH_COOL, &in->seat[k], 1);
        else if (in->seat[k].move_x || in->seat[k].move_y) {   /* same man: still ignore the stale pass stick */
            s->latch_stick[k][0] = in->seat[k].move_x;
            s->latch_stick[k][1] = in->seat[k].move_y;
            s->latch_ticks[k] = T_SWITCH_LATCH_TICKS;
        }
    }
}

void seats_update(SimState *s, const WpInput *in) {
    const WpBall *b = &s->ball;
    /* pass in flight bookkeeping */
    if (s->pass_target >= 0) {
        s->pass_ticks++;
        int deflected = b->owner < 0 && b->last_touch >= 0 && b->last_touch != s->pass_from;
        int ends = s->phase != WP_PHASE_PLAY || b->owner >= 0 || deflected ||
                   s->pass_ticks > T_PASS_FLIGHT_MAX || (b->z == 0 && fv2_len(b->vel) < FX(1.0));
        if (ends) {
            int team = s->players[s->pass_from].team;
            s->pass_target = -1;
            if (deflected && s->phase == WP_PHASE_PLAY) deflect_switch(s, in, team);
        }
    }
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        int me = s->seat_player[k];
        if (me < 0) continue;
        const WpSeatInput *si = &in->seat[k];
        if (s->switch_cool[k]) s->switch_cool[k]--;
        if (s->latch_ticks[k]) {                      /* inherited stick: until it changes / is released */
            if (si->move_x != s->latch_stick[k][0] || si->move_y != s->latch_stick[k][1]) s->latch_ticks[k] = 0;
            else s->latch_ticks[k]--;
        }
        if (s->pass_target == me && s->pass_stick[0] != -128) recv_update(s, me, si);
        const WpPlayer *p = &s->players[me];
        if (p->role == WP_ROLE_KEEPER && b->owner != me && s->phase == WP_PHASE_PLAY && s->pass_target != me) {
            /* our keeper has played it / lost it: hand control back to an outfield man (the AI keeps goal) */
            int to = nearest_to_ball(s, p->team, me, 0);
            if (to >= 0) { do_switch(s, k, to, T_SWITCH_COOL, si, 1); continue; }
        }
        int busy = (p->action == WP_ACT_KICK || p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER) &&
                   !p->kick.done;                                     /* mid-kick or going for a header */
        if (s->phase != WP_PHASE_PLAY || b->owner == me || busy) continue;
        int pressed = (si->buttons & WP_BTN_SWITCH) && !(s->prev_buttons[k] & WP_BTN_SWITCH);
        if (pressed) {
            int to = nearest_to_ball(s, p->team, me, 0);
            if (to >= 0) {
                if (s->pass_target == me) s->pass_stick[0] = s->pass_stick[1] = -128;
                do_switch(s, k, to, T_SWITCH_LOCK_MANUAL, si, 0);
            }
            continue;
        }
        /* they have the ball (or their pass is travelling): switch to the team-mate best placed to defend - the one
         * nearest a spot just goal-side of the ball - when he's clearly better than the current man */
        int theirs = (b->owner >= 0 && s->players[b->owner].team != p->team) ||
                     (s->pass_target >= 0 && s->players[s->pass_from].team != p->team);
        if (theirs) {
            if (s->switch_cool[k]) continue;
            fx sg = FX_INT(team_dir(s, p->team));
            fv2 spot = fv2_add(b->pos, fv2_make(-fx_mul(sg, FX(1.5)), 0));
            int best = -1; fx bd = 0;
            for (int j = p->team * s->n_per_side; j < (p->team + 1) * s->n_per_side; j++) {
                if (s->players[j].role == WP_ROLE_KEEPER || (j != me && seat_owns(s, j))) continue;
                fx d = fv2_len(fv2_sub(spot, s->players[j].pos));
                if (best < 0 || d < bd) { best = j; bd = d; }
            }
            fx dme = fv2_len(fv2_sub(spot, p->pos));
            if (best >= 0 && best != me && dme - bd > T_SWITCH_DEF_HYST) do_switch(s, k, best, T_SWITCH_DEF_COOL, si, 1);
            continue;
        }
        if (s->switch_cool[k] || s->pass_target >= 0 || b->owner >= 0) continue;
        fx dn;
        int to = nearest_to_ball(s, p->team, me, &dn);
        fx dme = fv2_len(fv2_sub(b->pos, p->pos));
        if (to >= 0 && dme - dn > T_SWITCH_HYST) do_switch(s, k, to, T_SWITCH_COOL, si, 1);
    }
}

/* Player i (not a seat's) just won the ball: a seat on his team takes him over (owner decision CP0.3: control
 * jumps to the AI team-mate - or the keeper - who wins it), the held stick latched so he doesn't run off with it. */
void seats_on_gain(SimState *s, int i, const WpInput *in) {
    if (seat_owns(s, i)) return;
    int team = s->players[i].team, seat = -1;
    for (int k = 0; k < WP_MAX_SEATS; k++)
        if (s->seat_player[k] >= 0 && s->players[s->seat_player[k]].team == team) { seat = k; break; }
    if (seat < 0) return;
    if (s->pass_target == s->seat_player[seat]) s->pass_target = -1;
    do_switch(s, seat, i, T_SWITCH_COOL, &in->seat[seat], 1);
}

/* Legs-only input for a seat's player (aim and buttons always stay the human's): the assisted receiver, or a
 * freshly auto-switched player chasing the loose ball while the inherited stick is still held. */
void seats_assist_receive(const SimState *s, int seat, WpSeatInput *io) {
    int me = s->seat_player[seat];
    if (me < 0) return;
    const WpPlayer *p = &s->players[me];
    uint16_t buttons = io->buttons;
    if (s->pass_target == me && s->pass_stick[0] != -128) {
        team_steer_speed(s, p, s->recv_goal, s->recv_speed, io);
    } else if (s->latch_ticks[seat] && s->ball.owner < 0) {
        Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
        team_steer_speed(s, p, m.pos, fx_min(fx_mul(fv2_len(fv2_sub(m.pos, p->pos)), FX(4.0)), player_top_speed(s, me, 1)), io);
    } else {
        return;
    }
    io->buttons = (uint16_t)((buttons & ~WP_BTN_SPRINT) | (io->buttons & WP_BTN_SPRINT));
}
