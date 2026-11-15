/* team_plan.c - the team layer of the instinct AI (D-030). Every tick, before anyone decides, each team gets:
 *  - a smoothed ball focus and a possession fade (the formation follows these: shape_spot());
 *  - ONE ball-winner (chase a loose ball / press the carrier), chosen by time to the ball, kept unless a challenger
 *    is clearly (15%) faster; frozen while an opponent's pass is still being "read" (difficulty reaction);
 *  - ONE cover (goal-side second defender; also presses during the counter-press window after losing it);
 *  - at most 2 markers (3 near our goal; 1 in 5v5), each tied to his own zone (9 m from his spot);
 *  - in possession: rest defenders (opponent counter-attackers + 1, 1..3), one near-support, one runner.
 * Ties break by formation slot (same order for both teams: mirror-fair), never by player index. It is a shared
 * observation for the brain (it gates which candidates exist), not a state machine. */
#include <string.h>
#include "sim/sim_internal.h"

static const uint8_t PASS_REACT[WP_AI_LEVELS] = T_AI_PASS_REACT_BY_LEVEL;

void team_plan_reset(SimState *s) {
    for (int t = 0; t < 2; t++) {
        WpTeamPlan *pl = &s->plan[t];
        memset(pl, 0, sizeof *pl);
        pl->focus = fv2_make(0, 0);
        pl->poss_fade = FX(0.5);
        pl->last_poss = -1;
        pl->winner = pl->cover = pl->near_sup = pl->runner = -1;
        for (int k = 0; k < WP_MAX_PER_SIDE; k++) pl->mark[k] = -1;
    }
}

static int seat_has(const SimState *s, int i) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == i) return 1;
    return 0;
}
static fx dist(fv2 a, fv2 b) { return fv2_len(fv2_sub(a, b)); }

/* time (s) for player j to get to the ball: loose / in flight -> meet prediction; at an opponent's feet -> to the
 * press point goal-side of it */
static fx time_to_ball(const SimState *s, int j, fv2 press_pt, int carried) {
    const WpPlayer *p = &s->players[j];
    if (carried) return fx_div(dist(p->pos, press_pt), T_SPRINT_SPEED);
    Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
    return m.found ? m.t : FX(5.0) + fx_div(dist(p->pos, s->ball.pos), T_SPRINT_SPEED);
}

/* pick with hysteresis: lowest t among eligible; the incumbent keeps it unless t_c + 0.1 < 0.85 (t_inc + 0.1) */
static int pick(const fx *t, const uint8_t *ok, int n, int base, int incumbent) {
    int best = -1;
    for (int k = 1; k < n; k++)
        if (ok[k] && (best < 0 || t[k] < t[best])) best = k;      /* ties: lower slot */
    if (best < 0) return -1;
    int inc = incumbent >= base && incumbent < base + n && ok[incumbent - base] ? incumbent - base : -1;
    if (inc >= 0 && inc != best && !(t[best] + FX(0.1) < fx_mul(FX(0.85), t[inc] + FX(0.1)))) best = inc;
    return base + best;
}

void team_plan_update(SimState *s) {
    const WpBall *b = &s->ball;
    int n = s->n_per_side;
    int poss = b->owner >= 0 ? s->players[b->owner].team
             : s->pass_target >= 0 ? s->players[s->pass_from].team : -1;
    int carried = b->owner >= 0 && !b->held;
    for (int t = 0; t < 2; t++) {
        WpTeamPlan *pl = &s->plan[t];
        int base = t * n;
        fx sg = t == 0 ? FX_ONE : -FX_ONE;
        fv2 own_goal = fv2_make(-fx_mul(sg, s->pitch.length / 2), 0);
        /* focus + possession fade */
        pl->focus = fv2_add(pl->focus, fv2_scale(fv2_sub(b->pos, pl->focus), T_PLAN_FOCUS_RATE));
        int us = poss < 0 ? pl->last_poss : poss == t;
        if (us >= 0 && us != pl->last_poss) { pl->trans_ticks = 0; pl->last_poss = (int8_t)us; }
        else if (pl->trans_ticks < 65535) pl->trans_ticks++;
        if (us == 1) pl->poss_fade = fx_min(pl->poss_fade + FX_ONE / 60, FX_ONE);
        else if (us == 0) pl->poss_fade = fx_max(pl->poss_fade - FX_ONE / 30, 0);
        for (int k = 0; k < n; k++) { pl->role[k] = WP_PR_ZONE; }
        if (s->phase != WP_PHASE_PLAY && s->phase != WP_PHASE_OUT) { pl->winner = pl->cover = -1; continue; }
        if (s->tick % T_PLAN_EVERY != (uint32_t)(t * 2 % T_PLAN_EVERY)) {   /* heavy part: every few ticks */
            if (pl->winner >= 0) pl->role[pl->winner - base] = WP_PR_WIN;
            if (pl->cover >= 0) pl->role[pl->cover - base] = WP_PR_COVER;
            for (int k = 0; k < n; k++) if (pl->mark[k] >= 0) pl->role[k] = WP_PR_MARK;
            if (pl->runner >= 0) pl->role[pl->runner - base] = WP_PR_RUN;
            if (pl->near_sup >= 0) pl->role[pl->near_sup - base] = WP_PR_SUPPORT;
            continue;
        }
        fv2 goal_dir = fv2_norm(fv2_sub(own_goal, b->pos));
        fv2 press_pt = fv2_add(b->pos, fv2_scale(goal_dir, FX(0.9)));
        fx tt[WP_MAX_PER_SIDE];
        uint8_t ok[WP_MAX_PER_SIDE];
        int reading = s->pass_target >= 0 && s->players[s->pass_from].team != t &&
                      s->pass_ticks < PASS_REACT[s->ai_level[t] < WP_AI_LEVELS ? s->ai_level[t] : 1];
        /* --- ball-winner --- */
        if (us == 1) {
            pl->winner = b->owner >= 0 && s->players[b->owner].team == t ? b->owner : -1;
        } else if (!reading) {
            int human_first = -1;
            for (int k = 0; k < n; k++) {
                int j = base + k;
                ok[k] = s->players[j].role != WP_ROLE_KEEPER;
                tt[k] = ok[k] ? time_to_ball(s, j, press_pt, carried) : FX(99.0);
            }
            /* a human counts only if he's fastest AND engaging (close, or moving at it); else the AI picks its own */
            int fastest = pick(tt, ok, n, base, -1);
            if (fastest >= 0 && seat_has(s, fastest)) {
                const WpPlayer *h = &s->players[fastest];
                fv2 to = fv2_sub(b->pos, h->pos);
                if (fv2_len(to) < FX(4.0) || fv2_dot(h->vel, to) > 0) human_first = fastest;
            }
            if (human_first >= 0) pl->winner = (int8_t)human_first;
            else {
                for (int k = 0; k < n; k++) if (seat_has(s, base + k)) ok[k] = 0;
                pl->winner = (int8_t)pick(tt, ok, n, base, pl->winner);
            }
        }
        /* --- cover (not in possession): to a point 5 m goal-side of the ball, 2 m toward the middle --- */
        if (us != 1) {
            fv2 cpt = fv2_add(b->pos, fv2_scale(goal_dir, FX(5.0)));
            cpt.y = cpt.y > 0 ? fx_max(cpt.y - FX(2.0), 0) : fx_min(cpt.y + FX(2.0), 0);
            for (int k = 0; k < n; k++) {
                int j = base + k;
                ok[k] = s->players[j].role != WP_ROLE_KEEPER && j != pl->winner && !seat_has(s, j);
                tt[k] = fx_div(dist(s->players[j].pos, cpt), T_SPRINT_SPEED);
            }
            pl->cover = (int8_t)pick(tt, ok, n, base, pl->cover);
        } else {
            pl->cover = -1;
        }
        /* --- markers (out of possession): capped, zonal --- */
        for (int k = 0; k < n; k++) pl->mark[k] = -1 == pl->mark[k] ? -1 : pl->mark[k];
        int cap = n <= 5 ? 1 : (dist(b->pos, own_goal) < FX(15.0) ? 3 : 2);
        int8_t newmark[WP_MAX_PER_SIDE];
        for (int k = 0; k < n; k++) newmark[k] = -1;
        if (us != 1) {
            int o0 = (1 - t) * n, done[WP_MAX_PER_SIDE] = {0}, marked = 0;
            for (int it = 0; it < cap; it++) {
                int best = -1; fx bthreat = 0;            /* most dangerous unmarked opponent (not the carrier) */
                for (int j = o0; j < o0 + n; j++) {
                    if (done[j - o0] || j == b->owner || s->players[j].role == WP_ROLE_KEEPER) continue;
                    fx th = -dist(s->players[j].pos, own_goal);
                    if (best < 0 || th > bthreat) { best = j; bthreat = th; }
                }
                if (best < 0) break;
                done[best - o0] = 1;
                int mk = -1; fx md = 0;
                for (int k = 1; k < n; k++) {
                    int j = base + k;
                    if (j == pl->winner || j == pl->cover || seat_has(s, j) || newmark[k] >= 0) continue;
                    fx d = dist(shape_spot(s, t, k), s->players[best].pos);
                    fx zone = fx_mul(T_PLAN_MARK_ZONE, fx_div(s->pitch.length, FX(55.0)));   /* scales with the pitch */
                    fx lim = pl->mark[k] == best ? zone + FX(2.0) : zone;   /* hysteresis */
                    if (d > lim) continue;
                    if (mk < 0 || d < md) { mk = k; md = d; }
                }
                if (mk >= 0) { newmark[mk] = (int8_t)best; marked++; }
            }
            (void)marked;
        }
        for (int k = 0; k < n; k++) pl->mark[k] = newmark[k];
        /* --- in possession: rest defence, near-support, runner --- */
        pl->near_sup = pl->near_sup >= 0 && us == 1 ? pl->near_sup : -1;
        if (us != 1) pl->runner = -1;
        if (us == 1) {
            int o0 = (1 - t) * n, counter = 0;
            fx bxa = fx_mul(b->pos.x, sg);
            for (int j = o0; j < o0 + n; j++)
                if (s->players[j].role != WP_ROLE_KEEPER && fx_mul(s->players[j].pos.x, sg) < bxa - FX(5.0)) counter++;
            int n_rest = n <= 5 ? 1 : (counter + 1 < 1 ? 1 : counter + 1 > 3 ? 3 : counter + 1);
            uint8_t rest[WP_MAX_PER_SIDE] = {0};
            for (int r = 0; r < n_rest; r++) {           /* ascending mindset, then deepest spot */
                int best = -1; fx bm = 0, bx = 0;
                for (int k = 1; k < n; k++) {
                    int j = base + k;
                    if (rest[k] || j == b->owner || seat_has(s, j)) continue;
                    fx m = slot_mindset(s, k), x = fx_mul(shape_spot(s, t, k).x, sg);
                    if (best < 0 || m < bm || (m == bm && x < bx)) { best = k; bm = m; bx = x; }
                }
                if (best < 0) break;
                rest[best] = 1;
                pl->role[best] = WP_PR_REST;
            }
            if (pl->sup_timer) pl->sup_timer = (uint16_t)(pl->sup_timer > T_PLAN_EVERY ? pl->sup_timer - T_PLAN_EVERY : 0);
            if (!pl->sup_timer || (pl->near_sup >= 0 && (rest[pl->near_sup - base] || pl->near_sup == b->owner))) {
                /* near-support: nearest non-rest man to a point 9 m from the carrier, forward-ish to the open side */
                fv2 sp = fv2_add(b->pos, fv2_make(fx_mul(sg, FX(5.0)), b->pos.y > 0 ? -FX(7.0) : FX(7.0)));
                int best = -1; fx bd = 0;
                for (int k = 1; k < n; k++) {
                    int j = base + k;
                    if (rest[k] || j == b->owner || seat_has(s, j)) continue;
                    fx d = dist(s->players[j].pos, sp);
                    if (best < 0 || d < bd) { best = k; bd = d; }
                }
                pl->near_sup = (int8_t)(best >= 0 ? base + best : -1);
                /* runner: a mindset-0.5 slot (wide mid) other than the near-support - the one on the far side */
                int rb = -1; fx rscore = 0;
                for (int k = 1; k < n; k++) {
                    int j = base + k;
                    if (rest[k] || j == b->owner || j == pl->near_sup || seat_has(s, j) || slot_mindset(s, k) != FX(0.5)) continue;
                    fx sc = -fx_abs(s->players[j].pos.y + b->pos.y);   /* furthest from the ball's side */
                    if (rb < 0 || sc > rscore) { rb = k; rscore = sc; }
                }
                pl->runner = (int8_t)(rb >= 0 ? base + rb : -1);
                pl->sup_timer = T_PLAN_SUPPORT_TICKS;
            }
        }
        if (pl->winner >= 0 && pl->winner >= base && pl->winner < base + n) pl->role[pl->winner - base] = WP_PR_WIN;
        if (pl->cover >= 0) pl->role[pl->cover - base] = WP_PR_COVER;
        for (int k = 0; k < n; k++) if (pl->mark[k] >= 0) pl->role[k] = WP_PR_MARK;
        if (pl->runner >= 0 && pl->role[pl->runner - base] == WP_PR_ZONE) pl->role[pl->runner - base] = WP_PR_RUN;
        if (pl->near_sup >= 0 && pl->role[pl->near_sup - base] == WP_PR_ZONE) pl->role[pl->near_sup - base] = WP_PR_SUPPORT;
    }
}
