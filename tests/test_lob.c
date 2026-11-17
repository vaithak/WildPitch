/* D-046: lofted passes from the D-indexed flight table. Meter landing on three pitch sizes (accurate, non-decreasing in
 * the hold, first bounce in play), lobs to moving team-mates (dropped on the predicted point), the LOB key's
 * precedence (slide tackle, ball in the hands), and AI lob completion vs the stage-0 baseline. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "aerial_metrics.h"

#define ME 4      /* team 0, seat 0 */
#define MATE 3

static WpInput IN;
static uint16_t step_in(SimState *s, WpSeatInput s0, WpSeatInput s1) {
    IN.seat[0] = s0; IN.seat[1] = s1;
    sim_step(s, &IN);
    return s->events;
}

/* pitch: 0 = 40 m regression, 1 = 60 m 5v5, 2 = 80 m 7v7. Everyone parked off the pitch except ME (and MATE) */
static void scene(SimState *s, int pitch, int keep_mate) {
    WpMatchConfig cfg = pitch == 2 ? sim_config_7v7() : pitch == 1 ? sim_config_5v5() : sim_default_config();
    cfg.rules = 0;
    sim_init(s, &cfg);
    s->flags |= WP_SIMF_STATIC_OTHERS;
    for (int i = 0; i < 2 * s->n_per_side; i++)
        if (i != ME && !(keep_mate && i == MATE)) s->players[i].pos = fv2_make(FX(-15.0) + i * FX(2.0), FX(80.0));
    for (int k = 0; k < WP_MAX_SEATS; k++) s->seat_player[k] = -1;
    s->seat_player[0] = ME;
    s->phase = WP_PHASE_PLAY;
    s->restart_kind = 0;
    memset(&IN, 0, sizeof IN);
}

static void give_ball(SimState *s, fv2 at) {
    s->players[ME].pos = at;
    s->players[ME].vel = fv2_make(0, 0);
    s->ball.pos = fv2_add(at, fv2_make(FX(0.4), 0)); s->ball.z = 0; s->ball.vz = 0; s->ball.vel = fv2_make(0, 0);
    s->ball.owner = ME; s->ball.last_touch = ME;
}

static int8_t stick(fx v) { return (int8_t)fx_clamp(v * 127 / FX_ONE, -127, 127); }

int main(void) {
    static SimState s;

    /* 1. meter into space on the 40 / 60 / 80 m pitches: first bounce within 1.5 m of D(h), measured from the ball at
     *    the strike (the carrier moves during the hold); non-decreasing in the hold; first bounce in play */
    for (int pitch = 0; pitch < 3; pitch++) {
        fx prev = 0, worst = 0;
        for (int hold = T_LOB_TAP_TICKS + 1; hold <= T_CHARGE_MAX_TICKS + 2; hold += 3) {
            scene(&s, pitch, 0);
            fx hl = s.pitch.length / 2;
            give_ball(&s, fv2_make(-hl + FX(4.0), FX(3.0)));
            for (int t = 0; t < 5; t++) step_in(&s, NOIN, NOIN);
            for (int t = 0; t < hold; t++) step_in(&s, (WpSeatInput){127, 0, WP_BTN_LOB}, NOIN);
            step_in(&s, (WpSeatInput){127, 0, 0}, NOIN);
            fx h = fx_min(fx_div(FX_INT(hold - T_LOB_TAP_TICKS), FX_INT(T_CHARGE_MAX_TICKS - T_LOB_TAP_TICKS)), FX_ONE);
            fv2 start = s.ball.pos; fx land = -1; int kicked = 0, in_play = 0;
            for (int t = 0; t < 400 && land < 0; t++) {
                uint16_t ev = step_in(&s, NOIN, NOIN);
                if ((ev & WP_EV_KICK) && !kicked) { kicked = 1; start = s.ball.pos; }   /* (one tick of flight: ~0.3 m) */
                if (kicked && (ev & WP_EV_BOUNCE)) {
                    land = fv2_len(fv2_sub(s.ball.pos, start));
                    in_play = fx_abs(s.ball.pos.x) < hl && fx_abs(s.ball.pos.y) < s.pitch.width / 2;
                }
            }
            fx want = lob_meter_dist(&s, h);
            printf("pitch %d m, hold %2d: meter %.1f m, lands %.1f m%s\n", (int)(s.pitch.length >> 16), hold, tod(want), tod(land),
                   in_play ? "" : " OUT");
            CHECKF(land > 0 && fx_abs(land - want) < FX(1.5), "pitch %d hold %d: meter %.1f landed %.1f", pitch, hold, tod(want), tod(land));
            CHECKF(land >= prev - FX(0.3), "pitch %d hold %d: %.1f after %.1f (not monotonic)", pitch, hold, tod(land), tod(prev));
            CHECK(in_play);
            if (fx_abs(land - want) > worst) worst = fx_abs(land - want);
            prev = land;
        }
        printf("pitch %d: worst landing error %.2f m, full meter %.1f m\n", pitch, tod(worst), tod(lob_max_d(&s)));
    }

    /* 2. never out of play: full meter from all over the 80 / 40 m pitches in 8 directions - the first bounce is inside
     *    (it may roll out after that; reported). Starts leave room for the dribble during the hold (a carrier who runs
     *    the ball onto the line and lobs out of the pitch gets D = 0: it pops up at his feet) */
    for (int pitch = 0; pitch <= 2; pitch += 2) {
        int n = 0, bad = 0, rolled_out = 0;
        for (int px = -2; px <= 2; px++)
            for (int py = -1; py <= 1; py++)
                for (int a = 0; a < 8; a++) {
                    scene(&s, pitch, 0);
                    fx hl = s.pitch.length / 2, hw = s.pitch.width / 2;
                    give_ball(&s, fv2_make(fx_mul(hl, FX(0.3)) * px, fx_mul(hw, FX(0.55)) * py));
                    fv2 d = fv2_from_angle((wp_angle)(a * 8192), FX_ONE);
                    WpSeatInput st = {stick(d.x), stick(d.y), WP_BTN_LOB};
                    for (int t = 0; t < T_CHARGE_MAX_TICKS + 2; t++) step_in(&s, st, NOIN);
                    st.buttons = 0;
                    step_in(&s, st, NOIN);
                    int kicked = 0, landed = 0, out = 0;
                    for (int t = 0; t < 400 && !out; t++) {
                        uint16_t ev = step_in(&s, NOIN, NOIN);
                        kicked |= (ev & WP_EV_KICK) != 0;
                        if (kicked && !landed && (ev & WP_EV_BOUNCE)) {
                            landed = 1;
                            if (fx_abs(s.ball.pos.x) >= hl || fx_abs(s.ball.pos.y) >= hw) bad++;
                        }
                        if (kicked && (ev & WP_EV_OUT)) { out = 1; if (!landed) bad++; else rolled_out++; }
                    }
                    n += kicked;
                }
        printf("full-meter lobs on the %d m pitch: %d struck, %d first bounces out of play, %d rolled out afterwards\n",
               (int)(s.pitch.length >> 16), n, bad, rolled_out);
        CHECKF(n >= 100 && bad == 0, "%d lobs: %d landed out", n, bad);
    }

    /* 3. lobs to a moving team-mate (seat 1 runs a straight line, assist off: he keeps running): the ball comes down
     *    within 1.0 m of the solved point P at z <= 1.6 m, and he is near P then (the prediction q + v t_c) */
    {
        int n = 0, ok = 0, mate_ok = 0;
        for (int c = 0; c < 48; c++) {
            scene(&s, 2, 1);
            s.seat_player[1] = MATE;
            s.assist_level[1] = WP_ASSIST_OFF;
            give_ball(&s, fv2_make(-FX(20.0) + FX_INT(c % 3) * 5, -FX(6.0) + FX_INT(c % 4) * 4));
            fx dist = FX(12.0) + FX_INT(c % 6) * 3;                     /* 12 .. 27 m ahead (the ring's range) */
            wp_angle side = (wp_angle)((c % 5) * 3000 - 6000);
            s.players[MATE].pos = fv2_add(s.ball.pos, fv2_from_angle(side, dist));
            int run = c / 12;                                            /* 0 standing, 1 jog away, 2 sprint across, 3 back */
            fv2 rv = run == 0 ? fv2_make(0, 0) : run == 1 ? fv2_make(FX(0.7), FX(0.0))
                   : run == 2 ? fv2_make(0, (c & 1) ? FX(1.0) : -FX(1.0)) : fv2_make(-FX(0.7), FX(0.3));
            WpSeatInput ms = {stick(rv.x), stick(rv.y), run == 2 ? WP_BTN_SPRINT : 0};
            for (int t = 0; t < 40; t++) step_in(&s, NOIN, ms);          /* he gets going */
            fv2 to = fv2_norm(fv2_sub(s.players[MATE].pos, s.players[ME].pos));
            WpSeatInput st = {stick(to.x), stick(to.y), WP_BTN_LOB};
            step_in(&s, st, ms);                                         /* tap A (released next tick) */
            st.buttons = 0;
            step_in(&s, st, ms);
            int kicked = 0; fv2 P = {0, 0}; fx best = FX(99.0), mate_d = FX(99.0);
            for (int t = 0; t < 300; t++) {
                uint16_t ev = step_in(&s, NOIN, ms);
                if ((ev & WP_EV_KICK) && !kicked) {
                    kicked = 1;
                    if (s.pass_target != MATE) { kicked = 2; break; }
                    P = s.pass_meet;
                }
                if (kicked == 1 && s.ball.z <= FX(1.6) && s.ball.vz < 0) {
                    fx d = fv2_len(fv2_sub(s.ball.pos, P));
                    if (d < best) { best = d; mate_d = fv2_len(fv2_sub(s.players[MATE].pos, P)); }
                }
                if (kicked && (s.ball.owner >= 0 || (s.ball.z == 0 && (ev & WP_EV_BOUNCE)))) break;
            }
            if (kicked != 1) continue;                                   /* the ring didn't pick him (range): not this test */
            n++;
            ok += best <= FX(1.0);
            mate_ok += mate_d <= FX(2.0);
            if (best > FX(1.0) || mate_d > FX(2.0))
                printf("  moving-mate case %d (run %d, %.0f m): ball %.2f m from P at z<=1.6, mate %.2f m from P\n", c, run,
                       tod(dist), tod(best), tod(mate_d));
        }
        printf("lobs to moving mates: ball within 1.0 m of P (z <= 1.6) in %d/%d, mate within 2 m of P then in %d/%d\n",
               ok, n, mate_ok, n);
        CHECKF(n >= 36 && ok * 10 >= n * 9, "only %d/%d lobs came down on P", ok, n);
        CHECKF(mate_ok * 10 >= n * 8, "prediction: only %d/%d mates near P", mate_ok, n);
    }

    /* 4. the meter snaps to a runner's PREDICTED point: aim the meter where he'll be, not where he is */
    {
        scene(&s, 2, 1);
        s.seat_player[1] = MATE;
        s.assist_level[1] = WP_ASSIST_OFF;
        s.assist_level[1] = WP_ASSIST_NORMAL;                           /* the receive assist takes him to the drop */
        give_ball(&s, fv2_make(-FX(25.0), -FX(10.0)));
        s.players[MATE].pos = fv2_make(-FX(5.0), -FX(16.0));
        WpSeatInput ms = {0, 127, WP_BTN_SPRINT};                       /* sprinting across, +y */
        for (int t = 0; t < 60; t++) step_in(&s, NOIN, ms);
        /* where he'll be in ~2 s */
        fv2 aim = fv2_add(s.players[MATE].pos, fv2_scale(s.players[MATE].vel, FX(2.0)));
        fv2 d = fv2_sub(aim, s.ball.pos);
        fx dd = fv2_len(d);
        int hold = T_LOB_TAP_TICKS + (int)((int64_t)(T_CHARGE_MAX_TICKS - T_LOB_TAP_TICKS) * (dd - T_LOB_MIN_D) / (lob_max_d(&s) - T_LOB_MIN_D));
        fv2 u = fv2_norm(d);
        WpSeatInput st = {stick(u.x), stick(u.y), WP_BTN_LOB};
        for (int t = 0; t < hold; t++) step_in(&s, st, ms);
        st.buttons = 0;
        step_in(&s, st, ms);
        int to = s.players[ME].kick.to - 1, got = -1;
        for (int t = 0; t < 300 && got < 0; t++) { step_in(&s, NOIN, ms); if (s.ball.owner >= 0 && s.ball.owner != ME) got = s.ball.owner; }
        printf("meter at a runner's destination (%.1f m, hold %d): snapped to %d, received by %d\n", tod(dd), hold, to, got);
        CHECK(to == MATE && got == MATE);
    }

    /* 5. LOB key precedence: (a) their carrier within 3 m = slide (D-043), not a lob charge; (b) ball in the keeper's
     *    hands = a throw, never the meter */
    {
        scene(&s, 2, 0);
        int opp = s.n_per_side + 5;
        s.players[opp].pos = fv2_make(FX(1.5), 0); s.players[opp].vel = fv2_make(-FX(3.0), 0);
        s.ball.owner = (int8_t)opp; s.ball.pos = fv2_make(FX(1.0), 0); s.ball.z = 0;
        s.players[ME].pos = fv2_make(-FX(0.5), -FX(1.5));
        s.flags &= (uint8_t)~WP_SIMF_STATIC_OTHERS;
        step_in(&s, (WpSeatInput){127, 0, WP_BTN_LOB}, NOIN);
        printf("LOB with their carrier 2.5 m away: action %d (slide %d), charging %d\n", s.players[ME].action, WP_ACT_SLIDE,
               s.players[ME].charge_kind != 0xFF);
        CHECK(s.players[ME].action == WP_ACT_SLIDE && s.players[ME].charge_kind == 0xFF);

        scene(&s, 2, 1);
        s.players[0].pos = fv2_make(-FX(36.0), 0);
        s.seat_player[0] = 0;
        s.players[MATE].pos = fv2_make(-FX(14.0), FX(4.0));
        s.ball.pos = s.players[0].pos;
        s.ball.owner = 0; s.ball.last_touch = 0;
        ball_to_hands(&s, 0);
        for (int t = 0; t < 3; t++) step_in(&s, NOIN, NOIN);
        int kind = -1;
        for (int t = 0; t < 30 && kind < 0; t++) {
            step_in(&s, (WpSeatInput){127, 0, t < 20 ? WP_BTN_LOB : 0}, NOIN);
            if (s.players[0].action == WP_ACT_KICK) kind = s.players[0].kick.kind;
        }
        printf("LOB with the ball in the keeper's hands: kick kind %d (throw %d)\n", kind, WP_KICK_THROW);
        CHECK(kind == WP_KICK_THROW);
    }

    /* 6. AI lob completion (7v7, mirrored stats, 20 x 5 min - the stage-0 setup): stage-0 baseline 70% (78/111; 69% =
     *    309/447 over 80 matches). Gate 69% (sampling noise at ~110 lobs is ~4%; D-046 measured 70% / 70.5% over 80) */
    {
        AerialStats a; memset(&a, 0, sizeof a);
        for (int m = 0; m < 20; m++) {
            WpMatchConfig cfg = sim_config_7v7();
            cfg.stats_mode = WP_STATS_MIRRORED;
            cfg.seed = (uint64_t)(m + 1);
            sim_init(&s, &cfg);
            for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
            s.kickoff_team = (uint8_t)(m & 1);
            s.ball.pos = fv2_make(FX_INT((m % 5) - 2), FX_INT((m % 3) - 1));
            WpInput in; memset(&in, 0, sizeof in);
            WpRng hr; wp_rng_seed(&hr, (uint64_t)(1000 + m), 7);
            AerialTrack tr; aerial_reset(&tr, &s);
            for (long t = 0; t < 60L * 60 * 5; t++) {
                sim_step(&s, &in);
                if (s.events & WP_EV_RESET)
                    for (int i = 0; i < 2 * s.n_per_side; i++) {
                        fx dx = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                        fx dy = (fx)wp_rng_range(&hr, (uint32_t)FX(1.0)) - FX(0.5);
                        s.players[i].pos = fv2_add(s.players[i].pos, fv2_make(dx, dy));
                    }
                aerial_update(&a, &tr, &s);
            }
        }
        aerial_print(&a);
        CHECKF(a.lobs >= 60 && a.lobs_done * 100 >= a.lobs * 69, "AI lob completion %ld/%ld below the baseline", a.lobs_done, a.lobs);
    }
    return TEST_DONE();
}
