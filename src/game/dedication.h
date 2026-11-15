/* dedication.h - the owner's dedication card, shown quietly on launch (D-018). The words are the owner's and stay
 * exactly as written. Presentation only: the sim never runs while it shows. */
#ifndef WP_DEDICATION_H
#define WP_DEDICATION_H
#include <stdbool.h>
#include <SDL3/SDL.h>

#define WP_DEDICATION_TEXT "In the memory of my brother Shubham Thakkar"
#define WP_DEDICATION_SECONDS 4.1f   /* 0.8 s fade in, 2.5 s hold, 0.8 s fade out */

/* The red heart that ends the line (a shape: the debug font is ASCII-only), centred at (cx, cy), size px. */
void wp_draw_heart(SDL_Renderer *r, float cx, float cy, float size, float a);

/* Draw the card at time t (seconds since it started) into a w x h output. */
void wp_dedication_draw(SDL_Renderer *r, int w, int h, float t);

/* Show the card until it ends or any key/click skips it. Returns false if the window was closed. */
bool wp_dedication_run(SDL_Renderer *r);
#endif
