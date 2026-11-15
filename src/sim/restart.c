/* restart.c - throw-ins, corners and goal kicks (owner: "throw-ins and corners not enforced properly"; D-029).
 *  - Ball over a touchline: THROW-IN to the team that didn't touch it last, from where it crossed.
 *  - Ball over a goal line (no goal): CORNER if the defending team touched it last, else GOAL KICK.
 * During the OUT pause everyone repositions; then the ball is placed at the spot and the nearest player of the
 * restart team (the keeper for a goal kick) takes it: he stands still with it (a throw-in from the hands), the other
 * team is held T_RESTART_DIST away until it's played, and control jumps to him if he's a seat's team-mate. The
 * set piece ends when he plays it (or after T_RESTART_MAX). Kick-offs (D-032) are placed by match_place_kickoff and
 * held here: everyone in his own half, the other team outside the centre circle. */
#include <string.h>
#include "sim/sim_internal.h"

void restart_out(SimState *s, int touchline, fx x_or_y, int goal_side) {
    const WpBall *b = &s->ball;
    int last_team = b->last_touch >= 0 ? s->players[b->last_touch].team : 0;
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    if (touchline) {
        s->restart_kind = WP_RS_THROW;
        s->restart_team = (uint8_t)(1 - last_team);
        s->restart_spot = fv2_make(fx_clamp(x_or_y, -hl + FX(1.0), hl - FX(1.0)), b->pos.y > 0 ? hw : -hw);
    } else {
        int defending = team_dir(s, 0) == (goal_side > 0 ? 1 : -1) ? 1 : 0;   /* the team whose goal it is */
        if (last_team == defending) {
            s->restart_kind = WP_RS_CORNER;
            s->restart_team = (uint8_t)(1 - defending);
            s->restart_spot = fv2_make(goal_side > 0 ? hl : -hl, x_or_y > 0 ? hw : -hw);
        } else {
            s->restart_kind = WP_RS_GOALKICK;
            s->restart_team = (uint8_t)defending;
            s->restart_spot = fv2_make(goal_side > 0 ? hl - T_GOALKICK_DEPTH : -hl + T_GOALKICK_DEPTH, 0);
        }
    }
    s->restart_taker = -1;
    clock_add_stoppage(s, T_STOPPAGE_RESTART_S);
}

/* end of the OUT pause: place the ball and the taker */
void restart_place(SimState *s, const WpInput *in) {
    WpBall *b = &s->ball;
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fv2 spot = s->restart_spot;
    /* the ball sits just inside the pitch */
    fv2 at = fv2_make(fx_clamp(spot.x, -hl + FX(0.3), hl - FX(0.3)), fx_clamp(spot.y, -hw + FX(0.3), hw - FX(0.3)));
    int team = s->restart_team, taker = -1; fx bd = 0;
    for (int j = team * s->n_per_side; j < (team + 1) * s->n_per_side; j++) {
        int gk = s->players[j].role == WP_ROLE_KEEPER;
        if ((s->restart_kind == WP_RS_GOALKICK) != gk) continue;
        fx d = fv2_len(fv2_sub(s->players[j].pos, at));
        if (taker < 0 || d < bd) { taker = j; bd = d; }
    }
    memset(b, 0, sizeof *b);
    b->pos = at;
    b->owner = -1;
    b->last_touch = -1;
    b->no_hands = 0;
    s->pass_target = -1;
    if (taker >= 0) {
        WpPlayer *p = &s->players[taker];
        fv2 in_dir = fv2_norm(fv2_sub(fv2_make(0, 0), at));             /* facing into the pitch */
        if (s->restart_kind == WP_RS_THROW) in_dir = fv2_make(0, at.y > 0 ? -FX_ONE : FX_ONE);
        p->pos = fv2_sub(at, fv2_scale(in_dir, FX(0.45)));
        if (s->restart_kind == WP_RS_THROW) p->pos = fv2_make(at.x, at.y > 0 ? hw + FX(0.1) : -hw - FX(0.1));
        p->vel = fv2_make(0, 0);
        p->facing = fx_atan2(in_dir.y, in_dir.x);
        p->action = WP_ACT_NONE; p->action_timer = 0;
        p->touch_cooldown = 0;
        p->ai_hold = 0;
        b->owner = (int8_t)taker;
        b->last_touch = (int8_t)taker;
        if (s->restart_kind == WP_RS_THROW) ball_to_hands(s, taker);
    }
    s->restart_taker = (int8_t)taker;
    s->restart_ticks = 0;
    s->events |= WP_EV_RESET;
    if (taker >= 0) seats_on_gain(s, taker, in);
}

/* every tick of a live set piece: taker stays put; the other team keeps its distance */
void restart_step(SimState *s) {
    if (!s->restart_kind || s->restart_taker < 0) return;
    WpBall *b = &s->ball;
    int moved = s->restart_kind == WP_RS_KICKOFF && fv2_len(b->pos) > FX(1.0);   /* (tests/drills place the ball) */
    if (b->owner != s->restart_taker || s->phase != WP_PHASE_PLAY || ++s->restart_ticks > T_RESTART_MAX || moved) {
        s->restart_kind = 0;                              /* played (or abandoned) */
        s->restart_taker = -1;
        return;
    }
    int other = 1 - s->restart_team, ko = s->restart_kind == WP_RS_KICKOFF;
    if (ko)                                               /* kick-off: everyone in his own half until it's played */
        for (int j = 0; j < 2 * s->n_per_side; j++) {
            WpPlayer *q = &s->players[j];
            if (j == s->restart_taker) continue;
            if (team_dir(s, q->team) > 0 && q->pos.x > -FX(0.2)) q->pos.x = -FX(0.2);
            if (team_dir(s, q->team) < 0 && q->pos.x < FX(0.2)) q->pos.x = FX(0.2);
        }
    fx keep = ko ? kickoff_r(s) : T_RESTART_DIST;
    for (int j = other * s->n_per_side; j < (other + 1) * s->n_per_side; j++) {
        WpPlayer *q = &s->players[j];
        fv2 d = fv2_sub(q->pos, b->pos);
        fx l = fv2_len(d);
        if (l >= keep) continue;
        if (l < FX(0.05)) d = fv2_make(0, FX_ONE), l = FX_ONE;
        q->pos = fv2_add(b->pos, fv2_scale(d, fx_div(keep, l)));
    }
}

int restart_is_taker(const SimState *s, int i) { return s->restart_kind && s->restart_taker == i; }
