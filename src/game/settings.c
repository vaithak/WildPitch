/* settings.c - see settings.h. */
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "game/settings.h"
#include "game/kits.h"
#include "sim/sim.h"

static const char *DIFF_ID[3] = {"easy", "normal", "hard"};
static const char *DIFF_NAME[3] = {"Easy", "Normal", "Hard"};
static const char *ASSIST_ID[4] = {"off", "low", "normal", "high"};
static const char *ASSIST_NAME[4] = {"Off", "Low", "Normal", "High"};
static const char *SWITCH_ID[4] = {"legacy", "manual", "events", "assisted"};   /* WP_SWITCH_* (D-050) */
static const char *SWITCH_NAME[4] = {"Legacy", "Manual", "Events", "Assisted"};
static const char *FORMAT_ID[2] = {"7v7", "5v5"};
static const int HALF_MIN[3] = {3, 5, 8};

void wp_settings_defaults(WpSettings *s) {
    memset(s, 0, sizeof *s);
    s->difficulty = WP_AI_NORMAL;
    s->assist = WP_ASSIST_NORMAL;
    s->switching = WP_SWITCH_EVENTS;
    s->vol_master = 8; s->vol_fx = 8; s->vol_crowd = 6;
    s->half_min = 5;
    s->format = WP_FORMAT_7V7;
    s->kit[0] = 0; s->kit[1] = 1;
}

static int find_id(const char *v, const char *const *ids, int n) {
    for (int k = 0; k < n; k++) if (!strcmp(v, ids[k])) return k;
    return -1;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void sanitize(WpSettings *s) {
    s->difficulty = clampi(s->difficulty, 0, 2);
    s->assist = clampi(s->assist, 0, 3);
    if (s->switching < WP_SWITCH_MANUAL || s->switching >= WP_SWITCH_MODES) s->switching = WP_SWITCH_EVENTS;
    s->vol_master = clampi(s->vol_master, 0, 10);
    s->vol_fx = clampi(s->vol_fx, 0, 10);
    s->vol_crowd = clampi(s->vol_crowd, 0, 10);
    if (s->half_min != 3 && s->half_min != 5 && s->half_min != 8) s->half_min = 5;
    s->format = clampi(s->format, 0, 1);
    for (int t = 0; t < 2; t++) s->kit[t] = clampi(s->kit[t], 0, WP_KIT_COUNT - 1);
    if (s->kit[0] == s->kit[1]) s->kit[1] = (s->kit[0] + 1) % WP_KIT_COUNT;
}

void wp_settings_from_text(WpSettings *s, const char *text) {
    const char *p = text;
    while (p && *p) {
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        char line[128];
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n); line[n] = 0;
        p = e ? e + 1 : NULL;
        char key[48], val[48];
        if (line[0] == '#' || sscanf(line, " %47[a-z_0-9] = %47s", key, val) != 2) continue;
        int k, iv = 0;
        bool num = sscanf(val, "%d", &iv) == 1;
        if (!strcmp(key, "difficulty") && (k = find_id(val, DIFF_ID, 3)) >= 0) s->difficulty = k;
        else if (!strcmp(key, "assist") && (k = find_id(val, ASSIST_ID, 4)) >= 0) s->assist = k;
        else if (!strcmp(key, "switching") && (k = find_id(val, SWITCH_ID, 4)) >= WP_SWITCH_MANUAL) s->switching = k;
        else if (!strcmp(key, "volume_master") && num) s->vol_master = iv;
        else if (!strcmp(key, "volume_effects") && num) s->vol_fx = iv;
        else if (!strcmp(key, "volume_crowd") && num) s->vol_crowd = iv;
        else if (!strcmp(key, "muted") && num) s->muted = iv != 0;
        else if (!strcmp(key, "frame_rate") && num) s->battery = iv == 30;
        else if (!strcmp(key, "half_minutes") && num) s->half_min = iv;
        else if (!strcmp(key, "format") && (k = find_id(val, FORMAT_ID, 2)) >= 0) s->format = k;
        else if (!strcmp(key, "kit_home") && num) s->kit[0] = iv;
        else if (!strcmp(key, "kit_away") && num) s->kit[1] = iv;
    }
    sanitize(s);
}

int wp_settings_to_text(const WpSettings *in, char *buf, int cap) {
    WpSettings s = *in;
    sanitize(&s);
    return snprintf(buf, (size_t)cap,
                    "# WildPitch settings\ndifficulty = %s\nassist = %s\nswitching = %s\n"
                    "volume_master = %d\nvolume_effects = %d\nvolume_crowd = %d\nmuted = %d\nframe_rate = %d\n"
                    "half_minutes = %d\nformat = %s\nkit_home = %d\nkit_away = %d\n",
                    DIFF_ID[s.difficulty], ASSIST_ID[s.assist], SWITCH_ID[s.switching], s.vol_master, s.vol_fx,
                    s.vol_crowd, s.muted ? 1 : 0, s.battery ? 30 : 60, s.half_min, FORMAT_ID[s.format], s.kit[0], s.kit[1]);
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

void wp_settings_load(WpSettings *s) {
    wp_settings_defaults(s);
    char *f = settings_file();
    if (!f) return;
    size_t len = 0;
    char *text = (char *)SDL_LoadFile(f, &len);
    SDL_free(f);
    if (!text) return;
    wp_settings_from_text(s, text);
    SDL_free(text);
}

bool wp_settings_save(const WpSettings *s) {
    char *f = settings_file();
    if (!f) return false;
    char buf[512];
    int n = wp_settings_to_text(s, buf, (int)sizeof buf);
    bool ok = n > 0 && n < (int)sizeof buf && SDL_SaveFile(f, buf, (size_t)n);
    SDL_free(f);
    return ok;
}

void wp_settings_row_text(const WpSettings *s, WpSetRow row, char *buf, int cap) {
    size_t c = (size_t)cap;
    switch (row) {
    case WP_SET_FORMAT:     snprintf(buf, c, "Format: < %s >", s->format == WP_FORMAT_5V5 ? "5 v 5" : "7 v 7"); break;
    case WP_SET_DIFFICULTY: snprintf(buf, c, "Difficulty: < %s >", DIFF_NAME[clampi(s->difficulty, 0, 2)]); break;
    case WP_SET_ASSIST:     snprintf(buf, c, "Movement assist: < %s >", ASSIST_NAME[clampi(s->assist, 0, 3)]); break;
    case WP_SET_SWITCHING:  snprintf(buf, c, "Auto switching: < %s >", SWITCH_NAME[clampi(s->switching, 0, 3)]); break;
    case WP_SET_HALF:       snprintf(buf, c, "Half length: < %d min >", s->half_min); break;
    case WP_SET_KIT0:       snprintf(buf, c, "Your kit: < %s >", wp_kit(s->kit[0])->name); break;
    case WP_SET_KIT1:       snprintf(buf, c, "Opponent kit: < %s >", wp_kit(s->kit[1])->name); break;
    case WP_SET_VOL_MASTER: snprintf(buf, c, "Master volume: < %d >%s", s->vol_master, s->muted ? "  (muted: M)" : ""); break;
    case WP_SET_VOL_FX:     snprintf(buf, c, "Effects volume: < %d >", s->vol_fx); break;
    case WP_SET_VOL_CROWD:  snprintf(buf, c, "Crowd volume: < %d >", s->vol_crowd); break;
    case WP_SET_FPS:        snprintf(buf, c, "Frame rate: < %s >", s->battery ? "30 (battery)" : "60"); break;
    default:                snprintf(buf, c, "?"); break;
    }
}

void wp_settings_row_change(WpSettings *s, WpSetRow row, int dir) {
    int d = dir < 0 ? -1 : 1;
    switch (row) {
    case WP_SET_FORMAT:     s->format = 1 - s->format; break;
    case WP_SET_DIFFICULTY: s->difficulty = (s->difficulty + 3 + d) % 3; break;
    case WP_SET_ASSIST:     s->assist = (s->assist + 4 + d) % 4; break;
    case WP_SET_SWITCHING: {                                   /* Manual / Events / Assisted (LEGACY is tests only) */
        int k = (s->switching < WP_SWITCH_MANUAL || s->switching >= WP_SWITCH_MODES ? WP_SWITCH_EVENTS : s->switching) - 1;
        s->switching = 1 + (k + 3 + d) % 3;
        break;
    }
    case WP_SET_HALF: {
        int k = s->half_min == 3 ? 0 : s->half_min == 8 ? 2 : 1;
        s->half_min = HALF_MIN[(k + 3 + d) % 3];
        break;
    }
    case WP_SET_KIT0: case WP_SET_KIT1: {                      /* never the other team's kit */
        int t = row == WP_SET_KIT1, k = s->kit[t];
        do k = (k + WP_KIT_COUNT + d) % WP_KIT_COUNT; while (k == s->kit[1 - t]);
        s->kit[t] = k;
        break;
    }
    case WP_SET_VOL_MASTER: s->vol_master = clampi(s->vol_master + d, 0, 10); if (d > 0) s->muted = false; break;
    case WP_SET_VOL_FX:     s->vol_fx = clampi(s->vol_fx + d, 0, 10); break;
    case WP_SET_VOL_CROWD:  s->vol_crowd = clampi(s->vol_crowd + d, 0, 10); break;
    case WP_SET_FPS:        s->battery = !s->battery; break;
    default: break;
    }
}

static bool same(const WpSettings *a, const WpSettings *b) {
    return a->difficulty == b->difficulty && a->assist == b->assist && a->switching == b->switching &&
           a->vol_master == b->vol_master && a->vol_fx == b->vol_fx && a->vol_crowd == b->vol_crowd &&
           a->muted == b->muted && a->battery == b->battery && a->half_min == b->half_min && a->format == b->format &&
           a->kit[0] == b->kit[0] && a->kit[1] == b->kit[1];
}

int wp_check_settings(void) {
    int bad = 0;
    WpSettings a, b;
    wp_settings_defaults(&a);
    bad += a.difficulty != WP_AI_NORMAL || a.assist != WP_ASSIST_NORMAL || a.switching != WP_SWITCH_EVENTS ||
           a.half_min != 5 || a.format != WP_FORMAT_7V7 || a.battery || a.kit[0] == a.kit[1];
    /* a pre-CP0.6 file still loads (and other keys keep their defaults) */
    wp_settings_defaults(&b);
    wp_settings_from_text(&b, "# WildPitch settings\ndifficulty = hard\nassist = low\nswitching = assisted\n");
    bad += b.difficulty != WP_AI_HARD || b.assist != WP_ASSIST_LOW || b.switching != WP_SWITCH_ASSISTED || b.vol_master != 8;
    /* round trip of every field */
    a.difficulty = 0; a.assist = 3; a.switching = WP_SWITCH_MANUAL; a.vol_master = 3; a.vol_fx = 10; a.vol_crowd = 0;
    a.muted = true; a.battery = true; a.half_min = 8; a.format = WP_FORMAT_5V5; a.kit[0] = 4; a.kit[1] = 2;
    char buf[512];
    wp_settings_to_text(&a, buf, (int)sizeof buf);
    wp_settings_defaults(&b);
    wp_settings_from_text(&b, buf);
    bad += !same(&a, &b);
    /* garbage is ignored, out-of-range values are clamped, kits never clash */
    wp_settings_defaults(&b);
    wp_settings_from_text(&b, "volume_master = 99\nhalf_minutes = 7\nkit_home = 1\nkit_away = 1\nnonsense\n= =\n");
    bad += b.vol_master != 10 || b.half_min != 5 || b.kit[0] == b.kit[1];
    /* rows cycle and stay valid */
    wp_settings_defaults(&b);
    for (int r = 0; r < WP_SET_COUNT; r++)
        for (int i = 0; i < 25; i++) {
            wp_settings_row_change(&b, (WpSetRow)r, (i % 3 == 0) ? -1 : 1);
            WpSettings c = b;
            sanitize(&c);
            bad += !same(&b, &c);
            bad += b.switching == WP_SWITCH_LEGACY;
        }
    printf("settings check: %s\n", bad ? "FAIL" : "ok");
    return bad ? 1 : 0;
}
