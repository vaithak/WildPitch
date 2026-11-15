/* Movement assist toward the ball (D-037, reports/Assisted movement toward the ball.md). Every scenario runs at
 * OFF and at a level; determinism makes the comparison exact. The contract: the human keeps whether to move,
 * how fast, sprint, every action and any direction outside the cone - only the last <= F of heading toward a ball
 * that is his is the game's. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

#define ME 4
static SimState s;

static void loose_ball_scene(fv2 ball, fv2 bvel, int level) {
    setup_empty(&s, ME);
    s.seat_player[0] = ME;
    s.players[ME].pos = fv2_make(0, 0);
    s.players[ME].facing = 0;
    s.ball.pos = ball; s.ball.vel = bvel; s.ball.owner = -1; s.ball.last_touch = -1;
    s.assist_level[0] = (uint8_t)level;
}

static fv2 dir_deg(int deg) { return fv2_from_angle((wp_angle)((deg % 360 + 360) % 360 * 65536 / 360), FX_ONE); }
static int8_t c127(fx v) { return (int8_t)fx_clamp(v * 127 / FX_ONE, -127, 127); }

/* run n ticks with a fixed stick; returns tick the ball was gained by ME (-1 never); records his path */
static int run(WpSeatInput in, int n, fv2 *path) {
    for (int t = 0; t < n; t++) {
        run_seat(&s, in, 1);
        if (path) path[t] = s.players[ME].pos;
        if (s.ball.owner == ME) return t;
    }
    return -1;
}

int main(void) {
    static fv2 pa[240], pb[240];
    /* 1. neutral stays still, every level */
    for (int lv = 1; lv < WP_ASSIST_LEVELS; lv++)
        for (int d = 2; d <= 6; d += 2) {
            loose_ball_scene(fv2_make(FX_INT(d), FX(0.5)), fv2_make(0, 0), WP_ASSIST_OFF);
            run(NOIN, 120, pa);
            loose_ball_scene(fv2_make(FX_INT(d), FX(0.5)), fv2_make(0, 0), lv);
            run(NOIN, 120, pb);
            int same = 1;
            for (int t = 0; t < 120; t++) same &= pa[t].x == pb[t].x && pa[t].y == pb[t].y;
            CHECKF(same, "neutral stick moved him (level %d, ball %d m)", lv, d);
        }
    /* 2. a stick 55 deg or more away from both the ball and the meeting point is untouched */
    static const int away[] = {56, 70, 90, 135, 180};
    for (int a = 0; a < 5; a++) {
        fv2 u = dir_deg(away[a]);
        WpSeatInput in = {c127(u.x), c127(u.y), 0};
        loose_ball_scene(fv2_make(FX(4.0), 0), fv2_make(0, 0), WP_ASSIST_OFF);
        run(in, 60, pa);
        loose_ball_scene(fv2_make(FX(4.0), 0), fv2_make(0, 0), WP_ASSIST_NORMAL);
        run(in, 60, pb);
        int same = 1;
        for (int t = 0; t < 60; t++) same &= pa[t].x == pb[t].x && pa[t].y == pb[t].y;
        CHECKF(same, "stick %d deg away was changed", away[a]);
    }
    /* 3. bounded: the assist never turns the stick more than F (+1 deg) - stationary and fast crossing balls */
    {
        int worst = 0, outside_turned = 0;
        for (int crossing = 0; crossing < 2; crossing++)
            for (int d = 2; d <= 6; d += 2)
                for (int deg = 0; deg <= 180; deg += 5) {
                    loose_ball_scene(fv2_make(FX_INT(d), crossing ? -FX(4.0) : 0), crossing ? fv2_make(0, FX(9.0)) : fv2_make(0, 0),
                                     WP_ASSIST_NORMAL);
                    fv2 u = dir_deg(deg);
                    WpSeatInput in = {c127(u.x), c127(u.y), 0}, out = in;
                    seats_chase_assist(&s, 0, &out);
                    int din = fx_atan2(FX_INT(in.move_y), FX_INT(in.move_x)), dout = fx_atan2(FX_INT(out.move_y), FX_INT(out.move_x));
                    int turn = (int16_t)(uint16_t)(dout - din);
                    if (turn < 0) turn = -turn;
                    int tdeg = turn * 360 / 65536;
                    if (tdeg > worst) worst = tdeg;
                    fv2 tb = fv2_norm(fv2_sub(s.ball.pos, s.players[ME].pos));
                    if (fv2_dot(u, tb) < FX(0.55) && !crossing && tdeg > 0) outside_turned++;
                }
        printf("bounded: largest turn %d deg (cap 30), turned outside the cone %d times\n", worst, outside_turned);
        CHECK(worst <= 31);
        CHECK(outside_turned == 0);
    }
    /* 4. keyboard 8-way onto a still ball at 2 / 3 / 5 m, bearings every 5 deg, the nearest of the 8 keys held */
    for (int d = 2; d <= 5; d += (d == 3 ? 2 : 1)) {
        int got[2] = {0, 0}, n = 0;
        for (int deg = 0; deg < 360; deg += 5) {
            fv2 bp = fv2_scale(dir_deg(deg), FX_INT(d));
            int key = ((deg + 22) / 45) * 45;              /* nearest 8-way key */
            fv2 u = dir_deg(key);
            WpSeatInput in = {(int8_t)(u.x > FX(0.38) ? 127 : u.x < -FX(0.38) ? -127 : 0), (int8_t)(u.y > FX(0.38) ? 127 : u.y < -FX(0.38) ? -127 : 0), 0};
            int limit = (int)(d * 60 / 6 + 30);
            for (int lv = 0; lv < 2; lv++) {
                loose_ball_scene(bp, fv2_make(0, 0), lv ? WP_ASSIST_NORMAL : WP_ASSIST_OFF);
                int t = run(in, limit, 0);
                if (t >= 0) got[lv]++;
            }
            n++;
        }
        printf("keyboard 8-way, ball %d m: gained OFF %d/%d, NORMAL %d/%d\n", d, got[0], n, got[1], n);
        CHECKF(got[1] == n, "ball %d m: normal gained %d/%d", d, got[1], n);
    }
    /* 5. not his ball: a team-mate much nearer it -> exactly the raw stick */
    {
        WpSeatInput in = {127, 0, 0};
        for (int lv = 0; lv < 2; lv++) {
            loose_ball_scene(fv2_make(FX(6.0), FX(1.5)), fv2_make(0, 0), lv ? WP_ASSIST_NORMAL : WP_ASSIST_OFF);
            s.players[3].pos = fv2_make(FX(6.3), FX(2.2));   /* a mate standing on it */
            run(in, 40, lv ? pb : pa);
        }
        int same = 1;
        for (int t = 0; t < 40; t++) same &= pa[t].x == pb[t].x && pa[t].y == pb[t].y;
        CHECKF(same, "pulled onto a team-mate's ball");
    }
    /* 6. never sprints for him: the assisted legs keep the human's buttons */
    {
        loose_ball_scene(fv2_make(FX(8.0), FX(2.0)), fv2_make(0, 0), WP_ASSIST_HIGH);
        WpSeatInput in = {127, 0, 0}, out = in;
        seats_chase_assist(&s, 0, &out);
        CHECK(out.buttons == 0 && (out.move_x != in.move_x || out.move_y != in.move_y));
    }
    /* 7. an AI team-mate's pass to the seat's man, stick released: he plays it (D-037 hand-off) */
    {
        int got = 0, n = 0;
        for (int sp = 0; sp < 6; sp++) {
            setup_empty(&s, ME);
            s.seat_player[0] = ME;
            s.assist_level[0] = WP_ASSIST_NORMAL;
            int mate = 3;
            s.players[mate].pos = fv2_make(-FX(10.0), FX_INT(sp * 2 - 5));
            s.players[ME].pos = fv2_make(FX(4.0), FX_INT(sp - 2));
            s.players[ME].vel = fv2_make(sp % 2 ? FX(4.0) : 0, 0);
            s.ball.pos = fv2_add(s.players[mate].pos, fv2_make(FX(0.4), 0)); s.ball.owner = (int8_t)mate; s.ball.last_touch = (int8_t)mate;
            fv2 d = fv2_sub(s.players[ME].pos, s.ball.pos);
            WpKick k = {0};
            k.kind = WP_KICK_PASS; k.dir = fx_atan2(d.y, d.x); k.to = ME + 1; k.windup = T_KICK_WINDUP_PASS; k.foot = 1;
            sim_player_kick(&s, mate, &k);
            int r = -1;
            for (int t = 0; t < 200 && r < 0; t++) { run_seat(&s, NOIN, 1); if (s.ball.owner >= 0 && s.ball.owner != mate) r = s.ball.owner; }
            n++; got += r == ME;
        }
        printf("AI pass to the human (stick released): received %d/%d\n", got, n);
        CHECK(got * 10 >= n * 9);
    }
    /* 8. the latch survives analogue jitter but not a real change */
    {
        setup_empty(&s, ME);
        s.seat_player[0] = ME;
        s.assist_level[0] = WP_ASSIST_NORMAL;
        s.ball.pos = fv2_make(FX(5.0), FX(3.0)); s.ball.owner = -1;
        s.latch_stick[0][0] = 100; s.latch_stick[0][1] = 0; s.latch_ticks[0] = T_SWITCH_LATCH_TICKS;
        run_seat(&s, (WpSeatInput){102, 1, 0}, 3);
        CHECK(s.latch_ticks[0] > 0);
        run_seat(&s, (WpSeatInput){0, 127, 0}, 1);
        CHECK(s.latch_ticks[0] == 0);
    }
    return TEST_DONE();
}
