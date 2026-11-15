/* CP0.2a: targeted + weighted ground passes, hand-off of control to the receiver, switching rules. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"

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
            run_seat(&s, (WpSeatInput){sx, sy, (uint16_t)(runs[r].b | WP_BTN_PASS)}, 1);
            int kick_t = -1, seat_before_kick_ok = 1, got = -1; fx arrive = 0;
            for (int t = 0; t < 300 && got < 0; t++) {
                int seat_before = s.seat_player[0];
                uint16_t ev = run_seat(&s, (WpSeatInput){0, 0, 0}, 1);
                if (ev & WP_EV_KICK) kick_t = t;
                if (kick_t < 0 && s.seat_player[0] != ME) seat_before_kick_ok = 0;
                if (kick_t >= 0 && s.ball.owner < 0 && dist_of(&s, MATE) < FX(1.0) && !arrive) arrive = fv2_len(s.ball.vel);
                if (kick_t >= 0 && s.ball.owner >= 0) got = s.ball.owner;
                (void)seat_before;
            }
            printf("pass %-16s %2d m: kicked t%d, arrives %.1f m/s, controlled by %d, seat -> %d\n", runs[r].name,
                   far ? 18 : 8, kick_t, tod(arrive), got, s.seat_player[0]);
            CHECKF(kick_t >= 0 && seat_before_kick_ok, "%s: kick %d, switched before contact %d", runs[r].name, kick_t, !seat_before_kick_ok);
            CHECKF(got == MATE, "%s %d m: ball ended with %d, not the receiver", runs[r].name, far ? 18 : 8, got);
            CHECKF(s.seat_player[0] == MATE, "%s: control didn't move to the receiver", runs[r].name);
            CHECKF(arrive > FX(3.0) && arrive < FX(9.0), "%s: arrival pace %.1f m/s", runs[r].name, tod(arrive));
        }
    }

    /* 2. the receiver's aim comes from the human's stick, not the assisted run: pass +x, keep holding +x, pass again
     *    on arrival -> the second pass goes on +x (to MATE2 beyond), not back toward the first passer */
    scene(&s, fv2_make(-FX(10.0), 0), fv2_make(-FX(1.0), 0), fv2_make(FX(9.0), FX(1.0)));
    run_seat(&s, (WpSeatInput){127, 0, WP_BTN_PASS}, 1);
    int second = 0;
    for (int t = 0; t < 300 && !second; t++) {
        int mine = s.seat_player[0] == MATE && s.ball.owner == MATE;
        uint16_t ev = run_seat(&s, (WpSeatInput){127, 0, (uint16_t)(mine ? WP_BTN_PASS : 0)}, 1);
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
    return TEST_DONE();
}
