/* menu.c - see menu.h. Up/Down select, Enter choose, Left/Right change, Esc back. Controls: Enter on an action, then
 * press the new key (Esc cancels); keys are swapped if already used; R resets to defaults. Changes are saved at once. */
#include <stdio.h>
#include <string.h>
#include "game/menu.h"
#include "sim/sim.h"

/* the pause menu's rows: actions, or a settings row (WpSetRow) */
enum { ROW_RESUME = -1, ROW_RESTART = -2, ROW_TITLE = -3, ROW_CONTROLS = -4, ROW_QUIT = -5 };
static const int ROWS[] = {ROW_RESUME, WP_SET_DIFFICULTY, WP_SET_ASSIST, WP_SET_SWITCHING, WP_SET_VOL_MASTER,
                           WP_SET_VOL_FX, WP_SET_VOL_CROWD, ROW_RESTART, ROW_CONTROLS, ROW_QUIT};
enum { N_ROWS = (int)(sizeof ROWS / sizeof ROWS[0]), SEL_CONTROLS = 8 };

void wp_menu_open(WpMenu *m) {
    WpSettings *s = m->set;
    memset(m, 0, sizeof *m);
    m->screen = WP_MENU_MAIN;
    m->set = s;
}

void wp_menu_open_controls(WpMenu *m) {
    wp_menu_open(m);
    m->screen = WP_MENU_CONTROLS;
    m->controls_only = true;
}

static bool next_match_row(int row) {
    return row == WP_SET_DIFFICULTY || row == WP_SET_ASSIST || row == WP_SET_SWITCHING;
}

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
    int rows = m->screen == WP_MENU_MAIN ? N_ROWS : WP_KEY_COUNT;
    if (sc == SDL_SCANCODE_UP)   { m->sel = (m->sel + rows - 1) % rows; m->note[0] = 0; return WP_MENU_NONE; }
    if (sc == SDL_SCANCODE_DOWN) { m->sel = (m->sel + 1) % rows; m->note[0] = 0; return WP_MENU_NONE; }
    int row = m->screen == WP_MENU_MAIN ? ROWS[m->sel] : 0;
    if (m->screen == WP_MENU_MAIN && row >= 0 && m->set &&
        (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER)) {
        wp_settings_row_change(m->set, (WpSetRow)row, sc == SDL_SCANCODE_LEFT ? -1 : 1);
        bool ok = wp_settings_save(m->set);
        char t[64];
        wp_settings_row_text(m->set, (WpSetRow)row, t, (int)sizeof t);
        snprintf(m->note, sizeof m->note, "%s%s", next_match_row(row) ? "Applies when you restart the match" : "",
                 ok ? "" : "  (couldn't save)");
        return WP_MENU_CHANGED;
    }
    if (sc == SDL_SCANCODE_ESCAPE) {
        if (m->screen == WP_MENU_CONTROLS && !m->controls_only) {
            m->screen = WP_MENU_MAIN; m->sel = SEL_CONTROLS; m->note[0] = 0; return WP_MENU_NONE;
        }
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
        if (row == ROW_RESUME) { m->screen = WP_MENU_CLOSED; return WP_MENU_RESUME; }
        if (row == ROW_RESTART) { m->screen = WP_MENU_CLOSED; return WP_MENU_RESTART; }
        if (row == ROW_TITLE) { m->screen = WP_MENU_CLOSED; return WP_MENU_TITLE; }
        if (row == ROW_CONTROLS) { m->screen = WP_MENU_CONTROLS; m->sel = 0; return WP_MENU_NONE; }
        if (row == ROW_QUIT) return WP_MENU_QUIT;
    }
    return WP_MENU_NONE;
}

void wp_menu_text(SDL_Renderer *r, float x, float y, float sc, const char *s, Uint8 cr, Uint8 cg, Uint8 cb) {
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
    if (m->screen == WP_MENU_MAIN) {
        float y = (float)h * 0.10f;
        wp_menu_text(r, x0, y, sc * 1.5f, "PAUSED", 240, 235, 220);
        y += lh * 2.6f;
        for (int i = 0; i < N_ROWS; i++, y += lh * 1.45f) {
            bool on = m->sel == i;
            char t[80], buf[96];
            int row = ROWS[i];
            if (row >= 0 && m->set) wp_settings_row_text(m->set, (WpSetRow)row, t, (int)sizeof t);
            else snprintf(t, sizeof t, "%s", row == ROW_RESUME ? "Resume" : row == ROW_RESTART ? "Restart match"
                                           : row == ROW_TITLE ? "Main menu" : row == ROW_CONTROLS ? "Controls" : "Quit game");
            snprintf(buf, sizeof buf, "%s %s", on ? ">" : " ", t);
            wp_menu_text(r, x0, y, sc, buf, on ? 255 : 170, on ? 225 : 170, on ? 90 : 170);
        }
        wp_menu_text(r, x0, y + lh * 0.5f, sc * 0.75f, "Up/Down choose, Enter select, Left/Right change, Esc resume",
                     120, 130, 120);
        if (m->note[0]) wp_menu_text(r, x0, y + lh * 1.5f, sc * 0.75f, m->note, 120, 220, 140);
        return;
    }
    float y = (float)h * 0.18f;
    wp_menu_text(r, x0, y, sc * 1.5f, "CONTROLS", 240, 235, 220);
    y += lh * 2.5f;
    for (int a = 0; a < WP_KEY_COUNT; a++, y += lh * 1.25f) {
        bool on = m->sel == a;
        char buf[96];
        snprintf(buf, sizeof buf, "%s %-32s %s", on ? ">" : " ", wp_keymap_action_name((WpKeyAction)a),
                 on && m->waiting ? "press a key..." : wp_keymap_key_name(km, (WpKeyAction)a));
        wp_menu_text(r, x0, y, sc, buf, on ? 255 : 190, on ? 225 : 190, on ? 90 : 190);
    }
    y += lh * 0.5f;
    wp_menu_text(r, x0, y, sc * 0.75f, "Enter: change key   R: reset defaults   Esc: back   (F3 debug, M mute are fixed)",
                 120, 130, 120);
    if (m->note[0]) wp_menu_text(r, x0, y + lh, sc * 0.75f, m->note, 120, 220, 140);
}
