/* title.h - the title screen after the dedication card (CP0.6): Quick match / Controls / Settings / Quit, the quick
 * match setup (format, difficulty, movement assist, auto switching, half length, kits) and a Settings screen
 * (volumes, frame rate). Pure UI over WpSettings (saved on every change); main.c runs it in the same blocking
 * wait-for-event loop as the pause menu, so it redraws only on input. The footer carries the dedication (D-018). */
#ifndef WP_TITLE_H
#define WP_TITLE_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "game/settings.h"

typedef enum { WP_TS_MAIN, WP_TS_SETUP, WP_TS_SETTINGS } WpTitleScreen;
typedef enum { WP_TITLE_NONE, WP_TITLE_KICKOFF, WP_TITLE_CONTROLS, WP_TITLE_QUIT, WP_TITLE_CHANGED } WpTitleResult;

typedef struct {
    WpTitleScreen screen;
    int           sel;
    char          note[96];
    WpSettings   *set;
} WpTitle;

void          wp_title_open(WpTitle *t);              /* main screen, "Quick match" highlighted; keeps t->set */
WpTitleResult wp_title_event(WpTitle *t, const SDL_Event *e);
void          wp_title_draw(const WpTitle *t, SDL_Renderer *r, int w, int h);
#endif
