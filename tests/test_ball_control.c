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

    /* steady-state dribble quality (owner feedback "dribbling feels off, worse sprinting"):
     * touch cadence, speed smoothness, gap inside reach, and cuts/reversals after LONG run-ups */
    for (int sprint = 0; sprint <= 1; sprint++) {
        uint16_t b = sprint ? WP_BTN_SPRINT : 0;
        const char *tag = sprint ? "sprint" : "jog";
        with_ball(&s);
        run_seat(&s, (WpSeatInput){127, 0, b}, 60);
        int last_touch = -1, max_gap_ticks = 0, min_gap_ticks = 999; fx max_dv = 0, max_lead = 0, prev_v = fv2_len(s.players[ME].vel);
        for (int t = 0; t < 180; t++) {
            uint16_t ev = run_seat(&s, (WpSeatInput){127, 0, b}, 1);
            fx v = fv2_len(s.players[ME].vel);
            if (fx_abs(v - prev_v) > max_dv) max_dv = fx_abs(v - prev_v);
            prev_v = v;
            if (ball_dist(&s) > max_lead) max_lead = ball_dist(&s);
            if (ev & WP_EV_TOUCH) {
                if (last_touch >= 0) { int g = t - last_touch; if (g > max_gap_ticks) max_gap_ticks = g; if (g < min_gap_ticks) min_gap_ticks = g; }
                last_touch = t;
            }
        }
        printf("%-6s steady: touches every %d..%d ticks, max speed change %.3f m/s/tick, max ball gap %.2f m\n",
               tag, min_gap_ticks, max_gap_ticks, tod(max_dv), tod(max_lead));
        /* CP0.3e (owner): a sprint knocks the ball on beyond foot reach and he runs onto it - touches are sparser,
         * the stride only checks AT the touch (accel-limited dip, <= T_ACCEL/60), and the ball stays well inside
         * the 2.2 m loss distance. Jogging keeps the close, smooth dribble. */
        CHECKF(max_gap_ticks <= (sprint ? 44 : 26) && min_gap_ticks >= 10, "%s cadence %d..%d ticks", tag, min_gap_ticks, max_gap_ticks);
        CHECKF(max_dv < (sprint ? FX(0.31) : FX(0.05)), "%s speed not smooth: %.3f m/s per tick", tag, tod(max_dv));
        CHECKF(max_lead < (sprint ? FX(1.7) : FX(0.95)), "%s ball gap %.2f", tag, tod(max_lead));
        int worst_cut = 0, worst_rev = 0, lost = 0;
        for (int run = 40; run <= 180; run += 7) {
            for (int kind = 0; kind < 3; kind++) {   /* 90 cut, 45 cut, reversal */
                with_ball(&s);
                run_seat(&s, (WpSeatInput){127, 0, b}, run);
                WpSeatInput turn = kind == 0 ? (WpSeatInput){0, 127, b} : kind == 1 ? (WpSeatInput){127, 127, b} : (WpSeatInput){-127, 0, b};
                int turned = -1;
                for (int t = 0; t < 60; t++) {
                    run_seat(&s, turn, 1);
                    fv2 want = fv2_norm(fv2_make((fx)turn.move_x * 516, (fx)turn.move_y * 516));
                    if (turned < 0 && fv2_dot(fv2_norm(s.ball.vel), want) > FX(0.7)) turned = t;
                }
                if (s.ball.owner != ME) lost++;
                if (kind < 2 && turned > worst_cut) worst_cut = turned;
                if (kind == 2 && turned > worst_rev) worst_rev = turned;
                if (turned < 0) { if (kind < 2) worst_cut = 99; else worst_rev = 99; }
            }
        }
        printf("%-6s after 40..180-tick run-ups: lost %d, worst cut %d ticks, worst reversal %d ticks\n", tag, lost, worst_cut, worst_rev);
        CHECKF(lost == 0, "%s lost the ball %d times after long run-ups", tag, lost);
        CHECKF(worst_cut <= (sprint ? 40 : 12), "%s worst cut %d ticks", tag, worst_cut);   /* sprint: reach the ball first (CP0.3e) */
        CHECKF(worst_rev <= (sprint ? 40 : 24), "%s worst reversal %d ticks", tag, worst_rev);
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
    /* no magnet (D-016, owner: "ball physics looks weird"): possession never moves the ball through the body,
     * never brakes it from beyond foot reach, never reels it back, never teleports its height */
    for (int sp = 0; sp <= 1; sp++) {
        uint16_t b = sp ? WP_BTN_SPRINT : 0;
        fx mind = FX(9.0), far_brake = 0, reel = 0;
        for (int off = 0; off < 20; off++) {
            with_ball(&s);
            run_seat(&s, (WpSeatInput){127, 0, b}, 60 + off);
            fv2 pv = s.ball.vel;
            for (int t = 0; t < 120; t++) {
                fx d0 = ball_dist(&s);
                run_seat(&s, NOIN, 1);
                fx d = ball_dist(&s), br = fv2_len(pv) - fv2_len(s.ball.vel);
                /* beyond reach both before AND after the tick (a sprinter closes 13 cm a tick) */
                if (d0 > T_TOUCH_REACH + FX(0.1) && d > T_TOUCH_REACH + FX(0.1) && br > FX(0.2) && br > far_brake) far_brake = br;
                if (fv2_len(s.players[ME].vel) < FX(0.1) && d0 - d > fx_mul(reel, WP_DT)) reel = fx_div(d0 - d, WP_DT);
                if (d < mind) mind = d;
                pv = s.ball.vel;
            }
            CHECKF(s.ball.owner == ME, "release off %d: lost the ball", off);
        }
        printf("%-6s release: closest to body %.2f m, braking from beyond reach %.2f m/s, reel-back %.2f m/s\n",
               sp ? "sprint" : "jog", tod(mind), tod(far_brake), tod(reel));
        CHECKF(mind > FX(0.35), "release: ball %.2f m from the player's centre (through the body)", tod(mind));
        CHECKF(far_brake == 0, "release: ball braked by %.2f m/s from beyond reach", tod(far_brake));
        CHECKF(reel < FX(0.35), "release: ball reeled back at %.2f m/s", tod(reel));
    }
    {   /* sprint + pass forward / sideways / back: the gather keeps the ball outside the body */
        static const int8_t ax[3] = {127, 0, -127}, ay[3] = {0, 127, 0};
        for (int k = 0; k < 3; k++) {
            fx mind = FX(9.0);
            for (int off = 0; off < 20; off += 2) {
                with_ball(&s);
                run_seat(&s, (WpSeatInput){127, 0, WP_BTN_SPRINT}, 60 + off);
                run_seat(&s, (WpSeatInput){ax[k], ay[k], WP_BTN_SPRINT | WP_BTN_PASS}, 1);
                int kicked = 0;
                for (int t = 0; t < 10 && !kicked; t++) {
                    kicked = (run_seat(&s, (WpSeatInput){ax[k], ay[k], WP_BTN_SPRINT}, 1) & WP_EV_KICK) != 0;
                    if (!kicked && ball_dist(&s) < mind) mind = ball_dist(&s);
                }
                CHECKF(kicked, "sprint pass dir %d off %d: no kick", k, off);
            }
            CHECKF(mind > FX(0.35), "sprint pass dir %d: gathered ball came %.2f m from the centre", k, tod(mind));
        }
    }
    {   /* a received pass with no stick is trapped on the receiver's side, not pulled through him */
        setup_empty(&s, ME); s.seat_player[0] = ME; s.players[ME].pos = fv2_make(0, 0);
        s.ball.pos = fv2_make(FX(8.0), FX(0.1)); s.ball.vel = fv2_make(-FX(8.0), 0); s.ball.owner = -1;
        fx mind = FX(9.0);
        for (int t = 0; t < 150; t++) { run_seat(&s, NOIN, 1); if (ball_dist(&s) < mind) mind = ball_dist(&s); }
        CHECKF(s.ball.owner == ME && mind > FX(0.35), "received pass: owner %d, closest %.2f m", s.ball.owner, tod(mind));
    }
    {   /* chest control then an instant shot: the ball falls under gravity, no height teleport */
        setup_empty(&s, ME); s.seat_player[0] = ME; s.players[ME].pos = fv2_make(0, 0);
        s.ball.pos = fv2_make(FX(3.0), 0); s.ball.z = FX(1.0); s.ball.vel = fv2_make(-FX(5.0), 0); s.ball.owner = -1;
        fx pz = s.ball.z, maxdrop = 0; int own = -1;
        for (int t = 0; t < 60; t++) {
            WpSeatInput in = {127, 0, 0};
            if (own >= 0 && t < own + 4) in.buttons = WP_BTN_SHOOT;
            run_seat(&s, in, 1);
            if (own < 0 && s.ball.owner == ME) own = t;
            if (pz - s.ball.z > maxdrop) maxdrop = pz - s.ball.z;
            pz = s.ball.z;
        }
        CHECKF(own >= 0 && maxdrop < FX(0.08), "chest+shot: controlled %d, max height drop %.2f m in one tick", own, tod(maxdrop));
    }
    return TEST_DONE();
}
