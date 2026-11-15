/* Ball physics, goal frame, nets, goals and out-of-play. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

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

    /* rolling friction (owner: "a simple pass doesn't stop"): speed-dependent decel, so passes stop in sane
     * distances and fast balls don't glide for ever. Ground balls along y=8 from x=-18 (+x, away from goals). */
    {
        static const int speeds[] = {3, 6, 12, 22};
        static const double lo[] = {2.0, 5.5, 14.0, 26.0}, hi[] = {4.0, 9.5, 22.0, 38.0};
        for (int k = 0; k < 4; k++) {
            setup_empty(&s, -1); s.seat_player[0] = -1;
            launch(&s, FX(-18.0), FX(8.0), 0, FX_INT(speeds[k]), 0, 0);
            int t = 0;
            while ((s.ball.vel.x || s.ball.vel.y) && t < 1200) { run_seat(&s, NOIN, 1); t++; }
            double d = tod(s.ball.pos.x) + 18.0;
            printf("roll %2d m/s: stops after %.1f m, %.1f s\n", speeds[k], d, t / 60.0);
            CHECKF(d > lo[k] && d < hi[k], "%d m/s roll %.1f m (want %.0f..%.0f)", speeds[k], d, lo[k], hi[k]);
            CHECKF(t < 60 * 7, "%d m/s ball still rolling after %.1f s", speeds[k], t / 60.0);
        }
    }

    /* inverse roll model (stopping touches now, weighted passes in CP0.2) agrees with the forward sim */
    for (int d = 2; d <= 30; d += 7) {
        setup_empty(&s, -1); s.seat_player[0] = -1;
        launch(&s, FX(-18.0), FX(8.0), 0, ball_roll_speed_for(FX_INT(d)), 0, 0);
        run_seat(&s, NOIN, 600);
        double got = tod(s.ball.pos.x) + 18.0;
        CHECKF(got > d * 0.99 - 0.05 && got < d * 1.01 + 0.05, "roll_speed_for(%d m) rolled %.2f m", d, got);
    }

    /* roll-time table agrees with the forward sim (flight times for pass solving, D-020) */
    for (int v = 3; v <= 24; v += 7) {
        setup_empty(&s, -1); s.seat_player[0] = -1;
        launch(&s, FX(-18.0), FX(8.0), 0, FX_INT(v), 0, 0);
        int t = 0;
        while ((s.ball.vel.x || s.ball.vel.y) && t < 1200) { run_seat(&s, NOIN, 1); t++; }
        fx want = ball_roll_time(FX_INT(v));
        CHECKF(fx_abs(FX_INT(t) / 60 - want) < FX(0.05), "roll time %d m/s: sim %.2f s, table %.2f s", v, t / 60.0, tod(want));
        CHECKF(fx_abs(ball_roll_speed_after(FX_INT(v), ball_roll_distance(FX_INT(v)) / 2) - ball_roll_speed_for(ball_roll_distance(FX_INT(v)) / 2)) < FX(0.01), "speed_after");
    }

    /* grass bounce (D-016): a 2 m drop rebounds into FIFA's 0.6-1.0 m window */
    {
        setup_empty(&s, -1); s.seat_player[0] = -1;
        launch(&s, 0, FX(8.0), FX(2.0), 0, 0, 0);
        run_seat(&s, NOIN, 45);                                     /* falls 0.64 s, lands */
        fx peak = 0;
        for (int t = 0; t < 60; t++) { run_seat(&s, NOIN, 1); if (s.ball.z > peak) peak = s.ball.z; }
        printf("bounce: 2 m drop rebounds %.2f m\n", tod(peak));
        CHECKF(peak > FX(0.6) && peak < FX(1.0), "2 m drop rebound %.2f m", tod(peak));
    }
    /* landing friction is continuous: no cliff as the landing speed crosses any threshold (the old model lost 0 vs
     * 1.65 m/s either side of 0.6 m/s). Small dips (<0.05) are real: a ball that bounces skips the rest-of-tick roll. */
    {
        fx prev_loss = -1, max_jump = 0;
        for (int k = 30; k <= 100; k++) {
            setup_empty(&s, -1); s.seat_player[0] = -1;
            launch(&s, 0, FX(8.0), FX(0.001), FX(11.0), 0, -(FX_INT(k) / 100));
            fx v0 = s.ball.vel.x;
            run_seat(&s, NOIN, 1);
            fx loss = v0 - s.ball.vel.x;
            if (prev_loss >= 0 && fx_abs(loss - prev_loss) > max_jump) max_jump = fx_abs(loss - prev_loss);
            CHECKF(prev_loss < 0 || loss >= prev_loss - FX(0.05), "landing loss not monotonic at vz -%d cm/s", k);
            prev_loss = loss;
        }
        CHECKF(max_jump < FX(0.06), "landing loss jumps %.3f m/s between 1 cm/s steps", tod(max_jump));
    }
    /* full-power chip sits down: first bounce ~21 m, stops well short of rolling the length of the pitch */
    {
        setup_empty(&s, -1); s.seat_player[0] = -1;
        launch(&s, FX(-19.0), FX(8.0), 0, FX(15.0), 0, FX(8.5));
        int t = 0;
        while ((s.ball.vel.x || s.ball.vz || s.ball.z) && t < 1200 && s.phase == WP_PHASE_PLAY) { run_seat(&s, NOIN, 1); t++; }
        double d = tod(s.ball.pos.x) + 19.0;
        printf("full chip stops after %.1f m\n", d);
        CHECKF(d > 24.0 && d < 36.0, "full chip stops at %.1f m", d);
    }

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

    /* over the bar: out, not a goal; a goal kick after the pause */
    setup_empty(&s, -1); s.seat_player[0] = -1;
    launch(&s, FX(15.0), 0, FX(2.6), FX(20.0), 0, FX(1.0));
    ev = run_seat(&s, NOIN, 40);
    CHECK(!(ev & WP_EV_GOAL) && (ev & WP_EV_OUT));
    CHECK(s.score[0] == 0);
    run_seat(&s, NOIN, T_OUT_PAUSE_TICKS);
    /* D-029: over their bar, last touched by us -> GOAL KICK for them, taken by their keeper at the spot */
    CHECKF(s.phase == WP_PHASE_PLAY && s.restart_kind == WP_RS_GOALKICK && s.restart_team == 1 &&
           s.ball.owner == s.n_per_side && fx_abs(s.ball.pos.x - (hl - T_GOALKICK_DEPTH)) < FX(0.5) &&
           fx_abs(s.ball.vel.x) < FX(0.1), "phase %d x %.2f kind %d taker %d", s.phase, tod(s.ball.pos.x), s.restart_kind, s.restart_taker);

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
