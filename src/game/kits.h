/* kits.h - team kits (presentation only): shirt / shorts / socks and a keeper shirt that stands out from them.
 * The quick-match setup picks one per team (never the same one); render.c paints the looks with it and the HUD
 * names the teams by it. Kit 0 (RED) and kit 1 (BLUE) are the pre-CP0.6 colours. */
#ifndef WP_KITS_H
#define WP_KITS_H
#include <SDL3/SDL.h>

typedef struct {
    const char *name;            /* "RED", shown in banners and the score line */
    SDL_FColor shirt, shorts, socks, keeper;
} WpKit;

#define WP_KIT_COUNT 6
extern const WpKit wp_kits[WP_KIT_COUNT];
static inline const WpKit *wp_kit(int k) { return &wp_kits[k >= 0 && k < WP_KIT_COUNT ? k : 0]; }
#endif
