/* keymap.c - see keymap.h. */
#include <stdio.h>
#include <string.h>
#include "game/keymap.h"

static const struct { const char *id, *name; SDL_Scancode def; } ACTIONS[WP_KEY_COUNT] = {
    [WP_KEY_UP]     = {"up",     "Move up",                     SDL_SCANCODE_UP},
    [WP_KEY_DOWN]   = {"down",   "Move down",                   SDL_SCANCODE_DOWN},
    [WP_KEY_LEFT]   = {"left",   "Move left",                   SDL_SCANCODE_LEFT},
    [WP_KEY_RIGHT]  = {"right",  "Move right",                  SDL_SCANCODE_RIGHT},
    [WP_KEY_SPRINT] = {"sprint", "Sprint",                      SDL_SCANCODE_E},
    [WP_KEY_PASS]   = {"pass",   "Pass (hold: through) / tackle", SDL_SCANCODE_S},
    [WP_KEY_SHOOT]  = {"shoot",  "Shoot (hold to charge)",      SDL_SCANCODE_D},
    [WP_KEY_LOB]    = {"lob",    "Lofted pass",                 SDL_SCANCODE_A},
    [WP_KEY_CHIP]   = {"chip",   "Chip shot (hold)",            SDL_SCANCODE_W},
    [WP_KEY_SWITCH] = {"switch", "Switch player",               SDL_SCANCODE_Q},
    [WP_KEY_ZOOM]   = {"zoom",   "Zoom",                        SDL_SCANCODE_C},
    [WP_KEY_LOOK]   = {"look",   "Change look",                 SDL_SCANCODE_P},
    [WP_KEY_REPLAY] = {"replay", "Replay last 10 s",            SDL_SCANCODE_R},
};

void wp_keymap_defaults(WpKeymap *m) {
    for (int a = 0; a < WP_KEY_COUNT; a++) m->key[a] = ACTIONS[a].def;
}
const char *wp_keymap_action_name(WpKeyAction a) { return ACTIONS[a].name; }
const char *wp_keymap_action_id(WpKeyAction a) { return ACTIONS[a].id; }
const char *wp_keymap_key_name(const WpKeymap *m, WpKeyAction a) {
    const char *n = SDL_GetScancodeName(m->key[a]);
    return n && n[0] ? n : "?";
}

int wp_keymap_to_text(const WpKeymap *m, char *buf, int cap) {
    int n = snprintf(buf, (size_t)cap, "# WildPitch controls: action = key (SDL key names)\n");
    for (int a = 0; a < WP_KEY_COUNT && n < cap; a++)
        n += snprintf(buf + n, (size_t)(cap - n), "%s = %s\n", ACTIONS[a].id, wp_keymap_key_name(m, (WpKeyAction)a));
    return n;
}

void wp_keymap_from_text(WpKeymap *m, const char *text) {
    char line[128];
    const char *p = text;
    while (*p) {
        int len = 0;
        while (p[len] && p[len] != '\n') len++;
        int cl = len < 127 ? len : 127;
        memcpy(line, p, (size_t)cl); line[cl] = 0;
        p += len + (p[len] == '\n');
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq) continue;
        *eq = 0;
        char *id = line, *key = eq + 1;
        while (*id == ' ') id++;
        for (char *e = id + strlen(id); e > id && e[-1] == ' '; ) *--e = 0;
        while (*key == ' ') key++;
        for (char *e = key + strlen(key); e > key && (e[-1] == ' ' || e[-1] == '\r'); ) *--e = 0;
        SDL_Scancode sc = SDL_GetScancodeFromName(key);
        if (sc == SDL_SCANCODE_UNKNOWN) continue;
        for (int a = 0; a < WP_KEY_COUNT; a++)
            if (!strcmp(id, ACTIONS[a].id)) wp_keymap_bind(m, (WpKeyAction)a, sc);
    }
}

void wp_keymap_bind(WpKeymap *m, WpKeyAction a, SDL_Scancode key) {
    for (int b = 0; b < WP_KEY_COUNT; b++)
        if (b != (int)a && m->key[b] == key) m->key[b] = m->key[a];   /* swap: one action per key */
    m->key[a] = key;
}

static char *pref_file(void) {
    char *dir = SDL_GetPrefPath("WildPitch", "WildPitch");
    if (!dir) return NULL;
    size_t n = strlen(dir) + 16;
    char *f = (char *)SDL_malloc(n);
    if (f) snprintf(f, n, "%scontrols.cfg", dir);
    SDL_free(dir);
    return f;
}

bool wp_keymap_load(WpKeymap *m) {
    char *f = pref_file();
    if (!f) return false;
    size_t len = 0;
    char *text = (char *)SDL_LoadFile(f, &len);
    SDL_free(f);
    if (!text) return false;
    wp_keymap_from_text(m, text);
    SDL_free(text);
    return true;
}

bool wp_keymap_save(const WpKeymap *m) {
    char *f = pref_file();
    if (!f) return false;
    char buf[2048];
    int n = wp_keymap_to_text(m, buf, (int)sizeof buf);
    bool ok = SDL_SaveFile(f, buf, (size_t)n);
    SDL_free(f);
    return ok;
}

WpSeatInput wp_keymap_read(const WpKeymap *m, const bool *k) {
    WpSeatInput in = {0, 0, 0};
    int mx = k[m->key[WP_KEY_RIGHT]] - k[m->key[WP_KEY_LEFT]];
    int my = k[m->key[WP_KEY_UP]] - k[m->key[WP_KEY_DOWN]];
    in.move_x = (int8_t)(mx * 127);
    in.move_y = (int8_t)(my * 127);
    if (k[m->key[WP_KEY_PASS]])   in.buttons |= WP_BTN_PASS;
    if (k[m->key[WP_KEY_SHOOT]])  in.buttons |= WP_BTN_SHOOT;
    if (k[m->key[WP_KEY_SPRINT]]) in.buttons |= WP_BTN_SPRINT;
    if (k[m->key[WP_KEY_SWITCH]]) in.buttons |= WP_BTN_SWITCH;
    if (k[m->key[WP_KEY_CHIP]])   in.buttons |= WP_BTN_CHIP;
    if (k[m->key[WP_KEY_LOB]])    in.buttons |= WP_BTN_LOB;
    return in;
}
