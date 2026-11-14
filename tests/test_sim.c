/* Behavioural tests: the sim must do what a player expects, not just be deterministic. */
#include <string.h>
#include "test_util.h"
#include "sim/sim.h"

static void steps(SimState *s, WpSeatInput in, int n) {
    WpInput all; memset(&all, 0, sizeof all); all.seat[0] = in;
    for (int i = 0; i < n; i++) sim_step(s, &all);
}

int main(void) {
    static SimState s;
    WpMatchConfig cfg = sim_default_config();

    /* movement: +x input moves the controlled player +x, capped at walk / sprint speed, stops on release */
    sim_init(&s, &cfg);
    int me = s.seat_player[0];
    CHECK(me >= 0);
    fx x0 = s.players[me].pos.x, y0 = s.players[me].pos.y;
    steps(&s, (WpSeatInput){127, 0, 0}, 120);
    CHECK(s.players[me].pos.x > x0 + FX(5.0));
    CHECK(fx_abs(s.players[me].pos.y - y0) < FX(0.01));
    CHECKF(fv2_len(s.players[me].vel) <= FX(6.05), "walk speed %d", fv2_len(s.players[me].vel));
    CHECK(fv2_len(s.players[me].vel) >= FX(5.9));
    steps(&s, (WpSeatInput){127, 0, WP_BTN_SPRINT}, 120);
    CHECK(fv2_len(s.players[me].vel) > FX(7.9) && fv2_len(s.players[me].vel) <= FX(8.05));
    steps(&s, (WpSeatInput){0, 0, 0}, 120);
    CHECK(fv2_len(s.players[me].vel) == 0);
    /* facing follows input */
    steps(&s, (WpSeatInput){0, 127, 0}, 10);
    CHECK(s.players[me].facing == 16384);

    /* jump (for headers): rises to a plausible apex, lands, action clears; can't double-jump */
    sim_init(&s, &cfg);
    sim_player_jump(&s, 2, WP_ACT_JUMP_HEADER);
    CHECK(s.players[2].action == WP_ACT_JUMP_HEADER);
    fx apex = 0; int air_ticks = 0;
    for (int t = 0; t < 120; t++) {
        steps(&s, (WpSeatInput){0, 0, 0}, 1);
        if (s.players[2].z > apex) apex = s.players[2].z;
        if (s.players[2].z > 0) air_ticks++;
        if (t == 5) { fx vz = s.players[2].vz; sim_player_jump(&s, 2, WP_ACT_JUMP_HEADER); CHECK(s.players[2].vz == vz); }
    }
    CHECKF(apex > FX(0.5) && apex < FX(0.7), "apex %d", apex);
    CHECKF(air_ticks > 35 && air_ticks < 50, "air ticks %d", air_ticks);   /* ~0.69 s */
    CHECK(s.players[2].z == 0 && s.players[2].action == WP_ACT_NONE);

    /* ball: a lobbed ball never goes below ground, bounces lower each time, and comes to rest */
    sim_init(&s, &cfg);
    s.ball.vel = fv2_make(FX(10.0), 0);
    s.ball.vz = FX(8.0);
    fx peak = 0, last_peak = FX(100.0);
    int bounces = 0, rising = 1;
    fx prev_z = 0;
    for (int t = 0; t < 60 * 20; t++) {
        steps(&s, (WpSeatInput){0, 0, 0}, 1);
        CHECK(s.ball.z >= 0);
        if (s.ball.z > peak) peak = s.ball.z;
        if (rising && s.ball.z < prev_z) {           /* just passed a peak */
            CHECKF(peak < last_peak, "bounce %d peak %d >= previous %d", bounces, peak, last_peak);
            last_peak = peak; bounces++; rising = 0;
        }
        if (!rising && s.ball.z > prev_z) { rising = 1; peak = 0; }
        prev_z = s.ball.z;
    }
    CHECK(bounces >= 2);
    CHECK(s.ball.z == 0 && s.ball.vz == 0);
    CHECK(s.ball.vel.x == 0 && s.ball.vel.y == 0);   /* at rest after 20 s */
    CHECK(s.ball.pos.x > FX(10.0));                   /* travelled forward */
    /* first-flight peak should be physically plausible: v^2/2g = 64/19.6 ~ 3.26 m */
    return TEST_DONE();
}
