/* Match clock (D-036): 9x clock, two halves, stoppage time, half time with change of ends, full time. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

static SimState s;

/* roll the ball into the +x goal; returns the team credited */
static int score_at_plus_x(void) {
    int before0 = s.score[0], before1 = s.score[1];
    s.restart_kind = 0; s.restart_taker = -1;
    s.ball.owner = -1; s.ball.held = 0;
    s.ball.pos = fv2_make(s.pitch.length / 2 - FX(2.0), 0); s.ball.z = 0;
    s.ball.vel = fv2_make(FX(15.0), 0); s.ball.vz = 0;
    for (int k = 0; k < WP_MAX_PLAYERS; k++) s.players[k].pos = fv2_make(-FX(20.0), FX_INT(k) - FX(7.0));
    WpInput in; memset(&in, 0, sizeof in);
    for (int t = 0; t < 30 && s.phase == WP_PHASE_PLAY; t++) sim_step(&s, &in);
    return s.score[0] > before0 ? 0 : s.score[1] > before1 ? 1 : -1;
}

int main(void) {
    WpMatchConfig cfg = sim_config_7v7();
    CHECK(cfg.half_ticks == T_HALF_TICKS && T_HALF_TICKS == 5 * 60 * WP_TICK_HZ);   /* 5 real minutes = 45' (9x) */
    sim_init(&s, &cfg);
    sim_kickoff_setpiece(&s);
    for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
    CHECK(team_dir(&s, 0) == 1 && s.kickoff_team == 0);
    WpInput in; memset(&in, 0, sizeof in);
    int ht_tick = -1, ft_tick = -1, second_ko = -1, prev_shown = -1, mono = 1, added1 = 0, added2 = 0, red_keeper_side = 0;
    for (long t = 0; t < 60L * 60 * 14 && ft_tick < 0; t++) {
        sim_step(&s, &in);
        int mm, ss, ad, ex;
        sim_clock_display(&s, &mm, &ss, &ad, &ex);
        int shown = mm * 60 + ss + ex;
        if (s.phase != WP_PHASE_HALFTIME && shown < prev_shown && !(s.half == 1 && prev_shown >= 2700 && shown == 2700)) mono = 0;
        prev_shown = shown;
        if (s.phase == WP_PHASE_HALFTIME && ht_tick < 0) { ht_tick = (int)t; added1 = s.stoppage; }
        if (ht_tick >= 0 && second_ko < 0 && s.half == 1) {
            second_ko = s.kickoff_team;
            red_keeper_side = s.players[0].pos.x > 0 ? 1 : -1;
        }
        if (s.phase == WP_PHASE_FULLTIME) { ft_tick = (int)t; added2 = s.stoppage; }
    }
    int m1 = added1 / (int)(T_HALF_TICKS / 45), m2 = added2 / (int)(T_HALF_TICKS / 45);
    printf("half time at %.1f s (+%d'), full time at %.1f s (+%d'), 2nd-half kick-off by team %d, red keeper at %+d x\n",
           ht_tick / 60.0, m1, ft_tick / 60.0, m2, second_ko, red_keeper_side);
    CHECK(ht_tick >= T_HALF_TICKS + T_HALF_TICKS / 45 && ht_tick <= T_HALF_TICKS + 5 * (T_HALF_TICKS / 45));
    CHECK(m1 >= 1 && m1 <= T_STOPPAGE_MAX_MIN && m2 >= 1 && m2 <= T_STOPPAGE_MAX_MIN);
    CHECK(ft_tick > ht_tick + T_HALFTIME_TICKS + T_HALF_TICKS);
    CHECK(second_ko == 1);                                 /* the other team starts the second half */
    CHECK(red_keeper_side == 1);                           /* ends changed: red now defends +x */
    CHECK(mono);
    /* full time is final: nothing moves the clock or the score */
    uint32_t c = s.clock; int sc0 = s.score[0], sc1 = s.score[1];
    for (int t = 0; t < 600; t++) sim_step(&s, &in);
    CHECK(s.phase == WP_PHASE_FULLTIME && s.clock == c && s.score[0] == sc0 && s.score[1] == sc1);
    /* the +x goal belongs to team 1 in the first half and to team 0 in the second */
    sim_init(&s, &cfg);
    for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
    CHECK(score_at_plus_x() == 0);
    sim_init(&s, &cfg);
    for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
    s.half = 1;
    match_place_kickoff(&s);
    CHECK(score_at_plus_x() == 1);
    /* stoppage collects from goals and restarts, regulation time only */
    sim_init(&s, &cfg);
    clock_add_stoppage(&s, T_STOPPAGE_GOAL_S);
    CHECK(s.stop_acc == (uint16_t)(T_HALF_TICKS / 45 * T_STOPPAGE_GOAL_S / 60));
    /* endless matches (tests, drills) have no clock */
    WpMatchConfig d = sim_default_config();
    sim_init(&s, &d);
    for (int t = 0; t < 600; t++) sim_step(&s, &in);
    CHECK(s.clock == 0 && s.half_ticks == 0);
    return TEST_DONE();
}
