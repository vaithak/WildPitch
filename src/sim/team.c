/* team.c - PLACEHOLDER movement for players no human controls (CP0.2; replaced by the instinct AI in CP0.3, D-008).
 * Produces a WpSeatInput per player so AI movement goes through exactly the same physics as humans (move()).
 *  - Team-mates of a human team: hold the 2-1-1 shape shifted with the ball; in possession the striker runs into
 *    space beyond the ball, the midfielder offers a side option, everyone keeps clear of the carrier.
 *  - Teams with no human ("cones that move a bit"): shuffle with the ball at half pace, never onto it.
 *  - Keepers: stay on the line, shading toward the ball side. No RNG: pure function of the state. */
#include "sim/sim_internal.h"


void team_steer(const WpPlayer *p, fv2 target, fx deadzone, fx slow_radius, WpSeatInput *out) {
    fv2 d = fv2_sub(target, p->pos);
    fx dist = fv2_len(d);
    out->move_x = out->move_y = 0;
    out->buttons = 0;
    if (dist <= deadzone) return;
    fx mag = fx_min(fx_div(dist, slow_radius), FX_ONE);
    fv2 v = fv2_scale(d, fx_div(fx_mul(mag, FX(127.0)), dist));
    out->move_x = (int8_t)fx_clamp(v.x / FX_ONE, -127, 127);
    out->move_y = (int8_t)fx_clamp(v.y / FX_ONE, -127, 127);
}

void team_steer_speed(const SimState *s, const WpPlayer *p, fv2 target, fx speed, WpSeatInput *out) {
    int i = (int)(p - s->players);
    fx walk = player_top_speed(s, i, 0), spr = player_top_speed(s, i, 1);
    fv2 d = fv2_sub(target, p->pos);
    fx dist = fv2_len(d);
    out->move_x = out->move_y = 0;
    out->buttons = 0;
    if (dist < FX(0.05) || speed <= 0) return;
    int sprint = speed > walk;
    fx mag = fx_min(fx_div(speed, sprint ? spr : walk), FX_ONE);
    fv2 v = fv2_scale(d, fx_div(fx_mul(mag, FX(127.0)), dist));
    out->move_x = (int8_t)fx_clamp(v.x / FX_ONE, -127, 127);
    out->move_y = (int8_t)fx_clamp(v.y / FX_ONE, -127, 127);
    if (!out->move_x && !out->move_y) out->move_x = (int8_t)(d.x > 0 ? 1 : -1);
    if (sprint) out->buttons = WP_BTN_SPRINT;
}

static int team_has_seat(const SimState *s, int team) {
    for (int k = 0; k < WP_MAX_SEATS; k++)
        if (s->seat_player[k] >= 0 && s->players[s->seat_player[k]].team == team) return 1;
    return 0;
}

static int possession_team(const SimState *s) {
    const WpBall *b = &s->ball;
    int who = b->owner >= 0 ? b->owner : (s->pass_target >= 0 ? s->pass_from : b->last_touch);
    return who >= 0 ? s->players[who].team : -1;
}

static fv2 keep_off(fv2 t, fv2 ball, fx r) {          /* push a target out of a circle around the ball */
    fv2 d = fv2_sub(t, ball);
    fx l = fv2_len(d);
    if (l >= r) return t;
    if (l < FX(0.05)) d = fv2_make(0, FX_ONE), l = FX_ONE;
    return fv2_add(ball, fv2_scale(d, fx_div(r, l)));
}

void team_input(const SimState *s, int i, WpSeatInput *out) {
    const WpPlayer *p = &s->players[i];
    const WpBall *b = &s->ball;
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    int team = p->team, k = i % s->n_per_side;
    fx sg = FX_INT(team_dir(s, team));                     /* attacking direction along x */
    if (s->phase != WP_PHASE_PLAY) { team_steer(p, p->pos, FX_ONE, FX_ONE, out); return; }
    if (b->owner == i) {                                   /* placeholder on the ball: carry it toward goal */
        team_steer(p, fv2_make(fx_mul(sg, hl), 0), FX(0.3), FX(2.0), out);
        return;
    }
    if (p->role == WP_ROLE_KEEPER) {
        fv2 t = fv2_make(-fx_mul(sg, hl - FX(0.8)), fx_clamp(fx_mul(b->pos.y, FX(0.25)), -FX(1.4), FX(1.4)));
        team_steer(p, t, FX(0.15), FX(1.0), out);
        return;
    }
    fx fxl, fyl;
    formation_frac(s->n_per_side, k, &fxl, &fyl);
    fx bx = fx_mul(fx_mul(fxl, hl), sg), by = fx_mul(fx_mul(fyl, hw), sg);
    int human_team = team_has_seat(s, team), attacking = possession_team(s) == team;
    fv2 t;
    if (!human_team) {                                     /* passive opposition */
        t = fv2_make(bx + fx_mul(b->pos.x, FX(0.3)), by + fx_mul(b->pos.y, FX(0.2)));
        t = keep_off(t, b->pos, FX(2.0));
    } else if (attacking) {
        if (k == 4)      t = fv2_make(b->pos.x + fx_mul(sg, FX(7.0)), b->pos.y > 0 ? -FX(3.5) : FX(3.5));
        else if (k == 3) t = fv2_make(b->pos.x - fx_mul(sg, FX(2.5)), b->pos.y > 0 ? b->pos.y - FX(6.0) : b->pos.y + FX(6.0));
        else             t = fv2_make(bx + fx_mul(b->pos.x, FX(0.6)) + fx_mul(sg, FX(3.0)), fx_mul(by, FX(1.15)) + fx_mul(b->pos.y, FX(0.2)));
        t = keep_off(t, b->pos, FX(3.5));
    } else {
        t = fv2_make(bx + fx_mul(b->pos.x, FX(0.5)) - sg, fx_mul(by, FX(0.8)) + fx_mul(b->pos.y, FX(0.35)));
        t = keep_off(t, b->pos, FX(1.5));
    }
    t.x = fx_clamp(t.x, -(hl - FX(1.5)), hl - FX(1.5));
    t.y = fx_clamp(t.y, -(hw - FX(1.0)), hw - FX(1.0));
    team_steer(p, t, FX(0.4), FX(2.0), out);
    if (!human_team) {                                     /* half pace */
        out->move_x = (int8_t)(out->move_x / 2);
        out->move_y = (int8_t)(out->move_y / 2);
    } else if (fv2_len(fv2_sub(t, p->pos)) > FX(7.0)) {
        out->buttons = WP_BTN_SPRINT;
    }
}
