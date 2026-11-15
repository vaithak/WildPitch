/* stadium.h - what's around the pitch (presentation only, D-033): the stands, the advertising boards and the corner
 * flags. Each board carries the game's banner (WILDPITCH + a football) and, at its right-hand end, the owner's
 * dedication in a smaller font (D-018 wording, from dedication.h). The boards' positions come from tuning.h so the
 * drawing matches the sim's walls exactly. */
#ifndef WP_STADIUM_H
#define WP_STADIUM_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "sim/sim.h"
#include "game/camera.h"

typedef struct {
    SDL_Texture *banner;       /* "WILDPITCH" + football logo panel */
    SDL_Texture *dedication;   /* dedication panel (right-hand end of every board) */
    bool         ready;
} WpStadium;

/* Ground beyond the boards (draw before the pitch). */
void wp_stadium_draw_ground(SDL_Renderer *r, const WpCamera *c, const WpPitch *p);
/* Boards and flags behind the play (far side, both ends): draw after the pitch, before the players. */
void wp_stadium_draw_back(SDL_Renderer *r, WpStadium *st, const WpCamera *c, const WpPitch *p, float time);
/* The near-side board and flags: draw last, in front of the players. */
void wp_stadium_draw_front(SDL_Renderer *r, WpStadium *st, const WpCamera *c, const WpPitch *p, float time);
/* Dev check (--boards --screenshot F.bmp): both board faces flat, 2x. */
void wp_stadium_draw_sheet(SDL_Renderer *r, WpStadium *st, int w, int h);
void wp_stadium_free(WpStadium *st);

#endif
