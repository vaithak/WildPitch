/* keymap.h - rebindable keyboard controls (owner request, CP0.3a). One key per action; saved as text
 * ("action = Key Name", SDL scancode names) in the user's pref dir, loaded at startup. Presentation/platform only:
 * the sim sees WpSeatInput, never keys. */
#ifndef WP_KEYMAP_H
#define WP_KEYMAP_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "sim/sim.h"

typedef enum {
    WP_KEY_UP, WP_KEY_DOWN, WP_KEY_LEFT, WP_KEY_RIGHT,
    WP_KEY_SPRINT, WP_KEY_PASS, WP_KEY_SHOOT, WP_KEY_LOB, WP_KEY_CHIP, WP_KEY_SWITCH,
    WP_KEY_ZOOM, WP_KEY_LOOK,
    WP_KEY_REPLAY,                       /* CP0.4; appended so old controls.cfg files keep their meaning */
    WP_KEY_COUNT
} WpKeyAction;

typedef struct { SDL_Scancode key[WP_KEY_COUNT]; } WpKeymap;

void        wp_keymap_defaults(WpKeymap *m);
const char *wp_keymap_action_name(WpKeyAction a);    /* "Pass (hold: through ball)" ... for the menu */
const char *wp_keymap_action_id(WpKeyAction a);      /* "pass" ... for the file */
/* text form: one "id = Key Name" per line. parse: unknown lines/keys ignored, missing actions keep defaults */
int         wp_keymap_to_text(const WpKeymap *m, char *buf, int cap);
void        wp_keymap_from_text(WpKeymap *m, const char *text);
bool        wp_keymap_load(WpKeymap *m);             /* from the pref dir; false = none saved (defaults kept) */
bool        wp_keymap_save(const WpKeymap *m);
/* bind `a` to `key`; if another action had that key, it gets `a`'s old key (swap, never two actions per key) */
void        wp_keymap_bind(WpKeymap *m, WpKeyAction a, SDL_Scancode key);
WpSeatInput wp_keymap_read(const WpKeymap *m, const bool *keyboard_state);
const char *wp_keymap_key_name(const WpKeymap *m, WpKeyAction a);
#endif
