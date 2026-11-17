/* D-048 (CP0.5a stage 3): the intent buffer and chains. A pass pressed 1 s before the ball arrives is never dropped;
 * stale presses are cancelled (they won it / too old / switched); chest -> pass reaches the mate at ground-pass pace;
 * chest -> volley from 12 m is on target >= 40%; a header pass reaches a running mate within 1.5 m >= 70%. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

#define ME 6      /* 7v7 team 0 striker, seat 0 */
#define MATE 4    /* team 0 midfielder */
#define BLUE 13

static WpInput IN;
static uint16_t step2(SimState *s, WpSeatInput a, WpSeatInput b) { IN.seat[0] = a; IN.seat[1] = b; sim_step(s, &IN); return s->events; }
static uint16_t step0(SimState *s, WpSeatInput a) { return step2(s, a, NOIN); }
static int8_t stick(fx v) { return (int8_t)fx_clamp(v * 127 / FX_ONE, -127, 127); }
static WpSeatInput toward(fv2 from, fv2 to, uint16_t b) {
    fv2 d = fv2_norm(fv2_sub(to, from));
    return (WpSeatInput){stick(d.x), stick(d.y), b};
}

static void scene(SimState *s, fv2 me, fv2 mate) {
    WpMatchConfig cfg = sim_config_7v7();
    cfg.rules = 0;
    cfg.stats_mode = WP_STATS_NEUTRAL;
    sim_init(s, &cfg);
    s->flags |= WP_SIMF_STATIC_OTHERS;
    for (int i = 0; i < 2 * s->n_per_side; i++) s->players[i].pos = fv2_make(FX(-15.0) + i * FX(2.0), FX(80.0));
    for (int k = 0; k < WP_MAX_SEATS; k++) s->seat_player[k] = -1;
    s->seat_player[0] = ME;
    s->phase = WP_PHASE_PLAY;
    s->restart_kind = 0;
    s->ball.owner = -1;
    s->players[ME].pos = me;
    s->players[MATE].pos = mate;
    memset(&IN, 0, sizeof IN);
}

/* a lofted ball from `from` coming down through z at `at` (the lob table's trajectory for that distance) */
static void lob_at(SimState *s, fv2 from, fv2 at, fx z_end) {
    fx d = fv2_len(fv2_sub(at, from)), D = d;
    for (int it = 0; it < 30; it++) {                    /* D whose ball comes down through z_end d away */
        fx vz = T_LOB_VZ_BASE + fx_mul(T_LOB_VZ_PER_M, D), x; int t;
        ball_flight_to(ball_lob_speed_for(D, vz), vz, 0, z_end, &x, &t);
        D += d - x;
    }
    fx vz = T_LOB_VZ_BASE + fx_mul(T_LOB_VZ_PER_M, D);
    s->ball.pos = from; s->ball.z = 0;
    s->ball.vel = fv2_scale(fv2_norm(fv2_sub(at, from)), ball_lob_speed_for(D, vz));
    s->ball.vz = vz;
}

/* ball speed when it first gets within 1.6 m of the mate (after a kick by ME) */
static fx arrival_pace(SimState *s, int *kicked_kind) {
    int kicked = 0;
    *kicked_kind = -1;
    for (int t = 0; t < 400; t++) {
        uint16_t ev = step0(s, NOIN);
        if ((ev & WP_EV_KICK) && s->ball.last_touch == ME && !kicked) { kicked = 1; *kicked_kind = s->players[ME].kick.kind; }
        if (kicked && fv2_len(fv2_sub(s->ball.pos, s->players[MATE].pos)) < FX(1.6)) return fv2_len(s->ball.vel);
        if (kicked && s->ball.owner >= 0) break;
    }
    return 0;
}

int main(void) {
    static SimState s;

    /* 1. a pass pressed ~1 s before the ball arrives (ground and thigh-high balls) is never dropped */
    {
        int n = 0, fired = 0;
        for (int c = 0; c < 12; c++) {
            fv2 me = fv2_make(FX(5.0), FX_INT(c % 3) * 4 - FX(4.0));
            scene(&s, me, fv2_add(me, fv2_make(-FX(4.0), FX(9.0))));
            fx v = FX(9.0) + FX_INT(c % 4);
            if (c < 6) { s.ball.pos = fv2_add(me, fv2_make(-FX(13.0), 0)); s.ball.vel = fv2_make(v + FX(2.0), 0); }   /* rolling */
            else lob_at(&s, fv2_add(me, fv2_make(-FX(22.0), 0)), fv2_add(me, fv2_make(-FX(0.4), 0)), FX(0.8));
            int pressed = 0, kick = -1, got_t = -1;
            for (int t = 0; t < 300 && kick < 0; t++) {
                /* press when the ball is about a second away */
                fx eta = fx_div(fv2_len(fv2_sub(s.ball.pos, s.players[ME].pos)), fx_max(fv2_len(s.ball.vel), FX(1.0)));
                WpSeatInput in = NOIN;
                if (!pressed && eta < FX(1.05)) { in = toward(s.players[ME].pos, s.players[MATE].pos, WP_BTN_PASS); pressed = 1; }
                uint16_t ev = step0(&s, in);
                if (s.ball.owner == ME && got_t < 0) got_t = t;
                if ((ev & WP_EV_KICK) && s.ball.last_touch == ME) kick = s.players[ME].kick.kind;
            }
            n++;
            fired += kick == WP_KICK_PASS || kick == WP_KICK_THROUGH;
            printf("buffered pass (%s): pressed %d, got it t%d, kick %d\n", c < 6 ? "rolling" : "thigh-high", pressed, got_t, kick);
        }
        CHECKF(fired == n, "buffered passes fired %d/%d", fired, n);
    }

    /* 2. stale presses are cancelled (the buffer is checked directly): (a) an opponent wins it in between - then it
     *    comes back to us and nothing is kicked; (b) pressed > 1.5 s before he gets it; (c) a switch */
    {
        fv2 me = fv2_make(FX(5.0), 0);
        /* (a) */
        scene(&s, me, fv2_add(me, fv2_make(-FX(4.0), FX(9.0))));
        s.seat_player[1] = BLUE; s.assist_level[1] = WP_ASSIST_OFF;      /* a live opponent in the way */
        s.players[BLUE].pos = fv2_add(me, fv2_make(-FX(7.0), FX(0.2)));
        s.ball.pos = fv2_add(me, fv2_make(-FX(16.0), 0)); s.ball.vel = fv2_make(FX(9.0), 0);
        step0(&s, toward(me, s.players[MATE].pos, WP_BTN_PASS));
        int buffered = s.intent[0] == WP_KICK_PASS, blue_got = 0, cleared = 0, me_got = 0, kick = 0;
        for (int t = 0; t < 300; t++) {
            uint16_t ev = step0(&s, NOIN);
            if (blue_got == 1 && s.ball.owner == BLUE) {     /* (the buffer sees it on the next tick) he loses it: back to us */
                cleared = s.intent[0] == 0xFF;
                blue_got = 2;
                s.seat_player[1] = -1;
                s.players[BLUE].pos = fv2_make(FX(30.0), FX(80.0));
                s.ball.owner = -1; s.ball.pos = fv2_add(s.players[ME].pos, fv2_make(-FX(5.0), 0)); s.ball.vel = fv2_make(FX(6.0), 0);
            }
            if (!blue_got && s.ball.owner == BLUE) blue_got = 1;
            if (s.ball.owner == ME) me_got = 1;
            if ((ev & WP_EV_KICK) && s.ball.last_touch == ME) kick = 1;
        }
        printf("stale press (they won it): buffered %d, blue won it %d, buffer cleared then %d, came back to him %d, kick %d\n",
               buffered, blue_got != 0, cleared, me_got, kick);
        CHECK(buffered && blue_got && cleared && me_got && !kick);
        /* (b) */
        scene(&s, me, fv2_add(me, fv2_make(-FX(4.0), FX(9.0))));
        s.ball.pos = fv2_add(me, fv2_make(-FX(12.0), 0));
        step0(&s, toward(me, s.players[MATE].pos, WP_BTN_PASS));
        buffered = s.intent[0] == WP_KICK_PASS; cleared = 0; me_got = 0; kick = 0;
        for (int t = 0; t < 300; t++) {
            if (t < 120) { s.ball.vel = fv2_make(0, 0); s.ball.pos = fv2_add(me, fv2_make(-FX(12.0), 0)); }   /* held up */
            if (t == 120) { cleared = s.intent[0] == 0xFF; s.ball.pos = fv2_add(me, fv2_make(-FX(8.0), 0)); s.ball.vel = fv2_make(FX(8.0), 0); }
            uint16_t ev = step0(&s, NOIN);
            if (s.ball.owner == ME) me_got = 1;
            if ((ev & WP_EV_KICK) && s.ball.last_touch == ME) kick = 1;
        }
        printf("stale press (too early): buffered %d, cleared after 2 s %d, he got it %d, kick %d\n", buffered, cleared, me_got, kick);
        CHECK(buffered && cleared && me_got && !kick);
        /* (c) */
        scene(&s, me, fv2_add(me, fv2_make(-FX(4.0), FX(9.0))));
        s.ball.pos = fv2_add(me, fv2_make(-FX(16.0), 0)); s.ball.vel = fv2_make(FX(9.0), 0);
        step0(&s, toward(me, s.players[MATE].pos, WP_BTN_PASS));
        buffered = s.intent[0] == WP_KICK_PASS;
        for (int t = 0; t < 4; t++) step0(&s, NOIN);
        int8_t before = s.seat_player[0];
        step0(&s, (WpSeatInput){0, 0, WP_BTN_SWITCH});
        printf("stale press (switched %d -> %d): buffered %d, cleared %d\n", before, s.seat_player[0], buffered, s.intent[0] == 0xFF);
        CHECK(buffered && s.seat_player[0] != before && s.intent[0] == 0xFF);
    }

    /* 3. chest -> pass: a pass pressed early on a ball dropping at chest height, mate 12 m away (no lay-off): he chests
     *    it down and passes when it reaches his feet - at ground-pass pace (+-15% of a pass from a ball at his feet) */
    {
        int n = 0, ok = 0;
        for (int c = 0; c < 6; c++) {
            fv2 me = fv2_make(FX(0.0), FX_INT(c) * 2 - FX(5.0)), mate = fv2_add(me, fv2_make(FX_INT(c % 2) * 4 - FX(2.0), FX(12.0)));
            /* reference: the same pass from a ball at his feet */
            scene(&s, me, mate);
            s.ball.pos = fv2_add(me, fv2_make(FX(0.4), 0)); s.ball.owner = ME;
            for (int t = 0; t < 5; t++) step0(&s, NOIN);
            step0(&s, toward(s.players[ME].pos, mate, WP_BTN_PASS));
            step0(&s, toward(s.players[ME].pos, mate, 0));
            int kk;
            fx ref = arrival_pace(&s, &kk);
            /* chest -> pass */
            scene(&s, me, mate);
            lob_at(&s, fv2_add(me, fv2_make(-FX(24.0), FX(3.0))), fv2_add(me, fv2_make(-FX(0.2), 0)), FX(1.3));
            int pressed = 0, chest = 0, kicked = 0;
            fx pace = 0;
            for (int t = 0; t < 400 && !pace; t++) {
                WpSeatInput in = NOIN;
                fx eta = fx_div(fv2_len(fv2_sub(s.ball.pos, s.players[ME].pos)), fx_max(fv2_len(s.ball.vel), FX(1.0)));
                if (!pressed && eta < FX(0.6)) { in = toward(s.players[ME].pos, mate, WP_BTN_PASS); pressed = 1; }
                fx z = s.ball.z;
                uint16_t ev = step0(&s, in);
                if (!chest && s.ball.owner == ME && z > T_CB_MID) chest = 1;
                if ((ev & WP_EV_KICK) && s.ball.last_touch == ME) kicked = 1;
                if (kicked && fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos)) < FX(1.6)) pace = fv2_len(s.ball.vel);
            }
            n++;
            int good = chest && pace > 0 && fx_abs(pace - ref) <= fx_mul(ref, FX(0.15));
            ok += good;
            printf("chest -> pass %d: chested %d, arrives %.1f m/s (ground pass %.1f)%s\n", c, chest, tod(pace), tod(ref), good ? "" : "  <--");
        }
        CHECKF(ok == n, "chest -> pass at ground pace %d/%d", ok, n);
    }

    /* 4. chest -> volley from 12 m: a shot pressed early on a ball dropping at chest height - on target >= 40% */
    {
        int n = 0, goals = 0, volleys = 0;
        for (int c = 0; c < 20; c++) {
            wp_angle a = (wp_angle)(32768 + (c % 5) * 2400 - 4800);       /* around the arc in front of the +x goal */
            fv2 goal = fv2_make(FX(40.0), 0);
            fv2 me = fv2_add(goal, fv2_from_angle(a, FX(12.0)));
            scene(&s, me, fv2_make(-FX(30.0), FX(20.0)));
            fv2 from = fv2_add(me, fv2_from_angle((wp_angle)(c * 3277), FX(20.0)));
            from.x = fx_min(from.x, FX(36.0));
            lob_at(&s, from, me, FX(1.3));
            int pressed = 0, vol = 0, goal_ev = 0;
            for (int t = 0; t < 400 && !goal_ev; t++) {
                WpSeatInput in = NOIN;
                fx eta = fx_div(fv2_len(fv2_sub(s.ball.pos, s.players[ME].pos)), fx_max(fv2_len(s.ball.vel), FX(1.0)));
                if (!pressed && eta < FX(0.6)) { in = toward(s.players[ME].pos, goal, WP_BTN_SHOOT); pressed = 1; }
                uint16_t ev = step0(&s, in);
                if ((ev & WP_EV_KICK) && s.ball.last_touch == ME && s.players[ME].kick.kind == WP_KICK_SHOT) vol = 1;
                if (ev & WP_EV_GOAL) goal_ev = 1;
                if (s.phase != WP_PHASE_PLAY) break;
            }
            n++; volleys += vol; goals += goal_ev;
        }
        printf("chest -> volley from 12 m: %d volleys, %d on target (goals, no keeper) of %d\n", volleys, goals, n);
        CHECKF(goals * 10 >= n * 4, "chest -> volley on target %d/%d", goals, n);
    }

    /* 5. header pass: a pass pressed early on a cross at head height, to a jogging mate 6-10 m away on the side it came
     *    from (a facing header; seat 1, assist off: he keeps going): the ball comes down within 1.5 m of him >= 70% */
    {
        int n = 0, ok = 0;
        for (int c = 0; c < 20; c++) {
            fv2 me = fv2_make(FX(0.0), FX_INT(c % 4) * 3 - FX(4.0));
            fv2 cross_from = fv2_add(me, fv2_make(-FX(18.0), FX(6.0)));
            wp_angle back = fx_atan2(cross_from.y - me.y, cross_from.x - me.x);   /* a facing header: within 80 deg of */
            wp_angle md = (wp_angle)(back + (c % 9) * 1800 - 7200);               /* where the cross came from */
            fv2 mate = fv2_add(me, fv2_from_angle(md, FX(6.0) + FX_INT(c % 3) * 2));
            scene(&s, me, mate);
            s.seat_player[1] = MATE;
            s.assist_level[1] = WP_ASSIST_OFF;
            WpSeatInput ms = {stick(fx_mul(fx_cos((wp_angle)(c * 9000)), FX(0.6))), stick(fx_mul(fx_sin((wp_angle)(c * 9000)), FX(0.6))), 0};   /* jogging */
            lob_at(&s, cross_from, s.players[ME].pos, FX(1.85));
            int pressed = 0, headed = 0;
            fx best = FX(99.0);
            for (int t = 0; t < 300; t++) {
                WpSeatInput in = NOIN;
                fx eta = fx_div(fv2_len(fv2_sub(s.ball.pos, s.players[ME].pos)), fx_max(fv2_len(s.ball.vel), FX(1.0)));
                if (!pressed && eta < FX(1.0)) { in = toward(s.players[ME].pos, s.players[MATE].pos, WP_BTN_PASS); pressed = 1; }
                uint16_t ev = step2(&s, in, pressed ? ms : NOIN);   /* he sets off as the header is called for */
                if ((ev & WP_EV_HEADER) && s.ball.last_touch == ME) headed = 1;
                if (headed && (s.ball.owner == MATE || (s.ball.z <= FX(0.5) && s.ball.vz < 0))) {
                    best = fv2_len(fv2_sub(s.ball.pos, s.players[MATE].pos));
                    if (s.ball.owner == MATE) best = 0;
                    break;
                }
            }
            n++;
            ok += headed && best <= FX(1.5);
            printf("header pass %d: headed %d, ball %.2f m from the mate\n", c, headed, tod(best));
        }
        printf("header passes within 1.5 m: %d/%d\n", ok, n);
        CHECKF(ok * 10 >= n * 7, "header passes %d/%d", ok, n);
    }
    return TEST_DONE();
}
