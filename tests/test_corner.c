/* A human's corner (D-053, owner: "the nearby player is too near and I am never able to send a long kick for header
 * in the D-area"): runners attack the box, the short option keeps ~10 m off, a skipped walk-up places everyone, and
 * the dead-ball lob meter is slow enough to pick a man in the box. */
#include <stdio.h>
#include <string.h>
#include "test_util.h"
#include "sim/sim.h"
#include "sim/sim_internal.h"
#include "sim/tuning.h"

static SimState s;
static void step(WpSeatInput in) { WpInput a; memset(&a, 0, sizeof a); a.seat[0] = in; sim_step(&s, &a); }
static const WpSeatInput NOIN = {0, 0, 0};

/* our corner at the +x end (first half: team 0 attacks +x); returns once the restart is set (SETUP) */
static void corner(uint32_t seed, int left) {
    WpMatchConfig c = sim_config_7v7();
    c.seed = seed;
    sim_init(&s, &c);
    s.ball.pos = fv2_make(s.pitch.length / 2 - FX(1.0), left ? -FX(8.0) : FX(8.0));
    s.ball.vel = fv2_make(FX(5.0), 0); s.ball.owner = -1; s.ball.last_touch = (int8_t)(s.n_per_side + 2);
    for (int t = 0; t < 1500 && !(s.restart_kind == WP_RS_CORNER && s.restart_taker >= 0); t++) step(NOIN);
}

static int in_their_box(fv2 p) {
    fx hl = s.pitch.length / 2;
    return p.x > hl - fx_mul(s.pitch.length, FX(0.157)) && fx_abs(p.y) < fx_mul(s.pitch.width, FX(0.3));
}

static void count(int *box, fx *nearest) {
    int tk = s.restart_taker;
    *box = 0; *nearest = FX(99.0);
    for (int j = 0; j < s.n_per_side; j++) {
        if (j == tk || s.players[j].role == WP_ROLE_KEEPER) continue;
        *box += in_their_box(s.players[j].pos);
        fx d = fv2_len(fv2_sub(s.players[j].pos, s.ball.pos));
        if (d < *nearest) *nearest = d;
    }
}

int main(void) {
    /* 1. waiting: the walk-up ends with runners in the box, nobody crowding the taker */
    for (int k = 0; k < 4; k++) {
        corner((uint32_t)(k + 1), k & 1);
        CHECK(s.seat_player[0] == s.restart_taker);
        for (int t = 0; t < 900 && !(s.phase == WP_PHASE_PLAY && s.ball.owner == s.restart_taker); t++) step(NOIN);
        int box; fx near;
        count(&box, &near);
        printf("corner %d (waited): our men in the box %d, nearest team-mate %.1f m\n", k, box, near / 65536.0);
        CHECK(box >= 3);
        CHECK(near >= FX(8.0));
    }
    /* 2. a button skips the walk-up: everyone is simply there */
    corner(7, 0);
    step(NOIN);
    step((WpSeatInput){0, 0, WP_BTN_PASS});
    step(NOIN);
    {
        int box; fx near;
        count(&box, &near);
        printf("corner (skipped): phase %d, our men in the box %d\n", s.phase, box);
        CHECK(s.phase == WP_PHASE_PLAY && box >= 3);
    }
    /* 3. the dead-ball meter: a mid hold aimed at the penalty spot goes to a man in the box (to his head) */
    {
        int got = 0, n = 0;
        for (int k = 0; k < 4; k++) {
            corner((uint32_t)(k + 11), k & 1);
            for (int t = 0; t < 900 && !(s.phase == WP_PHASE_PLAY && s.ball.owner == s.restart_taker); t++) step(NOIN);
            int me = s.restart_taker;
            CHECK(lob_ramp_ticks(&s, me) == T_LOB_SET_RAMP);
            fv2 u = fv2_norm(fv2_sub(fv2_make(s.pitch.length / 2 - FX(9.0), 0), s.ball.pos));
            WpSeatInput in = {(int8_t)(u.x * 127 / 65536), (int8_t)(u.y * 127 / 65536), WP_BTN_LOB};
            for (int t = 0; t < T_LOB_TAP_TICKS + T_LOB_SET_RAMP * 4 / 10; t++) step(in);
            in.buttons = 0;
            step(in);
            int r = s.players[me].kick.to - 1;
            n++;
            got += r >= 0 && in_their_box(s.players[r].pos);
        }
        printf("dead-ball meter at the penalty spot: to a man in the box %d/%d\n", got, n);
        CHECK(got == n);
    }
    return TEST_DONE();
}
