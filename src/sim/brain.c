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
       AI_DRIBBLE, AI_PASS, AI_THROUGH, AI_SHOOT, AI_KEEP, AI_CROSS, AI_HEAD, AI_REST, AI_N };

/* candidate features (fx). F_BIAS is always 1. */
enum { F_FWD = 0,    /* metres gained toward the opponent goal (target vs ball) */
       F_OPEN,       /* nearest opponent to the target point, m (capped 8) */
       F_LANE,       /* clearance of the ball's lane to the target, m (capped 4) */
       F_COST,       /* metres I must travel / pass length */
       F_THREAT,     /* 0..1: shot quality, or danger of the opponent I'd mark */
       F_ROLE,       /* 1: this is the job the team plan gave me (D-030) */
       F_CROWD,      /* team-mates already heading within 4 m of the target */
       F_HOME,       /* metres from my formation spot x leash (0 when it's my plan role) */
       F_BIAS, NF };

/* instinct weights per action (x feature). Units: utility points per feature unit. The anchor (SHAPE) is now
 * competitive and every off-ball target pays F_HOME, so players hold a shape and refine around it (D-030). */
static const fx W[AI_N][NF] = {
    /*            FWD        OPEN       LANE       COST        THREAT     ROLE       CROWD       HOME        BIAS */
    [AI_SHAPE]   = {0,        0,         0,         -FX(0.05),  0,         0,         -FX(1.0),   0,          FX(1.5)},
    [AI_SUPPORT] = {FX(0.10), FX(0.35),  FX(0.45),  -FX(0.20),  0,         FX(2.5),   -FX(1.5),   -FX(0.25),  FX(0.6)},
    [AI_RUN]     = {FX(0.15), FX(0.25),  FX(0.30),  -FX(0.10),  FX(1.0),   FX(1.0),   -FX(1.5),   -FX(0.15),  FX(0.2)},
    [AI_CHASE]   = {0,        0,         0,         -FX(0.10),  0,         FX(4.0),   0,          0,          FX(0.5)},
    [AI_PRESS]   = {0,        0,         0,         -FX(0.10),  0,         FX(4.0),   0,          0,          FX(0.5)},
    [AI_COVER]   = {0,        0,         0,         -FX(0.10),  0,         FX(2.4),   -FX(1.0),   0,          FX(0.5)},
    [AI_MARK]    = {0,        0,         0,         -FX(0.12),  FX(2.0),   FX(2.0),   -FX(2.0),   -FX(0.25),  FX(0.4)},
    [AI_RECEIVE] = {0,        0,         0,         0,          0,         0,         0,          0,          FX(20.0)},
    [AI_DRIBBLE] = {FX(0.30), FX(0.40),  FX(0.30),  0,          0,         0,         -FX(0.8),   0,          -FX(0.6)},
    [AI_PASS]    = {FX(0.13), FX(0.30),  FX(1.60),  -FX(0.06),  0,         0,         0,          0,          -FX(0.4)},
    [AI_THROUGH] = {FX(0.18), FX(0.30),  FX(1.60),  -FX(0.06),  FX(1.0),   0,         0,          0,          -FX(1.2)},
    [AI_SHOOT]   = {0,        0,         FX(0.30),  0,          FX(24.0),  0,         0,          0,          -FX(1.0)},
    [AI_KEEP]    = {0,        0,         0,         0,          0,         0,         0,          0,          FX(1.0)},
    [AI_CROSS]   = {FX(0.10), FX(0.30),  0,         -FX(0.04),  FX(6.0),   0,         0,          0,          -FX(1.2)},
    [AI_HEAD]    = {0,        0,         0,         0,          0,         0,         0,          0,          FX(1.0)},
    [AI_REST]    = {0,        0,         0,         -FX(0.05),  0,         FX(2.0),   0,          0,          FX(1.5)},
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

/* difficulty knobs for a team (D-028) */
static const uint8_t LV_DECIDE[WP_AI_LEVELS] = T_AI_DECIDE_BY_LEVEL;
static const uint8_t LV_PASS_REACT[WP_AI_LEVELS] = T_AI_PASS_REACT_BY_LEVEL;
static const uint8_t LV_KEEPER_REACT[WP_AI_LEVELS] = T_KEEPER_REACT_BY_LEVEL;
static const fx LV_EXPOSED[WP_AI_LEVELS] = T_AI_TACKLE_EXPOSED_BY_LEVEL;
static int lvl(const SimState *s, int team) { return s->ai_level[team] < WP_AI_LEVELS ? s->ai_level[team] : WP_AI_NORMAL; }
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

/* my formation spot (team plan + formation table, D-030) */
static fv2 home_spot(const Ctx *x, int attacking) {
    (void)attacking;
    return shape_spot(x->s, x->team, x->i % x->s->n_per_side);
}
static fx leash(const Ctx *x) { return FX(1.5) - slot_mindset(x->s, x->i % x->s->n_per_side); }

/* 0..1 shot quality from point `at`: distance, angle to the goal mouth, a clear lane, keeper not in the way */
static fx shot_quality(const Ctx *x, fv2 at) {
    fx d = dist(at, x->opp_goal);
    if (d > T_AI_SHOT_RANGE) return 0;
    fx r = fx_div(d, T_AI_SHOT_RANGE);
    fx q = FX_ONE - fx_mul(r, r);                       /* stays worth it from the edge of the box */
    fx ay = fx_abs(at.y), wide = fx_max(ay - x->s->pitch.goal_width, 0);
    q = fx_mul(q, FX_ONE - clampf(fx_div(wide, FX(6.0)), 0, FX_ONE));
    fx lane = fx_min(pass_lane_clear(x->s, x->team, at, x->opp_goal), FX(2.0));
    return fx_mul(q, FX(0.4) + fx_mul(FX(0.3), lane));
}


static int possession(const SimState *s) {          /* team in possession, -1 loose/none */
    const WpBall *b = &s->ball;
    if (b->owner >= 0) return s->players[b->owner].team;
    if (s->pass_target >= 0) return s->players[s->pass_from].team;
    return -1;
}

/* ---------------- candidates: off the ball (D-030: the team plan gates the ball jobs) ---------------- */
static void set_home(Ctx *x, Cand *c, fv2 spot) {
    if (c && c->f[F_ROLE] == 0) c->f[F_HOME] = fx_mul(dist(c->at, spot), leash(x));
}

static void gen_off_ball(Ctx *x) {
    const SimState *s = x->s;
    const WpPlayer *p = &s->players[x->i];
    const WpBall *b = &s->ball;
    const WpTeamPlan *pl = &s->plan[x->team];
    int k = x->i % s->n_per_side;
    int poss = possession(s), attacking = poss == x->team;
    int role = pl->role[k];
    fv2 spot = home_spot(x, attacking);
    Cand *c;
    if (s->pass_target == x->i) {                         /* a pass to me: run through the planned meet */
        Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
        fv2 at = s->pass_plan != WP_PLAN_CLOSEST ? s->pass_meet : m.pos;
        add(x, AI_RECEIVE, -1, at);
        return;
    }
    if ((c = add(x, AI_SHAPE, -1, spot))) {
        c->f[F_COST] = dist(c->at, p->pos);
        c->f[F_CROWD] = crowd(x, c->at);
    }
    /* the one escape from the plan: a ball right at my feet (~0.5 s away) is mine to take */
    fx dball = dist(b->pos, p->pos);
    int escape = b->owner < 0 && dball < FX(2.5);
    if (!attacking) {
        fv2 goal_dir = fv2_norm(fv2_sub(x->own_goal, b->pos));
        int counterpress = pl->trans_ticks < T_PLAN_COUNTERPRESS && pl->last_poss == 0;
        if (b->owner < 0) {                               /* loose: the winner goes; the cover drops goal-side */
            if (role == WP_PR_WIN || escape) {
                Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
                if ((c = add(x, AI_CHASE, -1, m.pos))) {
                    c->f[F_COST] = dist(m.pos, p->pos);
                    c->f[F_ROLE] = role == WP_PR_WIN ? FX_ONE : 0;
                }
            }
        } else if (!b->held && !s->restart_kind) {        /* they have it: winner presses (and the cover, briefly) */
            if (role == WP_PR_WIN || (role == WP_PR_COVER && counterpress) || dball < FX(2.0)) {
                if ((c = add(x, AI_PRESS, -1, fv2_add(b->pos, fv2_scale(goal_dir, FX(0.9)))))) {
                    c->f[F_COST] = dist(c->at, p->pos);
                    c->f[F_ROLE] = role == WP_PR_WIN || role == WP_PR_COVER ? FX_ONE : 0;
                }
            }
        }
        if (role == WP_PR_COVER) {
            fv2 cp = fv2_add(b->pos, fv2_scale(goal_dir, FX(5.0)));
            if ((c = add(x, AI_COVER, -1, in_pitch(x, cp, FX(1.0))))) {
                c->f[F_COST] = dist(c->at, p->pos);
                c->f[F_ROLE] = FX_ONE;
                c->f[F_CROWD] = crowd(x, c->at);
            }
        }
        int o0 = (1 - x->team) * s->n_per_side;
        for (int j = o0; j < o0 + s->n_per_side; j++) {   /* marking: my assigned man, or someone in my zone */
            const WpPlayer *o = &s->players[j];
            if (o->role == WP_ROLE_KEEPER || j == b->owner) continue;
            int mine = pl->mark[k] == j;
            if (!mine && dist(o->pos, spot) > fx_mul(FX(6.0), fx_div(s->pitch.length, FX(55.0)))) continue;
            fv2 at = fv2_add(o->pos, fv2_scale(fv2_norm(fv2_sub(x->own_goal, o->pos)), FX(1.5)));
            if (!(c = add(x, AI_MARK, (int8_t)j, at))) break;
            c->f[F_COST] = dist(at, p->pos);
            c->f[F_THREAT] = FX_ONE - clampf(fx_div(dist(o->pos, x->own_goal), FX(30.0)), 0, FX_ONE);
            c->f[F_ROLE] = mine ? FX_ONE : 0;
            fx others = 0;
            for (int q = x->team * s->n_per_side; q < (x->team + 1) * s->n_per_side; q++)
                if (q != x->i && s->players[q].ai_act == AI_MARK && s->players[q].ai_mate == j) others += FX(2.0);
            c->f[F_CROWD] = others;
            set_home(x, c, spot);
        }
        return;
    }
    if (escape && (c = add(x, AI_CHASE, -1, b->pos))) c->f[F_COST] = dball;
    /* our corner: attack the box - near post, penalty spot, far post (the cross is aimed at whoever's there) */
    if (s->restart_kind == WP_RS_CORNER && s->restart_team == x->team && s->restart_taker != x->i) {
        fx gx = x->opp_goal.x, side = s->restart_spot.y > 0 ? FX_ONE : -FX_ONE;
        static const fx bspot[3][2] = {{FX(3.5), FX(1.6)}, {FX(6.0), FX(0.0)}, {FX(4.5), -FX(2.2)}};
        for (int q = 0; q < 3; q++) {
            fv2 at = fv2_make(gx - fx_mul(FX_INT(x->sg), bspot[q][0]), fx_mul(side, bspot[q][1]));
            if ((c = add(x, AI_RUN, -1, at))) {
                c->f[F_FWD] = FX(6.0);
                c->f[F_THREAT] = FX_ONE;
                c->f[F_ROLE] = FX_ONE;                   /* a set-piece job: no distance cost (big pitches) */
                c->f[F_CROWD] = crowd(x, at);
            }
        }
    }
    /* rest defence: stay behind the ball, goal-side of their most advanced counter-attacker */
    if (role == WP_PR_REST) {
        fx sgx = FX_INT(x->sg);
        fx deep = fx_min(fx_mul(spot.x, sgx), fx_mul(b->pos.x, sgx) - FX(8.0));
        fv2 at = fv2_make(fx_mul(deep, sgx), spot.y);
        if ((c = add(x, AI_REST, -1, in_pitch(x, at, FX(1.5))))) {
            c->f[F_COST] = dist(c->at, p->pos);
            c->f[F_ROLE] = FX_ONE;
        }
        return;                                           /* a rest defender doesn't wander off to support */
    }
    /* support: refine around MY spot (3 m / 6 m, 4 directions), plus the touchline for wide slots; the plan's
     * near-support also gets points on a 9 m ring around the ball */
    for (int r = 0; r < 2; r++)
        for (int a = 0; a < 4; a++) {
            fv2 at = in_pitch(x, fv2_add(spot, fv2_from_angle((wp_angle)(a * 16384), r ? FX(6.0) : FX(3.0))), FX(1.5));
            if (!(c = add(x, AI_SUPPORT, -1, at))) break;
            c->f[F_FWD] = (at.x - b->pos.x) * x->sg;
            c->f[F_OPEN] = nearest_opp(x, at);
            c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
            c->f[F_COST] = dist(at, p->pos);
            c->f[F_CROWD] = crowd(x, at);
            set_home(x, c, spot);
        }
    if (slot_mindset(s, k) == FX(0.5) && fx_abs(spot.y) > fx_mul(x->hw, FX(0.3))) {
        fv2 at = in_pitch(x, fv2_make(spot.x, spot.y > 0 ? x->hw - FX(1.5) : -(x->hw - FX(1.5))), FX(1.0));
        if ((c = add(x, AI_SUPPORT, -1, at))) {
            c->f[F_FWD] = (at.x - b->pos.x) * x->sg;
            c->f[F_OPEN] = nearest_opp(x, at);
            c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
            c->f[F_COST] = dist(at, p->pos);
            c->f[F_CROWD] = crowd(x, at);
            set_home(x, c, spot);
        }
    }
    if (role == WP_PR_SUPPORT) {
        static const int16_t ang[3] = {0, 45, -45};
        fv2 fwd = fv2_make(FX_INT(x->sg), 0);
        for (int a = 0; a < 3; a++) {
            fv2 at = in_pitch(x, fv2_add(b->pos, fv2_scale(fv2_from_angle((wp_angle)(fx_atan2(fwd.y, fwd.x) +
                              DEG_TO_ANG(ang[a])), FX_ONE), fx_mul(FX(9.0), fx_div(s->pitch.length, FX(55.0))))), FX(1.5));
            if (!(c = add(x, AI_SUPPORT, -1, at))) break;
            c->f[F_FWD] = (at.x - b->pos.x) * x->sg;
            c->f[F_OPEN] = nearest_opp(x, at);
            c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, at), FX(4.0));
            c->f[F_COST] = dist(at, p->pos);
            c->f[F_ROLE] = FX_ONE;
            c->f[F_CROWD] = crowd(x, at);
        }
    }
    /* runs in behind: the striker always may; one designated runner from midfield */
    if (slot_mindset(s, k) >= FX_ONE || role == WP_PR_RUN) {
        fx last = -x->hl;
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
            c->f[F_ROLE] = FX_ONE;
            c->f[F_CROWD] = crowd(x, at);
        }
    }
}

/* time-aware lane (CP0.3c): the clearance left once each opponent has run at the ball's path for as long as the ball
 * takes to get there (minus a reaction). Long balls are slow, so a lane that looks open often isn't. Metres; < 0 =
 * someone gets there first. The last 1.5 m belong to the receiver. */
static fx lane_time(const Ctx *x, fv2 a, fv2 t) {
    const SimState *s = x->s;
    fv2 seg = fv2_sub(t, a);
    fx len = fv2_len(seg);
    if (len < FX(1.0)) return T_AI_LANE_CAP;
    fx umax = FX_ONE - fx_div(FX(1.5), len), len2 = fv2_dot(seg, seg);
    fx v = fx_min(T_PASS_ARR_MIN + fx_mul(T_AI_LANE_V_PER_M, len), T_AI_LANE_V_MAX);   /* the ball's mean pace */
    fx best = T_AI_LANE_CAP;
    int o0 = (1 - x->team) * s->n_per_side;
    for (int j = o0; j < o0 + s->n_per_side; j++) {
        const WpPlayer *o = &s->players[j];
        fx u = fx_clamp(fx_div(fv2_dot(fv2_sub(o->pos, a), seg), len2), 0, umax);
        fx c = dist(o->pos, fv2_add(a, fv2_scale(seg, u)));
        fx tb = fx_div(fx_mul(u, len), v);
        fx e = c - T_AI_LANE_REACH - fx_mul(T_AI_LANE_RUN, fx_max(tb - T_AI_LANE_REACT, 0));
        if (e < best) best = e;
    }
    return fx_max(best, -T_AI_LANE_CAP);
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
        c->f[F_CROWD] = fx_max(T_AI_PRESSURE_M - press, 0);   /* CP0.3c: a man on me makes dribbling costly */
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
            c->f[F_LANE] = lane_time(x, b->pos, at);
            c->f[F_COST] = d0;
            if (q->role == WP_ROLE_KEEPER) c->f[F_BIAS] = FX(3.0);      /* back to the keeper: last resort */
        }
        if (q->vel.x * x->sg > FX(3.0) && q->role != WP_ROLE_KEEPER) {  /* a runner: in behind */
            fv2 ahead = in_pitch(x, fv2_add(q->pos, fv2_scale(q->vel, FX(1.0))), FX(3.0));
            if ((c = add(x, AI_THROUGH, (int8_t)j, ahead))) {
                c->f[F_FWD] = (ahead.x - b->pos.x) * x->sg;
                c->f[F_OPEN] = nearest_opp(x, ahead);
                c->f[F_LANE] = lane_time(x, b->pos, ahead);
                c->f[F_COST] = dist(ahead, b->pos);
                c->f[F_THREAT] = shot_quality(x, ahead);
            }
        }
    }
    /* cross: from wide in the attacking third, a lofted ball to a team-mate near the goal (headers / volleys) */
    if (fx_abs(b->pos.y) > fx_mul(x->hw, FX(0.40)) && b->pos.x * x->sg > fx_mul(x->hl, FX(0.35)))
        for (int j = x->team * s->n_per_side; j < (x->team + 1) * s->n_per_side; j++) {
            const WpPlayer *q2 = &s->players[j];
            if (j == x->i || q2->role == WP_ROLE_KEEPER) continue;
            fx dq = dist(q2->pos, x->opp_goal);
            if (dq > T_AI_CROSS_ZONE || dist(q2->pos, b->pos) < FX(6.0)) continue;
            if ((c = add(x, AI_CROSS, (int8_t)j, q2->pos))) {
                c->f[F_FWD] = (q2->pos.x - b->pos.x) * x->sg;
                c->f[F_OPEN] = nearest_opp(x, q2->pos);
                c->f[F_COST] = dist(q2->pos, b->pos);
                c->f[F_THREAT] = FX_ONE - fx_div(dq, T_AI_CROSS_ZONE);
            }
        }
    fx q = shot_quality(x, b->pos);
    if (q > 0 && (c = add(x, AI_SHOOT, -1, x->opp_goal))) {
        c->f[F_THREAT] = fx_mul(q, FX_ONE + fx_mul(T_STAT_AI_SHOOT, stat_k(s, x->i, WP_STAT_SHOOTING)));   /* D-034 */
        c->f[F_LANE] = fx_min(pass_lane_clear(s, x->team, b->pos, x->opp_goal), FX(4.0));
    }
}

static fx score(const Cand *c, const WpPlayer *p) {
    fx u = 0;
    for (int k = 0; k < NF; k++) u += fx_mul(W[c->act][k], c->f[k]);
    if (c->act == p->ai_act && dist(c->at, p->ai_goal) < FX(2.0) && c->mate == p->ai_mate) u += T_AI_COMMIT;
    if (c->act == AI_RUN) u -= fx_mul(T_AI_TIRED_RUN, FX_ONE - p->stamina);   /* runs cost legs (0 when fresh) */
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
        k.windup = kind == WP_KICK_LOB ? T_KICK_WINDUP_CHIP : T_KICK_WINDUP_PASS;
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
    if (b->owner >= 0 && s->players[b->owner].team != x->team && db < T_KEEPER_NARROW_D)   /* close carrier: come off */
        out_d = clampf(fx_mul(db, T_KEEPER_NARROW_K), out_d, T_KEEPER_NARROW_MAX);          /* the line, narrow the angle */
    fv2 at = fv2_add(g, fv2_scale(fv2_norm(to_ball), out_d));
    fx gw = s->pitch.goal_width / 2;
    at.y = clampf(at.y, -gw, gw);
    /* a shot coming: after reacting (T_KEEPER_REACT), shuffle across to where it will cross my line */
    fx vin = -b->vel.x * x->sg;                           /* speed toward my goal */
    fx cap = player_top_speed(s, x->i, 1);
    int shot = 0;
    if (b->owner < 0 && vin > FX(4.0)) {
        fx t = fx_div(fx_abs(b->pos.x - p->pos.x), vin);
        fx cy = b->pos.y + fx_mul(b->vel.y, t);
        if (t < FX(1.5) && fx_abs(cy) < gw + FX(1.0)) {
            shot = 1;
            int react = (int)LV_KEEPER_REACT[lvl(s, x->team)]
                      - (int)(fx_mul(FX_INT(T_STAT_KEEP_REACT), stat_k(s, x->i, WP_STAT_KEEPING)) / FX_ONE);   /* D-034 */
            if (p->ai_wait < react) { p->ai_wait++; at = p->ai_goal; }   /* reading it */
            else {
                at = fv2_make(p->pos.x, clampf(cy, -gw - FX(0.5), gw + FX(0.5)));
                fx gap = cy - p->pos.y;
                /* too far to shuffle and it's nearly here: dive (only for balls heading for the goal) */
                if (p->action == WP_ACT_NONE && p->z == 0 && fx_abs(gap) > T_DIVE_MIN_GAP && t < T_DIVE_WHEN &&
                    fx_abs(cy) < gw + FX(0.3)) {
                    keeper_dive_start(s, x->i, gap > 0 ? 1 : -1);
                    p->ai_act = AI_KEEP;
                    return;
                }
            }
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
    team_steer_speed(s, p, at, fx_min(fx_mul(d, FX(5.0)), cap), out);
}

void brain_input(SimState *s, int i, WpSeatInput *out) {
    WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    out->move_x = out->move_y = 0;
    out->buttons = 0;
    /* the ball is dead between goal and kick-off: stand still. Before a set piece (OUT pause) everyone repositions
     * - the ball can't be played, so only movement matters */
    if (s->phase != WP_PHASE_PLAY && s->phase != WP_PHASE_OUT) { p->ai_act = AI_NONE; p->ai_hold = 0; return; }
    Ctx x;
    x.s = s; x.i = i; x.team = p->team; x.sg = team_dir(s, p->team); x.n = 0;
    x.hl = s->pitch.length / 2; x.hw = s->pitch.width / 2;
    x.own_goal = fv2_make(-x.hl * x.sg, 0);
    x.opp_goal = fv2_make(x.hl * x.sg, 0);
    int owner = b->owner == i;
    p->ai_hold = owner ? (uint8_t)(p->ai_hold < 255 ? p->ai_hold + 1 : 255) : 0;

    if (owner && restart_is_taker(s, i)) {               /* taking a set piece (D-029) */
        if (p->ai_hold < T_AI_RESTART_DELAY || p->action != WP_ACT_NONE) return;
        int best = -1; fx bs = 0;
        int corner = s->restart_kind == WP_RS_CORNER, throw_in = s->restart_kind == WP_RS_THROW;
        if (corner && s->restart_ticks < T_AI_CORNER_WAIT) {   /* wait for two team-mates to get into the box */
            int in_box = 0;
            for (int j = x.team * s->n_per_side; j < (x.team + 1) * s->n_per_side; j++)
                if (j != i && dist(s->players[j].pos, x.opp_goal) < FX(12.0)) in_box++;
            if (in_box < 2) return;
        }
        for (int j = x.team * s->n_per_side; j < (x.team + 1) * s->n_per_side; j++) {
            const WpPlayer *q = &s->players[j];
            if (j == i || q->role == WP_ROLE_KEEPER) continue;
            fx d = dist(q->pos, b->pos);
            if (d < FX(3.0) || d > (throw_in ? FX(16.0) : FX(26.0))) continue;
            fx open = nearest_opp(&x, q->pos), lane = fx_min(pass_lane_clear(s, x.team, b->pos, q->pos), FX(4.0));
            fx sc = open + fx_mul(lane, FX(1.2)) + fx_mul((q->pos.x - b->pos.x) * x.sg, FX(0.15));
            if (corner) sc = fx_max(T_AI_CROSS_ZONE - dist(q->pos, x.opp_goal), 0) * 2 + open;   /* someone in the box */
            if (best < 0 || sc > bs) { best = j; bs = sc; }
        }
        if (best < 0) {                                   /* nobody: the nearest team-mate */
            for (int j = x.team * s->n_per_side; j < (x.team + 1) * s->n_per_side; j++)
                if (j != i && s->players[j].role != WP_ROLE_KEEPER &&
                    (best < 0 || dist(s->players[j].pos, b->pos) < bs)) { best = j; bs = dist(s->players[j].pos, b->pos); }
        }
        if (best < 0) return;
        uint8_t kind = throw_in ? WP_KICK_THROW
                     : corner ? (dist(s->players[best].pos, x.opp_goal) < T_AI_CROSS_ZONE ? WP_KICK_LOB : WP_KICK_PASS)
                     : (dist(s->players[best].pos, b->pos) > FX(20.0) ? WP_KICK_LOB : WP_KICK_PASS);
        WpKick k = {0};
        fv2 d = fv2_sub(s->players[best].pos, b->pos);
        k.kind = kind; k.dir = fx_atan2(d.y, d.x); k.to = (uint8_t)(best + 1); k.foot = 1;
        k.power = kind == WP_KICK_THROW ? T_THROW_SPEED : T_PASS_SPEED;
        k.windup = kind == WP_KICK_PASS ? T_KICK_WINDUP_PASS : T_KICK_WINDUP_CHIP;
        sim_player_kick(s, i, &k);
        return;
    }
    if (p->role == WP_ROLE_KEEPER && !owner) { keeper(&x, out); return; }
    if (owner && b->held) {                              /* keeper with it in his hands: look up, then distribute */
        if (p->ai_hold < T_AI_KEEPER_HOLD || p->action != WP_ACT_NONE) return;
        int best = -1; fx bs = 0;
        for (int j = x.team * s->n_per_side; j < (x.team + 1) * s->n_per_side; j++) {
            const WpPlayer *q = &s->players[j];
            if (j == i) continue;
            fx d = dist(q->pos, b->pos);
            if (d < FX(5.0) || d > FX(22.0)) continue;
            fx open = nearest_opp(&x, q->pos), lane = fx_min(pass_lane_clear(s, x.team, b->pos, q->pos), FX(4.0));
            fx sc = open + fx_mul(lane, FX(1.5)) + fx_mul((q->pos.x - b->pos.x) * x.sg, FX(0.1));
            if (open < FX(4.0) || lane < FX(2.0)) continue;
            if (best < 0 || sc > bs) { best = j; bs = sc; }
        }
        WpKick k = {0};
        k.foot = 1;
        if (best >= 0) {                                 /* throw it to a free man */
            fv2 d = fv2_sub(s->players[best].pos, b->pos);
            k.kind = WP_KICK_THROW; k.dir = fx_atan2(d.y, d.x); k.to = (uint8_t)(best + 1);
            k.power = T_THROW_SPEED; k.windup = T_KICK_WINDUP_CHIP;
        } else {                                         /* nobody free: punt it toward the furthest-forward mate */
            int fwd = -1; fx fx_best = 0;
            for (int j = x.team * s->n_per_side; j < (x.team + 1) * s->n_per_side; j++)
                if (j != i && (fwd < 0 || s->players[j].pos.x * x.sg > fx_best)) { fwd = j; fx_best = s->players[j].pos.x * x.sg; }
            fv2 d = fv2_sub(s->players[fwd].pos, b->pos);
            k.kind = WP_KICK_PUNT; k.dir = fx_atan2(d.y, d.x); k.power = T_PUNT_SPEED; k.vz = T_PUNT_VZ; k.windup = 8;
        }
        sim_player_kick(s, i, &k);
        return;
    }
    /* kickoff: the team not kicking off holds its shape until the ball is first touched */
    if (s->restart_kind == WP_RS_KICKOFF && p->team != s->restart_team) {
        fv2 at = home_spot(&x, 0);
        p->ai_act = AI_SHAPE;
        p->ai_goal = at;
        team_steer_speed(s, p, at, fx_min(fx_mul(dist(at, p->pos), FX(2.0)), player_top_speed(s, i, 0)), out);
        return;
    }

    int lv = lvl(s, p->team);
    int decide = ((s->tick + (uint32_t)(i % s->n_per_side)) % LV_DECIDE[lv]) == 0   /* staggered by role: mirror-fair */ || p->ai_act == AI_NONE ||
                 (owner && p->ai_act < AI_DRIBBLE) || (!owner && p->ai_act >= AI_DRIBBLE && p->ai_act != AI_KEEP);
    if (s->pass_target == i && p->ai_act != AI_RECEIVE) decide = 1;
    /* aerial duel: a ball coming in high - the team's first man (on his decision beat) goes for a header. At goal near
     * their box, a clearance near ours, otherwise nod it on to a team-mate. The aim target is ai_goal. */
    int jump;
    if ((decide || s->pass_target == i) && p->action == WP_ACT_NONE && b->owner < 0 && !owner &&
        (s->plan[x.team].winner == i || dist(b->pos, p->pos) < FX(3.0)) && header_wanted(s, i, &jump)) {
        fx to_goal = dist(p->pos, x.opp_goal);
        int at_goal = to_goal < FX(14.0) && shot_quality(&x, p->pos) > 0;
        header_start(s, i, at_goal, jump);
        p->ai_act = AI_HEAD;
        if (at_goal) p->ai_goal = shot_target(&x);
        else if (p->pos.x * x.sg < -fx_mul(x.hl, FX(0.3)))                     /* our end: clear it upfield, wide */
            p->ai_goal = fv2_make(fx_mul(x.hl, FX_INT(x.sg)), p->pos.y > 0 ? x.hw : -x.hw);
        else p->ai_goal = fv2_add(p->pos, fv2_make(FX_INT(8 * x.sg), 0));       /* nod it on (pass_pick finds a mate) */
        return;
    }
    if (p->ai_act == AI_HEAD && (p->action == WP_ACT_JUMP_HEADER || p->action == WP_ACT_STAND_HEADER)) return;
    /* a pass by the other team has just been struck: defenders need a moment to react (keep doing what they were) */
    if (s->pass_target >= 0 && s->players[s->pass_from].team != p->team && s->pass_ticks < LV_PASS_REACT[lv]) decide = 0;
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
    case AI_PASS: case AI_THROUGH: case AI_SHOOT: case AI_CROSS:
        if (!owner) { p->ai_act = AI_NONE; break; }
        if (p->ai_hold < T_AI_FIRST_TOUCH || p->action != WP_ACT_NONE) break;    /* control it first */
        if (p->ai_act == AI_SHOOT) {            /* aim at the far side of the keeper, with an error that grows with
                                                 * distance and pressure (one RNG draw: SimState.rng) */
            fv2 tgt = shot_target(&x);
            fx err = T_AI_AIM_ERR + fx_mul(dist(b->pos, x.opp_goal), T_AI_AIM_ERR_M);
            if (nearest_opp(&x, b->pos) < FX(2.0)) err += T_AI_AIM_ERR_PRESS;
            err = fx_mul(err, FX_ONE - fx_mul(T_STAT_SHOT_AIM, stat_k(s, i, WP_STAT_SHOOTING)));   /* D-034 */
            fx dy = wp_rng_fx_signed(&s->rng, err);
            tgt.y += dy;
            do_kick(s, i, WP_KICK_SHOT, -1, tgt, T_AI_SHOT_POWER);
        }
        else if (p->ai_act == AI_CROSS) do_kick(s, i, WP_KICK_LOB, p->ai_mate, s->players[p->ai_mate].pos, 0);
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
        if (s->pass_target >= 0 && s->players[s->pass_from].team != p->team && s->pass_ticks < LV_PASS_REACT[lv]) break;
        Meet m = meet_choose(s, p, fv2_make(0, 0), 0);
        p->ai_goal = m.pos;
        team_steer_speed(s, p, m.pos, fx_min(fx_mul(dist(m.pos, p->pos), FX(4.0)) + FX(2.0), player_top_speed(s, i, 1)), out);
        return;
    }
    case AI_PRESS: {
        if (b->owner < 0 || s->players[b->owner].team == p->team) break;
        fv2 at = fv2_add(b->pos, fv2_scale(fv2_norm(fv2_sub(x.own_goal, b->pos)), FX(0.9)));
        p->ai_goal = at;
        /* tackle when the carrier's ball is exposed (away from his feet) right in front of me */
        fx db = dist(b->pos, p->pos), exposed = dist(b->pos, s->players[b->owner].pos);
        fv2 face = fv2_from_angle(p->facing, FX_ONE);
        fx need = fv2_len(s->players[b->owner].vel) < FX(2.0) ? T_AI_TACKLE_SHIELD : LV_EXPOSED[lv];
        need = fx_mul(need, FX_ONE - fx_mul(T_STAT_TACKLE_READ, stat_k(s, i, WP_STAT_TACKLING)));   /* D-034 */
        if (decide && p->action == WP_ACT_NONE && db < T_AI_TACKLE_DIST && exposed > need &&   /* at his decision beat */
            fv2_dot(face, fv2_sub(b->pos, p->pos)) > 0) {
            tackle_start(s, i);
            return;
        }
        team_steer_speed(s, p, at, fx_min(fx_mul(dist(at, p->pos), FX(4.0)) + FX(1.0), player_top_speed(s, i, 1)), out);
        return;
    }
    case AI_RECEIVE: {
        fx d = dist(p->ai_goal, p->pos);
        team_steer_speed(s, p, p->ai_goal, fx_min(fx_mul(d, FX(4.0)), player_top_speed(s, i, 1)), out);
        return;
    }
    default: break;
    }
    if (p->ai_act == AI_NONE) return;
    fx d = dist(p->ai_goal, p->pos);
    fx top = player_top_speed(s, i, 1);
    if (p->stamina < T_AI_SAVE_ENERGY && p->ai_act != AI_COVER) top = player_top_speed(s, i, 0);   /* tired: jog (D-034) */
    fx v = p->ai_act == AI_RUN || p->ai_act == AI_COVER ? top : fx_min(fx_mul(d, FX(2.0)), top);
    if (d < FX(0.4)) v = 0;
    team_steer_speed(s, p, p->ai_goal, v, out);
}
