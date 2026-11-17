/* switching.c - who the human controls (D-050, reports/Active player switching.md), for seats whose switch_mode isn't
 * LEGACY (seats.c keeps the pre-D-050 rules for those, bit for bit). Two separate decisions:
 *  - an ORACLE per seat (sw_next): the team-mate best placed to play the ball, by intercept ETA in ticks (position
 *    0.2 s ahead, turn and acceleration costs; their carrier -> a press point goal-side of him), held by GRF's
 *    hysteresis ((t_c + K) * 5 < (t_inc + K) * 4, K = 0.5 s);
 *  - a GATE on when control may follow it without being asked: forced hand-overs fire at once (our pass struck,
 *    gaining the ball, a deflection, the keeper handing back, restarts); unprompted ones need the dwell over, no
 *    other block (his ball / kick / charge / buffered press / our pass travelling), not the man just left (return
 *    lock), and a ratio: 1/2 (Events) inside a 9-tick window after an EVENT (the ball went loose by a tackle, a
 *    block, a clearance or a shot; their lofted pass; they took it off us; our pass ended loose), otherwise only a
 *    3x override (their ground passes and dribbles included); a stick held at the ball raises either to 4x.
 *    Manual: no unprompted.
 *  - The SWITCH button goes to exactly the man the indicator shows (sw_q): with a direction held, the team-mate that
 *    way seen from the ball; else the oracle's man (a press just after it changed goes to the previous one); presses
 *    within 40 ticks cycle through men not yet visited. Locks unprompted switching for 1.5 s.
 *  - After any switch, for ~0.3 s, the new man's heading keeps part (<= half the angle) of his own run while the
 *    stick is held.
 * All integer; the ball path is meet_choose's cached one (one prediction per ball state). */
#include "sim/sim_internal.h"

static const uint8_t EV_NUM[3] = T_SW_EV_NUM, EV_DEN[3] = T_SW_EV_DEN, OV_NUM[3] = T_SW_OV_NUM, OV_DEN[3] = T_SW_OV_DEN,
                     INTENT_X[3] = T_SW_INTENT_X, DWELL[3] = T_SW_DWELL, BACK[3] = T_SW_BACK, EVWIN[3] = T_SW_EVENT_WIN,
                     BLEND[3] = T_SW_BLEND;

static int mode_ix(const SimState *s, int seat) {
    int m = s->switch_mode[seat];
    return (m < WP_SWITCH_MANUAL || m >= WP_SWITCH_MODES ? WP_SWITCH_EVENTS : m) - 1;
}
static int owned_by_seat(const SimState *s, int j) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == j) return 1;
    return 0;
}
/* a man seat k may be switched to: outfield team-mate, not his own man, not another seat's */
static int eligible(const SimState *s, int k, int j) {
    const WpPlayer *q = &s->players[j];
    int me = s->seat_player[k];
    return j != me && q->team == s->players[me].team && q->role != WP_ROLE_KEEPER && !owned_by_seat(s, j);
}
static uint16_t to_ticks(fx sec) {
    int64_t t = ((int64_t)fx_max(sec, 0) * WP_TICK_HZ) >> FX_SHIFT;
    return (uint16_t)(t > 65000 ? 65000 : t);
}

/* where their carrier is pressed: led by his velocity, goal-side toward our goal's centre */
static fv2 press_point(const SimState *s, int team, fv2 *u_goal) {
    const WpBall *b = &s->ball;
    fv2 own_goal = fv2_make(-fx_mul(FX_INT(team_dir(s, team)), s->pitch.length / 2), 0);
    fv2 u = fv2_norm(fv2_sub(own_goal, b->pos));
    if (u_goal) *u_goal = u;
    fv2 at = fv2_add(b->pos, fv2_scale(s->players[b->owner].vel, T_SW_DEF_LEAD));
    return fv2_add(at, fv2_scale(u, T_SW_GS_DIST));
}

/* turn + acceleration cost (s) of setting off from p toward `at` */
static fx setoff_cost(const WpPlayer *p, fv2 at, fx top) {
    fv2 to = fv2_sub(at, p->pos);
    if (fv2_len2_wide(to) < (int64_t)FX(0.05) * FX(0.05)) return 0;
    fv2 u = fv2_norm(to);
    fx cf = fv2_dot(u, fv2_from_angle(p->facing, FX_ONE));
    fx turn = fx_mul(T_SW_TURN_S, (FX_ONE - cf) / 2);
    fx along = fx_clamp(fx_div(fv2_dot(p->vel, u), top), 0, FX_ONE);
    return turn + fx_mul(T_SW_ACC_PEN, FX_ONE - along);
}

/* Intercept ETA of player j in ticks: the first 50 ms step of the ball's path (at or below 1.9 m) he can reach -
 * react + turn + acceleration (toward that point) + run from where he'll be in 0.2 s at his sprint speed, compared in
 * squares. None in 4 s: his time to the path's end, at least 241 (continuous: no cliff). Their carrier: the press point
 * (+0.5 s if he is beaten, the wrong side of the ball). Our ball / one in the hands: the run to it. */
uint16_t switch_eta(const SimState *s, int j) {
    const WpPlayer *p = &s->players[j];
    const WpBall *b = &s->ball;
    fx top = player_top_speed(s, j, 1);
    fv2 p0 = fv2_add(p->pos, fv2_scale(p->vel, T_SW_LOOK));
    if (b->owner >= 0 || b->held) {
        if (b->owner >= 0 && !b->held && s->players[b->owner].team != p->team) {
            fv2 u;
            fv2 at = press_point(s, p->team, &u);
            fx t = T_CONTACT_REACT + setoff_cost(p, at, top) + fx_div(fv2_len(fv2_sub(at, p0)), top);
            if (fv2_dot(fv2_sub(p->pos, b->pos), u) < -T_SW_GS_TOL) t += T_SW_BEATEN_PEN;
            return to_ticks(t);
        }
        return to_ticks(T_CONTACT_REACT + fx_div(fv2_len(fv2_sub(b->pos, p0)), top));
    }
    const fv2 *at;
    const uint8_t *high;
    int n = meet_path(s, &at, &high);
    /* optimistic set-off (no turn, the best acceleration his speed allows) prunes without a root; a step that passes
     * it is checked with the real bearing to that point (so the cost doesn't swing as the ball passes overhead) */
    fx best_acc = fx_mul(T_SW_ACC_PEN, FX_ONE - fx_clamp(fx_div(fv2_len(p->vel), top), 0, FX_ONE));
    for (int k = 1; k <= n; k++) {
        if (high[k - 1]) continue;
        fx t = FX(0.05) * k;
        fx slack = t - T_CONTACT_REACT - best_acc;
        if (slack <= 0) continue;
        int64_t d2 = fv2_len2_wide(fv2_sub(at[k - 1], p0)), r = fx_mul(top, slack);
        if (d2 > r * r) continue;
        r = fx_mul(top, t - T_CONTACT_REACT - setoff_cost(p, at[k - 1], top));
        if (r > 0 && d2 <= r * r) return (uint16_t)(3 * k);
    }
    fx base = T_CONTACT_REACT + setoff_cost(p, at[n - 1], top);
    uint16_t late = to_ticks(base + fx_div(fv2_len(fv2_sub(at[n - 1], p0)), top));   /* to where it ends up */
    return (uint16_t)(late > 3 * n ? late : 3 * n + 1);
}

/* the eligible man with the smallest ETA (forced hand-overs), -1 none */
int switch_best(const SimState *s, int k) {
    int me = s->seat_player[k], best = -1;
    uint16_t bt = 0;
    int base = s->players[me].team * s->n_per_side;
    for (int j = base; j < base + s->n_per_side; j++) {
        if (!eligible(s, k, j)) continue;
        uint16_t t = switch_eta(s, j);
        if (best < 0 || t < bt) { best = j; bt = t; }
    }
    return best;
}

/* the man a press goes to: Q + a direction = the team-mate that way seen from the ball; repeated presses skip the
 * men this cycle visited; otherwise the oracle (or the one it showed a moment ago) */
static int press_target(const SimState *s, int k, const WpSeatInput *si, const uint16_t *eta) {
    int me = s->seat_player[k], base = s->players[me].team * s->n_per_side;
    int cycling = s->sw_cycle[k] < T_SW_CYCLE_WIN;
    uint16_t visited = cycling ? s->sw_visited[k] : 0;
    int mx = si->move_x, my = si->move_y;
    int dir = mx * mx + my * my >= T_CHASE_STICK_MIN * T_CHASE_STICK_MIN;
    for (int pass = 0; pass < 2; pass++) {
        int best = -1;
        if (dir) {
            fv2 u = fv2_norm(fv2_make(FX_INT(mx), FX_INT(my)));
            fx bs = 0;
            for (int j = base; j < base + s->n_per_side; j++) {
                if (!eligible(s, k, j) || (visited >> (j - base) & 1)) continue;
                fv2 v = fv2_sub(s->players[j].pos, s->ball.pos);
                fx d = fv2_len(v);
                fx c = d < FX(0.5) ? FX_ONE : fx_div(fv2_dot(u, v), d);
                if (c < T_SW_DIR_COS) continue;
                fx sc = fx_mul(d, 2 * FX_ONE - c);
                if (best < 0 || sc < bs) { best = j; bs = sc; }
            }
        }
        if (best < 0 && !cycling && s->sw_next[k] >= 0 && eligible(s, k, s->sw_next[k])) {
            best = s->sw_next[k];
            if (s->sw_next_age[k] < T_SW_RTC && s->sw_next_old[k] >= 0 && eligible(s, k, s->sw_next_old[k]))
                best = s->sw_next_old[k];
        }
        if (best < 0)                                     /* nobody that way / cycling: best ETA not yet visited */
            for (int j = base; j < base + s->n_per_side; j++)
                if (eligible(s, k, j) && !(visited >> (j - base) & 1) && (best < 0 || eta[j - base] < eta[best - base]))
                    best = j;
        if (best >= 0 || !cycling) return best;
        visited = (uint16_t)(1u << (me - base));          /* everyone visited: start the cycle again */
    }
    return -1;
}

/* per tick for a non-LEGACY seat, after the pass bookkeeping (seats.c). Returns after at most one switch. */
void switch_seat(SimState *s, int k, const WpSeatInput *si) {
    int me = s->seat_player[k], mi = mode_ix(s, k);
    const WpPlayer *p = &s->players[me];
    const WpBall *b = &s->ball;
    uint8_t back = s->sw_back[k];                     /* the lock as it stood: covers T_SW_BACK full ticks */
    if (back) s->sw_back[k]--;
    if (s->sw_event[k]) s->sw_event[k]--;
    if (s->sw_cycle[k] < 255) s->sw_cycle[k]++;
    if (s->sw_next_age[k] < 255) s->sw_next_age[k]++;
    if (p->role == WP_ROLE_KEEPER && b->owner != me && s->phase == WP_PHASE_PLAY && s->pass_target != me) {
        int to = switch_best(s, k);                       /* our keeper has played it: hand back (forced) */
        if (to >= 0) { seats_switch_to(s, k, to, DWELL[mi], si, 1, WP_SW_KEEPER); return; }
    }
    if (s->phase != WP_PHASE_PLAY) { s->sw_q[k] = -1; return; }
    /* the oracle: every T_PLAN_EVERY ticks (like the team plan) and on a press */
    int pressed = (si->buttons & WP_BTN_SWITCH) && !(s->prev_buttons[k] & WP_BTN_SWITCH);
    int shown = s->sw_q[k];                           /* what the indicator shows (<= 3 ticks old) */
    int fresh = s->tick % T_PLAN_EVERY == 0 || pressed;
    int base = p->team * s->n_per_side;
    uint16_t eta[WP_MAX_PER_SIDE];
    if (fresh) {
        int best = -1;
        for (int j = base; j < base + s->n_per_side; j++) {
            int ok = eligible(s, k, j);
            eta[j - base] = ok || j == me ? switch_eta(s, j) : 0xFFFF;
            if (ok && (best < 0 || eta[j - base] < eta[best - base])) best = j;
        }
        int inc = s->sw_next[k];
        if (inc >= 0 && (!eligible(s, k, inc))) inc = -1;
        if (inc >= 0 && best >= 0 && best != inc &&
            !(((int32_t)eta[best - base] + T_SW_K) * T_SW_NEXT_DEN < ((int32_t)eta[inc - base] + T_SW_K) * T_SW_NEXT_NUM))
            best = inc;
        if (best != s->sw_next[k]) { s->sw_next_old[k] = s->sw_next[k]; s->sw_next[k] = (int8_t)best; s->sw_next_age[k] = 0; }
        s->sw_q[k] = (int8_t)press_target(s, k, si, eta);
        s->sw_hot[k] = 0;
    }
    int acting = (p->action == WP_ACT_KICK || p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER) &&
                 !p->kick.done;
    if (b->owner == me || acting) return;
    /* the button */
    if (pressed) {                                    /* exactly the man shown, if he still can be */
        int to = shown >= 0 && eligible(s, k, shown) ? shown : s->sw_q[k];
        if (to < 0) return;
        int dir = si->move_x * si->move_x + si->move_y * si->move_y >= T_CHASE_STICK_MIN * T_CHASE_STICK_MIN;
        uint16_t vis = s->sw_cycle[k] < T_SW_CYCLE_WIN ? s->sw_visited[k] : (uint16_t)(1u << (me - base));
        if (s->pass_target == me) s->pass_stick[0] = s->pass_stick[1] = -128;
        seats_switch_to(s, k, to, T_SWITCH_LOCK_MANUAL, si, 0, dir ? WP_SW_MANUAL_DIR : WP_SW_MANUAL);
        s->sw_visited[k] = (uint16_t)(vis | (1u << (to - base)));
        s->sw_cycle[k] = 0;
        return;
    }
    if (!fresh) return;
    /* unprompted: never in MANUAL; gates first */
    if (s->switch_mode[k] == WP_SWITCH_MANUAL) return;
    if (b->held || (b->owner >= 0 && s->players[b->owner].team == p->team)) return;   /* ours / in the hands */
    if (s->pass_target == me || (s->pass_target >= 0 && s->players[s->pass_target].team == p->team)) return;
    if (s->pass_target >= 0 && !s->sw_event[k]) return;   /* their pass travelling: wait for where it ends up */
    if ((p->kick_charge && p->charge_kind != 0xFF) || s->intent[k] != 0xFF) return;   /* a charge / buffered press */
    int cand = s->sw_next[k];
    if (cand < 0 || (cand == s->sw_prev[k] && back)) return;
    int carrier = b->owner >= 0 && s->players[b->owner].team != p->team;   /* their man on the ball (D-029) */
    int defend = s->sw_event[k] || carrier;
    int32_t num = defend ? EV_NUM[mi] : OV_NUM[mi], den = defend ? EV_DEN[mi] : OV_DEN[mi];
    int mx = si->move_x, my = si->move_y;
    if (mx * mx + my * my >= T_CHASE_STICK_MIN * T_CHASE_STICK_MIN) {   /* stick intent: held at the ball */
        fv2 st = fv2_norm(fv2_make(FX_INT(mx), FX_INT(my)));
        const fv2 *at;
        const uint8_t *high;
        int n = meet_path(s, &at, &high);
        fv2 soon = b->owner >= 0 ? b->pos : at[n / 8];
        fv2 tb = fv2_sub(b->pos, p->pos), ts = fv2_sub(soon, p->pos);
        fx c = fv2_len2_wide(tb) > (int64_t)FX(0.1) * FX(0.1) ? fv2_dot(st, fv2_norm(tb)) : FX_ONE;
        if (fv2_len2_wide(ts) > (int64_t)FX(0.1) * FX(0.1)) c = fx_max(c, fv2_dot(st, fv2_norm(ts)));
        if (c >= T_SW_INTENT_COS) { num = 1; den = INTENT_X[mi]; }
    }
    int32_t a = (int32_t)eta[cand - base] + T_SW_K, m = (int32_t)eta[me - base] + T_SW_K;
    s->sw_hot[k] = (uint8_t)(s->sw_event[k] || a * den * 4 < m * num * 5);
    if (s->switch_cool[k]) return;                        /* the dwell */
    if (a * den < m * num) seats_switch_to(s, k, cand, DWELL[mi], si, 1, defend ? WP_SW_EVENT : WP_SW_OVERRIDE);
}

/* bookkeeping every switch of a non-LEGACY seat does (seats_switch_to) */
void switch_note(SimState *s, int k, int from, uint8_t reason) {
    int mi = mode_ix(s, k);
    s->sw_reason[k] = reason;
    s->sw_blend[k] = BLEND[mi];
    if (reason != WP_SW_MANUAL && reason != WP_SW_MANUAL_DIR) s->switch_cool[k] = DWELL[mi];   /* one dwell for all */
    if (reason == WP_SW_MANUAL || reason == WP_SW_MANUAL_DIR) { s->sw_prev[k] = -1; s->sw_back[k] = 0; }
    else { s->sw_prev[k] = (int8_t)from; s->sw_back[k] = BACK[mi]; }
    s->sw_hot[k] = 0;
}

/* the ball went loose (not by our targeted pass) / our pass ended without our man: the event window opens */
void switch_events(SimState *s, int ended_team) {
    const WpBall *b = &s->ball;
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        int me = s->seat_player[k];
        if (me < 0 || s->switch_mode[k] == WP_SWITCH_LEGACY) continue;
        int team = s->players[me].team;
        int flying = s->pass_target >= 0, ours_flying = flying && s->players[s->pass_from].team == team;
        int aerial = b->z > 0 || b->vz > 0;
        /* loose by anything but a targeted pass (a tackle, a block, a clearance, a shot) - or their lofted pass */
        int loose_now = s->sw_last_owner >= 0 && b->owner < 0 && !ours_flying && (!flying || aerial);
        int lost = b->owner >= 0 && s->sw_last_owner >= 0 && s->players[b->owner].team != team &&
                   s->players[s->sw_last_owner].team == team;   /* they took it off us */
        if (loose_now || lost || ended_team == team) s->sw_event[k] = EVWIN[mode_ix(s, k)];
    }
}

/* Legs only, after a switch: while the stick is held, the new man's heading keeps part of his own run for a moment
 * (GRF / EA's move assistance) - the stick's direction is turned toward his current motion by w = r / (2 T_SW_BLEND),
 * at most half the angle, fading linearly to nothing; the human's magnitude is kept. A stick meant for the old man
 * doesn't yank him round (a vector blend would cancel to a dead stop on a reversal). */
void switch_blend(const SimState *s, int seat, WpSeatInput *io) {
    int me = s->seat_player[seat], r = s->sw_blend[seat];
    if (me < 0 || !r || s->switch_mode[seat] == WP_SWITCH_LEGACY) return;
    int mx = io->move_x, my = io->move_y;
    if (!mx && !my) return;
    const WpPlayer *p = &s->players[me];
    if (fv2_len2_wide(p->vel) < (int64_t)FX(0.5) * FX(0.5)) return;
    fx w = fx_div(FX_INT(r), FX_INT(BLEND[mode_ix(s, seat)])) / 2;
    wp_angle a = fx_atan2(FX_INT(my), FX_INT(mx));
    int32_t dl = ang_diff(fx_atan2(p->vel.y, p->vel.x), a);
    a = (wp_angle)(a + (int32_t)(((int64_t)dl * w) >> 16));
    fv2 d = fv2_from_angle(a, fv2_len(fv2_make(FX_INT(mx), FX_INT(my))));
    io->move_x = (int8_t)fx_clamp(d.x / FX_ONE, -127, 127);
    io->move_y = (int8_t)fx_clamp(d.y / FX_ONE, -127, 127);
}
