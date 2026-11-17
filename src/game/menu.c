/* menu.c - see menu.h. Up/Down select, Enter choose, Esc back. Controls: Enter on an action, then press the new
 * key (Esc cancels); keys are swapped if already used; R resets to defaults. Changes are saved immediately. */
#include <stdio.h>
#include <string.h>
#include "game/menu.h"
#include "sim/sim.h"

enum { ITEM_RESUME, ITEM_DIFFICULTY, ITEM_ASSIST, ITEM_SWITCH, ITEM_RESTART, ITEM_CONTROLS, ITEM_QUIT, ITEM_COUNT };
static const char *ASSIST_NAME[4] = {"Off", "Low", "Normal", "High"};
static const char *ASSIST_ID[4] = {"off", "low", "normal", "high"};
static const char *SWITCH_NAME[4] = {"Legacy", "Manual", "Events", "Assisted"};   /* WP_SWITCH_* (D-050) */
static const char *SWITCH_ID[4] = {"legacy", "manual", "events", "assisted"};
static const char *DIFF_NAME[3] = {"Easy", "Normal", "Hard"};
static const char *DIFF_ID[3] = {"easy", "normal", "hard"};

void wp_menu_open(WpMenu *m) {
    int d = m->difficulty, a = m->assist, sw = m->switching;
    memset(m, 0, sizeof *m);
    m->screen = WP_MENU_MAIN;
    m->difficulty = d;
    m->assist = a;
    m->switching = sw;
}

static char *settings_file(void) {
    char *dir = SDL_GetPrefPath("WildPitch", "WildPitch");
    if (!dir) return NULL;
    size_t n = strlen(dir) + 16;
    char *f = (char *)SDL_malloc(n);
    if (f) snprintf(f, n, "%ssettings.cfg", dir);
    SDL_free(dir);
    return f;
}
int wp_settings_load_difficulty(void) {
    char *f = settings_file();
    int lv = 1;
    if (!f) return lv;
    size_t len = 0;
    char *text = (char *)SDL_LoadFile(f, &len);
    SDL_free(f);
    if (!text) return lv;
    for (int k = 0; k < 3; k++) {
        char want[32];
        snprintf(want, sizeof want, "difficulty = %s", DIFF_ID[k]);
        if (strstr(text, want)) lv = k;
    }
    SDL_free(text);
    return lv;
}
int wp_settings_load_assist(void) {
    char *f = settings_file();
    int lv = 2;
    if (!f) return lv;
    size_t len = 0;
    char *text = (char *)SDL_LoadFile(f, &len);
    SDL_free(f);
    if (!text) return lv;
    for (int k = 0; k < 4; k++) {
        char want[32];
        snprintf(want, sizeof want, "assist = %s\n", ASSIST_ID[k]);
        if (strstr(text, want)) lv = k;
    }
    SDL_free(text);
    return lv;
}
int wp_settings_load_switching(void) {
    char *f = settings_file();
    int sw = WP_SWITCH_EVENTS;
    if (!f) return sw;
    size_t len = 0;
    char *text = (char *)SDL_LoadFile(f, &len);
    SDL_free(f);
    if (!text) return sw;
    for (int k = WP_SWITCH_MANUAL; k < WP_SWITCH_MODES; k++) {
        char want[40];
        snprintf(want, sizeof want, "switching = %s\n", SWITCH_ID[k]);
        if (strstr(text, want)) sw = k;
    }
    SDL_free(text);
    return sw;
}
bool wp_settings_save(int level, int assist, int switching) {
    char *f = settings_file();
    if (!f) return false;
    char buf[160];
    int n = snprintf(buf, sizeof buf, "# WildPitch settings\ndifficulty = %s\nassist = %s\nswitching = %s\n",
                     DIFF_ID[level < 0 || level > 2 ? 1 : level], ASSIST_ID[assist < 0 || assist > 3 ? 2 : assist],
                     SWITCH_ID[switching < WP_SWITCH_MANUAL || switching >= WP_SWITCH_MODES ? WP_SWITCH_EVENTS : switching]);
    bool ok = SDL_SaveFile(f, buf, (size_t)n);
    SDL_free(f);
    return ok;
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
    int rows = m->screen == WP_MENU_MAIN ? ITEM_COUNT : WP_KEY_COUNT;
    if (sc == SDL_SCANCODE_UP)   { m->sel = (m->sel + rows - 1) % rows; m->note[0] = 0; return WP_MENU_NONE; }
    if (sc == SDL_SCANCODE_DOWN) { m->sel = (m->sel + 1) % rows; m->note[0] = 0; return WP_MENU_NONE; }
    if (m->screen == WP_MENU_MAIN && m->sel == ITEM_DIFFICULTY &&
        (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_RETURN)) {
        m->difficulty = (m->difficulty + (sc == SDL_SCANCODE_LEFT ? 2 : 1)) % 3;
        wp_settings_save(m->difficulty, m->assist, m->switching);
        snprintf(m->note, sizeof m->note, "Opposition: %s - applies when you restart the match", DIFF_NAME[m->difficulty]);
        return WP_MENU_NONE;
    }
    if (m->screen == WP_MENU_MAIN && m->sel == ITEM_ASSIST &&
        (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_RETURN)) {
        m->assist = (m->assist + (sc == SDL_SCANCODE_LEFT ? 3 : 1)) % 4;
        wp_settings_save(m->difficulty, m->assist, m->switching);
        snprintf(m->note, sizeof m->note, "Movement assist: %s - applies when you restart the match", ASSIST_NAME[m->assist]);
        return WP_MENU_NONE;
    }
    if (m->screen == WP_MENU_MAIN && m->sel == ITEM_SWITCH &&
        (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT || sc == SDL_SCANCODE_RETURN)) {   /* Manual/Events/Assisted */
        int k = (m->switching < WP_SWITCH_MANUAL || m->switching >= WP_SWITCH_MODES ? WP_SWITCH_EVENTS : m->switching) - 1;
        m->switching = 1 + (k + (sc == SDL_SCANCODE_LEFT ? 2 : 1)) % 3;
        wp_settings_save(m->difficulty, m->assist, m->switching);
        snprintf(m->note, sizeof m->note, "Auto switching: %s - applies when you restart the match", SWITCH_NAME[m->switching]);
        return WP_MENU_NONE;
    }
    if (sc == SDL_SCANCODE_ESCAPE) {
        if (m->screen == WP_MENU_CONTROLS) { m->screen = WP_MENU_MAIN; m->sel = ITEM_CONTROLS; m->note[0] = 0; return WP_MENU_NONE; }
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
        if (m->sel == ITEM_RESUME) { m->screen = WP_MENU_CLOSED; return WP_MENU_RESUME; }
        if (m->sel == ITEM_RESTART) { m->screen = WP_MENU_CLOSED; return WP_MENU_RESTART; }
        if (m->sel == ITEM_CONTROLS) { m->screen = WP_MENU_CONTROLS; m->sel = 0; return WP_MENU_NONE; }
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
        for (int i = 0; i < ITEM_COUNT; i++, y += lh * 1.6f) {
            bool on = m->sel == i;
            char buf[96];
            static const char *items[ITEM_COUNT] = {"Resume", NULL, NULL, NULL, "Restart match", "Controls", "Quit game"};
            if (i == ITEM_DIFFICULTY) snprintf(buf, sizeof buf, "%s Difficulty: < %s >", on ? ">" : " ", DIFF_NAME[m->difficulty]);
            else if (i == ITEM_ASSIST) snprintf(buf, sizeof buf, "%s Movement assist: < %s >", on ? ">" : " ", ASSIST_NAME[m->assist]);
            else if (i == ITEM_SWITCH) snprintf(buf, sizeof buf, "%s Auto switching: < %s >", on ? ">" : " ",
                                                SWITCH_NAME[m->switching >= 0 && m->switching < 4 ? m->switching : 2]);
            else snprintf(buf, sizeof buf, "%s %s", on ? ">" : " ", items[i]);
            text(r, x0, y, sc, buf, on ? 255 : 170, on ? 225 : 170, on ? 90 : 170);
        }
        text(r, x0, y + lh, sc * 0.75f, "Up/Down choose, Enter select, Left/Right change, Esc resume", 120, 130, 120);
        if (m->note[0]) text(r, x0, y + lh * 2, sc * 0.75f, m->note, 120, 220, 140);
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
