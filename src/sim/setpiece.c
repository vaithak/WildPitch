/* setpiece.c - where the restart team's players go to receive a throw-in or a goal kick (owner, CP0.3e: "players
 * should be spread out like in actual matches to receive the ball"; goal kick: "everyone going out of the D area and
 * the team spreading out"). A short list of receiving spots per restart, filled greedily by the nearest team-mate
 * (taker and keeper excluded); everyone else keeps the team plan. Spots are in metres around the restart spot, in
 * the restart team's attacking frame, so they work in both halves (D-036). */
#include "sim/sim_internal.h"

static int predicted_taker(const SimState *s) {
    if (s->restart_taker >= 0) return s->restart_taker;
    int t = s->restart_team, n = s->n_per_side, best = -1; fx bd = 0;
    if (s->restart_kind == WP_RS_GOALKICK) return t * n;  /* the keeper */
    for (int j = t * n; j < (t + 1) * n; j++) {
        if (s->players[j].role == WP_ROLE_KEEPER) continue;
        fx d = fv2_len(fv2_sub(s->players[j].pos, s->restart_spot));
        if (best < 0 || d < bd) { best = j; bd = d; }
    }
    return best;
}

/* greedy: spots in order, each to the nearest eligible player (mask); 1 + spot if it's player i */
static int assign(const SimState *s, int i, const fv2 *spot, int ns, const uint8_t *ok, fv2 *at) {
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    uint8_t used[WP_MAX_PLAYERS] = {0};
    for (int k = 0; k < ns; k++) {
        fv2 p = fv2_make(fx_clamp(spot[k].x, -hl + FX(1.0), hl - FX(1.0)), fx_clamp(spot[k].y, -hw + FX(1.0), hw - FX(1.0)));
        int best = -1; fx bd = 0;
        for (int j = 0; j < 2 * s->n_per_side; j++) {
            if (!ok[j] || used[j]) continue;
            fx d = fv2_len(fv2_sub(s->players[j].pos, p));
            if (best < 0 || d < bd) { best = j; bd = d; }
        }
        if (best < 0) break;
        used[best] = 1;
        if (best == i) { *at = p; return 1; }
    }
    return 0;
}

/* free kicks near goal: the defending team's wall; penalties: everyone else along the edge of the box (D-040) */
static int foul_spot(const SimState *s, int i, fv2 *at) {
    int kind = s->restart_kind, def = 1 - s->restart_team, n = s->n_per_side;
    int live = s->phase == WP_PHASE_OUT || s->phase == WP_PHASE_SETUP || (s->phase == WP_PHASE_PLAY && s->restart_taker >= 0);
    if (!live) return 0;
    int taker = predicted_taker(s);
    fx hl = s->pitch.length / 2;
    fv2 goal = fv2_make(fx_mul(FX_INT(-team_dir(s, def)), hl), 0);   /* the defending team's goal */
    fv2 sp = s->restart_spot;
    uint8_t ok[WP_MAX_PLAYERS] = {0};
    fv2 spot[12];
    int ns = 0;
    if (kind == WP_RS_FREEKICK) {
        if (s->players[i].team != def || fv2_len(fv2_sub(goal, sp)) > fx_mul(s->pitch.length, T_AI_WALL_FRAC)) return 0;
        fv2 u = fv2_norm(fv2_sub(goal, sp)), side = fv2_make(-u.y, u.x);
        fv2 c = fv2_add(sp, fv2_scale(u, fx_mul(s->pitch.length, T_CORNER_DIST_FRAC) + FX(0.3)));
        int nw = n >= 7 ? 3 : 2;
        for (int k = 0; k < nw; k++) spot[ns++] = fv2_add(c, fv2_scale(side, FX_INT(2 * k - (nw - 1)) * 7 / 20));   /* 0.7 m apart */
        for (int j = def * n; j < (def + 1) * n; j++) ok[j] = s->players[j].role != WP_ROLE_KEEPER;
    } else {                                              /* penalty */
        if (i == taker || (s->players[i].role == WP_ROLE_KEEPER)) return 0;
        fx depth = fx_mul(s->pitch.length, FX(0.15)), bw = fx_mul(s->pitch.width, FX(0.24));
        fv2 u = fv2_make(FX_INT(team_dir(s, def)), 0);     /* out of their goal */
        fv2 e = fv2_add(goal, fv2_scale(u, depth + FX(1.5)));
        for (int k = 0; k < 2 * n - 3 && ns < 12; k++)    /* along the edge of the box, both sides of the D */
            spot[ns++] = fv2_add(e, fv2_make(0, -bw + fx_mul(bw * 2, FX_INT(k) / (2 * n - 3))));
        for (int j = 0; j < 2 * n; j++) ok[j] = j != taker && s->players[j].role != WP_ROLE_KEEPER;
        fx r = fx_mul(s->pitch.length, T_CORNER_DIST_FRAC) + FX(0.5);   /* and outside the D (9.15 m scaled) */
        for (int k = 0; k < ns; k++) {
            fv2 d = fv2_sub(spot[k], sp);
            fx l = fv2_len(d);
            if (l < r && l > FX(0.05)) spot[k] = fv2_add(sp, fv2_scale(d, fx_div(r, l)));
        }
    }
    return assign(s, i, spot, ns, ok, at);
}

int setpiece_spot(const SimState *s, int i, fv2 *at) {
    int kind = s->restart_kind;
    if (kind == WP_RS_FREEKICK || kind == WP_RS_PENALTY) return foul_spot(s, i, at);
    if (kind != WP_RS_THROW && kind != WP_RS_GOALKICK) return 0;
    int live = s->phase == WP_PHASE_OUT || s->phase == WP_PHASE_SETUP ||
               (s->phase == WP_PHASE_PLAY && s->restart_taker >= 0 && s->ball.owner == s->restart_taker);
    if (!live || s->players[i].team != s->restart_team || s->players[i].role == WP_ROLE_KEEPER) return 0;
    int taker = predicted_taker(s);
    if (i == taker) return 0;
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2, sg = FX_INT(team_dir(s, s->restart_team));
    fv2 sp = fv2_make(fx_clamp(s->restart_spot.x, -hl, hl), fx_clamp(s->restart_spot.y, -hw, hw));
    fv2 spot[5];
    int ns = 0;
    if (kind == WP_RS_THROW) {
        fx in = sp.y > 0 ? -FX_ONE : FX_ONE;              /* into the pitch */
        static const fx F[4][2] = {{FX(7.0), FX(3.0)}, {FX(1.0), FX(9.0)}, {-FX(7.0), FX(5.0)}, {FX(15.0), FX(4.0)}};
        for (int k = 0; k < 4; k++)                       /* down the line, inside, back, long down the line */
            spot[ns++] = fv2_make(sp.x + fx_mul(sg, F[k][0]), sp.y + fx_mul(in, F[k][1]));
    } else {
        fx depth = fx_mul(s->pitch.length, FX(0.15)), bw = fx_mul(s->pitch.width, FX(0.24));
        fx x0 = -fx_mul(sg, hl);                          /* our goal line */
        spot[ns++] = fv2_make(x0 + fx_mul(sg, fx_mul(depth, FX(0.5))), bw + FX(2.0));    /* split wide of the box */
        spot[ns++] = fv2_make(x0 + fx_mul(sg, fx_mul(depth, FX(0.5))), -bw - FX(2.0));
        spot[ns++] = fv2_make(x0 + fx_mul(sg, depth + FX(10.0)), hw - FX(4.0));        /* wide and higher */
        spot[ns++] = fv2_make(x0 + fx_mul(sg, depth + FX(10.0)), -hw + FX(4.0));
        spot[ns++] = fv2_make(x0 + fx_mul(sg, depth + FX(15.0)), 0);                   /* a midfielder showing */
    }
    int used[WP_MAX_PLAYERS] = {0};
    int n = s->n_per_side, t = s->restart_team;
    for (int k = 0; k < ns; k++) {
        fv2 p = fv2_make(fx_clamp(spot[k].x, -hl + FX(1.5), hl - FX(1.5)), fx_clamp(spot[k].y, -hw + FX(1.5), hw - FX(1.5)));
        int best = -1; fx bd = 0;
        for (int j = t * n; j < (t + 1) * n; j++) {
            if (j == taker || used[j] || s->players[j].role == WP_ROLE_KEEPER) continue;
            fx d = fv2_len(fv2_sub(s->players[j].pos, p));
            if (best < 0 || d < bd) { best = j; bd = d; }
        }
        if (best < 0) break;
        used[best] = 1;
        if (best == i) { *at = p; return 1; }
    }
    return 0;
}
