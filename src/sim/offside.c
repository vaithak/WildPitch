/* offside.c - Law 11 (D-041). When a team plays the ball (any kick or header), its players then in an offside
 * position - in the opponents' half, beyond the ball and the second-last opponent (level is onside) - are flagged.
 * If a flagged player is the next to play the ball before an opponent does, offside: an indirect free kick to the
 * other team where he played it. Throw-ins, goal kicks and corners can't be offside. The AI uses offside_line()
 * to hold its runs and never passes to a man beyond it. */
#include "sim/sim_internal.h"

/* the offside line for `team` in its attacking frame (x * team_dir): max(second-last opponent, ball, halfway) */
fx offside_line(const SimState *s, int team) {
    if (!(s->rules & WP_RULE_OFFSIDE)) return FX(9999.0);   /* no offside: no line to hold */
    int sg = team_dir(s, team), o0 = (1 - team) * s->n_per_side;
    fx first = -FX(999.0), second = -FX(999.0);
    for (int j = o0; j < o0 + s->n_per_side; j++) {
        fx x = s->players[j].pos.x * sg;
        if (x > first) { second = first; first = x; }
        else if (x > second) second = x;
    }
    return fx_max(fx_max(second, s->ball.pos.x * sg), 0);
}

int offside_position(const SimState *s, int j) {
    if (!(s->rules & WP_RULE_OFFSIDE)) return 0;
    return s->players[j].pos.x * team_dir(s, s->players[j].team) > offside_line(s, s->players[j].team);
}

void offside_on_kick(SimState *s, int kicker) {
    int team = s->players[kicker].team;
    s->offside_mask = 0;
    if (!(s->rules & WP_RULE_OFFSIDE)) return;
    if (restart_is_taker(s, kicker) && (s->restart_kind == WP_RS_THROW || s->restart_kind == WP_RS_GOALKICK ||
                                        s->restart_kind == WP_RS_CORNER)) return;   /* no offside from these */
    s->offside_team = (uint8_t)team;
    s->offside_from = (int8_t)kicker;
    for (int j = team * s->n_per_side; j < (team + 1) * s->n_per_side; j++)
        if (j != kicker && offside_position(s, j)) s->offside_mask = (uint16_t)(s->offside_mask | (1u << j));
}

void offside_step(SimState *s) {
    if (!s->offside_mask) return;
    if (s->phase != WP_PHASE_PLAY) { s->offside_mask = 0; return; }
    int l = s->ball.last_touch;
    if (l < 0) return;
    if (s->players[l].team != s->offside_team) { s->offside_mask = 0; return; }   /* they played it: reset */
    if (!(s->offside_mask & (1u << l))) {                 /* an onside team-mate (or the passer again) plays it: a new
                                                           * phase - the old flags no longer count */
        if (l != s->offside_from || ((s->events & (WP_EV_TOUCH | WP_EV_BODY)) && !(s->events & WP_EV_KICK)))
            s->offside_mask = 0;                          /* (not the pass's own tick: its gather touch) */
        return;
    }
    {
        s->offside_mask = 0;
        restart_foul(s, 1 - s->offside_team, s->players[l].pos, 1);
    }
}
