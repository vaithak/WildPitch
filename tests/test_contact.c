/* D-047 (CP0.5a stage 2): aerial control by stance. A clean ball at each stance's v_clean drops 0.3-1.0 m from the
 * body 0.3-0.7 s later; lofted passes up to ~17.4 m/s are controlled within 1.3 m; the drop grows with the pace
 * (graded, not a gate); a ball at a standing man always ends controlled or deflected off him (never through him). */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

#define ME 6      /* 7v7: team 0 striker, seat 0 */

static WpInput IN;
static uint16_t step0(SimState *s, WpSeatInput in) { IN.seat[0] = in; sim_step(s, &IN); return s->events; }

static void scene(SimState *s) {
    WpMatchConfig cfg = sim_config_7v7();
    cfg.rules = 0;
    cfg.stats_mode = WP_STATS_NEUTRAL;
    sim_init(s, &cfg);
    s->flags |= WP_SIMF_STATIC_OTHERS;
    for (int i = 0; i < 2 * s->n_per_side; i++) if (i != ME) s->players[i].pos = fv2_make(FX(-15.0) + i * FX(2.0), FX(80.0));
    for (int k = 0; k < WP_MAX_SEATS; k++) s->seat_player[k] = -1;
    s->seat_player[0] = ME;
    s->phase = WP_PHASE_PLAY;
    s->restart_kind = 0;
    s->ball.owner = -1;
    s->players[ME].pos = fv2_make(FX(10.0), 0);
    s->players[ME].vel = fv2_make(0, 0);
    s->players[ME].facing = 32768;                       /* facing the ball (coming from -x... he faces -x) */
    memset(&IN, 0, sizeof IN);
}

/* follow the ball after a touch: where (from his feet at the touch) and when it first comes down to READY_Z */
typedef struct { int got, touch_t, body; fx drop, t_ready, speed_in; } Rx;
static Rx follow(SimState *s, int max_t) {
    Rx r = {0, -1, 0, -1, -1, 0};
    fv2 at = {0, 0};
    for (int t = 0; t < max_t; t++) {
        fx sp = fv2_len(fv2_make(fv2_len(s->ball.vel), s->ball.vz));
        uint16_t ev = step0(s, NOIN);
        if ((ev & WP_EV_BODY) && s->ball.last_touch == ME && r.touch_t < 0) { r.body = 1; break; }
        if (r.touch_t < 0 && s->ball.owner == ME) { r.got = 1; r.touch_t = t; at = s->players[ME].pos; r.speed_in = sp; continue; }
        if (r.touch_t >= 0 && s->ball.vz <= 0 && s->ball.z <= T_CONTACT_READY_Z) {
            r.drop = fv2_len(fv2_sub(s->ball.pos, at));
            r.t_ready = FX_INT(t - r.touch_t) / WP_TICK_HZ;
            break;
        }
    }
    return r;
}

int main(void) {
    static SimState s;

    /* 1. each stance, a clean ball (v_clean, full setup, no pressure, k = 0): drop 0.3-1.0 m from his feet, ready
     *    (down to T_CONTACT_READY_Z) in 0.3-0.7 s */
    static const struct { const char *name; double z, v; } st[] = {
        {"instep (LOW)", 0.42, 12.5}, {"thigh (MID)", 0.80, 14.5}, {"chest (CHEST)", 1.25, 16.5}, {"head control (HEAD)", 1.80, 10.5},
    };
    for (int k = 0; k < 4; k++) {
        scene(&s);
        /* the ball 3 m out at the stance height, flying flat at him; he has planned for it for a second */
        s.ball.pos = fv2_make(FX(7.0), FX(0.0)); s.ball.z = (fx)(st[k].z * 65536) + FX(0.05);
        s.ball.vel = fv2_make((fx)(st[k].v * 65536), 0); s.ball.vz = FX(0.5);
        step0(&s, NOIN);
        s.players[ME].plan.since = (uint16_t)(s.players[ME].plan.since - 60);
        Rx r = follow(&s, 200);
        printf("%-20s in %.1f m/s: got %d, drops %.2f m from his feet after %.2f s\n", st[k].name, tod(r.speed_in), r.got,
               tod(r.drop), tod(r.t_ready));
        CHECKF(r.got && r.drop >= FX(0.3) && r.drop <= FX(1.0), "%s: drop %.2f", st[k].name, tod(r.drop));
        if (k > 0)                                       /* (the instep keeps it in the LOW band: ready at once) */
            CHECKF(r.t_ready >= FX(0.3) && r.t_ready <= FX(0.7), "%s: ready after %.2f s", st[k].name, tod(r.t_ready));
    }

    /* 2. lofted passes of 10..47 m (the table's end; 17 m/s) at a standing receiver where they come down through 1 m:
     *    controlled, drop <= 1.3 m */
    for (int D = 10; D <= 47; D += D == 45 ? 2 : 5) {
        scene(&s);
        fx vz = T_LOB_VZ_BASE + fx_mul(T_LOB_VZ_PER_M, FX_INT(D)), vh = ball_lob_speed_for(FX_INT(D), vz);
        fx dc = lob_recv_dist(FX_INT(D), 0);
        s.ball.pos = fv2_make(FX(10.0) - dc, FX(0.0)); s.ball.z = 0;
        s.ball.vel = fv2_make(vh, 0); s.ball.vz = vz;
        Rx r = follow(&s, 400);
        printf("lob %d m: arrives %.1f m/s, got %d, drops %.2f m away after %.2f s\n", D, tod(r.speed_in), r.got, tod(r.drop), tod(r.t_ready));
        CHECKF(r.got && r.drop >= 0 && r.drop <= FX(1.3), "lob %d m: got %d drop %.2f", D, r.got, tod(r.drop));
    }

    /* 3. graded: the same chest ball, faster and faster - the drop never shrinks (no error dice on a clean touch) */
    {
        fx prev = 0;
        for (int v = 14; v <= 26; v += 3) {
            scene(&s);
            s.ball.pos = fv2_make(FX(6.0), 0); s.ball.z = FX(1.25);
            s.ball.vel = fv2_make(FX_INT(v), 0); s.ball.vz = FX(0.6);
            step0(&s, NOIN);
            s.players[ME].plan.since = (uint16_t)(s.players[ME].plan.since - 60);
            Rx r = follow(&s, 200);
            fx d = r.got ? r.drop : FX(9.0);
            printf("chest at %d m/s: %s, drop %.2f m\n", v, r.got ? "controlled" : r.body ? "deflected" : "missed", tod(r.drop));
            CHECKF(r.got || r.body, "chest %d m/s: neither controlled nor deflected", v);
            CHECKF(d >= prev - FX(0.15), "chest %d m/s: drop %.2f < %.2f", v, tod(d), tod(prev));
            prev = d;
        }
    }

    /* 4. a ball at a standing man's body (driven or dropping, 8..30 m/s, offset up to 0.3 m) never passes through him */
    {
        int n = 0, bad = 0;
        for (int v = 8; v <= 30; v += 4)
            for (int zi = 0; zi < 4; zi++)
                for (int off = -1; off <= 1; off++) {
                    scene(&s);
                    static const fx zz[4] = {FX(0.45), FX(0.8), FX(1.2), FX(1.6)};
                    s.ball.pos = fv2_make(FX(5.0), FX_INT(off) * 3 / 10); s.ball.z = zz[zi];
                    s.ball.vel = fv2_make(FX_INT(v), 0); s.ball.vz = FX(0.8);
                    int done = 0;
                    for (int t = 0; t < 60 && !done; t++) {
                        uint16_t ev = step0(&s, NOIN);
                        if (s.ball.owner == ME || ((ev & WP_EV_BODY) && s.ball.last_touch == ME)) done = 1;
                        else if (s.ball.pos.x > s.players[ME].pos.x + FX(0.5)) done = 2;   /* went past */
                    }
                    n++;
                    if (done != 1 && s.ball.z < FX(1.7)) { bad++; printf("  through him: %d m/s z %.2f off %d\n", v, tod(zz[zi]), off); }
                }
        printf("balls at a standing man: %d, %d passed through him\n", n, bad);
        CHECK(bad == 0);
    }
    return TEST_DONE();
}
