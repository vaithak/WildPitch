/* hud.c - see hud.h. */
#include <stdio.h>
#include "game/hud.h"

void wp_hud_player(SDL_Renderer *r, const SimState *s, int seat, int w, int h) {
    int i = s->seat_player[seat];
    if (i < 0 || !s->stats_mode) return;
    const uint8_t *st = s->stats[i];
    char buf[96];
    if (s->players[i].role == WP_ROLE_KEEPER)
        snprintf(buf, sizeof buf, "KEEPER  KEEP %2d  PAS %2d  PAC %2d  ENERGY", st[WP_STAT_KEEPING], st[WP_STAT_PASSING], st[WP_STAT_PACE]);
    else
        snprintf(buf, sizeof buf, "PAC %2d  CTL %2d  PAS %2d  SHO %2d  TCK %2d  STA %2d  ENERGY", st[WP_STAT_PACE],
                 st[WP_STAT_CONTROL], st[WP_STAT_PASSING], st[WP_STAT_SHOOTING], st[WP_STAT_TACKLING], st[WP_STAT_STAMINA]);
    float x = 12, y = (float)h - 36;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 120);
    SDL_FRect bg = {x - 4, y - 4, (float)SDL_strlen(buf) * 8 + 98, 16};
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDebugText(r, x, y, buf);
    float e = (float)s->players[i].stamina / 65536.0f, bx = x + (float)SDL_strlen(buf) * 8 + 8, bw = 80;
    SDL_FRect frame = {bx, y, bw, 8}, fill = {bx + 1, y + 1, (bw - 2) * e, 6};
    SDL_SetRenderDrawColor(r, 30, 30, 34, 255);
    SDL_RenderFillRect(r, &frame);
    SDL_SetRenderDrawColor(r, (Uint8)(230 - 170 * e), (Uint8)(60 + 160 * e), 60, 255);   /* red -> green */
    SDL_RenderFillRect(r, &fill);
    (void)w;
}
