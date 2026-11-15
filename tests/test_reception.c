/* D-020 acceptance (owner: "something off about the through ball power and passing/through ball reception";
 * "while a ball is being received, and the moving key is pressed, player moves ahead without reception").
 * Scenarios mirror the headless playtest that measured the faults (research_notes/Through ball and pass reception/
 * playtest_findings.md). 8-way keyboard stick values throughout. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

#define ME 4
#define MATE 3

static SimState s;

/* passer ME with the ball at `at`, MATE at `mate` moving at `mvel` (kept moving until the kick by a co-op seat 1
 * holding its stick, or - if assisted - by writing his velocity, harness only) */
static void scene(fv2 at, fv2 mate) {
    setup_empty(&s, ME);
    s.seat_player[0] = ME;
    s.players[ME].pos = at;
    s.players[MATE].pos = mate;
    s.ball.pos = fv2_add(at, fv2_make(FX(0.4), 0));
    run_seat(&s, NOIN, 5);
}
static WpInput both(WpSeatInput a, WpSeatInput b) { WpInput in; memset(&in, 0, sizeof in); in.seat[0] = a; in.seat[1] = b; return in; }
static uint16_t step2(WpSeatInput a, WpSeatInput b) { WpInput in = both(a, b); sim_step(&s, &in); return s.events; }
static int8_t c127(fx v) { return (int8_t)fx_clamp(v * 127 / FX_ONE, -127, 127); }

/* hold J for `hold` ticks with the stick at MATE (tap if hold <= T_PASS_TAP_TICKS), release; returns kick tick */
static void pass_to_mate(int hold, WpSeatInput mate_in, int keep_mate_vel, fv2 mvel) {
    fv2 to = fv2_norm(fv2_sub(s.players[MATE].pos, s.players[ME].pos));
    WpSeatInput st = {c127(to.x), c127(to.y), WP_BTN_PASS};
    for (int t = 0; t < hold; t++) { if (keep_mate_vel) s.players[MATE].vel = mvel; step2(st, mate_in); }
    st.buttons = 0;
    for (int t = 0; t < 12; t++) {
        if (keep_mate_vel) s.players[MATE].vel = mvel;
        if (step2(st, mate_in) & WP_EV_KICK) return;
        to = fv2_norm(fv2_sub(s.players[MATE].pos, s.players[ME].pos));
        st.move_x = c127(to.x); st.move_y = c127(to.y);
    }
}

int main(void) {
    /* ---- A. solver geometry (finding 2/4): runner driven by seat 1 straight on; ball must meet him AHEAD ---- */
    static const struct { const char *name; fx mx, my; int8_t sx, sy; uint16_t b; fx v; } rows[] = {
        {"6 m ahead jog", FX(6.0), FX(3.0), 127, 0, 0, FX(6.0)},
        {"6 m ahead sprint", FX(6.0), FX(3.0), 127, 0, WP_BTN_SPRINT, FX(8.0)},
        {"level 8 m wide sprint", 0, FX(8.0), 127, 0, WP_BTN_SPRINT, FX(8.0)},
        {"12 m ahead sprint", FX(12.0), FX(3.0), 127, 0, WP_BTN_SPRINT, FX(8.0)},
    };
    for (int r = 0; r < 4; r++)
        for (int hold = 0; hold < 3; hold++) {
            /* he is `mx` ahead AT CONTACT at full speed; the kick is started directly with the hold value h
             * (harness), so the J hold time doesn't move him: this checks the solver's geometry */
            fx h = hold == 0 ? 0 : hold == 1 ? FX(0.5) : FX_ONE;
            fx back = fx_mul(rows[r].v, FX_INT(T_KICK_WINDUP_PASS) / 60);
            scene(fv2_make(-FX(15.0), 0), fv2_make(-FX(15.0) + rows[r].mx - back, rows[r].my));
            s.seat_player[1] = MATE;
            s.players[MATE].vel = fv2_make(rows[r].v, 0); s.players[MATE].facing = 0;
            WpSeatInput mi = {rows[r].sx, rows[r].sy, rows[r].b};
            fv2 dd = fv2_sub(s.players[MATE].pos, s.players[ME].pos);
            WpKick k = {hold == 0 ? T_PASS_SPEED : h, 0, 0, fx_atan2(dd.y, dd.x), 1,
                        (uint8_t)(hold == 0 ? WP_KICK_PASS : WP_KICK_THROUGH), T_KICK_WINDUP_PASS, 0, MATE + 1};
            sim_player_kick(&s, ME, &k);
            for (int t = 0; t < 10 && !(step2((WpSeatInput){0, 0, 0}, mi) & WP_EV_KICK); t++) {}
            fx v0 = fv2_len(s.ball.vel), gap = 0;
            uint16_t meet_tick = s.pass_meet_tick;
            int at_meet = 0;
            for (int t = 0; t < 240 && !at_meet; t++) {
                s.players[MATE].touch_cooldown = 5;              /* harness: he can't collect it early */
                step2((WpSeatInput){0, 0, 0}, mi);
                if (s.tick == meet_tick) { at_meet = 1; gap = s.ball.pos.x - s.players[MATE].pos.x; }
            }
            fx lat = fx_abs(s.ball.pos.y - s.players[MATE].pos.y);
            printf("geom %-22s %s: launch %5.1f, at the planned meet the ball is %+.2f m ahead, %.2f m aside\n",
                   rows[r].name, hold == 0 ? "tap " : hold == 1 ? "mid " : "full", tod(v0), tod(gap), tod(lat));
            if (hold > 0 && r < 3)    /* through balls: just ahead of him, on his line. 12 m row = out of in-stride
                                         range: the plan eases him to ~6 m/s (assisted, checked below) */
                CHECKF(at_meet && gap > -FX(0.3) && gap < FX(2.0) && lat < FX(1.0),
                       "%s hold %d: at the meet the ball is %+.2f m along his run, %.2f aside", rows[r].name, hold, tod(gap), tod(lat));
        }

    /* ---- B/C. assisted receivers keep running: through ball and tap to a runner (findings 2, 3) ---- */
    for (int kind = 0; kind < 2; kind++)
        for (int sp = 0; sp < 6; sp++) {          /* sp 3..5: same, passer directly BEHIND him (y = 0) */
            fx v = sp % 3 ? FX(7.5) : FX(5.5);
            scene(fv2_make(-FX(15.0), 0), fv2_make(sp % 3 == 2 ? -FX(3.0) : -FX(9.0), sp >= 3 ? 0 : FX(3.0)));
            pass_to_mate(kind ? 24 : 6, NOIN, 1, fv2_make(v, 0));
            fx vrec = 0, cosv = 0, maxclose = 0, mind = FX(99.0); int got = -1, t = 0;
            for (; t < 300 && got < 0; t++) {
                if (s.ball.owner < 0 && fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos)) < mind)
                    mind = fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos));
                fv2 rel = fv2_sub(s.ball.vel, s.players[MATE].vel);
                if (s.ball.owner < 0 && fv2_len(rel) > maxclose && fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos)) < FX(2.0)) maxclose = fv2_len(rel);
                step2(NOIN, NOIN);
                if (s.ball.owner >= 0) { got = s.ball.owner; vrec = fv2_len(s.players[MATE].vel);
                    cosv = vrec > 0 ? fx_div(s.players[MATE].vel.x, vrec) : 0; }
            }
            printf("%s to a runner at %.1f m/s%s: received by %d after %.2f s, his speed %.1f m/s, heading cos %.2f, closing %.1f, closest %.2f\n",
                   kind ? "through ball" : "tap pass    ", tod(v), sp >= 3 ? " (in line)" : "", got, t / 60.0, tod(vrec), tod(cosv), tod(maxclose), tod(mind));
            CHECKF(mind > FX(0.35), "kind %d sp %d: ball passed %.2f m from his centre (through the body)", kind, sp, tod(mind));
            CHECKF(got == MATE, "kind %d speed %d: not received (%d)", kind, sp, got);
            CHECKF(vrec >= fx_mul(v, sp % 3 == 2 ? FX(0.5) : FX(0.6)) && cosv > FX(0.7), "kind %d sp %d: runner slowed to %.1f / turned (cos %.2f)", kind, sp, tod(vrec), tod(cosv));
            if (kind) CHECKF(vrec >= FX(4.5), "through: received at %.1f m/s (want >= 4.5)", tod(vrec));
            CHECKF(maxclose < T_CONTROL_MAX_REL, "closing speed %.1f", tod(maxclose));
        }

    /* ---- D/E. keys pressed or released while the pass travels (finding 1, the owner's report) ---- */
    {
        static const int8_t dirs[4][2] = {{0, 0}, {0, 127}, {127, 0}, {-127, 0}};   /* release, sideways, forward, toward */
        static const char *dn[4] = {"release", "sideways", "forward", "toward"};
        for (int kind = 0; kind < 2; kind++)
            for (int d = 0; d < 4; d++)
                for (int when = 0; when < 4; when++) {   /* nudge after hand-off / mid-flight / hold late / hold from hand-off */
                    int miss = 0, n = 0, runs_bad = 0; fx maxc = 0;
                    for (int pl = 0; pl < 6; pl++) {
                        fv2 mate = fv2_make(-FX(10.0) + FX_INT(7 + pl * 2), FX_INT((pl % 3) * 3 - 3));
                        scene(fv2_make(-FX(10.0), 0), mate);
                        pass_to_mate(kind ? 24 : 6, NOIN, 0, fv2_make(0, 0));
                        int got = -1, scored = 0;
                        fv2 start = s.players[MATE].pos;
                        for (int t = 0; t < 300 && got < 0; t++) {
                            WpSeatInput in = NOIN;
                            int nudge = when == 0 ? t < 12 : when == 1 ? (t >= 20 && t < 32) : when == 2 ? t >= 25 : 1;
                            fv2 rel = fv2_sub(s.ball.vel, s.players[MATE].vel);
                            if (s.ball.owner < 0 && fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos)) < FX(1.5) && fv2_len(rel) > maxc) maxc = fv2_len(rel);
                            if (nudge) { in.move_x = dirs[d][0]; in.move_y = dirs[d][1]; }
                            if (when == 0 && d == 0 && t < 12) { in.move_x = 127; }   /* "release": held then let go */
                            if (step2(in, NOIN) & WP_EV_GOAL) { scored = 1; break; }   /* empty net (keepers parked) */
                            if (s.ball.owner >= 0) got = s.ball.owner;
                        }
                        n++; miss += got != MATE;
                        if (got != MATE && (dirs[d][0] || dirs[d][1])) {   /* a takeover must be a real run that way */
                            fv2 mv = fv2_sub(s.players[MATE].pos, start);
                            fx along = fx_mul(mv.x, FX_INT(dirs[d][0]) / 127) + fx_mul(mv.y, FX_INT(dirs[d][1]) / 127);
                            if (along < FX(1.0) && !scored) runs_bad++;
                        }
                    }
                    printf("receive %-7s %-8s %-18s: %d/%d missed\n", kind ? "through" : "tap", dn[d],
                           when == 0 ? "nudge early" : when == 1 ? "nudge mid" : when == 2 ? "hold late" : "hold from hand-off", miss, n);
                    CHECKF(maxc < T_CONTROL_MAX_REL, "closing speed %.1f m/s (bounce-off)", tod(maxc));
                    /* holding straight back toward the passer for the rest of the flight is a deliberate takeover */
                    /* a HELD (>= 250 ms) direction that isn't toward the ball is a deliberate takeover (owner: standing
                     * receivers can be taken over to make a run): misses allowed, but he must have run that way */
                    int deliberate = when >= 2 && (d == 1 || d == 2);
                    if (!deliberate) CHECKF(miss <= 1, "%s %s %d: %d/%d missed", kind ? "through" : "tap", dn[d], when, miss, n);
                    CHECKF(runs_bad == 0, "%s %s %d: %d takeovers didn't run the stick's way", kind ? "through" : "tap", dn[d], when, runs_bad);
                }
    }

    /* ---- F. deflection: control goes to the best-placed team-mate at once (finding 5) ---- */
    {
        scene(fv2_make(-FX(10.0), 0), fv2_make(FX(4.0), 0));
        s.players[2].pos = fv2_make(-FX(2.0), FX(4.0));                     /* a team-mate near the lane */
        s.players[9].pos = fv2_make(-FX(4.0), FX(0.1));                     /* opponent statue in the lane */
        pass_to_mate(6, NOIN, 0, fv2_make(0, 0));
        int defl_t = -1, switched_t = -1;
        for (int t = 0; t < 120; t++) {
            uint16_t ev = step2((WpSeatInput){127, -127, 0}, NOIN);          /* still holding the pass stick */
            if (defl_t < 0 && (ev & WP_EV_BODY)) defl_t = t;
            if (defl_t >= 0 && switched_t < 0 && s.seat_player[0] != MATE) switched_t = t;
        }
        printf("deflection at t%d, control moved at t%d to %d\n", defl_t, switched_t, s.seat_player[0]);
        /* follow-through: the new man goes for the ball (not away on the stale stick) and control doesn't ping-pong */
        {
            int me = s.seat_player[0], switches = 0, away = 0; int8_t seat = s.seat_player[0];
            fx d0 = fv2_len(fv2_sub(s.ball.pos, s.players[me].pos));
            scene(fv2_make(-FX(10.0), 0), fv2_make(FX(4.0), 0));
            s.players[2].pos = fv2_make(-FX(2.0), FX(4.0));
            s.players[9].pos = fv2_make(-FX(4.0), FX(0.1));
            pass_to_mate(6, NOIN, 0, fv2_make(0, 0));
            int after = -1;
            for (int t = 0; t < 120; t++) {
                int8_t before = s.seat_player[0];
                uint16_t ev = step2((WpSeatInput){127, -127, 0}, NOIN);
                if (after < 0 && (ev & WP_EV_BODY)) after = t;
                if (after >= 0 && t > after + 1) {
                    if (s.seat_player[0] != before) switches++;
                    int cur = s.seat_player[0];
                    fv2 tb = fv2_sub(s.ball.pos, s.players[cur].pos);
                    if (s.ball.owner < 0 && fv2_len(s.players[cur].vel) > FX(1.0) && fv2_dot(tb, s.players[cur].vel) < 0) away++;
                    if (s.ball.owner >= 0 || t > after + 60) break;
                }
            }
            (void)d0; (void)seat;
            printf("after the deflection: %d more switches, %d ticks moving away from the loose ball\n", switches, away);
            CHECKF(switches <= 1 && away <= 6, "deflection follow-through: %d switches, %d ticks away", switches, away);
        }
        CHECKF(defl_t >= 0 && switched_t >= 0 && switched_t - defl_t <= 6, "deflection switch: defl %d switch %d", defl_t, switched_t);
    }

    /* ---- G. hold with nobody in the cone = more power (finding 4) ---- */
    {
        /* along the pitch (space ahead); D-021: a hold is longer up to the line, a tap stops 2.5 m inside it */
        fx prev = 0;
        for (int hold = 6; hold <= 36; hold += 10) {
            scene(fv2_make(-FX(10.0), FX(6.0)), fv2_make(-FX(10.0), -FX(9.0)));
            for (int t = 0; t < hold; t++) step2((WpSeatInput){127, 0, WP_BTN_PASS}, NOIN);
            for (int t = 0; t < 12 && !(step2((WpSeatInput){127, 0, 0}, NOIN) & WP_EV_KICK); t++) {}
            fx v = fv2_len(s.ball.vel);
            uint16_t ev = 0;
            for (int t = 0; t < 600 && (s.ball.vel.x || s.ball.vel.y); t++) ev |= step2(NOIN, NOIN);
            printf("untargeted hold %d: %.1f m/s, stops at x %.1f, out %d\n", hold, tod(v), tod(s.ball.pos.x), (ev & WP_EV_OUT) != 0);
            CHECKF(v > prev + FX(1.0), "untargeted hold %d not firmer (%.1f)", hold, tod(v));
            CHECKF(!(ev & (WP_EV_OUT | WP_EV_GOAL)), "untargeted hold %d went out", hold);
            prev = v;
        }
    }
    /* ---- H. live short room (findings 2+6): real team movement, striker near goal, through ball from midfield,
     *      stick released after the pass. Opposition parked off the pitch (harness, as in the playtest). ---- */
    {
        int ok = 0, back_runs = 0, outs = 0, n = 0;
        for (int trial = 0; trial < 20; trial++) {
            WpMatchConfig cfg = sim_default_config();
            sim_init(&s, &cfg);
            s.seat_player[0] = 3;
            s.players[3].pos = fv2_make(FX(2.0), 0);
            s.players[4].pos = fv2_make(FX_INT(9 + trial % 4), FX_INT((trial % 3) * 3 - 3));
            s.ball.pos = fv2_make(FX(2.4), 0);
            for (int t = 0; t < 20; t++) {
                for (int j = 6; j < 10; j++) s.players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
                step2(NOIN, NOIN);
            }
            if (s.ball.owner != 3) continue;
            fv2 to = fv2_norm(fv2_sub(s.players[4].pos, s.players[3].pos));
            int hold = 15 + trial * 24 / 20;
            for (int t = 0; t < hold; t++) step2((WpSeatInput){c127(to.x), c127(to.y), WP_BTN_PASS}, NOIN);
            fx x_kick = 0; int kicked = 0, got = -1; uint16_t ev = 0;
            fx max_back = 0;
            for (int t = 0; t < 360 && got < 0; t++) {
                for (int j = 6; j < 10; j++) s.players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
                uint16_t e = step2(kicked ? NOIN : (WpSeatInput){c127(to.x), c127(to.y), 0}, NOIN);
                if (!kicked && (e & WP_EV_KICK)) { kicked = 1; x_kick = s.players[4].pos.x; continue; }
                if (kicked) {
                    ev |= e;
                    if (x_kick - s.players[4].pos.x > max_back) max_back = x_kick - s.players[4].pos.x;
                    if (s.ball.owner >= 0) got = s.ball.owner;
                    if (e & WP_EV_OUT) break;
                }
            }
            n++;
            ok += got == 4;
            outs += (ev & WP_EV_OUT) != 0;
            back_runs += max_back > FX(1.0);
            if (max_back > FX(1.0)) printf("  trial %d: striker from (%.1f,%.1f) hold %d ran back %.2f m, plan %d\n", trial, tod(x_kick), tod(s.players[4].pos.y), hold, tod(max_back), s.pass_plan);
        }
        printf("live short room: %d/%d through balls received, %d out of play, %d backward runs > 1 m\n", ok, n, outs, back_runs);
        CHECKF(n >= 18 && ok * 10 >= n * 9 && back_runs == 0, "short room: %d/%d received, %d back runs", ok, n, back_runs);
    }
    /* ---- re-test findings (N1-N3) ---- */
    {   /* N1: from kickoff, tap J west toward the midfielder while holding W: never a firm ball to our own keeper,
         * and holding W after the hand-off doesn't dribble into our own net */
        int own_goals = 0, to_keeper = 0;
        for (int run = 0; run < 12; run++) {
            WpMatchConfig cfg = sim_default_config();
            sim_init(&s, &cfg);
            for (int t = 0; t < 20 + run * 3; t++) step2((WpSeatInput){127, 0, 0}, NOIN);
            step2((WpSeatInput){-127, 0, WP_BTN_PASS}, NOIN);
            for (int t = 0; t < 12; t++) {
                if (step2((WpSeatInput){-127, 0, 0}, NOIN) & WP_EV_KICK) to_keeper += s.pass_target == 0;
            }
            for (int t = 0; t < 150; t++) step2((WpSeatInput){-127, 0, 0}, NOIN);
            own_goals += s.score[1];
        }
        printf("tap west from kickoff (x12): %d passes to our keeper, %d own goals while holding W\n", to_keeper, own_goals);
        CHECKF(to_keeper == 0 && own_goals == 0, "N1: %d to keeper, %d own goals", to_keeper, own_goals);
    }
    {   /* N2: a pass coming straight to a standing receiver rolls in to his foot (no stretch at 1.3 m) */
        fx worst = 0;
        for (int d = 6; d <= 16; d += 5) {
            scene(fv2_make(-FX(10.0), 0), fv2_make(-FX(10.0) + FX_INT(d), FX(0.5)));
            pass_to_mate(6, NOIN, 0, fv2_make(0, 0));
            fx last = 0;
            for (int t = 0; t < 200 && s.ball.owner < 0; t++) {
                last = fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos));
                step2(NOIN, NOIN);
            }
            if (last > worst) worst = last;
        }
        printf("pass to a standing receiver: controlled at <= %.2f m\n", tod(worst));
        CHECKF(worst <= FX(1.2), "N2: controlled at %.2f m (stretch for a ball coming to him)", tod(worst));
    }
    {   /* N3: striker sprinting toward the byline wide right is still the target (prediction kept on the pitch) */
        setup_empty(&s, ME);
        s.seat_player[0] = 3;
        s.players[3].pos = fv2_make(FX(0.4), 0);
        s.ball.pos = fv2_make(FX(0.8), 0);
        s.players[4].pos = fv2_make(FX(15.0), FX(8.5));
        run_seat(&s, NOIN, 3);
        s.players[4].vel = fv2_make(FX(7.0), 0);
        step2((WpSeatInput){127, 127, WP_BTN_PASS}, NOIN);
        int tgt = -2; uint16_t ev = 0;
        for (int t = 0; t < 12; t++) { s.players[4].vel = fv2_make(FX(7.0), 0);
            uint16_t e = step2((WpSeatInput){127, 127, 0}, NOIN); if (e & WP_EV_KICK) tgt = s.pass_target; }
        for (int t = 0; t < 240 && s.ball.owner < 0; t++) ev |= step2(NOIN, NOIN);
        printf("NE tap to a striker running at the byline: target %d, out %d, owner %d\n", tgt, (ev & WP_EV_OUT) != 0, s.ball.owner);
        CHECKF(tgt == 4 && !(ev & WP_EV_OUT), "N3: target %d, out %d", tgt, (ev & WP_EV_OUT) != 0);
    }
    return TEST_DONE();
}
