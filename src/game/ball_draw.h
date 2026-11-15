/* ball_draw.h - the ball on screen (presentation only, floats OK). Keeps a visual orientation that rolls with
 * the drawn motion (rolling without slipping: axis = up x motion, angle = distance / radius), so a rolling ball
 * visibly turns instead of sliding like a puck. Panels blur out at speed (no wagon-wheel), fast balls get a short
 * streak, and the shadow separates from a rising ball along the light direction. */
#ifndef WP_BALL_DRAW_H
#define WP_BALL_DRAW_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "game/camera.h"

typedef struct {
    float m[3][3];        /* orientation: ball-space -> world (z up) */
    float last_x, last_y;
    bool  init;
} WpBallVis;

/* Advance orientation from the drawn (interpolated) ball position. Call once per frame. */
void wp_ball_vis_update(WpBallVis *v, float x, float y, float z);
/* Teleport (kickoff / reset): forget the last position so nothing spins across the jump. */
static inline void wp_ball_vis_reset(WpBallVis *v) { v->init = false; }

void wp_ball_draw_shadow(SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam, float x, float y, float z);
/* vx, vy: ball velocity (m/s), for motion blur / streak */
void wp_ball_draw(SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam, const WpBallVis *v,
                  float x, float y, float z, float vx, float vy);
#endif
