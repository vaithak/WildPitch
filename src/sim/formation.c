/* formation.c - where each formation slot stands as a function of the ball (D-030). A parametric block: the back
 * line follows the ball's depth (clamped), the block has a length and width, its centre slides toward the ball's
 * side, and each slot sits at a depth fraction u and lateral fraction v inside it. Two shapes - in possession
 * (longer, wider) and out of possession (compact, shifted harder to the ball side) - blended by the plan's
 * poss_fade. Attack-direction terms, point-reflected for team 1, so both teams are mirror-fair by construction.
 * Research: reports/Team shape and collaborative AI.md (7v7 tracking: ~18x24 m with the ball, ~15x19 m without). */
#include "sim/sim_internal.h"

typedef struct {
    fx back_off, back_min, back_max;   /* back line x = clamp(ball_x - back_off, back_min, back_max) */
    fx len, wid;                       /* block length (back line to front slot), width */
    fx shift, shift_max;               /* lateral centre = clamp(shift * ball_y, +-shift_max) */
} Block;
typedef struct { Block in, out; const fx (*slot)[3]; fx ref_len, ref_wid; } Format;   /* slot: u, v, mindset;
                                                    metres are for a ref_len x ref_wid pitch and scale with the real one */

static const fx SLOT7[7][3] = {        /* keeper + 2-3-1 */
    {0, 0, 0},
    {0, -FX(0.4), 0}, {0, FX(0.4), 0},
    {FX(0.6), -FX(1.0), FX(0.5)}, {FX(0.45), 0, FX(0.25)}, {FX(0.6), FX(1.0), FX(0.5)},
    {FX(1.0), 0, FX(1.0)},
};
static const fx SLOT5[5][3] = {        /* keeper + 2-1-1 */
    {0, 0, 0},
    {0, -FX(0.5), 0}, {0, FX(0.5), 0},
    {FX(0.5), 0, FX(0.5)},
    {FX(1.0), 0, FX(1.0)},
};
static const Format F7 = {
    {FX(12.0), -FX(20.0), FX(8.0), FX(18.0), FX(24.0), FX(0.35), FX(4.0)},
    {FX(10.0), -FX(22.0), FX(2.0), FX(15.0), FX(19.0), FX(0.6), FX(7.0)}, SLOT7, FX(55.0), FX(35.0)};
static const Format F5 = {
    {FX(9.0), -FX(15.0), FX(5.0), FX(13.0), FX(17.0), FX(0.35), FX(3.0)},
    {FX(7.0), -FX(16.0), FX(1.0), FX(11.0), FX(14.0), FX(0.6), FX(5.0)}, SLOT5, FX(40.0), FX(25.0)};

static const Format *format_of(const SimState *s) {
    return s->n_per_side == 7 ? &F7 : s->n_per_side == 5 ? &F5 : 0;
}

fx slot_mindset(const SimState *s, int k) {
    const Format *f = format_of(s);
    if (!f || k >= s->n_per_side) return FX(0.5);
    return f->slot[k][2];
}

static fv2 block_spot(const Block *bl, const fx *slot, fx bx, fx by, fx sx, fx sy) {
    fx back = fx_clamp(bx - fx_mul(bl->back_off, sx), fx_mul(bl->back_min, sx), fx_mul(bl->back_max, sx));
    fx yc = fx_clamp(fx_mul(bl->shift, by), -fx_mul(bl->shift_max, sy), fx_mul(bl->shift_max, sy));
    return fv2_make(back + fx_mul(slot[0], fx_mul(bl->len, sx)), yc + fx_mul(slot[1], fx_mul(bl->wid, sy) / 2));
}

/* where slot k of `team` should stand now (world coordinates) */
fv2 shape_spot(const SimState *s, int team, int k) {
    const Format *f = format_of(s);
    fx sg = FX_INT(team_dir(s, team)), hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    if (!f) {                                             /* other formats: the kick-off shape shifted with the ball */
        fx fxl, fyl;
        formation_frac(s->n_per_side, k, &fxl, &fyl);
        fv2 b = s->plan[team].focus;
        return fv2_make(fx_mul(sg, fx_mul(fxl, hl) + fx_mul(fx_mul(b.x, sg), FX(0.5))), fx_mul(sg, fx_mul(fyl, hw)));
    }
    fv2 foc = s->plan[team].focus;
    fx bx = fx_mul(foc.x, sg), by = fx_mul(foc.y, sg);   /* ball in attack terms */
    fx sx = fx_div(s->pitch.length, f->ref_len), sy = fx_div(s->pitch.width, f->ref_wid);   /* pitch scale */
    fv2 a = block_spot(&f->in, f->slot[k], bx, by, sx, sy), d = block_spot(&f->out, f->slot[k], bx, by, sx, sy);
    fx w = s->plan[team].poss_fade;
    fv2 p = fv2_add(d, fv2_scale(fv2_sub(a, d), w));
    /* out of possession everyone but the striker gets goal-side of the ball (behind it), however deep it is */
    if (f->slot[k][2] < FX_ONE) {
        fx limit = bx - FX(1.5), over = fx_max(p.x - limit, 0);
        p.x -= fx_mul(over, FX_ONE - w);
    }
    if (s->rules & WP_RULE_OFFSIDE) {                     /* D-041: our men stay onside; the back line can hold */
        fx ol = offside_line(s, team) - T_ONSIDE_MARGIN;
        if (p.x > ol) p.x = ol;
    } else if (f->slot[k][2] == 0) {                      /* no offside (D-035): stay goal-side of their deepest man */
        fx deep = hl;
        int o0 = (1 - team) * s->n_per_side;
        for (int j = o0; j < o0 + s->n_per_side; j++)
            if (s->players[j].role != WP_ROLE_KEEPER) deep = fx_min(deep, fx_mul(s->players[j].pos.x, sg));
        fx line = fx_max(deep - T_SHAPE_LAST_GAP, -hl + T_SHAPE_LAST_MIN);
        if (p.x > line) p.x = line;
    }
    p.x = fx_clamp(p.x, -hl + FX(2.0), hl - FX(4.0));
    p.y = fx_clamp(p.y, -hw + FX(1.0), hw - FX(1.0));
    return fv2_make(fx_mul(p.x, sg), fx_mul(p.y, sg));
}
