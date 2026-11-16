/* clock.c - the match clock (D-036). Two halves of half_ticks each (the game: 5 real minutes = 45' at 9x); the
 * clock runs through goals and restarts like a real one; goals and set pieces add stoppage time, announced (whole
 * minutes, 1..T_STOPPAGE_MAX_MIN) when 45' is reached; the half ends at the first live moment after it runs out.
 * Half time: a pause, then the teams change ends and the team that didn't start the match kicks off. half_ticks = 0
 * = an endless match (tests, drills): nothing here runs and nothing extra is hashed. */
#include "sim/sim_internal.h"

static uint32_t game_min_ticks(const SimState *s) { return s->half_ticks / 45 ? s->half_ticks / 45 : 1; }

void clock_add_stoppage(SimState *s, int game_seconds) {
    if (!s->half_ticks || s->clock >= s->half_ticks) return;   /* only regulation time collects it */
    uint32_t add = game_min_ticks(s) * (uint32_t)game_seconds / 60, acc = s->stop_acc + add;
    s->stop_acc = (uint16_t)(acc > 65535 ? 65535 : acc);
}

void clock_step(SimState *s) {
    if (!s->half_ticks) return;
    if (s->phase == WP_PHASE_HALFTIME || s->phase == WP_PHASE_FULLTIME) {
        if (s->phase == WP_PHASE_FULLTIME || !s->phase_timer || --s->phase_timer) return;
        s->half = 1;                                      /* change ends, the other team kicks off */
        s->kickoff_team = (uint8_t)(1 - s->first_kickoff);
        s->clock = 0; s->stoppage = 0; s->stop_acc = 0;
        match_place_kickoff(s);
        sim_kickoff_setpiece(s);
        s->phase = WP_PHASE_PLAY;
        return;
    }
    s->clock++;
    if (s->clock == s->half_ticks) {                      /* 45': the board goes up */
        uint32_t m = game_min_ticks(s), mins = (s->stop_acc + m - 1) / m;
        if (mins < 1) mins = 1;
        if (mins > T_STOPPAGE_MAX_MIN) mins = T_STOPPAGE_MAX_MIN;
        s->stoppage = (uint16_t)(mins * m);
    }
    if (s->clock >= s->half_ticks + s->stoppage && s->clock > s->half_ticks && s->phase == WP_PHASE_PLAY &&
        !s->restart_kind && !s->adv_ticks) {         /* never in the middle of a set piece / advantage */
        s->events |= WP_EV_WHISTLE;
        s->restart_kind = 0; s->restart_taker = -1; s->pass_target = -1;
        s->ball.owner = -1; s->ball.held = 0;
        s->ball.vel = fv2_make(0, 0); s->ball.vz = 0;
        if (s->half == 0) { s->phase = WP_PHASE_HALFTIME; s->phase_timer = T_HALFTIME_TICKS; }
        else s->phase = WP_PHASE_FULLTIME;
    }
}

void sim_clock_display(const SimState *s, int *minutes, int *seconds, int *added_min, int *extra) {
    uint32_t ht = s->half_ticks ? s->half_ticks : 1;
    uint32_t c = s->clock < ht ? s->clock : ht, over = s->clock > ht ? s->clock - ht : 0;
    uint32_t secs = (uint32_t)((uint64_t)c * 2700u / ht) + (s->half ? 2700u : 0u);
    uint32_t xs = (uint32_t)((uint64_t)over * 2700u / ht);
    *minutes = (int)(secs / 60); *seconds = (int)(secs % 60);
    *extra = (int)xs;                                     /* seconds into stoppage time */
    *added_min = s->stoppage ? (int)(s->stoppage / game_min_ticks(s)) : 0;
}
