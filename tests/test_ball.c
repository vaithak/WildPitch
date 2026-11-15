/* Ball physics, goal frame, nets, goals and out-of-play. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"

static void launch(SimState *s, fx x, fx y, fx z, fx vx, fx vy, fx vz) {
    s->ball.pos = fv2_make(x, y); s->ball.z = z;
    s->ball.vel = fv2_make(vx, vy); s->ball.vz = vz; s->ball.spin = 0; s->ball.owner = -1;
}

int main(void) {
    static SimState s;
    fx hl = FX(20.0), gw2 = FX(2.0), gh = FX(2.0);

    /* curl: positive spin bends a +x ball toward +y */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    launch(&s, FX(-15.0), 0, FX(0.2), FX(25.0), 0, FX(3.0)); s.ball.spin = FX(3.0);
    run_seat(&s, NOIN, 60);
    CHECKF(s.ball.pos.y > FX(0.6), "curl y=%.2f", tod(s.ball.pos.y));

    /* full-power ground shots dead at a post from several distances: must bounce out, never tunnel/score */
    for (int dist = 3; dist <= 15; dist += 6)
        for (int side = -1; side <= 1; side += 2) {
            setup_empty(&s, -1); s.seat_player[0] = -1;
            fx py = side * (gw2 + T_POST_R);
            launch(&s, hl - FX_INT(dist), py, 0, FX(30.0), 0, 0);
            uint16_t ev = run_seat(&s, NOIN, 40);
            CHECKF(ev & WP_EV_POST, "post dist %d side %d: no post event", dist, side);
            CHECKF(s.score[0] == 0, "post dist %d side %d: scored through the post", dist, side);
        }
    /* crossbar */
    for (int dist = 3; dist <= 9; dist += 6) {
        setup_empty(&s, -1); s.seat_player[0] = -1;
        launch(&s, hl - FX_INT(dist), 0, gh + T_POST_R + FX(0.05) + (dist > 3 ? FX(0.4) : 0), FX(30.0), 0, 0);
        uint16_t ev = run_seat(&s, NOIN, 40);
        CHECKF(ev & WP_EV_POST, "bar dist %d: no post event (z=%.2f)", dist, tod(s.ball.z));
        CHECK(s.score[0] == 0);
    }

    /* goal at +x end: team 0 scores; ball stays in the net; celebration then kickoff reset */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    s.players[3].z = FX(0.3); s.players[3].vz = FX(1.0); s.players[3].action = WP_ACT_JUMP_HEADER;  /* mid-air at the goal */
    launch(&s, FX(14.0), FX(0.5), 0, FX(25.0), 0, FX(1.0));
    uint16_t ev = run_seat(&s, NOIN, 60);
    CHECK(ev & WP_EV_GOAL);
    CHECK(s.score[0] == 1 && s.score[1] == 0);
    CHECK(s.phase == WP_PHASE_GOAL);
    CHECK(ev & WP_EV_NET);
    CHECKF(s.ball.pos.x > hl && s.ball.pos.x < hl + FX(1.2) && fx_abs(s.ball.pos.y) < gw2,
           "ball escaped the net: %.2f %.2f", tod(s.ball.pos.x), tod(s.ball.pos.y));
    ev = run_seat(&s, NOIN, T_GOAL_PAUSE_TICKS);
    CHECK(ev & WP_EV_RESET);
    CHECK(s.phase == WP_PHASE_PLAY);
    CHECK(s.ball.pos.x == 0 && s.ball.pos.y == 0 && s.ball.vel.x == 0);
    CHECK(s.kickoff_team == 1);
    for (int i = 0; i < 10; i++) CHECK(s.players[i].z == 0 && s.players[i].action == WP_ACT_NONE);

    /* goal at -x end: team 1 scores */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    launch(&s, -FX(14.0), -FX(1.0), 0, -FX(20.0), 0, 0);
    run_seat(&s, NOIN, 60);
    CHECK(s.score[1] == 1 && s.score[0] == 0);

    /* over the bar: out, not a goal; ball returns inside after the pause */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    launch(&s, FX(15.0), 0, FX(2.6), FX(20.0), 0, FX(1.0));
    ev = run_seat(&s, NOIN, 40);
    CHECK(!(ev & WP_EV_GOAL) && (ev & WP_EV_OUT));
    CHECK(s.score[0] == 0);
    run_seat(&s, NOIN, T_OUT_PAUSE_TICKS);
    CHECK(s.phase == WP_PHASE_PLAY && fx_abs(s.ball.pos.x) < hl && s.ball.vel.x == 0);

    /* wide of the post: out, not a goal */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    launch(&s, FX(15.0), FX(3.0), 0, FX(20.0), 0, 0);
    ev = run_seat(&s, NOIN, 40);
    CHECK(!(ev & WP_EV_GOAL) && (ev & WP_EV_OUT));

    /* touchline */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    launch(&s, 0, FX(10.0), 0, 0, FX(10.0), 0);
    ev = run_seat(&s, NOIN, 40);
    CHECK(ev & WP_EV_OUT);

    /* a 30 m/s shot straight at a (non-controlling) body bounces off; never passes through */
    setup_empty(&s, 2); s.seat_player[0] = -1;
    s.players[2].pos = fv2_make(FX(5.0), 0);
    launch(&s, -FX(5.0), 0, FX(0.3), FX(30.0), 0, 0);
    ev = run_seat(&s, NOIN, 30);
    CHECK(ev & WP_EV_BODY);
    CHECKF(s.ball.pos.x < FX(5.0), "ball tunnelled through body: x=%.2f", tod(s.ball.pos.x));
    return TEST_DONE();
}
