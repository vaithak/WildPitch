/* hud.h - the controlled player's card (D-034): his stats and an energy bar, bottom-left above the key hints.
 * Presentation only; drawn only when the match has stats on. */
#ifndef WP_HUD_H
#define WP_HUD_H
#include <SDL3/SDL.h>
#include "sim/sim.h"

void wp_hud_player(SDL_Renderer *r, const SimState *s, int seat, int w, int h);

#endif
