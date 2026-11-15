/* Ball control, dribbling feel targets, kicks (never whiff when owned), contact height bands.
 * Prints a shot table and kick latency (copy into docs/STATUS.md when tuning changes). */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"

#define ME 4
static void with_ball(SimState *s) {
    setup_empty(s, ME);
    s->seat_player[0] = ME;
    s->players[ME].pos = fv2_make(-FX(10.0), 0);
    s->ball.pos = fv2_make(-FX(9.6), 0);
    run_seat(s, NOIN, 5);
}
static fx ball_dist(const SimState *s) { return fv2_len(fv2_sub(s->ball.pos, s->players[ME].pos)); }

int main(void) {
    static SimState s;
    with_ball(&s);
    CHECK(s.ball.owner == ME);

    for (int sprint = 0; sprint <= 1; sprint++) {
        uint16_t b = sprint ? WP_BTN_SPRINT : 0;
        const char *tag = sprint ? "sprint" : "jog";
        /* straight dribble keeps possession, ball stays close */
        with_ball(&s);
        fx maxd = 0;
        for (int t = 0; t < 90; t++) { run_seat(&s, (WpSeatInput){127, 0, b}, 1); if (ball_dist(&s) > maxd) maxd = ball_dist(&s);
                                       CHECKF(s.ball.owner == ME, "%s straight: lost ball at t=%d", tag, t); if (s.ball.owner != ME) break; }
        CHECKF(maxd < (sprint ? FX(1.8) : FX(1.3)), "%s straight: max dist %.2f", tag, tod(maxd));
        printf("%-6s straight: max ball gap %.2f m\n", tag, tod(maxd));
        /* 180 reversal, 90 cut, dead stop: swept across a full touch cycle (start offsets 0..15);
         * no offset may lose the ball. Worst cases are printed for docs/STATUS.md. */
        int worst_rev = 0, worst_cut = 0; fx worst_rev_gap = 0, worst_stop = 0;
        for (int off = 0; off < 16; off++) {
            with_ball(&s);
            run_seat(&s, (WpSeatInput){127, 0, b}, 45 + off);
            int turned = -1; maxd = 0;
            for (int t = 0; t < 60; t++) { run_seat(&s, (WpSeatInput){-127, 0, b}, 1);
                if (ball_dist(&s) > maxd) maxd = ball_dist(&s);
                if (turned < 0 && s.ball.vel.x < 0) turned = t; }
            CHECKF(s.ball.owner == ME, "%s reversal off %d: lost ball", tag, off);
            CHECKF(turned >= 0 && turned <= (sprint ? 45 : 20), "%s reversal off %d: heading back after %d", tag, off, turned);
            if (turned > worst_rev) worst_rev = turned;
            if (maxd > worst_rev_gap) worst_rev_gap = maxd;

            with_ball(&s);
            run_seat(&s, (WpSeatInput){127, 0, b}, 45 + off);
            turned = -1;
            for (int t = 0; t < 40; t++) { run_seat(&s, (WpSeatInput){0, 127, b}, 1);
                if (turned < 0 && s.ball.vel.y > fx_abs(s.ball.vel.x)) turned = t; }
            CHECKF(s.ball.owner == ME && turned >= 0 && turned <= (sprint ? 30 : 16), "%s 90 cut off %d: %d ticks", tag, off, turned);
            if (turned > worst_cut) worst_cut = turned;
            run_seat(&s, NOIN, 60);
            CHECKF(fv2_len(s.ball.vel) < FX(0.3) && ball_dist(&s) < FX(1.0) && s.ball.owner == ME,
                   "%s stop off %d: ball speed %.2f dist %.2f", tag, off, tod(fv2_len(s.ball.vel)), tod(ball_dist(&s)));
            if (ball_dist(&s) > worst_stop) worst_stop = ball_dist(&s);
        }
        printf("%-6s 180 turn (worst of 16 offsets): ball heading back after %2d ticks, max gap %.2f m (lose at %.1f)\n",
               tag, worst_rev, tod(worst_rev_gap), tod(T_LOSE_DIST));
        printf("%-6s  90 cut  (worst of 16 offsets): ball going the new way after %2d ticks\n", tag, worst_cut);
        printf("%-6s stop     (worst of 16 offsets): ball settles %.2f m from the player\n", tag, tod(worst_stop));

        /* never whiff: shoot at every tick offset across a touch cycle; measure release->contact latency */
        int max_lat = 0;
        for (int off = 0; off < 20; off++) {
            with_ball(&s);
            run_seat(&s, (WpSeatInput){127, 0, b}, 40 + off);
            run_seat(&s, (WpSeatInput){127, 0, (uint16_t)(b | WP_BTN_SHOOT)}, 12);
            int lat = -1;
            for (int t = 0; t < 30; t++) { uint16_t ev = run_seat(&s, (WpSeatInput){127, 0, b}, 1); if (ev & WP_EV_KICK) { lat = t + 1; break; } }
            CHECKF(lat > 0, "%s offset %d: shot whiffed", tag, off);
            CHECKF(fv2_len(s.ball.vel) > T_SHOT_MIN, "%s offset %d: weak shot %.1f", tag, off, tod(fv2_len(s.ball.vel)));
            if (lat > max_lat) max_lat = lat;
        }
        printf("%-6s shot: release -> ball leaves foot in <= %d ticks (%.0f ms)\n", tag, max_lat, max_lat * 1000.0 / 60);
        for (int off = 0; off < 20; off += 3) {           /* passes too */
            with_ball(&s);
            run_seat(&s, (WpSeatInput){127, 0, b}, 40 + off);
            uint16_t ev = run_seat(&s, (WpSeatInput){127, 0, (uint16_t)(b | WP_BTN_PASS)}, 1);
            ev |= run_seat(&s, (WpSeatInput){127, 0, b}, 10);
            CHECKF(ev & WP_EV_KICK, "%s offset %d: pass whiffed", tag, off);
        }
    }

    /* overhead ball (head height) can't be controlled; chest-height slow ball can (dropped to feet) */
    setup_empty(&s, ME); s.seat_player[0] = ME;
    s.players[ME].pos = fv2_make(0, 0);
    s.ball.pos = fv2_make(-FX(4.0), 0); s.ball.z = FX(2.4); s.ball.vel = fv2_make(FX(14.0), 0); s.ball.vz = FX(1.5);
    run_seat(&s, NOIN, 40);
    CHECKF(s.ball.owner == -1 && s.ball.pos.x > FX(1.0), "overhead ball was trapped (owner %d x %.2f)", s.ball.owner, tod(s.ball.pos.x));
    setup_empty(&s, ME); s.seat_player[0] = ME;
    s.players[ME].pos = fv2_make(0, 0);
    s.ball.pos = fv2_make(-FX(1.5), 0); s.ball.z = FX(1.1); s.ball.vel = fv2_make(FX(5.0), 0); s.ball.vz = FX(0.5);
    run_seat(&s, NOIN, 20);
    CHECKF(s.ball.owner == ME, "chest ball not controlled");
    /* a hot ball at the feet is not controlled (deflects off the body) */
    setup_empty(&s, ME); s.seat_player[0] = ME;
    s.players[ME].pos = fv2_make(0, 0);
    s.ball.pos = fv2_make(-FX(3.0), 0); s.ball.vel = fv2_make(FX(25.0), 0);
    uint16_t ev = run_seat(&s, NOIN, 10);
    CHECK(s.ball.owner == -1 && (ev & WP_EV_BODY));

    /* shot table from a standstill, aimed straight down the pitch (no aim assist effect on distance) */
    printf("kind  charge  speed  carry(1st bounce)  peak   flight\n");
    fx prev_carry = 0;
    for (int kind = 0; kind < 2; kind++)
        for (int c = 0; c <= T_CHARGE_MAX_TICKS; c += T_CHARGE_MAX_TICKS / 2) {
            setup_empty(&s, ME); s.seat_player[0] = ME;
            s.players[ME].pos = fv2_make(-FX(19.0), FX(0.0));
            s.ball.pos = fv2_make(-FX(18.6), 0);
            run_seat(&s, NOIN, 3);
            uint16_t btn = kind ? WP_BTN_CHIP : WP_BTN_SHOOT;
            run_seat(&s, (WpSeatInput){0, 0, btn}, c ? c : 1);
            fx x0 = 0, peak = 0; int t0 = -1, flight = 0;
            for (int t = 0; t < 240; t++) {
                uint16_t e = run_seat(&s, NOIN, 1);
                if ((e & WP_EV_KICK) && t0 < 0) { t0 = t; x0 = s.ball.pos.x; continue; }
                if (t0 >= 0 && s.ball.z > peak) peak = s.ball.z;
                if (t0 >= 0 && (e & WP_EV_BOUNCE)) { flight = t - t0; break; }
                if (t0 >= 0 && peak > 0 && s.ball.z == 0 && s.ball.vz == 0) { flight = t - t0; break; }  /* low shot: no bounce event */
            }
            fx carry = s.ball.pos.x - x0;
            printf("%-5s %5d%%  %5.1f  %8.1f m        %4.2f m  %4.2f s\n", kind ? "chip" : "shot", c * 100 / T_CHARGE_MAX_TICKS,
                   tod(kind ? fx_lerp(T_CHIP_MIN, T_CHIP_MAX, fx_div(FX_INT(c), FX_INT(T_CHARGE_MAX_TICKS)))
                            : fx_lerp(T_SHOT_MIN, T_SHOT_MAX, fx_div(FX_INT(c), FX_INT(T_CHARGE_MAX_TICKS)))),
                   tod(carry), tod(peak), flight / 60.0);
            CHECK(t0 >= 0);
            if (c > 0) CHECKF(carry > prev_carry, "carry not increasing with charge");
            prev_carry = carry;
            if (c == T_CHARGE_MAX_TICKS) prev_carry = 0;
        }
    return TEST_DONE();
}
