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
            fx ga = fx_mul(s->pitch.length, T_GOALAREA_FRAC), py = s->pitch.goal_width / 2;   /* Law 16 (CP0.3e) */
            s->restart_spot = fv2_make(goal_side > 0 ? hl - ga : -hl + ga, x_or_y > 0 ? py : -py);
        }
    }
    s->restart_taker = -1;
    clock_add_stoppage(s, T_STOPPAGE_RESTART_S);
}

/* a foul or offside (D-040/D-041): the whistle, then a free kick to `team` at `spot` - a penalty if a direct foul
 * happened in the other team's penalty area */
void restart_foul(SimState *s, int team, fv2 spot, int indirect) {
    WpBall *b = &s->ball;
    int pen = !indirect && in_own_box(s, 1 - team, spot);
    s->restart_kind = pen ? WP_RS_PENALTY : WP_RS_FREEKICK;
    s->restart_team = (uint8_t)team;
    s->restart_indirect = (uint8_t)(indirect != 0);
    s->indirect_live = 0;
    s->restart_taker = -1;
    if (pen) spot = fv2_make(fx_mul(FX_INT(-team_dir(s, 1 - team)), s->pitch.length / 2 - fx_mul(s->pitch.length, T_PENALTY_FRAC)), 0);
    s->restart_spot = spot;
    b->owner = -1; b->held = 0; b->vel = fv2_make(0, 0); b->vz = 0; b->spin = 0;
    s->pass_target = -1;
    s->phase = WP_PHASE_OUT;
    s->phase_timer = T_FOUL_PAUSE;
    s->events |= indirect ? WP_EV_OFFSIDE : WP_EV_FOUL;
    clock_add_stoppage(s, T_STOPPAGE_FOUL_S);
}

/* end of the OUT pause: place the ball and the taker */
/* the ball's spot: just inside the pitch */
static fv2 ball_spot(const SimState *s) {
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fv2 spot = s->restart_spot;
    return fv2_make(fx_clamp(spot.x, -hl + FX(0.3), hl - FX(0.3)), fx_clamp(spot.y, -hw + FX(0.3), hw - FX(0.3)));
}

static int pick_taker(const SimState *s, fv2 at) {
    int team = s->restart_team, taker = -1; fx bd = 0;
    for (int j = team * s->n_per_side; j < (team + 1) * s->n_per_side; j++) {
        int gk = s->players[j].role == WP_ROLE_KEEPER;
        if ((s->restart_kind == WP_RS_GOALKICK) != gk) continue;
        fx d = fv2_len(fv2_sub(s->players[j].pos, at));
        if (s->restart_kind == WP_RS_PENALTY)            /* the best finisher takes it (D-040) */
            d = FX_INT(100 - (int)s->stats[j][WP_STAT_SHOOTING]) * 4 + d / 8;
        if (taker < 0 || d < bd) { taker = j; bd = d; }
    }
    return taker;
}

/* where the taker stands for it, and which way he faces */
static fv2 take_pos(const SimState *s, fv2 at, wp_angle *facing) {
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fv2 in_dir = fv2_norm(fv2_sub(fv2_make(0, 0), at));             /* facing into the pitch */
    if (s->restart_kind == WP_RS_THROW) in_dir = fv2_make(0, at.y > 0 ? -FX_ONE : FX_ONE);
    if (s->restart_kind == WP_RS_FREEKICK || s->restart_kind == WP_RS_PENALTY)   /* facing their goal */
        in_dir = fv2_norm(fv2_sub(fv2_make(fx_mul(FX_INT(team_dir(s, s->restart_team)), hl), 0), at));
    if (facing) *facing = fx_atan2(in_dir.y, in_dir.x);
    if (s->restart_kind == WP_RS_THROW) return fv2_make(at.x, at.y > 0 ? hw + FX(0.1) : -hw - FX(0.1));
    return fv2_sub(at, fv2_scale(in_dir, FX(0.45)));
}

/* end of the OUT pause (D-044): the ball goes on its spot, the taker is chosen and RUNS to it while everyone else
 * walks into place; restart_setup_step() starts it once they're there (or the time's up, or a human skips) */
void restart_setup(SimState *s, const WpInput *in) {
    WpBall *b = &s->ball;
    fv2 at = ball_spot(s);
    int taker = pick_taker(s, at);
    memset(b, 0, sizeof *b);
    b->pos = at;
    b->owner = -1;
    b->last_touch = -1;
    s->pass_target = -1;
    s->restart_taker = (int8_t)taker;
    s->phase = WP_PHASE_SETUP;
    s->phase_timer = T_SETUP_MAX;
    s->events |= WP_EV_RESET;                             /* the ball is placed: don't interpolate it */
    if (taker >= 0) seats_on_gain(s, taker, in);
}

static int is_seat(const SimState *s, int j) {
    for (int k = 0; k < WP_MAX_SEATS; k++) if (s->seat_player[k] == j) return 1;
    return 0;
}

/* is everyone where the law wants them? (the taker at his spot, nobody of theirs inside the zone) */
static int setup_ready(const SimState *s) {
    int t = s->restart_taker;
    if (t >= 0 && fv2_len(fv2_sub(s->players[t].pos, take_pos(s, ball_spot(s), 0))) > T_SETUP_ARRIVE) return 0;
    fv2 dummy;
    for (int j = 0; j < 2 * s->n_per_side; j++)
        if (j != t && !is_seat(s, j) && restart_legal_target(s, j, &dummy)) return 0;   /* AI still walking out */
    return 1;
}

void restart_setup_step(SimState *s, const WpInput *in) {
    int skip = 0;
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        uint16_t pressed = (uint16_t)(in->seat[k].buttons & ~s->prev_buttons[k]);
        if (s->seat_player[k] >= 0 && (pressed & (WP_BTN_PASS | WP_BTN_SHOOT | WP_BTN_LOB | WP_BTN_CHIP))) skip = 1;
    }
    if (s->phase_timer) s->phase_timer--;
    if (s->flags & WP_SIMF_STATIC_OTHERS) skip = 1;     /* statues never walk: isolation tests keep the instant restart */
    if (!skip && s->phase_timer && !setup_ready(s)) return;
    restart_place(s, in);
    s->phase = WP_PHASE_PLAY;
}

/* the taker's legs during the setup: run to his spot (sprint when far), then face the way he'll play it */
int restart_taker_walk(const SimState *s, int i, fv2 *dir, fx *speed) {
    if (s->phase != WP_PHASE_SETUP || i != s->restart_taker) return 0;
    fv2 to = fv2_sub(take_pos(s, ball_spot(s), 0), s->players[i].pos);
    fx d = fv2_len(to);
    if (d < FX(0.05)) { *speed = 0; *dir = fv2_make(0, 0); return 1; }
    *dir = fv2_scale(to, fx_div(FX_ONE, d));
    *speed = fx_min(fx_mul(d, FX(3.0)), player_top_speed(s, i, d > FX(6.0)));
    return 1;
}

/* where player j must go for the restart to be legal (1 + *out), or 0 if he's fine where he is */
int restart_legal_target(const SimState *s, int j, fv2 *out) { return restart_legal_point(s, j, s->players[j].pos, out); }

/* would player j standing at `pos` break the restart's distance rules? (1 + the nearest legal point in *out) */
int restart_legal_point(const SimState *s, int j, fv2 pos, fv2 *out) {
    int kind = s->restart_kind;
    if (!kind || kind == WP_RS_KICKOFF || j == s->restart_taker) return 0;
    WpPlayer qq = s->players[j];
    qq.pos = pos;
    const WpPlayer *q = &qq;
    fv2 at = ball_spot(s);
    fx hl = s->pitch.length / 2;
    if (kind == WP_RS_PENALTY) {
        int def = 1 - s->restart_team, sgd = team_dir(s, def);
        if (q->team == def && q->role == WP_ROLE_KEEPER) {   /* the keeper: on his line */
            fx lx = fx_mul(-hl + FX(0.3), FX_INT(sgd));
            if (fx_abs(q->pos.x - lx) < FX(0.3)) return 0;
            *out = fv2_make(lx, 0);
            return 1;
        }
        fx depth = fx_mul(s->pitch.length, FX(0.15)), edge = -hl + depth + FX(1.0);
        fx r = fx_mul(s->pitch.length, T_CORNER_DIST_FRAC) + FX(0.5);
        fv2 p = q->pos;
        int bad = 0;
        if (fx_mul(p.x, FX_INT(sgd)) < edge) { p.x = fx_mul(edge, FX_INT(sgd)); bad = 1; }
        fv2 d = fv2_sub(p, at);
        fx l = fv2_len(d);
        if (l < r && l > FX(0.05)) { p = fv2_add(at, fv2_scale(d, fx_div(r, l))); bad = 1; }
        if (bad) *out = p;
        return bad;
    }
    if (q->team == s->restart_team) return 0;              /* only the other team has to keep away */
    if (kind == WP_RS_GOALKICK) {
        if (!in_own_box(s, s->restart_team, q->pos)) return 0;
        int sg = team_dir(s, s->restart_team);
        fx depth = fx_mul(s->pitch.length, FX(0.15));
        *out = fv2_make(fx_mul(-hl + depth + FX(1.5), FX_INT(sg)), q->pos.y);
        return 1;
    }
    fx keep = kind == WP_RS_THROW ? T_THROWIN_DIST : fx_mul(s->pitch.length, T_CORNER_DIST_FRAC);
    fv2 d = fv2_sub(q->pos, at);
    fx l = fv2_len(d);
    if (l >= keep) return 0;
    if (l < FX(0.05)) d = fv2_make(0, FX_ONE), l = FX_ONE;
    *out = fv2_add(at, fv2_scale(d, fx_div(keep + FX(1.0), l)));
    return 1;
}

void restart_place(SimState *s, const WpInput *in) {
    WpBall *b = &s->ball;
    fv2 at = ball_spot(s);
    int team = s->restart_team;
    int taker = s->restart_taker >= 0 ? s->restart_taker : pick_taker(s, at);
    (void)team;
    memset(b, 0, sizeof *b);
    b->pos = at;
    b->owner = -1;
    b->last_touch = -1;
    b->no_hands = 0;
    s->pass_target = -1;
    if (taker >= 0) {
        WpPlayer *p = &s->players[taker];
        wp_angle face;
        p->pos = take_pos(s, at, &face);
        p->vel = fv2_make(0, 0);
        p->facing = face;
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
    if (!s->restart_kind || s->restart_taker < 0 || s->phase == WP_PHASE_SETUP) return;   /* (still walking up, D-044) */
    WpBall *b = &s->ball;
    int moved = s->restart_kind == WP_RS_KICKOFF && fv2_len(b->pos) > FX(1.0);   /* (tests/drills place the ball) */
    if (b->owner != s->restart_taker || s->phase != WP_PHASE_PLAY || ++s->restart_ticks > T_RESTART_MAX || moved) {
        if (s->restart_indirect && b->last_touch == s->restart_taker) s->indirect_live = (uint8_t)(s->restart_taker + 1);
        s->restart_indirect = 0;
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
    fx keep = ko ? kickoff_r(s) : s->restart_kind == WP_RS_THROW ? T_THROWIN_DIST
            : (s->restart_kind == WP_RS_CORNER || s->restart_kind == WP_RS_FREEKICK || s->restart_kind == WP_RS_PENALTY)
              ? fx_mul(s->pitch.length, T_CORNER_DIST_FRAC) : T_RESTART_DIST;
    if (s->restart_kind == WP_RS_PENALTY) {               /* Law 14: everyone but the taker and their keeper out of
                                                           * the box and the D; the keeper on his goal line */
        int def = 1 - s->restart_team, sgd = team_dir(s, def);
        fx hl = s->pitch.length / 2, depth = fx_mul(s->pitch.length, FX(0.15)), edge = -hl + depth + FX(0.5);
        for (int j = 0; j < 2 * s->n_per_side; j++) {
            WpPlayer *q = &s->players[j];
            if (j == s->restart_taker) continue;
            fx xd = fx_mul(q->pos.x, FX_INT(sgd));        /* in the defending team's frame (their goal at -hl) */
            if (q->team == def && q->role == WP_ROLE_KEEPER) {
                q->pos.x = fx_mul(-hl + FX(0.3), FX_INT(sgd));
                q->pos.y = fx_clamp(q->pos.y, -s->pitch.goal_width / 2, s->pitch.goal_width / 2);
                q->vel.x = 0;
                continue;
            }
            if (xd < edge) q->pos.x = fx_mul(edge, FX_INT(sgd));
            fv2 d = fv2_sub(q->pos, b->pos);
            fx l = fv2_len(d);
            if (l < keep && l > FX(0.05)) q->pos = fv2_add(b->pos, fv2_scale(d, fx_div(keep, l)));
        }
        return;
    }
    if (s->restart_kind == WP_RS_GOALKICK) {             /* Law 16: opponents outside the penalty area until it's played */
        fx hl = s->pitch.length / 2, depth = fx_mul(s->pitch.length, FX(0.15)), hw = fx_mul(s->pitch.width, FX(0.24));
        int sg = team_dir(s, s->restart_team);           /* the kicking team defends -sg */
        for (int j = other * s->n_per_side; j < (other + 1) * s->n_per_side; j++) {
            WpPlayer *q = &s->players[j];
            if (!in_own_box(s, s->restart_team, q->pos)) continue;
            fx ex = -hl + depth + FX(0.5);               /* box edge, in the kicking team's frame */
            fx toy = (q->pos.y > 0 ? hw : -hw) + (q->pos.y > 0 ? FX(0.5) : -FX(0.5));
            fx outx = fx_abs((-hl + depth) - q->pos.x * sg), outy = fx_abs(toy - q->pos.y);
            if (outx <= outy) q->pos.x = ex * sg;        /* nearest way out */
            else q->pos.y = toy;
        }
    }
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
