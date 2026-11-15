/* scripted_match.h - deterministic input script shared by the determinism test and the benchmark.
 * Seat 0 chases the ball and mashes kick/pass/switch buttons (dribbling, kicks, targeted passes, switching and
 * goals are exercised; everyone else runs the placeholder team logic); seat 1 wanders
 * randomly on team 1. The script has its own RNG (never the sim's). Every few seconds it also makes a player
 * jump and occasionally launches the ball, so airborne/header and flight code is in the golden hash. */
#ifndef WP_SCRIPTED_MATCH_H
#define WP_SCRIPTED_MATCH_H
#include <string.h>
#include "sim/sim.h"

static void scripted_setup(SimState *s) { s->seat_player[1] = (int8_t)(2 * s->n_per_side - 1); }

static void scripted_input(WpRng *r, uint32_t tick, const SimState *s, WpInput *in) {
    static WpSeatInput held[WP_MAX_SEATS];
    memset(in, 0, sizeof *in);
    if (tick % 30 == 0) {
        held[0].buttons = (uint16_t)(wp_rng_u32(r) & (WP_BTN_PASS | WP_BTN_SHOOT | WP_BTN_SPRINT | WP_BTN_CHIP | WP_BTN_SWITCH));
        held[1].move_x = (int8_t)((int)wp_rng_range(r, 255) - 127);
        held[1].move_y = (int8_t)((int)wp_rng_range(r, 255) - 127);
        held[1].buttons = (uint16_t)(wp_rng_u32(r) & 0x3F);
    }
    /* seat 0: steer at the ball (8-way, like a keyboard) */
    int me = s->seat_player[0];
    if (me >= 0) {
        fx dx = s->ball.pos.x - s->players[me].pos.x, dy = s->ball.pos.y - s->players[me].pos.y;
        held[0].move_x = (int8_t)(dx > FX(0.3) ? 127 : (dx < -FX(0.3) ? -127 : 0));
        held[0].move_y = (int8_t)(dy > FX(0.3) ? 127 : (dy < -FX(0.3) ? -127 : 0));
    }
    for (int k = 0; k < 2; k++) in->seat[k] = held[k];
}

static void scripted_poke(WpRng *r, uint32_t tick, SimState *s) {
    if (tick % 97 == 0) sim_player_jump(s, (int)(tick / 97) % (2 * s->n_per_side), WP_ACT_JUMP_HEADER);
    if (tick % 600 != 0 || s->phase != WP_PHASE_PLAY) return;
    /* Sequence RNG calls in separate statements: C leaves argument evaluation order unspecified
     * (gcc and clang differ), which would silently break cross-compiler determinism. */
    fx vx = wp_rng_fx_signed(r, FX(18.0));
    fx vy = wp_rng_fx_signed(r, FX(12.0));
    s->ball.vel = fv2_make(vx, vy);
    s->ball.vz = (fx)wp_rng_range(r, (uint32_t)FX(9.0));
    s->ball.owner = -1;
}
#endif
