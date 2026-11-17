/* render.h - presentation layer. Floats allowed here (never fed back into the sim).
 * All drawing goes through this module so the backend (SDL_Renderer today, maybe SDL_GPU later,
 * see DECISIONS D-006) can be swapped without touching game logic. */
#ifndef WP_RENDER_H
#define WP_RENDER_H

#include <SDL3/SDL.h>
#include "sim/sim.h"
#include "game/camera.h"
#include "game/avatar.h"
#include "game/stance_vis.h"
#include "game/ball_draw.h"
#include "game/stadium.h"

/* Render context: presentation-only state that persists across frames. */
typedef struct {
    SDL_Texture  *soft;                  /* soft disc for antialiased shapes */
    WpAvatarAnim  anim[WP_MAX_PLAYERS];
    WpStanceVis   stance[WP_MAX_PLAYERS];  /* aerial contact stance being shown (D-052) */
    WpAvatarLook  look[WP_MAX_PLAYERS];
    WpStadium     stadium;               /* boards, flags, stands (D-033) */
    WpBallVis     ball;                  /* visual ball orientation (rolls with the drawn motion) */
    uint64_t      look_seed;
    bool          looks_ready;
    int           style;                 /* index into wp_avatar_styles */
    bool          hide_markers;          /* --demo: no 'controlled player' arrows */
    uint16_t      frame_events;          /* OR of sim events over the ticks stepped this frame (set by main) */
    float         time;
} WpRenderCtx;

void wp_render_ctx_init(WpRenderCtx *ctx, SDL_Renderer *r, uint64_t seed);
void wp_render_reset_anim(WpRenderCtx *ctx);

/* Draw the whole match view. alpha in [0,1] interpolates between prev and cur tick; dt = frame seconds. */
void wp_render_match(SDL_Renderer *r, WpRenderCtx *ctx, const WpCamera *cam,
                     const SimState *prev, const SimState *cur, float alpha, float dt);

void wp_fill_ellipse(SDL_Renderer *r, float cx, float cy, float rx, float ry, SDL_FColor col);

#endif
