/* menu.h - pause menu (Esc): Resume / settings rows / Restart / Main menu / Controls / Quit, and the Controls screen
 * (rebind keys). The game is paused while it's open and it only redraws when something changes (runs cool).
 * The settings rows edit the game's WpSettings in place and save them at once; match rules (difficulty, assist,
 * switching) apply on the next match, volumes and the frame rate at once (main.c re-applies on WP_MENU_CHANGED). */
#ifndef WP_MENU_H
#define WP_MENU_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "game/keymap.h"
#include "game/settings.h"

typedef enum { WP_MENU_CLOSED, WP_MENU_MAIN, WP_MENU_CONTROLS } WpMenuScreen;
typedef enum { WP_MENU_NONE, WP_MENU_RESUME, WP_MENU_QUIT, WP_MENU_RESTART, WP_MENU_CHANGED, WP_MENU_TITLE } WpMenuResult;

typedef struct {
    WpMenuScreen screen;
    int          sel;          /* highlighted row */
    bool         waiting;      /* Controls: waiting for the new key of row `sel` */
    bool         controls_only;/* opened from the title screen: Esc on Controls closes (back to the title) */
    char         note[96];     /* status line ("Saved", ...) */
    WpSettings  *set;          /* the game's settings (edited in place, saved on change) */
} WpMenu;

void         wp_menu_open(WpMenu *m);                 /* pause menu on Resume; keeps m->set */
void         wp_menu_open_controls(WpMenu *m);        /* straight to Controls (title screen) */
/* handle one event; edits the keymap (and saves it) when rebinding */
WpMenuResult wp_menu_event(WpMenu *m, const SDL_Event *e, WpKeymap *km);
void         wp_menu_draw(const WpMenu *m, const WpKeymap *km, SDL_Renderer *r, int w, int h);
/* shared with the title screen: a text line at pixel (x, y) scaled by sc */
void         wp_menu_text(SDL_Renderer *r, float x, float y, float sc, const char *s, Uint8 cr, Uint8 cg, Uint8 cb);
#endif
