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

int setpiece_spot(const SimState *s, int i, fv2 *at) {
    int kind = s->restart_kind;
    if (kind != WP_RS_THROW && kind != WP_RS_GOALKICK) return 0;
    int live = s->phase == WP_PHASE_OUT ||
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
