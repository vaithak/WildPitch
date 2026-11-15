/* menu.c - see menu.h. Up/Down select, Enter choose, Esc back. Controls: Enter on an action, then press the new
 * key (Esc cancels); keys are swapped if already used; R resets to defaults. Changes are saved immediately. */
#include <stdio.h>
#include <string.h>
#include "game/menu.h"

static const char *MAIN_ITEMS[3] = {"Resume", "Controls", "Quit game"};

void wp_menu_open(WpMenu *m) { memset(m, 0, sizeof *m); m->screen = WP_MENU_MAIN; }

WpMenuResult wp_menu_event(WpMenu *m, const SDL_Event *e, WpKeymap *km) {
    if (e->type != SDL_EVENT_KEY_DOWN || e->key.repeat) return WP_MENU_NONE;
    SDL_Scancode sc = e->key.scancode;
    if (m->screen == WP_MENU_CONTROLS && m->waiting) {
        m->waiting = false;
        if (sc == SDL_SCANCODE_ESCAPE) { snprintf(m->note, sizeof m->note, "Cancelled"); return WP_MENU_NONE; }
        wp_keymap_bind(km, (WpKeyAction)m->sel, sc);
        snprintf(m->note, sizeof m->note, wp_keymap_save(km) ? "Saved" : "Couldn't save (kept for this session)");
        return WP_MENU_NONE;
    }
    int rows = m->screen == WP_MENU_MAIN ? 3 : WP_KEY_COUNT;
    if (sc == SDL_SCANCODE_UP)   { m->sel = (m->sel + rows - 1) % rows; m->note[0] = 0; return WP_MENU_NONE; }
    if (sc == SDL_SCANCODE_DOWN) { m->sel = (m->sel + 1) % rows; m->note[0] = 0; return WP_MENU_NONE; }
    if (sc == SDL_SCANCODE_ESCAPE) {
        if (m->screen == WP_MENU_CONTROLS) { m->screen = WP_MENU_MAIN; m->sel = 1; m->note[0] = 0; return WP_MENU_NONE; }
        m->screen = WP_MENU_CLOSED;
        return WP_MENU_RESUME;
    }
    if (m->screen == WP_MENU_CONTROLS && sc == SDL_SCANCODE_R) {
        wp_keymap_defaults(km);
        snprintf(m->note, sizeof m->note, wp_keymap_save(km) ? "Defaults restored and saved" : "Defaults restored");
        return WP_MENU_NONE;
    }
    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
        if (m->screen == WP_MENU_CONTROLS) { m->waiting = true; m->note[0] = 0; return WP_MENU_NONE; }
        if (m->sel == 0) { m->screen = WP_MENU_CLOSED; return WP_MENU_RESUME; }
        if (m->sel == 1) { m->screen = WP_MENU_CONTROLS; m->sel = 0; return WP_MENU_NONE; }
        return WP_MENU_QUIT;
    }
    return WP_MENU_NONE;
}

static void text(SDL_Renderer *r, float x, float y, float sc, const char *s, Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderScale(r, sc, sc);
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDebugText(r, x / sc, y / sc, s);
    SDL_SetRenderScale(r, 1, 1);
}

void wp_menu_draw(const WpMenu *m, const WpKeymap *km, SDL_Renderer *r, int w, int h) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 10, 14, 12, 255);
    SDL_RenderClear(r);
    float sc = (float)w >= 1100.0f ? 2.0f : 1.0f, lh = 14.0f * sc, x0 = (float)w * 0.5f - 230.0f * sc;
    if (x0 < 16) x0 = 16;
    float y = (float)h * 0.18f;
    if (m->screen == WP_MENU_MAIN) {
        text(r, x0, y, sc * 1.5f, "PAUSED", 240, 235, 220);
        y += lh * 3;
        for (int i = 0; i < 3; i++, y += lh * 1.6f) {
            bool on = m->sel == i;
            char buf[64];
            snprintf(buf, sizeof buf, "%s %s", on ? ">" : " ", MAIN_ITEMS[i]);
            text(r, x0, y, sc, buf, on ? 255 : 170, on ? 225 : 170, on ? 90 : 170);
        }
        text(r, x0, y + lh, sc * 0.75f, "Up/Down choose, Enter select, Esc resume", 120, 130, 120);
        return;
    }
    text(r, x0, y, sc * 1.5f, "CONTROLS", 240, 235, 220);
    y += lh * 2.5f;
    for (int a = 0; a < WP_KEY_COUNT; a++, y += lh * 1.25f) {
        bool on = m->sel == a;
        char buf[96];
        snprintf(buf, sizeof buf, "%s %-32s %s", on ? ">" : " ", wp_keymap_action_name((WpKeyAction)a),
                 on && m->waiting ? "press a key..." : wp_keymap_key_name(km, (WpKeyAction)a));
        text(r, x0, y, sc, buf, on ? 255 : 190, on ? 225 : 190, on ? 90 : 190);
    }
    y += lh * 0.5f;
    text(r, x0, y, sc * 0.75f, "Enter: change key   R: reset defaults   Esc: back   (F3 debug is fixed)", 120, 130, 120);
    if (m->note[0]) text(r, x0, y + lh, sc * 0.75f, m->note, 120, 220, 140);
}
