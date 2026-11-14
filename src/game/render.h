/* render.h - presentation layer. Floats allowed here (never fed back into the sim).
 * All drawing goes through this module so the backend (SDL_Renderer today, maybe SDL_GPU later,
 * see DECISIONS D-006) can be swapped without touching game logic. */
#ifndef WP_RENDER_H
#define WP_RENDER_H

#include <SDL3/SDL.h>
#include "sim/sim.h"

typedef struct {
    float cx, cy;          /* world point the camera looks at (metres) */
    float ppm;             /* pixels per metre at the look-at point */
    float tilt;            /* 0 = top-down, ~0.55 = tabletop; vertical foreshortening of depth */
    float persp;           /* perspective strength per metre of depth */
    int   w, h;            /* viewport pixels */
} WpCamera;

typedef struct { float x, y, s; } WpScreenPt;   /* s = local scale (for sizing sprites) */

WpScreenPt wp_project(const WpCamera *c, float x, float y, float z);

/* Draw the whole match view. alpha in [0,1] interpolates between prev and cur tick. */
void wp_render_match(SDL_Renderer *r, const WpCamera *cam, const SimState *prev, const SimState *cur, float alpha);

void wp_fill_ellipse(SDL_Renderer *r, float cx, float cy, float rx, float ry, SDL_FColor col);

#endif
