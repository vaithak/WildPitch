/* aerial_ai.c - the AI's choice at a ball coming in the air (D-051, CP0.5a stage 4). The AI gets exactly the contacts
 * a human gets (header shot / header pass / flick-on / lofted clearance header / volley / half-volley pass / chest
 * lay-off / a control touch), chosen D-008 style: candidates x features x weights, ties in fixed order, no dice.
 * brain.c carries the choice out (movement, header_start); the first-time foot contacts go through contact.c with
 * WpPlayer.ai_intent / ai_aim in place of a seat's buffered press.
 *
 * Inputs, all from the contact ring (contact.c, the real ball's future while nothing touches it):
 *  - "attack": his earliest reachable contact with the ball at head height (T_INTENT_HEAD_Z .. T_CB_JUMP);
 *  - his contact plan (the control stance the planner chose, its tick and point) for foot / body contacts;
 *  - the opponents' earliest contact with the ball at any playable height.
 * CONTEST = an opponent gets there first (graded over a few ticks). A control touch later than the opponent's
 * contact is headed away before it happens, so a contested receiver attacks the ball at the earliest point (head it
 * on, at goal, or clear it); a free one lets it drop and controls it (stage 2's lesson: moving every receiver to the
 * control point lost the duel to markers in front). */
#include "sim/sim_internal.h"

enum { AA_CONTROL = 0, AA_HEAD_SHOT, AA_HEAD_PASS, AA_HEAD_CLEAR, AA_VOLLEY, AA_HALFVOLLEY, AA_LAYOFF, AA_N };
/* features (fx); G_BIAS is always 1 */
enum { G_THREAT = 0,   /* 0..1 shot quality (from the contact; for a pass: at the mate) */
       G_OPEN,         /* the mate's nearest opponent, m (capped 8) */
       G_FWD,          /* metres the ball gains toward their goal */
       G_DIST,         /* pass length, m */
       G_CONTEST,      /* 0..1: an opponent gets to the ball first */
       G_PRESS,        /* 0..1: an opponent close to the contact point */
       G_DIFF,         /* the planned touch's difficulty (stance base + speed over v_clean) */
       G_DANGER,       /* 0..1: how close the contact is to our goal (our half) */
       G_ERR,          /* a strike at goal: its aim error at the goal (the contact's own error law) / half the goal, <= 2 */
       G_BIAS, NG };

/* weights per candidate x feature (utility points per unit). The learned brain (Phase 1) adds its own score on these
 * same candidates and features. */
static const fx W[AA_N][NG] = {
    /*               THREAT     OPEN       FWD        DIST        CONTEST     PRESS      DIFF       DANGER     ERR        BIAS */
    [AA_CONTROL]    = {0,        0,         0,         0,          -FX(10.0),  -FX(1.0),  -FX(2.0),  -FX(1.0),  0,         FX(2.0)},
    [AA_HEAD_SHOT]  = {FX(12.0), 0,         0,         0,          -FX(1.0),   0,         0,         0,         -FX(4.0),  -FX(0.5)},
    [AA_HEAD_PASS]  = {FX(6.0),  FX(0.15),  FX(0.08),  -FX(0.06),  -FX(1.0),   0,         0,         -FX(1.0),  0,         -FX(0.2)},
    [AA_HEAD_CLEAR] = {0,        0,         0,         0,          FX(1.0),    0,         0,         FX(4.0),   0,         -FX(1.0)},
    [AA_VOLLEY]     = {FX(12.0), 0,         0,         0,          -FX(10.0),  -FX(1.0),  -FX(3.0),  0,         -FX(4.0),  -FX(1.5)},
    [AA_HALFVOLLEY] = {FX(2.0),  FX(0.15),  FX(0.08),  -FX(0.04),  -FX(10.0),  0,         -FX(2.0),  -FX(1.0),  0,         -FX(0.6)},
    [AA_LAYOFF]     = {FX(2.0),  FX(0.20),  FX(0.04),  0,          -FX(10.0),  FX(0.5),   0,         -FX(1.0),  0,         -FX(0.6)},
};

static fx dist(fv2 a, fv2 b) { return fv2_len(fv2_sub(a, b)); }

static fx nearest_opp(const SimState *s, int team, fv2 at) {
    fx best = FX(8.0);
    int o0 = (1 - team) * s->n_per_side;
    for (int j = o0; j < o0 + s->n_per_side; j++) {
        fx d = dist(s->players[j].pos, at);
        if (d < best) best = d;
    }
    return best;
}

/* 0..1: an opponent's contact at t_opp beats ours at t (one tick earlier = 1/2, CONTEST_SPAN earlier = 1) */
static fx contest(int t, int t_opp) {
    if (t_opp < 0) return 0;
    int lead = t - t_opp + T_AIR_CONTEST_SLACK;
    if (lead <= 0) return 0;
    return lead >= T_AIR_CONTEST_SPAN ? FX_ONE : fx_div(FX_INT(lead), FX_INT(T_AIR_CONTEST_SPAN));
}

/* the aim error (half-width of the uniform spread, wp_angle units) of a strike from `at` toward `tgt` off the ball
 * `bt` arriving at player p: the header law (header.c, D-048) or the volley law (contact.c) - what the strike will
 * actually get. As G_ERR: that spread at the goal over half the goal width (capped 2); -1 = it can't be aimed at all
 * (turned >= T_FLICK_DEG: a flick-on). */
static fx err_feature(const SimState *s, const WpPlayer *p, const WpBall *bt, fv2 at, fv2 tgt, int head) {
    if (!bt) return FX(2.0);
    fv2 rh = fv2_sub(bt->vel, p->vel), w = fv2_sub(tgt, at);
    fx rs = fv2_len(fv2_make(fv2_len(rh), bt->vz - p->vz)), d = fv2_len(w), sig;
    if (head) {
        int32_t phi = 0;
        if (fv2_len(rh) > FX(0.5)) { phi = ang_diff(fx_atan2(w.y, w.x), fx_atan2(-rh.y, -rh.x)); if (phi < 0) phi = -phi; }
        if (phi >= DEG_TO_ANG(T_FLICK_DEG)) return -FX_ONE;          /* a flick-on can't be aimed at goal */
        fx over = fx_div(fx_max(rs - T_HEADER_CLEAN_V, 0), T_HEADER_CLEAN_V);
        sig = fx_mul(FX_INT(DEG_TO_ANG(T_HEADER_ERR_DEG)), fx_mul(FX_ONE + fx_div(FX_INT(phi), FX_INT(DEG_TO_ANG(90))), FX_ONE + over));
        if (phi > DEG_TO_ANG(90)) sig *= 3;
    } else {
        fx vc = FX(13.0), over = fx_div(fx_max(rs - vc, 0), vc);      /* (the instep's v_clean, as contact_strike) */
        sig = fx_mul(FX_INT(DEG_TO_ANG(T_VOLLEY_ERR_DEG) * (bt->z > T_CB_LOW ? 3 : 2)), FX_ONE + over);
    }
    /* spread at the goal: tan(sig) ~ sig (rad) = sig / 65536 x 2 pi */
    fx miss = fx_mul(fx_mul(sig / 65536, FX(6.2832)), d);
    return fx_min(fx_div(miss, fx_max(s->pitch.goal_width / 2, FX(0.5))), FX(2.0));
}

typedef struct { uint8_t act; int8_t mate; fv2 aim; fx f[NG]; } ACand;

static ACand *add(ACand *c, int *n, uint8_t act, int8_t mate, fv2 aim) {
    if (*n >= 16) return 0;
    ACand *k = &c[(*n)++];
    k->act = act; k->mate = mate; k->aim = aim;
    for (int g = 0; g < NG; g++) k->f[g] = 0;
    k->f[G_BIAS] = FX_ONE;
    return k;
}

int aerial_choose(const SimState *s, int i, AirChoice *out) {
    const WpPlayer *p = &s->players[i];
    int team = p->team, sg = team_dir(s, team), n_side = s->n_per_side;
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    fv2 opp_goal = fv2_make(hl * sg, 0), own_goal = fv2_make(-hl * sg, 0);
    /* his contacts: attack (head height, earliest) and his planned control contact */
    fv2 A = p->pos, C = p->pos;
    fx za = 0;
    int lt = s->ball.last_touch, ours = lt >= 0 && s->players[lt].team == team;
    /* attack point: our ball comes to him - met where it passes closest; theirs is raced for (the earliest) */
    int ta = contact_reachable(s, i, T_INTENT_HEAD_Z, T_AIR_HEAD_TOP, T_AIR_HEAD_MAX_T, ours, &A, &za);
    int tc = -1, band = WP_CB_NONE;
    if (p->plan.kind) {
        tc = (int16_t)(uint16_t)(p->plan.tick - (uint16_t)s->tick);
        if (tc < 0) tc = 0;
        C = p->plan.at;
        band = p->plan.band;
    }
    if (ta < 0 && tc < 0) return 0;
    /* their ball: a defender reads its flight late - he commits to a contact only this close to it (before that the
     * brain keeps him on his job: marking, or the winner's chase to where it comes down). Our own ball's receiver
     * knows where it is going. */
    if (lt >= 0 && !ours) {
        int soon = ta >= 0 && (tc < 0 || ta < tc) ? ta : tc;
        if (soon > T_AIR_DEF_READ) return 0;
    }
    /* the opponents' earliest contact (any playable height); only as far as it can still matter */
    int t_opp = -1, horizon = (ta > tc ? ta : tc) + T_AIR_CONTEST_SLACK;
    for (int j = (1 - team) * n_side; j < (2 - team) * n_side; j++) {
        if (s->players[j].role == WP_ROLE_KEEPER) continue;
        fv2 at; fx z;
        int t = contact_earliest(s, j, 0, T_CB_JUMP, t_opp >= 0 ? t_opp - 1 : horizon, &at, &z);
        if (t >= 0) t_opp = t;
    }
    ACand c[16];
    int n = 0;
    ACand *k;
    fx danger_a = fx_clamp(FX_ONE - fx_div(dist(A, own_goal), hl), 0, FX_ONE);
    fx danger_c = fx_clamp(FX_ONE - fx_div(dist(C, own_goal), hl), 0, FX_ONE);
    if (tc >= 0 && (k = add(c, &n, AA_CONTROL, -1, C))) {
        k->f[G_CONTEST] = contest(tc, t_opp);
        k->f[G_PRESS] = contact_pressure(s, team, C);
        k->f[G_DIFF] = fx_max(contact_plan_difficulty(s, i), 0);
        k->f[G_DANGER] = danger_c;
    }
    if (ta >= 0) {
        fx cta = contest(ta, t_opp);
        fx q = brain_shot_quality(s, team, A);
        /* the corner away from their keeper, unless the other one asks for less of a turn (a header can't be
         * steered much: the error grows with the turn, a flick can't be aimed at all) */
        fv2 tgt = brain_shot_target(s, team), alt = fv2_make(tgt.x, -tgt.y);
        fx herr = -FX_ONE;
        if (q > 0 && dist(A, opp_goal) < T_AIR_HEAD_SHOT_D) {
            const WpBall *bt = contact_ring(s, ta);
            fx e1 = err_feature(s, p, bt, A, tgt, 1), e2 = err_feature(s, p, bt, A, alt, 1);
            if (e2 >= 0 && (e1 < 0 || e2 + T_AIR_FAR_CORNER < e1)) { tgt = alt; e1 = e2; }
            herr = e1;
            /* aim inside the post by the spread it will get (a wide spread: at the middle of the goal) */
            fx half = s->pitch.goal_width / 2, in = fx_max(fx_mul(half, FX_ONE - fx_min(herr, FX_ONE)), 0);
            if (herr >= 0 && in < fx_abs(tgt.y)) tgt.y = tgt.y > 0 ? in : -in;
        }
        if (herr >= 0 && (k = add(c, &n, AA_HEAD_SHOT, -1, tgt))) {
            k->f[G_THREAT] = q;
            k->f[G_ERR] = herr;
            k->f[G_CONTEST] = cta;
        }
        if (A.x * sg < 0 && (k = add(c, &n, AA_HEAD_CLEAR, -1, fv2_make(A.x + FX_INT(25 * sg), A.y >= 0 ? fx_mul(hw, FX(0.8)) : -fx_mul(hw, FX(0.8)))))) {
            k->f[G_CONTEST] = cta;
            k->f[G_DANGER] = danger_a;
        }
        for (int j = team * n_side; j < (team + 1) * n_side; j++) {
            const WpPlayer *q2 = &s->players[j];
            if (j == i || q2->role == WP_ROLE_KEEPER) continue;
            fx d = dist(q2->pos, A);
            if (d < T_AIR_HEADPASS_MIN || d > T_AIR_HEADPASS_MAX) continue;
            if ((k = add(c, &n, AA_HEAD_PASS, (int8_t)j, q2->pos))) {
                k->f[G_THREAT] = brain_shot_quality(s, team, q2->pos);
                k->f[G_OPEN] = nearest_opp(s, team, q2->pos);
                k->f[G_FWD] = (q2->pos.x - A.x) * sg;
                k->f[G_DIST] = d;
                k->f[G_CONTEST] = cta;
                k->f[G_DANGER] = danger_a;
            }
        }
    }
    if (tc >= 0 && band >= WP_CB_LOW && band <= WP_CB_CHEST) {
        fx cc = contest(tc, t_opp), diff = fx_max(contact_plan_difficulty(s, i), 0);
        fx q = brain_shot_quality(s, team, C);
        (void)q;
        if (band != WP_CB_MID)
            for (int j = team * n_side; j < (team + 1) * n_side; j++) {
                const WpPlayer *q2 = &s->players[j];
                if (j == i || q2->role == WP_ROLE_KEEPER) continue;
                fx d = dist(q2->pos, C);
                int lay = band == WP_CB_CHEST;
                if (d < FX(3.0) || d > (lay ? T_LAYOFF_DIST : T_AIR_HALFVOLLEY_MAX)) continue;
                if ((k = add(c, &n, lay ? AA_LAYOFF : AA_HALFVOLLEY, (int8_t)j, q2->pos))) {
                    k->f[G_THREAT] = brain_shot_quality(s, team, q2->pos);
                    k->f[G_OPEN] = nearest_opp(s, team, q2->pos);
                    k->f[G_FWD] = (q2->pos.x - C.x) * sg;
                    k->f[G_DIST] = d;
                    k->f[G_CONTEST] = cc;
                    k->f[G_PRESS] = contact_pressure(s, team, C);
                    k->f[G_DIFF] = diff;
                    k->f[G_DANGER] = danger_c;
                }
            }
    }
    /* volley: his earliest reachable contact with the ball dropping through the volley bands (LOW / MID) - not
     * necessarily the control plan's band: with a SHOT intent the planner aims at that band (contact.c) */
    fv2 V = p->pos;
    fx zv = 0;
    int tv = contact_reachable(s, i, T_CB_GROUND + 1, T_CB_MID, T_AIR_HEAD_MAX_T, 0, &V, &zv);
    if (tv >= 0) {
        fx q = brain_shot_quality(s, team, V);
        if (q > 0 && (k = add(c, &n, AA_VOLLEY, -1, brain_shot_target(s, team)))) {
            const WpBall *bt = contact_ring(s, tv);
            k->f[G_THREAT] = q;
            k->f[G_ERR] = err_feature(s, p, bt, V, k->aim, 0);
            k->f[G_CONTEST] = contest(tv, t_opp);
            k->f[G_PRESS] = contact_pressure(s, team, V);
            k->f[G_DIFF] = bt ? fx_div(fx_max(fv2_len(fv2_make(fv2_len(bt->vel), bt->vz)) - FX(13.0), 0), FX(13.0)) : FX_ONE;
        }
    }
    int best = -1, latch = ours;
    fx bs = 0;
    for (int m = 0; m < n; m++) {
        fx u = 0;
        for (int g = 0; g < NG; g++) u += fx_mul(W[c[m].act][g], c[m].f[g]);
        if (p->ai_air == c[m].act + 1 && (c[m].mate < 0 || c[m].mate == p->ai_mate))
            u += latch && (c[m].act == AA_HEAD_SHOT || c[m].act == AA_HEAD_PASS || c[m].act == AA_HEAD_CLEAR)
               ? T_AIR_LATCH : T_AIR_COMMIT;       /* our ball: a header once chosen is kept (no mid-flight flips) */
        if (best < 0 || u > bs) { best = m; bs = u; }     /* tie -> first generated */
    }
    if (best < 0) return 0;
    const ACand *w = &c[best];
    int head = w->act == AA_HEAD_SHOT || w->act == AA_HEAD_PASS || w->act == AA_HEAD_CLEAR;
    out->act = w->act;
    out->head = (uint8_t)head;
    out->mate = w->mate;
    out->aim = w->aim;
    out->intent = w->act == AA_HEAD_SHOT || w->act == AA_VOLLEY ? WP_KICK_SHOT + 1
                : w->act == AA_HEAD_CLEAR ? WP_KICK_LOB + 1 : w->act == AA_CONTROL ? 0 : WP_KICK_PASS + 1;
    int t = head ? ta : tc;
    out->to = head ? fv2_sub(A, fv2_from_angle(p->facing, T_HEAD_FWD)) : contact_stand_at(s, i);
    if (w->act == AA_VOLLEY) {                            /* (until the plan follows: the volley point itself) */
        t = tv;
        if (p->plan.kind != WP_CK_INSTEP && p->plan.kind != WP_CK_THIGH) out->to = V;
    }
    /* there and settled in time: full effort until ~0.25 s out, then ease in (an arrival at speed would turn a
     * cushion into a deflection) */
    out->speed = fx_min(fx_mul(dist(out->to, p->pos), T_AIR_EASE), player_top_speed(s, i, 1));
    out->t = t;
    out->z = head ? za : p->plan.z;
    return 1;
}
