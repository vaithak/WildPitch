/* CP0.2a: targeted + weighted ground passes, hand-off of control to the receiver, switching rules. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

#define ME 4      /* team 0 striker (seat 0) */
#define MATE 3    /* team 0 midfielder */
#define MATE2 2

static fx dist_of(const SimState *s, int i) { return fv2_len(fv2_sub(s->ball.pos, s->players[i].pos)); }

/* me at `at` with the ball, team-mates where given, everyone else parked off the pitch as statues */
static void scene(SimState *s, fv2 at, fv2 mate, fv2 mate2) {
    setup_empty(s, ME);
    s->seat_player[0] = ME;
    s->players[ME].pos = at;
    s->players[MATE].pos = mate;
    s->players[MATE2].pos = mate2;
    s->ball.pos = fv2_add(at, fv2_make(FX(0.4), 0));
    run_seat(s, NOIN, 5);
}

int main(void) {
    static SimState s;

    /* 1. pass to a team-mate from standing / jogging / sprinting sideways / sprinting away: control moves to him on
     *    the contact tick, the ball reaches him at a controllable pace, he gets it */
    static const struct { const char *name; int8_t rx, ry; uint16_t b; int run; } runs[] = {
        {"standing", 0, 0, 0, 0}, {"jogging +x", 127, 0, 0, 40}, {"sprint sideways", 0, 127, WP_BTN_SPRINT, 50},
        {"sprint away", -127, 0, WP_BTN_SPRINT, 50},
    };
    for (int r = 0; r < 4; r++) {
        for (int far = 0; far <= 1; far++) {
            fx md = far ? FX(18.0) : FX(8.0);
            scene(&s, fv2_make(-FX(8.0), 0), fv2_make(-FX(8.0) + md, FX(2.0)), fv2_make(-FX(8.0), -FX(9.0)));
            if (runs[r].run) run_seat(&s, (WpSeatInput){runs[r].rx, runs[r].ry, runs[r].b}, runs[r].run);
            if (s.ball.owner != ME) { CHECKF(0, "%s: lost ball before passing", runs[r].name); continue; }
            /* stick toward the mate */
            fv2 to = fv2_norm(fv2_sub(s.players[MATE].pos, s.players[ME].pos));
            int8_t sx = (int8_t)(to.x * 127 / FX_ONE), sy = (int8_t)(to.y * 127 / FX_ONE);
            run_seat(&s, (WpSeatInput){sx, sy, (uint16_t)(runs[r].b | WP_BTN_PASS)}, 2);   /* tap J ... */
            run_seat(&s, (WpSeatInput){sx, sy, runs[r].b}, 1);                             /* ... release, stick held */
            int kick_t = -1, seat_before_kick_ok = 1, got = -1; fx arrive = 0;
            for (int t = 0; t < 300 && got < 0; t++) {
                int seat_before = s.seat_player[0];
                uint16_t ev = run_seat(&s, (WpSeatInput){0, 0, 0}, 1);
                if (ev & WP_EV_KICK) kick_t = t;
                if (kick_t < 0 && s.seat_player[0] != ME) seat_before_kick_ok = 0;
                if (kick_t >= 0 && s.ball.owner < 0 && dist_of(&s, MATE) < FX(1.6) && !arrive) arrive = fv2_len(s.ball.vel);
                if (kick_t >= 0 && s.ball.owner >= 0) got = s.ball.owner;
                (void)seat_before;
            }
            printf("pass %-16s %2d m: kicked t%d, arrives %.1f m/s, controlled by %d, seat -> %d\n", runs[r].name,
                   far ? 18 : 8, kick_t, tod(arrive), got, s.seat_player[0]);
            CHECKF(kick_t >= 0 && seat_before_kick_ok, "%s: kick %d, switched before contact %d", runs[r].name, kick_t, !seat_before_kick_ok);
            CHECKF(got == MATE, "%s %d m: ball ended with %d, not the receiver", runs[r].name, far ? 18 : 8, got);
            CHECKF(s.seat_player[0] == MATE, "%s: control didn't move to the receiver", runs[r].name);
            CHECKF(arrive > FX(3.0) && arrive < FX(10.0), "%s: arrival pace %.1f m/s", runs[r].name, tod(arrive));
        }
    }

    /* 2. the receiver's aim comes from the human's stick, not the assisted run: pass +x, keep holding +x, pass again
     *    on arrival -> the second pass goes on +x (to MATE2 beyond), not back toward the first passer */
    scene(&s, fv2_make(-FX(10.0), 0), fv2_make(-FX(1.0), 0), fv2_make(FX(9.0), FX(1.0)));
    run_seat(&s, (WpSeatInput){127, 0, WP_BTN_PASS}, 2);
    int second = 0, tapped = 0;
    for (int t = 0; t < 300 && !second; t++) {
        int mine = s.seat_player[0] == MATE && s.ball.owner == MATE && !tapped;
        if (mine) { run_seat(&s, (WpSeatInput){127, 0, WP_BTN_PASS}, 2); tapped = 1; }
        uint16_t ev = run_seat(&s, (WpSeatInput){127, 0, 0}, 1);
        if ((ev & WP_EV_KICK) && s.ball.last_touch == MATE) second = 1;
    }
    CHECKF(second && s.ball.vel.x > 0, "one-two: second pass went %.1f m/s in x (want +x)", tod(s.ball.vel.x));
    CHECK(s.seat_player[0] == MATE2);

    /* 3. never switch away from the ball carrier; manual switch when not on the ball -> nearest team-mate */
    scene(&s, fv2_make(-FX(8.0), 0), fv2_make(-FX(4.0), FX(2.0)), fv2_make(-FX(8.0), -FX(9.0)));
    run_seat(&s, (WpSeatInput){127, 0, WP_BTN_SWITCH}, 1);
    run_seat(&s, (WpSeatInput){127, 0, 0}, 1);
    run_seat(&s, (WpSeatInput){127, 0, WP_BTN_SWITCH}, 1);
    CHECKF(s.seat_player[0] == ME && s.ball.owner == ME, "switched away from the ball carrier");
    s.ball.pos = fv2_make(-FX(6.0), FX(3.0)); s.ball.owner = -1; s.ball.vel = fv2_make(0, 0);   /* loose ball */
    run_seat(&s, NOIN, 1);
    run_seat(&s, (WpSeatInput){0, 0, WP_BTN_SWITCH}, 1);
    CHECKF(s.seat_player[0] == MATE, "manual switch went to %d", s.seat_player[0]);
    /* ... and auto-switch doesn't immediately undo a manual one */
    run_seat(&s, NOIN, 30);
    CHECK(s.seat_player[0] == MATE);

    /* 4. auto-switch on a loose ball: the nearer team-mate takes over, the old player's charge is cleared */
    scene(&s, fv2_make(-FX(8.0), 0), fv2_make(FX(6.0), FX(2.0)), fv2_make(-FX(8.0), -FX(9.0)));
    run_seat(&s, (WpSeatInput){0, 0, WP_BTN_SHOOT}, 3);                       /* start charging */
    s.ball.owner = -1; s.ball.pos = fv2_make(FX(5.0), FX(2.0)); s.ball.vel = fv2_make(0, 0);
    run_seat(&s, (WpSeatInput){0, 0, WP_BTN_SHOOT}, 2);
    CHECKF(s.seat_player[0] == MATE, "auto-switch to the player nearest a loose ball: seat %d", s.seat_player[0]);
    CHECK(s.players[ME].charge_kind == 0xFF && s.players[ME].kick_charge == 0);
    /* 5. through ball (hold J): lands in the space AHEAD of the receiver, who runs onto it and gets it */
    for (int hold = 14; hold <= 36; hold += 22) {
        scene(&s, fv2_make(-FX(12.0), 0), fv2_make(-FX(4.0), FX(3.0)), fv2_make(-FX(12.0), -FX(9.0)));
        fv2 to = fv2_norm(fv2_sub(s.players[MATE].pos, s.players[ME].pos));
        int8_t sx = (int8_t)(to.x * 127 / FX_ONE), sy = (int8_t)(to.y * 127 / FX_ONE);
        fx mate_x0 = s.players[MATE].pos.x;
        run_seat(&s, (WpSeatInput){sx, sy, WP_BTN_PASS}, hold);
        run_seat(&s, (WpSeatInput){sx, sy, 0}, 1);
        int got = -1, kicked = 0;
        for (int t = 0; t < 300 && got < 0; t++) {
            kicked |= (run_seat(&s, NOIN, 1) & WP_EV_KICK) != 0;
            if (kicked && s.ball.owner >= 0) got = s.ball.owner;
        }
        printf("through ball (hold %d): received by %d, %.1f m beyond where he started\n", hold, got,
               tod(s.players[MATE].pos.x - mate_x0));
        CHECKF(got == MATE, "through ball hold %d: ended with %d", hold, got);
        CHECKF(s.players[MATE].pos.x - mate_x0 > (hold > 20 ? FX(6.0) : FX(1.5)), "through ball hold %d: only %.1f m ahead",
               hold, tod(s.players[MATE].pos.x - mate_x0));
    }

    /* 6. lofted pass (L) over a defender: goes over (high), dropped on the receiver */
    scene(&s, fv2_make(-FX(12.0), 0), fv2_make(FX(4.0), FX(1.0)), fv2_make(-FX(12.0), -FX(9.0)));
    s.players[7].pos = fv2_make(-FX(6.0), FX(0.3));           /* an opponent statue in the lane */
    {
        fx peak = 0; int got = -1, kicked = 0;
        run_seat(&s, (WpSeatInput){127, 8, WP_BTN_LOB}, 1);
        for (int t = 0; t < 300 && got < 0; t++) {
            kicked |= (run_seat(&s, NOIN, 1) & WP_EV_KICK) != 0;
            if (s.ball.z > peak) peak = s.ball.z;
            if (kicked && s.ball.owner >= 0) got = s.ball.owner;
        }
        printf("lofted pass: peak %.1f m, received by %d\n", tod(peak), got);
        CHECKF(peak > FX(2.5) && got == MATE, "lob: peak %.1f m, got %d", tod(peak), got);
    }
    /* lob solver agrees with the real flight */
    for (int d = 8; d <= 28; d += 10) {
        fx vz = FX(6.0);
        setup_empty(&s, -1); s.seat_player[0] = -1;
        s.ball.pos = fv2_make(-FX(18.0), FX(8.0)); s.ball.z = 0; s.ball.owner = -1;
        s.ball.vel = fv2_make(ball_lob_speed_for(FX_INT(d), vz), 0); s.ball.vz = vz;
        fx landed = 0;
        for (int t = 0; t < 400 && !landed; t++) { if (run_seat(&s, NOIN, 1) & WP_EV_BOUNCE) landed = s.ball.pos.x + FX(18.0); }
        CHECKF(landed > FX_INT(d) - FX(0.4) && landed < FX_INT(d) + FX(0.4), "lob solver %d m landed %.2f", d, tod(landed));
    }

    /* 7. headers: a cross arriving at head height -> K = header at goal (scores vs an empty goal), J = header pass;
     *    too high to reach standing -> the player jumps (and still gets his head to it) */
    for (int kind = 0; kind < 3; kind++) {
        setup_empty(&s, ME); s.seat_player[0] = ME;
        s.players[ME].pos = fv2_make(FX(14.0), FX(1.0)); s.players[ME].facing = 0;
        s.players[MATE].pos = fv2_make(FX(8.0), -FX(6.0));
        fx hz = kind == 2 ? FX(2.15) : FX(1.65);                  /* height at arrival */
        /* ball from the right touchline area flying across to arrive at ME's head in ~0.6 s */
        fv2 from = fv2_make(FX(14.0), FX(10.0));
        fx T = FX(0.6), z0 = FX(1.0);
        s.ball.pos = from; s.ball.z = z0; s.ball.owner = -1;
        s.ball.vel = fv2_scale(fv2_sub(s.players[ME].pos, from), fx_div(FX_ONE, T));
        s.ball.vz = fx_div(hz - z0, T) + fx_mul(T_GRAVITY, T) / 2;
        uint16_t btn = kind == 1 ? WP_BTN_PASS : WP_BTN_SHOOT;
        run_seat(&s, (WpSeatInput){0, 0, btn}, 1);
        int acted = s.players[ME].action;
        uint16_t ev = 0; fx top = 0;
        WpSeatInput hold = kind == 1 ? (WpSeatInput){-83, -96, 0} : NOIN;   /* header pass: stick at the mate */
        for (int t = 0; t < 90; t++) { ev |= run_seat(&s, hold, 1); if (s.players[ME].z > top) top = s.players[ME].z; }
        printf("header %s: action %d, head contact %d, jumped %.2f m, score %d, ball heading x %.1f\n",
               kind == 1 ? "pass" : kind == 2 ? "high" : "shot", acted, (ev & WP_EV_HEADER) != 0, tod(top), s.score[0],
               tod(s.ball.vel.x));
        CHECKF(ev & WP_EV_HEADER, "header kind %d: no head contact", kind);
        if (kind == 2) CHECKF(acted == WP_ACT_JUMP_HEADER && top > FX(0.3), "high ball: no jump (%d, %.2f)", acted, tod(top));
        else CHECKF(acted == WP_ACT_STAND_HEADER, "head-height ball: action %d", acted);
        if (kind != 1) CHECKF(s.score[0] == 1 || s.phase == WP_PHASE_GOAL, "header at goal didn't score");
        if (kind == 1) CHECKF(s.seat_player[0] == MATE, "header pass: control not handed to the team-mate");
    }
    /* 8. a missed header doesn't lock the player: free again soon after the ball has gone past, body back on */
    {
        setup_empty(&s, ME); s.seat_player[0] = ME;
        s.players[ME].pos = fv2_make(FX(10.0), 0);
        s.ball.pos = fv2_make(FX(10.0) - FX(4.5), FX(1.9)); s.ball.z = FX(1.6); s.ball.owner = -1;
        s.ball.vel = fv2_make(FX(15.0), 0); s.ball.vz = fx_mul(T_GRAVITY, FX(0.15));  /* passes 1.9 m wide in 0.3 s: unreachable */
        run_seat(&s, (WpSeatInput){0, 0, WP_BTN_SHOOT}, 1);
        int started = s.players[ME].action == WP_ACT_STAND_HEADER || s.players[ME].action == WP_ACT_JUMP_HEADER;
        int passed_t = -1, free_t = -1; uint16_t ev = 0;
        for (int t = 0; t < 120 && free_t < 0; t++) {
            ev |= run_seat(&s, NOIN, 1);
            if (passed_t < 0 && s.ball.pos.x > s.players[ME].pos.x + FX(0.5)) passed_t = t;
            if (passed_t >= 0 && s.players[ME].action == WP_ACT_NONE) free_t = t;
        }
        printf("missed header: started %d, ball past at t%d, player free at t%d\n", started, passed_t, free_t);
        CHECKF(started && !(ev & WP_EV_HEADER), "miss setup: started %d header %d", started, (ev & WP_EV_HEADER) != 0);
        CHECKF(free_t >= 0 && free_t - passed_t <= 20, "missed header locked the player for %d ticks", free_t - passed_t);
        CHECK(s.players[ME].kick.done);                     /* body collision applies again */
    }

    /* 9. LIVE setup (no statue flag, one seat): taps to MOVING team-mates complete */
    {
        int ok = 0, tries = 0;
        for (int trial = 0; trial < 24; trial++) {
            WpMatchConfig cfg = sim_default_config();
            sim_init(&s, &cfg);
            int me = s.seat_player[0];
            run_seat(&s, (WpSeatInput){127, (int8_t)((trial % 3 - 1) * 90), 0}, 20 + trial * 6);   /* dribble, let them move */
            if (s.ball.owner != me) continue;
            int j = 1 + trial % 3;                           /* a defender or the midfielder */
            fv2 to = fv2_norm(fv2_sub(s.players[j].pos, s.players[me].pos));
            int8_t sx = (int8_t)(to.x * 127 / FX_ONE), sy = (int8_t)(to.y * 127 / FX_ONE);
            run_seat(&s, (WpSeatInput){sx, sy, WP_BTN_PASS}, 2);
            run_seat(&s, (WpSeatInput){sx, sy, 0}, 1);
            int got = -1, kicked = 0;
            for (int t = 0; t < 300 && got < 0; t++) {
                kicked |= (run_seat(&s, NOIN, 1) & WP_EV_KICK) != 0;
                if (kicked && s.ball.owner >= 0) got = s.ball.owner;
            }
            tries++;
            ok += got >= 0 && s.players[got].team == 0 && got != me;
        }
        printf("live passes to moving team-mates: %d/%d completed\n", ok, tries);
        CHECKF(tries >= 15 && ok * 100 >= tries * 80, "live pass completion %d/%d", ok, tries);
    }
    return TEST_DONE();
}
