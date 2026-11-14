/* scripted_match.h - deterministic pseudo-random input script shared by the determinism test
 * and the benchmark. Uses its own RNG (not the sim's) so the script doesn't depend on sim internals. */
#ifndef WP_SCRIPTED_MATCH_H
#define WP_SCRIPTED_MATCH_H
#include <string.h>
#include "sim/sim.h"
static void scripted_input(WpRng *r, uint32_t tick, WpInput *in) {
    memset(in, 0, sizeof *in);
    /* change intent every ~0.5 s, like a human would */
    static WpSeatInput held[WP_MAX_SEATS];
    if (tick % 30 == 0)
        for (int k = 0; k < 2; k++) {
            held[k].move_x = (int8_t)((int)wp_rng_range(r, 255) - 127);
            held[k].move_y = (int8_t)((int)wp_rng_range(r, 255) - 127);
            held[k].buttons = (uint16_t)(wp_rng_u32(r) & 0x3F);
        }
    for (int k = 0; k < 2; k++) in->seat[k] = held[k];
}
/* Until the sim can kick, the script launches the ball itself every 3 s (test-only state poke), and
 * seat 1 drives a team-1 player, so flight/bounce/roll and two movers are covered by the golden hash. */
static void scripted_setup(SimState *s) { s->seat_player[1] = (int8_t)(2 * s->n_per_side - 1); }
static void scripted_poke(WpRng *r, uint32_t tick, SimState *s) {
    if (tick % 97 == 0) sim_player_jump(s, (int)(tick / 97) % (2 * s->n_per_side), WP_ACT_JUMP_HEADER);
    if (tick % 180 != 0) return;
    /* Sequence RNG calls in separate statements: C leaves argument evaluation order unspecified
     * (gcc and clang differ), which would silently break cross-compiler determinism. */
    fx vx = wp_rng_fx_signed(r, FX(18.0));
    fx vy = wp_rng_fx_signed(r, FX(12.0));
    s->ball.vel = fv2_make(vx, vy);
    s->ball.vz = (fx)wp_rng_range(r, (uint32_t)FX(9.0));
    if (s->ball.pos.x > FX(15.0) || s->ball.pos.x < FX(-15.0) || s->ball.pos.y > FX(10.0) || s->ball.pos.y < FX(-10.0))
        s->ball.pos = fv2_make(0, 0);   /* keep it on the pitch until rules exist */
}
#endif
