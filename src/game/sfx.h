/* sfx.h - what the match sounds like (D-055): reads SimState and its WP_EV_* bits after every sim step (live or
 * replay) and turns them into sounds - graded kicks, bounces, posts, net, whistles, the crowd. Never writes the sim.
 * Called per TICK, not per frame, so a 30 fps frame with two ticks still hears both (and their order). */
#ifndef WP_SFX_H
#define WP_SFX_H
#include <stdbool.h>
#include <stdint.h>
#include "sim/sim.h"
#include "game/audio.h"

typedef struct {
    uint32_t n;                /* ticks observed (rate limits; replays rewind the sim tick, this never does) */
    uint32_t last_touch, last_bounce, last_net, last_body;
    int      shot_live;        /* ticks left in which an out of play over the goal line is a "miss" (groan) */
    float    danger;           /* smoothed 0..1: the ball near a goal */
    float    goal_hold;        /* seconds of goal roar left (the excited crowd stays up) */
    bool     start_whistle;    /* the opening kick-off whistle is still to come */
} WpSfx;

void wp_sfx_reset(WpSfx *s);   /* a new match */
/* after each sim_step. cam_cx / half_view_m: the camera's x and half its visible width (stereo pan) */
void wp_sfx_tick(WpSfx *s, WpAudio *a, const SimState *prev, const SimState *cur, bool replay, float cam_cx, float half_view_m);
/* once per drawn frame: the crowd bed follows the danger */
void wp_sfx_frame(WpSfx *s, WpAudio *a, const SimState *cur, float dt);
#endif
