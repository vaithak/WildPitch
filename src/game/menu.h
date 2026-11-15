/* menu.h - pause menu (Esc): Resume / Controls / Quit, and the Controls screen (rebind keys). The game is paused
 * while it's open and it only redraws when something changes (runs cool). */
#ifndef WP_MENU_H
#define WP_MENU_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "game/keymap.h"

typedef enum { WP_MENU_CLOSED, WP_MENU_MAIN, WP_MENU_CONTROLS } WpMenuScreen;
typedef enum { WP_MENU_NONE, WP_MENU_RESUME, WP_MENU_QUIT, WP_MENU_RESTART } WpMenuResult;

typedef struct {
    WpMenuScreen screen;
    int          sel;          /* highlighted row */
    bool         waiting;      /* Controls: waiting for the new key of row `sel` */
    char         note[96];     /* status line ("Saved", ...) */
    int          difficulty;   /* WP_AI_EASY / NORMAL / HARD for the opposition (applies on Restart) */
    int          assist;       /* WP_ASSIST_OFF..HIGH: movement assist toward the ball (D-037, applies on Restart) */
} WpMenu;

/* settings.cfg in the pref dir (separate from controls.cfg): "difficulty = easy|normal|hard",
 * "assist = off|low|normal|high" */
int  wp_settings_load_difficulty(void);              /* default NORMAL */
int  wp_settings_load_assist(void);                  /* default NORMAL */
bool wp_settings_save(int difficulty, int assist);

void         wp_menu_open(WpMenu *m);                 /* keeps m->difficulty and m->assist */
/* handle one event; edits the keymap (and saves it) when rebinding */
WpMenuResult wp_menu_event(WpMenu *m, const SDL_Event *e, WpKeymap *km);
void         wp_menu_draw(const WpMenu *m, const WpKeymap *km, SDL_Renderer *r, int w, int h);
#endif
