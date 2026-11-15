/* brain.c - the INSTINCT layer of every player no human controls (CP0.3, D-008 / D-023).
 *
 *   observation -> candidate actions -> per-candidate FEATURES -> utility = W[action] . features (+ commitment)
 *   -> the best candidate becomes a WpSeatInput (same physics as a human) or an action (kick / tackle).
 *
 * The utility is a linear function of a small, fixed feature vector per candidate. That is deliberate: the learned
 * per-player brain (Phase 1) adds its own score on exactly these candidates and features, so "training" changes
 * which candidate wins, never the physics. All geometry is in attack-direction terms (`sg`), so both teams run the
 * same code. No dice: imperfection comes from the decision cadence (T_AI_DECIDE, staggered per player) and the
 * reaction time built into the meet-point prediction.
 *
 * Off the ball: SHAPE (formation spot shifted with the ball), SUPPORT (an open spot offering a pass), RUN (beyond
 * the last defender), CHASE (first to a loose ball), PRESS (goal-side of the carrier), COVER (second defender),
 * MARK (goal-side of an opponent), RECEIVE (a pass to me: run through the planned meet). TACKLE fires when pressing
 * and the carrier's ball is exposed in front of me.
 * On the ball: DRIBBLE (8 directions), PASS / THROUGH to each team-mate, SHOOT.
 * Keepers: KEEP (stay on the line between ball and goal centre, come out with the ball; meet a shot's line). */
#include "sim/sim_internal.h"

enum { AI_NONE = 0, AI_SHAPE, AI_SUPPORT, AI_RUN, AI_CHASE, AI_PRESS, AI_COVER, AI_MARK, AI_RECEIVE,
       AI_DRIBBLE, AI_PASS, AI_THROUGH, AI_SHOOT, AI_KEEP, AI_N };

/* candidate features (fx). F_BIAS is always 1. */
enum { F_FWD = 0,    /* metres gained toward the opponent goal (target vs ball) */
       F_OPEN,       /* nearest opponent to the target point, m (capped 8) */
       F_LANE,       /* clearance of the ball's lane to the target, m (capped 4) */
       F_COST,       /* metres I must travel / pass length */
       F_THREAT,     /* 0..1: shot quality, or danger of the opponent I'd mark */
       F_FIRST,      /* 0..1: I'm the team's first (1) / second (0.5) man to the ball */
       F_CROWD,      /* team-mates already heading within 4 m of the target */
       F_BIAS, NF };

/* instinct weights per action (x feature). Units: utility points per feature unit. */
static const fx W[AI_N][NF] = {
    /*            FWD        OPEN       LANE       COST        THREAT     FIRST      CROWD       BIAS */
    [AI_SHAPE]   = {0,        0,         0,         -FX(0.15),  0,         0,         -FX(1.0),   FX(1.0)},
    [AI_SUPPORT] = {FX(0.10), FX(0.35),  FX(0.45),  -FX(0.20),  0,         0,         -FX(1.5),   FX(0.6)},
    [AI_RUN]     = {FX(0.15), FX(0.25),  FX(0.30),  -FX(0.10),  FX(1.0),   0,         -FX(1.5),   FX(0.2)},
    [AI_CHASE]   = {0,        0,         0,         -FX(0.10),  0,         FX(4.0),   0,          FX(0.5)},
    [AI_PRESS]   = {0,        0,         0,         -FX(0.10),  0,         FX(4.0),   0,          FX(0.5)},
    [AI_COVER]   = {0,        0,         0,         -FX(0.10),  0,         FX(2.4),   -FX(1.0),   FX(0.5)},
    [AI_MARK]    = {0,        0,         0,         -FX(0.12),  FX(2.0),   0,         -FX(2.0),   FX(0.4)},
    [AI_RECEIVE] = {0,        0,         0,         0,          0,         0,         0,          FX(20.0)},
    [AI_DRIBBLE] = {FX(0.30), FX(0.40),  FX(0.30),  0,          0,         0,         0,          -FX(0.6)},
    [AI_PASS]    = {FX(0.16), FX(0.30),  FX(0.80),  -FX(0.06),  0,         0,         0,          -FX(0.4)},
    [AI_THROUGH] = {FX(0.20), FX(0.30),  FX(0.80),  -FX(0.06),  FX(1.0),   0,         0,          -FX(1.2)},
    [AI_SHOOT]   = {0,        0,         FX(0.30),  0,          FX(14.0),  0,         0,          -FX(1.0)},
    [AI_KEEP]    = {0,        0,         0,         0,          0,         0,         0,          FX(1.0)},
};

typedef struct { uint8_t act; int8_t mate; fv2 at; fx f[NF]; fx score; } Cand;

#define MAX_CAND 40

typedef struct {
    const SimState *s;
    int i, team, sg, n;
    fx hl, hw;
    fv2 own_goal, opp_goal;
    Cand c[MAX_CAND];
} Ctx;

static fx clampf(fx v, fx lo, fx hi) { return fx_clamp(v, lo, hi); }
static fx dist(fv2 a, fv2 b) { return fv2_len(fv2_sub(a, b)); }

static fx nearest_opp(const Ctx *x, fv2 at) {
    fx best = FX(8.0);
    int o0 = (1 - x->team) * x->s->n_per_side;
    for (int j = o0; j < o0 + x->s->n_per_side; j++) {
        fx d = dist(x->s->players[j].pos, at);
        if (d < best) best = d;
    }
    return best;
}

/* team-mates (other than me) whose current AI goal is within 4 m of `at` */
static fx crowd(const Ctx *x, fv2 at) {
    fx n = 0;
    for (int j = x->team * x->s->n_per_side; j < (x->team + 1) * x->s->n_per_side; j++) {
        if (j == x->i) continue;
        const WpPlayer *q = &x->s->players[j];
        fv2 g = q->ai_act ? q->ai_goal : q->pos;
        if (dist(g, at) < FX(4.0)) n += FX_ONE;
    }
    return n;
}

static fv2 in_pitch(const Ctx *x, fv2 p, fx m) {
    return fv2_make(clampf(p.x, -(x->hl - m), x->hl - m), clampf(p.y, -(x->hw - m), x->hw - m));
}

static Cand *add(Ctx *x, uint8_t act, int8_t mate, fv2 at) {
    if (x->n >= MAX_CAND) return 0;
    Cand *c = &x->c[x->n++];
    c->act = act; c->mate = mate; c->at = at;
    for (int k = 0; k < NF; k++) c->f[k] = 0;
    c->f[F_BIAS] = FX_ONE;
    return c;
}

/* the formation spot (team 0 orientation) shifted with the ball; in possession pushed up */
static fv2 home_spot(const Ctx *x, int attacking) {
    const SimState *s = x->s;
    int k = x->i % s->n_per_side;
    fx fxl, fyl;
    formation_frac(s->n_per_side, k, &fxl, &fyl);
    fx bxa = s->ball.pos.x * x->sg;                       /* ball x in attack terms */
    fx hx = fx_mul(fxl, x->hl) + fx_mul(bxa, FX(0.55)) + (attacking ? FX(4.0) : -FX(1.0));
    fx hy = fx_mul(fyl, x->hw) + fx_mul(s->ball.pos.y * x->sg, FX(0.3));
    return in_pitch(x, fv2_make(hx * x->sg, hy * x->sg), FX(1.5));
}

/* 0..1 shot quality from point `at`: distance, angle to the goal mouth, a clear lane, keeper not in the way */
static fx shot_quality(const Ctx *x, fv2 at) {
    fx d = dist(at, x->opp_goal);
    if (d > FX(18.0)) return 0;
    fx q = FX_ONE - fx_div(d, FX(18.0));
    fx ay = fx_abs(at.y), wide = fx_max(ay - x->s->pitch.goal_width, 0);
    q = fx_mul(q, FX_ONE - clampf(fx_div(wide, FX(6.0)), 0, FX_ONE));
    fx lane = fx_min(pass_lane_clear(x->s, x->team, at, x->opp_goal), FX(2.0));
    return fx_mul(q, FX(0.4) + fx_mul(FX(0.3), lane));
}

/* how many team-mates (incl. the human) reach the ball before me: 0 = I'm first */
static int ball_rank(const Ctx *x) {
    const SimState *s = x->s;
    fv2 b = fv2_add(s->ball.pos, fv2_scale(s->ball.vel, FX(0.4)));
    fx mine = dist(s->players[x->i].pos, b);
    int r = 0;
    for (int j = x->team * s->n_per_side; j < (x->team + 1) * s->n_per_side; j++) {
        if (j == x->i || s->players[j].role == WP_ROLE_KEEPER) continue;
        fx d = dist(s->players[j].pos, b);
        if (d < mine || (d == mine && j < x->i)) r++;
    }
    return r;
}

static int possession(const SimState *s) {          /* team in possession, -1 loose/none */
    const WpBall *b = &s->ball;
    if (b->owner >= 0) return s->players[b->owner].team;
    if (s->pass_target >= 0) return s->players[s->pass_from].team;
    return -1;
}

/* ---------------- candidates: off the ball ---------------- */
static void gen_off_ball(Ctx *x) {
    const SimState *s = x->s;
    const WpPlayer *p = &s->players[x->i];
    const WpBall *b = &s->ball;
    int poss = possession(s), attacking = poss == x->team, rank = ball_rank(x);
    Cand *c;
    if (s->pass_target == x->i) {                         /* a pass to me: run through the planned meet */
        Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
        fv2 at = s->pass_plan != WP_PLAN_CLOSEST ? s->pass_meet : m.pos;
        add(x, AI_RECEIVE, -1, at);
        return;
    }
    if ((c = add(x, AI_SHAPE, -1, home_spot(x, attacking)))) {
        c->f[F_COST] = dist(c->at, p->pos);
        c->f[F_CROWD] = crowd(x, c->at);
    }
    if (!attacking) {
        fv2 goal_dir = fv2_norm(fv2_sub(x->own_goal, b->pos));
        if (b->owner < 0) {                               /* loose: go and get it */
            Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
            if ((c = add(x, AI_CHASE, -1, m.pos))) {
                c->f[F_COST] = dist(m.pos, p->pos);
                c->f[F_FIRST] = rank == 0 ? FX_ONE : 0;
            }
        } else {                                          /* they have it: press, cover, mark */
            if ((c = add(x, AI_PRESS, -1, fv2_add(b->pos, fv2_scale(goal_dir, FX(0.9)))))) {
                c->f[F_COST] = dist(c->at, p->pos);
                c->f[F_FIRST] = rank == 0 ? FX_ONE : 0;
            }
            if ((c = add(x, AI_COVER, -1, in_pitch(x, fv2_add(b->pos, fv2_scale(goal_dir, FX(5.0))), FX(1.0))))) {
                c->f[F_COST] = dist(c->at, p->pos);
                c->f[F_FIRST] = rank == 1 ? FX_ONE : 0;
                c->f[F_CROWD] = crowd(x, c->at);
            }
        }
        int o0 = (1 - x->team) * s->n_per_side;
        for (int j = o0; j < o0 + s->n_per_side; j++) {   /* mark the dangerous ones */
            const WpPlayer *o = &s->players[j];
            if (o->role == WP_ROLE_KEEPER || j == b->owner) continue;
            fv2 at = fv2_add(o->pos, fv2_scale(fv2_norm(fv2_sub(x->own_goal, o->pos)), FX(1.5)));
            if (!(c = add(x, AI_MARK, (int8_t)j, at))) break;
            c->f[F_COST] = dist(at, p->pos);
            c->f[F_THREAT] = FX_ONE - clampf(fx_div(dist(o->pos, x->own_goal), FX(30.0)), 0, FX_ONE);
            fx others = 0;                                /* someone already marking him? */
            for (int k = x->team * s->n_per_side; k < (x->team + 1) * s->n_per_side; k++)
                if (k != x->i && s->players[k].ai_act == AI_MARK && s->players[k].ai_mate == j) others += FX(2.0);
            c->f[F_CROWD] = others;
        }
        return;
    }
    /* attacking: offer an open option; forwards also run beyond the last defender */
    static const int16_t ang[7] = {0, 30, -30, 60, -60, 110, -110};
    static const fx rad[2] = {FX(7.0), FX(12.0)};
    fv2 fwd = fv2_make(FX_INT(x->sg), 0);
    for (int a = 0; a < 7; a++)
        for (int r = 0; r < 2; r++) {
            fv2 at = in_pitch(x, fv2_add(b->pos, fv2_scale(fv2_from_angle((wp_angle)(fx_atan2(fwd.y, fwd.x) +
                              DEG_TO_ANG(ang[a])), FX_ONE), rad[r])), FX(1.5));
            if (!(c = add(x, AI_SUPPORT, -1, at))) break;
            c->f[F_FWD] = (at.x - b->pos.x) * x->sg;
            c->f[F_OPEN] = nearest_opp(x, at);
            c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
            c->f[F_COST] = fx_max(dist(at, p->pos), dist(at, home_spot(x, 1)) / 2);
            c->f[F_CROWD] = crowd(x, at);
        }
    fx fxk, fyk;
    formation_frac(s->n_per_side, x->i % s->n_per_side, &fxk, &fyk);
    (void)fyk;
    if (fxk > -FX(0.40)) {                                /* midfielders / striker: run in behind */
        fx last = -x->hl;                                 /* deepest outfield opponent, attack terms */
        int o0 = (1 - x->team) * s->n_per_side;
        for (int j = o0; j < o0 + s->n_per_side; j++)
            if (s->players[j].role != WP_ROLE_KEEPER && s->players[j].pos.x * x->sg > last) last = s->players[j].pos.x * x->sg;
        fx rx = fx_min(fx_max(last + FX(3.0), b->pos.x * x->sg + FX(6.0)), x->hl - FX(4.0));
        fv2 at = in_pitch(x, fv2_make(rx * x->sg, p->pos.y), FX(2.0));
        if ((c = add(x, AI_RUN, -1, at))) {
            c->f[F_FWD] = (at.x - b->pos.x) * x->sg;
            c->f[F_OPEN] = nearest_opp(x, at);
            c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
            c->f[F_COST] = dist(at, p->pos);
            c->f[F_THREAT] = shot_quality(x, at);
            c->f[F_CROWD] = crowd(x, at);
        }
    }
}

/* ---------------- candidates: on the ball ---------------- */
static void gen_on_ball(Ctx *x) {
    const SimState *s = x->s;
    const WpBall *b = &s->ball;
    Cand *c;
    fx press = nearest_opp(x, b->pos);
    for (int d = 0; d < 8; d++) {
        fv2 dir = fv2_from_angle((wp_angle)(d * 8192), FX_ONE);
        fv2 at = fv2_add(b->pos, fv2_scale(dir, FX(4.0)));
        if (fx_abs(at.x) > x->hl - FX(1.0) || fx_abs(at.y) > x->hw - FX(1.0)) continue;
        if (!(c = add(x, AI_DRIBBLE, -1, at))) break;
        c->f[F_FWD] = fx_mul(dir.x, FX_INT(x->sg)) * 4;
        c->f[F_OPEN] = nearest_opp(x, at);
        c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
    }
    for (int j = x->team * s->n_per_side; j < (x->team + 1) * s->n_per_side; j++) {
        if (j == x->i) continue;
        const WpPlayer *q = &s->players[j];
        fx d0 = dist(q->pos, b->pos);
        if (d0 < FX(3.0) || d0 > T_PASS_MAX_DIST) continue;
        fv2 at = fv2_add(q->pos, fv2_scale(q->vel, fx_div(d0, T_PICK_FLIGHT_V)));
        if ((c = add(x, AI_PASS, (int8_t)j, at))) {
            c->f[F_FWD] = (at.x - b->pos.x) * x->sg;
            c->f[F_OPEN] = nearest_opp(x, at);
            c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
            c->f[F_COST] = d0;
            if (q->role == WP_ROLE_KEEPER) c->f[F_BIAS] = FX(3.0);      /* back to the keeper: last resort */
        }
        if (q->vel.x * x->sg > FX(3.0) && q->role != WP_ROLE_KEEPER) {  /* a runner: in behind */
            fv2 ahead = in_pitch(x, fv2_add(q->pos, fv2_scale(q->vel, FX(1.0))), FX(3.0));
            if ((c = add(x, AI_THROUGH, (int8_t)j, ahead))) {
                c->f[F_FWD] = (ahead.x - b->pos.x) * x->sg;
                c->f[F_OPEN] = nearest_opp(x, ahead);
                c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, ahead), FX(4.0));
                c->f[F_COST] = dist(ahead, b->pos);
                c->f[F_THREAT] = shot_quality(x, ahead);
            }
        }
    }
    fx q = shot_quality(x, b->pos);
    if (q > 0 && (c = add(x, AI_SHOOT, -1, x->opp_goal))) {
        c->f[F_THREAT] = q;
        c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, x->opp_goal), FX(4.0));
    }
    (void)press;
}

static fx score(const Cand *c, const WpPlayer *p) {
    fx u = 0;
    for (int k = 0; k < NF; k++) u += fx_mul(W[c->act][k], c->f[k]);
    if (c->act == p->ai_act && dist(c->at, p->ai_goal) < FX(2.0) && c->mate == p->ai_mate) u += T_AI_COMMIT;
    return u;
}

/* ---------------- carrying out the decision ---------------- */
static void do_kick(SimState *s, int i, uint8_t kind, int to, fv2 at, fx power) {
    WpPlayer *p = &s->players[i];
    fv2 d = fv2_sub(at, s->ball.pos);
    WpKick k = {0};
    k.kind = kind;
    k.dir = fx_atan2(d.y, d.x);
    k.to = (uint8_t)(to + 1);
    k.foot = 1;
    if (kind == WP_KICK_SHOT) {
        k.power = power;
        k.vz = fx_lerp(T_SHOT_VZ_MIN, T_SHOT_VZ_MAX, FX(0.25));
        k.windup = T_KICK_WINDUP_SHOT;
        k.to = 0;
    } else {
        k.power = kind == WP_KICK_THROUGH ? FX(0.4) : T_PASS_SPEED;
        k.windup = T_KICK_WINDUP_PASS;
    }
    sim_player_kick(s, i, &k);
    (void)p;
}

/* aim a shot at the corner further from their keeper */
static fv2 shot_target(const Ctx *x) {
    const SimState *s = x->s;
    int kp = (1 - x->team) * s->n_per_side;              /* their keeper is index 0 of their team */
    fx side = fx_mul(s->pitch.goal_width, FX(0.38));
    fx ky = s->players[kp].pos.y;
    return fv2_make(x->opp_goal.x, ky > 0 ? -side : side);
}

static void keeper(Ctx *x, WpSeatInput *out) {
    SimState *s = (SimState *)x->s;
    WpPlayer *p = &s->players[x->i];
    const WpBall *b = &s->ball;
    fv2 g = x->own_goal;
    fv2 to_ball = fv2_sub(b->pos, g);
    fx db = fv2_len(to_ball);
    fx out_d = clampf(fx_mul(db, FX(0.12)), FX(0.6), FX(3.0));
    fv2 at = fv2_add(g, fv2_scale(fv2_norm(to_ball), out_d));
    fx gw = s->pitch.goal_width / 2;
    at.y = clampf(at.y, -gw, gw);
    /* a shot coming: after reacting (T_KEEPER_REACT), shuffle across to where it will cross my line */
    fx vin = -b->vel.x * x->sg;                           /* speed toward my goal */
    fx cap = T_SPRINT_SPEED;
    int shot = 0;
    if (b->owner < 0 && vin > FX(4.0)) {
        fx t = fx_div(fx_abs(b->pos.x - p->pos.x), vin);
        fx cy = b->pos.y + fx_mul(b->vel.y, t);
        if (t < FX(1.5) && fx_abs(cy) < gw + FX(1.0)) {
            shot = 1;
            if (p->ai_wait < T_KEEPER_REACT) { p->ai_wait++; at = p->ai_goal; }        /* still reading it */
            else at = fv2_make(p->pos.x, clampf(cy, -gw - FX(0.5), gw + FX(0.5)));
            cap = T_KEEPER_SHUFFLE;
        }
    }
    if (!shot) p->ai_wait = 0;
    /* a loose, slow ball in front of me that I get to first: come and take it (never to cut out a shot) */
    if (!shot && b->owner < 0 && db < FX(7.0) && fv2_len(b->vel) < T_KEEPER_RUSH_V) {
        Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
        fx mine = m.found ? m.t : FX(9.0), theirs = FX(9.0);
        int o0 = (1 - x->team) * s->n_per_side;
        for (int j = o0; j < o0 + s->n_per_side; j++) {
            Meet mo = meet_choose(s, &s->players[j], fv2_make(0, 0), 0);
            if (mo.found && mo.t < theirs) theirs = mo.t;
        }
        if (m.found && mine < theirs) at = m.pos;
    }
    p->ai_act = AI_KEEP;
    p->ai_goal = at;
    fx d = dist(at, p->pos);
    team_steer_speed(p, at, fx_min(fx_mul(d, FX(5.0)), cap), out);
}

void brain_input(SimState *s, int i, WpSeatInput *out) {
    WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    out->move_x = out->move_y = 0;
    out->buttons = 0;
    if (s->phase != WP_PHASE_PLAY) { p->ai_act = AI_NONE; p->ai_hold = 0; return; }
    Ctx x;
    x.s = s; x.i = i; x.team = p->team; x.sg = p->team == 0 ? 1 : -1; x.n = 0;
    x.hl = s->pitch.length / 2; x.hw = s->pitch.width / 2;
    x.own_goal = fv2_make(-x.hl * x.sg, 0);
    x.opp_goal = fv2_make(x.hl * x.sg, 0);
    int owner = b->owner == i;
    p->ai_hold = owner ? (uint8_t)(p->ai_hold < 255 ? p->ai_hold + 1 : 255) : 0;

    if (p->role == WP_ROLE_KEEPER && !owner) { keeper(&x, out); return; }
    /* kickoff: the team not kicking off holds its shape until the ball is first touched */
    if (b->last_touch < 0 && b->owner < 0 && b->pos.x == 0 && b->pos.y == 0 && p->team != s->kickoff_team) {
        fv2 at = home_spot(&x, 0);
        p->ai_act = AI_SHAPE;
        p->ai_goal = at;
        team_steer_speed(p, at, fx_min(fx_mul(dist(at, p->pos), FX(2.0)), T_WALK_SPEED), out);
        return;
    }

    int decide = ((s->tick + (uint32_t)(i % s->n_per_side)) % T_AI_DECIDE) == 0   /* staggered by role: mirror-fair */ || p->ai_act == AI_NONE ||
                 (owner && p->ai_act < AI_DRIBBLE) || (!owner && p->ai_act >= AI_DRIBBLE && p->ai_act != AI_KEEP);
    if (s->pass_target == i && p->ai_act != AI_RECEIVE) decide = 1;
    /* a pass by the other team has just been struck: defenders need a moment to react (keep doing what they were) */
    if (s->pass_target >= 0 && s->players[s->pass_from].team != p->team && s->pass_ticks < T_AI_PASS_REACT) decide = 0;
    if (decide) {
        if (owner) gen_on_ball(&x); else gen_off_ball(&x);
        int best = -1; fx bs = 0;
        for (int k = 0; k < x.n; k++) {
            x.c[k].score = score(&x.c[k], p);
            if (best < 0 || x.c[k].score > bs) { best = k; bs = x.c[k].score; }   /* tie -> first generated */
        }
        if (best >= 0) {
            p->ai_act = x.c[best].act;
            p->ai_mate = x.c[best].mate;
            p->ai_goal = x.c[best].at;
        }
    }
    /* carry it out */
    switch (p->ai_act) {
    case AI_PASS: case AI_THROUGH: case AI_SHOOT:
        if (!owner) { p->ai_act = AI_NONE; break; }
        if (p->ai_hold < T_AI_FIRST_TOUCH || p->action != WP_ACT_NONE) break;    /* control it first */
        if (p->ai_act == AI_SHOOT) do_kick(s, i, WP_KICK_SHOT, -1, shot_target(&x), T_AI_SHOT_POWER);
        else do_kick(s, i, p->ai_act == AI_THROUGH ? WP_KICK_THROUGH : WP_KICK_PASS, p->ai_mate,
                     s->players[p->ai_mate].pos, 0);
        p->ai_act = AI_NONE;
        return;
    case AI_DRIBBLE: {
        fv2 d = fv2_norm(fv2_sub(p->ai_goal, b->pos));
        out->move_x = (int8_t)fx_clamp(d.x * 127 / FX_ONE, -127, 127);
        out->move_y = (int8_t)fx_clamp(d.y * 127 / FX_ONE, -127, 127);
        if (nearest_opp(&x, b->pos) > FX(4.0)) out->buttons = WP_BTN_SPRINT;
        return;
    }
    case AI_CHASE: {                                      /* the ball moves: keep re-aiming (after reacting) */
        if (s->pass_target >= 0 && s->players[s->pass_from].team != p->team && s->pass_ticks < T_AI_PASS_REACT) break;
        Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
        p->ai_goal = m.pos;
        team_steer_speed(p, m.pos, fx_min(fx_mul(dist(m.pos, p->pos), FX(4.0)) + FX(2.0), T_SPRINT_SPEED), out);
        return;
    }
    case AI_PRESS: {
        if (b->owner < 0 || s->players[b->owner].team == p->team) break;
        fv2 at = fv2_add(b->pos, fv2_scale(fv2_norm(fv2_sub(x.own_goal, b->pos)), FX(0.9)));
        p->ai_goal = at;
        /* tackle when the carrier's ball is exposed (away from his feet) right in front of me */
        fx db = dist(b->pos, p->pos), exposed = dist(b->pos, s->players[b->owner].pos);
        fv2 face = fv2_from_angle(p->facing, FX_ONE);
        fx need = fv2_len(s->players[b->owner].vel) < FX(2.0) ? T_AI_TACKLE_SHIELD : T_AI_TACKLE_EXPOSED;
        if (decide && p->action == WP_ACT_NONE && db < T_AI_TACKLE_DIST && exposed > need &&   /* at his decision beat */
            fv2_dot(face, fv2_sub(b->pos, p->pos)) > 0) {
            tackle_start(s, i);
            return;
        }
        team_steer_speed(p, at, fx_min(fx_mul(dist(at, p->pos), FX(4.0)) + FX(1.0), T_SPRINT_SPEED), out);
        return;
    }
    case AI_RECEIVE: {
        fx d = dist(p->ai_goal, p->pos);
        team_steer_speed(p, p->ai_goal, fx_min(fx_mul(d, FX(4.0)), T_SPRINT_SPEED), out);
        return;
    }
    default: break;
    }
    if (p->ai_act == AI_NONE) return;
    fx d = dist(p->ai_goal, p->pos);
    fx v = p->ai_act == AI_RUN || p->ai_act == AI_COVER ? T_SPRINT_SPEED : fx_min(fx_mul(d, FX(2.0)), T_SPRINT_SPEED);
    if (d < FX(0.4)) v = 0;
    team_steer_speed(p, p->ai_goal, v, out);
}
