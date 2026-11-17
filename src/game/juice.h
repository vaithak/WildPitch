/* juice.h - small visual effects (presentation only, floats, fixed pools, time-based so 30 and 60 fps look the same):
 *  - a fading ribbon behind fast balls (shots, long lobs, punts),
 *  - the net bulging and rippling where the ball hit it,
 *  - dust puffs at slide tackles, hard landings (jumps, keeper dives) and hard bounces,
 *  - a goal flash + a bigger "GOAL!" banner in the scorer's colours.
 * All of it reads SimState (and the frame's OR'ed WP_EV_* bits); nothing is written back. */
#ifndef WP_JUICE_H
#define WP_JUICE_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "sim/sim.h"
#include "game/camera.h"

#define WP_TRAIL_N 14
#define WP_PUFF_N  48

typedef struct { float x, y, z, t; } WpTrailPt;
typedef struct { float x, y, z, vx, vy, age, life, size; } WpPuff;

typedef struct {
    WpTrailPt trail[WP_TRAIL_N];
    int       trail_head, trail_n;
    WpPuff    puff[WP_PUFF_N];
    int       puff_next;
    float     net_t[2], net_y[2], net_z[2], net_amp[2];   /* per goal (0: -x end, 1: +x end); net_t < 0 = still */
    float     flash, banner_t;                            /* goal flash 1 -> 0; seconds since the goal */
    int       goal_team;                                  /* who scored the last goal (0 / 1) */
    uint8_t   score_seen[2];
    float     last_z[WP_MAX_PLAYERS];
    uint8_t   last_action[WP_MAX_PLAYERS];
    float     slide_emit[WP_MAX_PLAYERS];
    uint16_t  dive_t[WP_MAX_PLAYERS];      /* his dive's action_timer last frame */
    float     time;
    bool      have_last;
} WpJuice;

void wp_juice_reset(WpJuice *j);                          /* teleports / replay jumps: no streaks across them */
/* once per drawn frame, before drawing: (bx, by, bz) is the drawn ball, px/py/pz the drawn players */
void wp_juice_update(WpJuice *j, const SimState *cur, uint16_t frame_ev, float bx, float by, float bz,
                     const float *px, const float *py, const float *pz, float dt);
/* outward displacement (m, along the goal's depth) of the net at (y, z) for the goal at end sgn (-1 / +1),
 * scaled by how deep the point is (depth01: 0 at the goal line .. 1 at the back) */
float wp_juice_net_bulge(const WpJuice *j, int sgn, float y, float z, float depth01);
void  wp_juice_draw_trail(const WpJuice *j, SDL_Renderer *r, const WpCamera *cam);
void  wp_juice_draw_puffs(const WpJuice *j, SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam);
/* screen-space: the goal flash and the GOAL! banner (call while phase == GOAL; names / colours of the two teams) */
void  wp_juice_draw_goal(const WpJuice *j, SDL_Renderer *r, int w, int h, const SimState *cur,
                         const char *name0, const char *name1, SDL_FColor col0, SDL_FColor col1);
#endif
