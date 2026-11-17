/* title.c - see title.h. Up/Down choose, Left/Right change, Enter select, Esc back. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/title.h"
#include "game/menu.h"         /* wp_menu_text */
#include "game/dedication.h"   /* WP_DEDICATION_TEXT, wp_draw_heart (D-018: the words are never retyped) */
#include "game/kits.h"

enum { ACT_QUICK = -1, ACT_CONTROLS = -2, ACT_SETTINGS = -3, ACT_QUIT = -4, ACT_KICKOFF = -5, ACT_BACK = -6 };
static const int MAIN_ROWS[] = {ACT_QUICK, ACT_CONTROLS, ACT_SETTINGS, ACT_QUIT};
static const int SETUP_ROWS[] = {WP_SET_FORMAT, WP_SET_DIFFICULTY, WP_SET_ASSIST, WP_SET_SWITCHING, WP_SET_HALF,
                                 WP_SET_KIT0, WP_SET_KIT1, ACT_KICKOFF, ACT_BACK};
static const int SETTINGS_ROWS[] = {WP_SET_VOL_MASTER, WP_SET_VOL_FX, WP_SET_VOL_CROWD, WP_SET_FPS, ACT_BACK};
#define NROWS(a) ((int)(sizeof a / sizeof a[0]))

static const int *rows_of(WpTitleScreen s, int *n) {
    if (s == WP_TS_SETUP) { *n = NROWS(SETUP_ROWS); return SETUP_ROWS; }
    if (s == WP_TS_SETTINGS) { *n = NROWS(SETTINGS_ROWS); return SETTINGS_ROWS; }
    *n = NROWS(MAIN_ROWS);
    return MAIN_ROWS;
}

void wp_title_open(WpTitle *t) {
    WpSettings *s = t->set;
    memset(t, 0, sizeof *t);
    t->set = s;
}

static void go(WpTitle *t, WpTitleScreen s, int sel) { t->screen = s; t->sel = sel; t->note[0] = 0; }

WpTitleResult wp_title_event(WpTitle *t, const SDL_Event *e) {
    if (e->type != SDL_EVENT_KEY_DOWN || e->key.repeat) return WP_TITLE_NONE;
    SDL_Scancode sc = e->key.scancode;
    int n;
    const int *rows = rows_of(t->screen, &n);
    if (sc == SDL_SCANCODE_UP)   { t->sel = (t->sel + n - 1) % n; t->note[0] = 0; return WP_TITLE_NONE; }
    if (sc == SDL_SCANCODE_DOWN) { t->sel = (t->sel + 1) % n; t->note[0] = 0; return WP_TITLE_NONE; }
    if (sc == SDL_SCANCODE_ESCAPE) {
        if (t->screen == WP_TS_SETUP) go(t, WP_TS_MAIN, 0);
        else if (t->screen == WP_TS_SETTINGS) go(t, WP_TS_MAIN, 2);
        return WP_TITLE_NONE;
    }
    int row = rows[t->sel];
    bool enter = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER;
    if (row >= 0 && t->set && (enter || sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT)) {
        wp_settings_row_change(t->set, (WpSetRow)row, sc == SDL_SCANCODE_LEFT ? -1 : 1);
        if (!wp_settings_save(t->set)) snprintf(t->note, sizeof t->note, "Couldn't save settings (kept for this session)");
        return WP_TITLE_CHANGED;
    }
    if (!enter) return WP_TITLE_NONE;
    switch (row) {
    case ACT_QUICK:    go(t, WP_TS_SETUP, NROWS(SETUP_ROWS) - 2); return WP_TITLE_NONE;   /* on "Kick off!" */
    case ACT_SETTINGS: go(t, WP_TS_SETTINGS, 0); return WP_TITLE_NONE;
    case ACT_CONTROLS: return WP_TITLE_CONTROLS;
    case ACT_QUIT:     return WP_TITLE_QUIT;
    case ACT_KICKOFF:  return WP_TITLE_KICKOFF;
    case ACT_BACK:     go(t, WP_TS_MAIN, t->screen == WP_TS_SETUP ? 0 : 2); return WP_TITLE_NONE;
    default:           return WP_TITLE_NONE;
    }
}

static void rect(SDL_Renderer *r, float x, float y, float w, float h, SDL_FColor c) {
    SDL_SetRenderDrawColorFloat(r, c.r, c.g, c.b, c.a);
    SDL_FRect q = {x, y, w, h};
    SDL_RenderFillRect(r, &q);
}

/* a quiet pitch behind the menu: mown stripes, the halfway line and centre circle (static: drawn only on input) */
static void backdrop(SDL_Renderer *r, float W, float H) {
    SDL_SetRenderDrawColor(r, 12, 30, 17, 255);
    SDL_RenderClear(r);
    for (int i = 0; i < 12; i++)
        rect(r, W * (float)i / 12.0f, 0, W / 12.0f + 1, H, (i & 1) ? (SDL_FColor){0.07f, 0.17f, 0.09f, 1} : (SDL_FColor){0.08f, 0.19f, 0.10f, 1});
    SDL_SetRenderDrawColor(r, 60, 96, 66, 255);
    SDL_RenderLine(r, W * 0.5f, 0, W * 0.5f, H);
    float cr = H * 0.22f;
    for (int i = 0; i < 64; i++) {
        float a0 = (float)i * 6.2831853f / 64, a1 = (float)(i + 1) * 6.2831853f / 64;
        SDL_RenderLine(r, W * 0.5f + cosf(a0) * cr, H * 0.55f + sinf(a0) * cr * 0.6f,
                       W * 0.5f + cosf(a1) * cr, H * 0.55f + sinf(a1) * cr * 0.6f);
    }
}

void wp_title_draw(const WpTitle *t, SDL_Renderer *r, int w, int h) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    float W = (float)w, H = (float)h;
    backdrop(r, W, H);
    float big = floorf(fminf(7.0f, W * 0.6f / (8.0f * 9.0f)));
    if (big < 2) big = 2;
    const char *name = "WILDPITCH";
    float tw = 8.0f * 9.0f * big, ty = H * 0.09f;
    wp_menu_text(r, (W - tw) * 0.5f + big * 0.6f, ty + big * 0.6f, big, name, 0, 0, 0);
    wp_menu_text(r, (W - tw) * 0.5f, ty, big, name, 245, 232, 120);
    float sc = W >= 1100.0f ? 2.0f : 1.0f, lh = 14.0f * sc;
    const char *sub = t->screen == WP_TS_SETUP ? "QUICK MATCH" : t->screen == WP_TS_SETTINGS ? "SETTINGS"
                    : "a football game";
    wp_menu_text(r, (W - 8.0f * (float)strlen(sub) * sc) * 0.5f, ty + 8.0f * big + lh * 0.8f, sc, sub, 170, 200, 170);

    int n;
    const int *rows = rows_of(t->screen, &n);
    /* the panel sits between the title and the footer whatever the row count */
    float gap = n > 6 ? 1.35f : 1.55f, pw = 400.0f * sc, px = (W - pw) * 0.5f, ph = (float)n * lh * gap + lh * 1.2f;
    float top = ty + 8.0f * big + lh * 2.4f, bottom = H - 28.0f * sc - lh * 2.6f;
    float py = fmaxf(top, fminf(H * 0.36f, bottom - ph));
    rect(r, px, py, pw, ph, (SDL_FColor){0.02f, 0.05f, 0.03f, 0.72f});
    float y = py + lh * 0.7f;
    for (int i = 0; i < n; i++, y += lh * gap) {
        bool on = t->sel == i;
        char txt[80], buf[96];
        int row = rows[i];
        if (row >= 0 && t->set) wp_settings_row_text(t->set, (WpSetRow)row, txt, (int)sizeof txt);
        else snprintf(txt, sizeof txt, "%s", row == ACT_QUICK ? "Quick match" : row == ACT_CONTROLS ? "Controls"
                                       : row == ACT_SETTINGS ? "Settings" : row == ACT_QUIT ? "Quit"
                                       : row == ACT_KICKOFF ? "Kick off!" : "Back");
        snprintf(buf, sizeof buf, "%s %s", on ? ">" : " ", txt);
        wp_menu_text(r, px + lh, y, sc, buf, on ? 255 : 185, on ? 225 : 190, on ? 90 : 185);
        if ((row == WP_SET_KIT0 || row == WP_SET_KIT1) && t->set) {      /* swatch: shirt over shorts */
            const WpKit *k = wp_kit(t->set->kit[row == WP_SET_KIT1]);
            float sx = px + pw - lh * 2.2f, s = lh * 0.9f;
            rect(r, sx - 1, y - 1, s + 2, s * 1.3f + 2, (SDL_FColor){0, 0, 0, 1});
            rect(r, sx, y, s, s * 0.8f, k->shirt);
            rect(r, sx, y + s * 0.8f, s, s * 0.5f, k->shorts);
        }
    }
    const char *help = t->screen == WP_TS_MAIN ? "Up/Down choose   Enter select"
                                               : "Up/Down choose   Left/Right change   Enter select   Esc back";
    wp_menu_text(r, (W - 8.0f * (float)strlen(help) * sc * 0.75f) * 0.5f, py + ph + lh * 0.6f, sc * 0.75f, help, 120, 140, 120);
    if (t->note[0])
        wp_menu_text(r, (W - 8.0f * (float)strlen(t->note) * sc * 0.75f) * 0.5f, py + ph + lh * 1.6f, sc * 0.75f, t->note, 120, 220, 140);

    /* footer: the owner's dedication (D-018), the same line and heart as the launch card */
    const char *ded = WP_DEDICATION_TEXT;
    float ds = sc, len = 8.0f * (float)strlen(ded) * ds, heart = 10.0f * ds;
    float dx = (W - (len + 6.0f * ds + heart)) * 0.5f, dy = H - 28.0f * ds;
    wp_menu_text(r, dx, dy, ds, ded, 236, 232, 222);
    wp_draw_heart(r, dx + len + 6.0f * ds + heart * 0.5f, dy + 4.0f * ds, heart, 1.0f);
}
