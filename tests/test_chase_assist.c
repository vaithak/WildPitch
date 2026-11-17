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

/* D-049 helpers: the stick at `deg_off` degrees from the bearing to `at`, and how far (deg, signed) the assist turned
 * it toward that point (positive = toward) */
static WpSeatInput stick_off(fv2 at, int deg_off) {
    fv2 to = fv2_sub(at, s.players[ME].pos);
    int bearing = (int)((int32_t)fx_atan2(to.y, to.x) * 360 / 65536);
    fv2 u = dir_deg(bearing + deg_off);
    WpSeatInput in = {c127(u.x), c127(u.y), 0};
    return in;
}
static int turned_deg(WpSeatInput in, WpSeatInput out) {
    int din = fx_atan2(FX_INT(in.move_y), FX_INT(in.move_x)), dout = fx_atan2(FX_INT(out.move_y), FX_INT(out.move_x));
    int t = (int16_t)(uint16_t)(dout - din);
    return t * 360 / 65536;
}
/* how close ME gets to `at` within n ticks with a fixed stick, at OFF (lv 0) or NORMAL (0 = he has the ball) */
static double dist_after(void (*scene)(int), WpSeatInput in, int n, fv2 at, int lv) {
    scene(lv ? WP_ASSIST_NORMAL : WP_ASSIST_OFF);
    double best = tod(fv2_len(fv2_sub(at, s.players[ME].pos)));
    for (int t = 0; t < n && s.ball.owner != ME; t++) {
        run_seat(&s, in, 1);
        double d = tod(fv2_len(fv2_sub(at, s.players[ME].pos)));
        if (d < best) best = d;
    }
    return s.ball.owner == ME ? 0 : best;
}
/* the D-049 scenes are played on the 80 x 52 m pitch with everyone else parked in the far corner (setup_empty's
 * "off the pitch" spot is clamped to the boards of the small pitch, close enough to take control by a switch) */
static void wide(void) {
    s.pitch = sim_config_7v7().pitch;
    for (int i = 0; i < 2 * s.n_per_side; i++) if (i != ME) s.players[i].pos = fv2_make(FX(28.0) + i * FX(1.0), FX(25.0));
}
static void scene_runaway(int lv) {                      /* a ball rolling away from him faster than he jogs */
    loose_ball_scene(fv2_make(FX(3.0), FX(1.0)), fv2_make(FX(12.0), FX(4.0)), lv);
    wide();
}
static void scene_slow_lob(int lv) {                     /* a 32 m lob (vz 15.5) that comes down 8 m from him in > 3 s */
    fx vz = FX(15.5);
    loose_ball_scene(fv2_make(-FX(18.0), FX(6.0)), fv2_make(ball_lob_speed_for(FX(32.0), vz), 0), lv);
    s.players[ME].pos = fv2_make(FX(9.0), 0);
    s.ball.z = FX(0.1); s.ball.vz = vz;
    wide();
}
static void scene_drop(int lv) {                         /* a ball dropping at chest height a few metres away */
    loose_ball_scene(fv2_make(-FX(9.0), FX(5.0)), fv2_make(FX(9.0), -FX(2.0)), lv);
    s.ball.z = FX(1.2); s.ball.vz = FX(4.0);
    wide();
}
static void scene_their_pass(int lv) {                   /* their pass crossing in front of him */
    loose_ball_scene(fv2_make(FX(5.0), -FX(9.0)), fv2_make(0, FX(8.0)), lv);
    s.pass_target = 8; s.pass_from = 7; s.pass_ticks = 1; s.ball.last_touch = 7;
    wide();
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
    /* 9. an opponent on the ball (CP0.3e): a stick 25 deg off him is turned toward him, 70 deg off is untouched */
    {
        int opp = 2 * 5 - 1;                              /* 5v5 regression: their striker */
        for (int c = 0; c < 2; c++) {
            loose_ball_scene(fv2_make(FX(6.0), 0), fv2_make(0, 0), WP_ASSIST_NORMAL);
            s.players[opp].pos = fv2_make(FX(6.3), 0); s.players[opp].vel = fv2_make(0, FX(3.0));
            s.ball.owner = (int8_t)opp; s.ball.pos = fv2_make(FX(6.0), 0);
            fv2 u = dir_deg(c ? -70 : 25);                 /* -70: away from where he is running */
            WpSeatInput in = {c127(u.x), c127(u.y), 0}, out = in;
            seats_chase_assist(&s, 0, &out);
            int turned = out.move_x != in.move_x || out.move_y != in.move_y;
            CHECKF(turned == (c == 0), "carrier press: stick %d deg turned %d", c ? -70 : 25, turned);
        }
    }
    /* 10. a slow pass coming in, stick held away for the last part: he still receives it first (CP0.3e) */
    {
        int got = 0, n = 0;
        for (int ang = 90; ang <= 180; ang += 45) {
            setup_empty(&s, ME);
            s.seat_player[0] = 3;
            s.assist_level[0] = WP_ASSIST_NORMAL;
            s.players[3].pos = fv2_make(-FX(6.0), 0);
            s.players[ME].pos = fv2_make(FX(3.0), 0);
            s.ball.pos = fv2_make(-FX(5.6), 0); s.ball.owner = 3; s.ball.last_touch = 3;
            WpKick k = {0};
            k.kind = WP_KICK_PASS; k.dir = 0; k.to = ME + 1; k.windup = T_KICK_WINDUP_PASS; k.foot = 1;
            sim_player_kick(&s, 3, &k);
            for (int t = 0; t < 12; t++) run_seat(&s, NOIN, 1);   /* struck, control handed to ME */
            fv2 u = dir_deg(ang);
            int r = -1;
            for (int t = 0; t < 120 && r < 0; t++) { run_seat(&s, (WpSeatInput){c127(u.x), c127(u.y), 0}, 1); if (s.ball.owner >= 0 && s.ball.owner != 3) r = s.ball.owner; }
            n++; got += r == ME;
        }
        printf("short pass, receiver's stick held away (90/135/180 deg): received %d/%d\n", got, n);
        CHECK(got == n);
    }
    /* ---- D-049 (CP0.5b, owner: "whenever the player being actively controlled is near the ball, always have some
     * system assist ... chasing a stray ball / defending / receiving a pass / through ball / loft ball") ---- */
    /* 11. near the ball, a team-mate quicker to it: assisted now (D-037 faded it to zero); far: still raw */
    {
        loose_ball_scene(fv2_make(FX(6.0), FX(1.5)), fv2_make(0, 0), WP_ASSIST_NORMAL);
        s.players[3].pos = fv2_make(FX(6.3), FX(2.2));     /* a mate standing on it */
        WpSeatInput in = stick_off(s.ball.pos, 25), out = in;
        seats_chase_assist(&s, 0, &out);
        int tn = turned_deg(in, out);
        loose_ball_scene(fv2_make(FX(16.0), FX(1.5)), fv2_make(0, 0), WP_ASSIST_NORMAL);
        s.players[3].pos = fv2_make(FX(16.3), FX(2.2));
        WpSeatInput fin = stick_off(s.ball.pos, 25), fout = fin;
        seats_chase_assist(&s, 0, &fout);
        printf("D-049 near, mate quicker: turned %d deg (6 m); far (16 m): turned %d deg\n", -tn, turned_deg(fin, fout));
        CHECKF(tn < -10 && tn >= -31, "near ball with a quicker mate: turned %d", tn);
        CHECK(fout.move_x == fin.move_x && fout.move_y == fin.move_y);
    }
    /* 12. a ball rolling away faster than he jogs (no reachable point at his pace in 4 s): toward where it ends up
     * (D-037 gave the raw stick) */
    {
        WpSeatInput in;
        scene_runaway(WP_ASSIST_NORMAL);
        in = stick_off(s.ball.pos, -30);
        WpSeatInput out = in;
        seats_chase_assist(&s, 0, &out);
        int tn = turned_deg(in, out);
        WpBall rest = s.ball;                             /* where it stops */
        for (int t = 0; t < 900 && (rest.vel.x || rest.vel.y); t++) ball_predict_tick(&s.pitch, &rest);
        fv2 end = rest.pos;
        double off = dist_after(scene_runaway, in, 90, end, 0), on = dist_after(scene_runaway, in, 90, end, 1);
        printf("D-049 ball running away: turned %d deg; closest to its stop %.1f m (OFF) vs %.1f m (NORMAL)\n", tn, off, on);
        CHECKF(tn > 5 && tn <= 31, "runaway ball: turned %d", tn);
        CHECK(on < off);
    }
    /* 13. a long lob coming down 8 m from him (contact plan from ~3 s out): steered onto it from the start */
    {
        scene_slow_lob(WP_ASSIST_NORMAL);
        WpBall fl = s.ball;                               /* where and when it first lands */
        int land = 0;
        for (fx pvz = fl.vz; land < 600; land++) {         /* the first bounce: vz flips up (or it rests) */
            ball_predict_tick(&s.pitch, &fl);
            if (fl.z == 0 || (pvz < 0 && fl.vz >= 0)) break;
            pvz = fl.vz;
        }
        WpSeatInput in = stick_off(fl.pos, 30), out = in;
        seats_chase_assist(&s, 0, &out);
        int tn = turned_deg(in, out);
        double off = dist_after(scene_slow_lob, in, land, fl.pos, 0), on = dist_after(scene_slow_lob, in, land, fl.pos, 1);
        printf("D-049 slow lob (lands in %.1f s, %.1f m away): turned %d deg; closest to it %.1f m (OFF) vs %.1f m (NORMAL)\n",
               land / 60.0, tod(fv2_len(fv2_sub(fl.pos, s.players[ME].pos))), -tn, off, on);
        CHECKF(tn < -10 && tn >= -31, "slow lob: turned %d", tn);
        CHECK(on < off);
    }
    /* 14. an aerial ball with a contact plan: steered to the plan's point (contact.c), and he gets there sooner */
    {
        scene_drop(WP_ASSIST_NORMAL);
        run_seat(&s, NOIN, 1);
        CHECK(s.players[ME].plan.kind != WP_CK_NONE);
        fv2 at = s.players[ME].plan.at;
        int left = (int16_t)(uint16_t)(s.players[ME].plan.tick - (uint16_t)s.tick);
        WpSeatInput in = stick_off(at, 35), out = in;
        seats_chase_assist(&s, 0, &out);
        int tn = turned_deg(in, out);
        WpSeatInput keys = stick_off(at, 35);
        double off = dist_after(scene_drop, keys, left, at, 0), on = dist_after(scene_drop, keys, left, at, 1);
        printf("D-049 aerial plan (%d ticks ahead): turned %d deg; closest to it %.2f m (OFF) vs %.2f m (NORMAL)\n",
               left, -tn, off, on);
        CHECKF(tn < -10 && tn >= -31, "aerial plan: turned %d", tn);
        CHECK(on < off);
    }
    /* 15. their pass in flight in front of him: steered onto its line, and he intercepts it */
    {
        WpSeatInput in;
        scene_their_pass(WP_ASSIST_NORMAL);
        in = stick_off(s.ball.pos, 50);
        int got[2];
        for (int lv = 0; lv < 2; lv++) {
            scene_their_pass(lv ? WP_ASSIST_NORMAL : WP_ASSIST_OFF);
            got[lv] = run(in, 120, 0) >= 0;
        }
        scene_their_pass(WP_ASSIST_NORMAL);
        s.players[3].pos = fv2_make(FX(5.0), -FX(1.0));    /* a team-mate right on its line: still assisted */
        WpSeatInput out = in;
        seats_chase_assist(&s, 0, &out);
        printf("D-049 their pass: intercepted OFF %d, NORMAL %d; with a mate on its line turned %d deg\n",
               got[0], got[1], turned_deg(in, out));
        CHECK(got[1] && !got[0]);
        CHECK(turned_deg(in, out) != 0);
    }
    /* 16. our pass to another team-mate: his ball only if he is quicker to it than the receiver */
    {
        for (int c = 0; c < 2; c++) {
            loose_ball_scene(fv2_make(FX(3.0), -FX(4.0)), fv2_make(0, FX(7.0)), WP_ASSIST_NORMAL);
            s.pass_target = 3; s.pass_from = 2; s.pass_ticks = 1; s.ball.last_touch = 2;
            s.players[3].pos = c ? fv2_make(FX(3.2), -FX(2.6)) : fv2_make(FX(3.0), FX(30.0));   /* receiver close / far */
            WpSeatInput in = stick_off(fv2_make(FX(3.0), FX(1.0)), 30), out = in;
            seats_chase_assist(&s, 0, &out);
            int turned = out.move_x != in.move_x || out.move_y != in.move_y;
            CHECKF(turned == (c == 0), "our pass to a mate (receiver %s): turned %d", c ? "close" : "far", turned);
        }
    }
    /* 17. the contract in every D-049 situation: neutral never moves him, the turn never exceeds F, never sprints */
    {
        void (*scenes[4])(int) = {scene_runaway, scene_slow_lob, scene_drop, scene_their_pass};
        int worst = 0, moved = 0, sprinted = 0;
        for (int sc = 0; sc < 4; sc++) {
            scenes[sc](WP_ASSIST_OFF);
            run(NOIN, 90, pa);
            scenes[sc](WP_ASSIST_NORMAL);
            run(NOIN, 90, pb);
            for (int t = 0; t < 90; t++) moved += pa[t].x != pb[t].x || pa[t].y != pb[t].y;
            for (int deg = 0; deg < 360; deg += 10) {
                scenes[sc](WP_ASSIST_NORMAL);
                run_seat(&s, NOIN, 1);
                fv2 u = dir_deg(deg);
                WpSeatInput in = {c127(u.x), c127(u.y), 0}, out = in;
                seats_chase_assist(&s, 0, &out);
                int tn = turned_deg(in, out);
                if (tn < 0) tn = -tn;
                if (tn > worst) worst = tn;
                sprinted += out.buttons != in.buttons;
            }
        }
        printf("D-049 contract: neutral moved %d ticks, largest turn %d deg (cap 30), sprint added %d\n", moved, worst, sprinted);
        CHECK(moved == 0 && worst <= 31 && sprinted == 0);
    }
    return TEST_DONE();
}
