/* main.c - platform loop: window, input -> WpInput, fixed 60 Hz sim step, interpolated render.
 * The loop never spins: vsync blocks (interval chosen so we present ~60 fps even on 120/144 Hz screens),
 * and if vsync is unavailable we sleep to the frame cap.
 *
 * Flags (for humans and for agents doing visual checks):
 *   --demo                 8 scripted players run/kick/jump (avatar motion check; test harness only)
 *   --gallery              pose sheet of the avatar at close-up and match scale
 *   --style N              avatar preset (0 Modern, 1 Arcade, 2 Retro pixel); P cycles in game
 *   --zoom N               camera zoom preset (0 wide, 1 medium, 2 close); Z cycles in game
 *   --screenshot F.bmp     save frame number --frames N (default 60) to F.bmp and exit
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim/sim.h"
#include "game/render.h"

static const float ZOOM_VIEW_M[3] = {50.0f, 36.0f, 27.0f};   /* metres visible across the screen */
static const char *ZOOM_NAME[3] = {"wide", "medium", "close"};

typedef struct {
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Texture  *lowres;          /* retro pixel render target */
    int           lowres_w, lowres_h;
    SimState      prev, cur;
    WpCamera      cam;
    WpRenderCtx   rctx;
    int           zoom;
    bool          show_debug, demo;
    double        sim_ms_avg;
} Game;

static WpSeatInput read_keyboard(void) {
    const bool *k = SDL_GetKeyboardState(NULL);
    WpSeatInput in = {0, 0, 0};
    int mx = (k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) - (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]);
    int my = (k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) - (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]);
    in.move_x = (int8_t)(mx * 127);
    in.move_y = (int8_t)(my * 127);
    if (k[SDL_SCANCODE_J] || k[SDL_SCANCODE_SPACE]) in.buttons |= WP_BTN_PASS;
    if (k[SDL_SCANCODE_K]) in.buttons |= WP_BTN_SHOOT;
    if (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) in.buttons |= WP_BTN_SPRINT;
    if (k[SDL_SCANCODE_L]) in.buttons |= WP_BTN_SWITCH;
    if (k[SDL_SCANCODE_I]) in.buttons |= WP_BTN_CHIP;
    return in;
}

/* ---- demo harness: scripted runners so avatar motion can be judged without AI. Writes action/jump
 * directly into the sim — acceptable ONLY because this is a test harness, not gameplay. ---- */
static void demo_setup(SimState *s) {
    int k = 0;
    for (int i = 0; i < 2 * s->n_per_side && k < WP_MAX_SEATS; i++)
        if (s->players[i].role != WP_ROLE_KEEPER) s->seat_player[k++] = (int8_t)i;
}

static void demo_input(SimState *s, WpInput *in) {
    uint32_t t = s->tick;
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        int i = s->seat_player[k];
        if (i < 0) continue;
        WpPlayer *p = &s->players[i];
        float x = (float)p->pos.x / 65536.0f, y = (float)p->pos.y / 65536.0f;
        float ang = (float)t * 0.004f * (1.0f + (float)k * 0.31f) + (float)k * 1.7f;
        float dx = cosf(ang), dy = sinf(ang);
        dx -= x * 0.06f; dy -= y * 0.09f;                    /* stay on the pitch */
        float len = sqrtf(dx * dx + dy * dy);
        int phase = (int)((t / 120 + (uint32_t)k) % 4);       /* walk / jog / sprint / stand */
        float mag = phase == 3 ? 0.0f : (phase == 0 ? 0.35f : 1.0f);
        in->seat[k].move_x = (int8_t)(dx / len * 127.0f * mag);
        in->seat[k].move_y = (int8_t)(dy / len * 127.0f * mag);
        in->seat[k].buttons = phase == 2 ? WP_BTN_SPRINT : 0;
        if ((t + (uint32_t)k * 37) % 200 == 0) sim_player_jump(s, i, WP_ACT_JUMP_HEADER);
        if ((t + (uint32_t)k * 53) % 170 == 0 && p->z == 0) { p->action = WP_ACT_KICK; p->action_timer = 0; }
    }
}

/* ---- gallery: fixed poses drawn directly through the avatar API ---- */
static void draw_gallery(Game *g, int w, int h) {
    typedef struct { const char *label; float facing, speed, phase, z; uint8_t action; float at; bool keeper; } GP;
    const float PI = 3.14159265f;
    GP poses[] = {
        {"idle front", -PI / 2, 0, 0, 0, 0, 0, false}, {"idle 3/4", -PI / 4, 0, 0, 0, 0, 0, false},
        {"idle side", 0, 0, 0, 0, 0, 0, false},         {"idle back", PI / 2, 0, 0, 0, 0, 0, false},
        {"run 0", -PI / 5, 7, 0, 0, 0, 0, false},       {"run 1/4", -PI / 5, 7, PI / 2, 0, 0, 0, false},
        {"run 1/2", -PI / 5, 7, PI, 0, 0, 0, false},    {"run 3/4", -PI / 5, 7, 1.5f * PI, 0, 0, 0, false},
        {"kick back", -PI / 6, 0, 0, 0, WP_ACT_KICK, 0.17f, false},
        {"kick strike", -PI / 6, 0, 0, 0, WP_ACT_KICK, 0.29f, false},
        {"header rise", -PI / 3, 0, 0, 0.35f, WP_ACT_JUMP_HEADER, 0.18f, false},
        {"header nod", -PI / 3, 0, 0, 0.58f, WP_ACT_JUMP_HEADER, 0.34f, false},
        {"keeper", -PI / 2, 0, 0, 0, 0, 0, true},       {"run side", 0, 5, PI / 3, 0, 0, 0, false},
    };
    int np = (int)(sizeof poses / sizeof poses[0]);
    const WpAvatarStyle *st = &wp_avatar_styles[g->rctx.style];
    SDL_SetRenderDrawColor(g->ren, 64, 140, 70, 255);
    SDL_RenderClear(g->ren);
    int cols = 7, cw = w / cols;
    float view = ZOOM_VIEW_M[g->zoom];
    for (int pass = 0; pass < 3; pass++) {           /* rows 0,1: close-up; row 2: match scale */
        for (int i = 0; i < (pass < 2 ? cols : np); i++) {
            int pi = pass < 2 ? pass * cols + i : i;
            if (pi >= np) break;
            GP *p = &poses[pi];
            int cellw = pass < 2 ? cw : w / np, cellh = pass < 2 ? (int)((float)h * 0.36f) : (int)((float)h * 0.28f);
            SDL_Rect vp = {pass < 2 ? i * cw : i * cellw, pass < 2 ? pass * cellh : 2 * (int)((float)h * 0.36f), cellw, cellh};
            SDL_SetRenderViewport(g->ren, &vp);
            WpCamera cam = {0, 0, pass < 2 ? (float)cellh / 2.6f : (float)w / view, 0.62f, 0.0f, cellw, cellh};
            WpAvatarLook look;
            wp_avatar_make_look(&look, 7, pi * 3 + 1, pi & 1, p->keeper);
            WpAvatarPose pose = {p->facing, p->speed, p->phase, p->z, p->action, p->at, 0, p->keeper};
            float yoff = -(float)cellh * 0.30f / (cam.ppm * cam.tilt);   /* move feet down the cell */
            WpScreenPt sh = wp_project(&cam, 0.1f, yoff - 0.06f, 0);
            wp_soft_disc(g->ren, g->rctx.soft, sh.x, sh.y, 0.4f * cam.ppm, 0.4f * cam.ppm * cam.tilt, (SDL_FColor){0, 0, 0, 0.3f});
            wp_avatar_draw(g->ren, g->rctx.soft, &cam, 0, yoff, &pose, &look, st);
            SDL_SetRenderDrawColor(g->ren, 255, 255, 255, 255);
            if (pass < 2) SDL_RenderDebugText(g->ren, 6, 6, p->label);
        }
    }
    SDL_SetRenderViewport(g->ren, NULL);
    char buf[96];
    snprintf(buf, sizeof buf, "style: %s   (bottom row = match scale, zoom %s)", st->name, ZOOM_NAME[g->zoom]);
    SDL_SetRenderDrawColor(g->ren, 255, 255, 255, 255);
    SDL_RenderDebugText(g->ren, 8, (float)h - 14, buf);
}

static void apply_vsync(Game *g) {
    SDL_DisplayID d = SDL_GetDisplayForWindow(g->win);
    const SDL_DisplayMode *m = SDL_GetCurrentDisplayMode(d);
    int interval = 1;
    if (m && m->refresh_rate > 75.0f) interval = (int)(m->refresh_rate / 60.0f + 0.5f);  /* ~60 fps cap */
    if (!SDL_SetRenderVSync(g->ren, interval)) SDL_SetRenderVSync(g->ren, 1);
}

int main(int argc, char **argv) {
    const char *shot_path = NULL;
    int shot_frame = 60, start_style = 0, start_zoom = 1;
    bool gallery = false, demo = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shot_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--style") && i + 1 < argc) start_style = atoi(argv[++i]) % WP_AVATAR_STYLE_COUNT;
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) start_zoom = atoi(argv[++i]) % 3;
        else if (!strcmp(argv[i], "--gallery")) gallery = true;
        else if (!strcmp(argv[i], "--demo")) demo = true;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    static Game g;
    memset(&g, 0, sizeof g);
    if (!SDL_CreateWindowAndRenderer("WildPitch", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &g.win, &g.ren)) {
        fprintf(stderr, "window failed: %s\n", SDL_GetError());
        return 1;
    }
    apply_vsync(&g);
    int vs = 0;
    bool vsync = SDL_GetRenderVSync(g.ren, &vs) && vs != 0;

    WpMatchConfig cfg = sim_default_config();
    sim_init(&g.cur, &cfg);
    g.demo = demo;
    if (demo) demo_setup(&g.cur);
    g.prev = g.cur;
    wp_render_ctx_init(&g.rctx, g.ren, cfg.seed);
    g.rctx.style = start_style;
    g.zoom = start_zoom;
    g.cam = (WpCamera){0, -2.0f, 30.0f, 0.62f, 0.010f, 1280, 720};

    const Uint64 tick_ns = SDL_NS_PER_SECOND / WP_TICK_HZ;
    Uint64 last = SDL_GetTicksNS(), acc = 0;
    int frames = 0;
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_ESCAPE) running = false;
                if (e.key.key == SDLK_F3) g.show_debug = !g.show_debug;
                if (e.key.key == SDLK_P) g.rctx.style = (g.rctx.style + 1) % WP_AVATAR_STYLE_COUNT;
                if (e.key.key == SDLK_Z) g.zoom = (g.zoom + 1) % 3;
            }
            if (e.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED) apply_vsync(&g);
        }
        Uint64 now = SDL_GetTicksNS();
        Uint64 dt = now - last; last = now;
        if (dt > 250 * SDL_NS_PER_MS) dt = 250 * SDL_NS_PER_MS;   /* after suspend: don't fast-forward */
        float dts = (float)dt / 1e9f;
        if (shot_path) dts = 1.0f / 60.0f;                        /* deterministic-ish captures */

        int w, h;
        SDL_GetRenderOutputSize(g.ren, &w, &h);
        if (gallery) {
            draw_gallery(&g, w, h);
        } else {
            acc += shot_path ? tick_ns : dt;
            WpInput in;
            memset(&in, 0, sizeof in);
            while (acc >= tick_ns) {
                if (g.demo) demo_input(&g.cur, &in);
                else in.seat[0] = read_keyboard();
                Uint64 t0 = SDL_GetTicksNS();
                g.prev = g.cur;
                sim_step(&g.cur, &in);
                double ms = (double)(SDL_GetTicksNS() - t0) / 1e6;
                g.sim_ms_avg = g.sim_ms_avg * 0.98 + ms * 0.02;
                acc -= tick_ns;
            }
            float alpha = (float)acc / (float)tick_ns;

            const WpAvatarStyle *st = &wp_avatar_styles[g.rctx.style];
            int f = st->pixel_factor;
            int tw = w / f, th = h / f;
            /* camera eases toward play (time-based so it's frame-rate independent) */
            float bx = (float)g.cur.ball.pos.x / 65536.0f, by = (float)g.cur.ball.pos.y / 65536.0f;
            int me = g.cur.seat_player[0];
            float fx_ = me >= 0 ? (float)g.cur.players[me].pos.x / 65536.0f : bx;
            float fy_ = me >= 0 ? (float)g.cur.players[me].pos.y / 65536.0f : by;
            if (g.demo) { fx_ = 0; fy_ = 0; }
            float ease = 1.0f - expf(-dts * 3.0f);
            g.cam.cx += (fx_ * 0.6f - g.cam.cx) * ease;
            g.cam.cy += (fy_ * 0.5f - 1.5f - g.cam.cy) * ease;
            WpCamera cam = g.cam;
            cam.w = tw; cam.h = th;
            cam.ppm = (float)tw / ZOOM_VIEW_M[g.zoom];
            if (f > 1) {
                if (!g.lowres || g.lowres_w != tw || g.lowres_h != th) {
                    if (g.lowres) SDL_DestroyTexture(g.lowres);
                    g.lowres = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, tw, th);
                    SDL_SetTextureScaleMode(g.lowres, SDL_SCALEMODE_NEAREST);
                    g.lowres_w = tw; g.lowres_h = th;
                }
                SDL_SetRenderTarget(g.ren, g.lowres);
            }
            SDL_SetRenderDrawColor(g.ren, 18, 40, 22, 255);
            SDL_RenderClear(g.ren);
            wp_render_match(g.ren, &g.rctx, &cam, &g.prev, &g.cur, alpha, dts);
            if (f > 1) {
                SDL_SetRenderTarget(g.ren, NULL);
                SDL_RenderTexture(g.ren, g.lowres, NULL, NULL);
            }
            SDL_SetRenderDrawColor(g.ren, 255, 255, 255, 255);
            char buf[160];
            snprintf(buf, sizeof buf, "WILDPITCH  %d - %d", g.cur.score[0], g.cur.score[1]);
            SDL_RenderDebugText(g.ren, 12, 12, buf);
            snprintf(buf, sizeof buf, "[P] look: %s   [Z] zoom: %s", st->name, ZOOM_NAME[g.zoom]);
            SDL_RenderDebugText(g.ren, 12, (float)h - 20, buf);
            if (g.show_debug) {
                snprintf(buf, sizeof buf, "tick %u  sim %.3f ms  vsync %s  frame %.1f ms", g.cur.tick, g.sim_ms_avg,
                         vsync ? "on" : "off", (double)dts * 1000.0);
                SDL_RenderDebugText(g.ren, 12, 28, buf);
            }
        }
        if (shot_path && ++frames == shot_frame) {
            SDL_Surface *surf = SDL_RenderReadPixels(g.ren, NULL);
            if (surf) { SDL_SaveBMP(surf, shot_path); SDL_DestroySurface(surf); }
            running = false;
        }
        SDL_RenderPresent(g.ren);
        if (!vsync) SDL_DelayNS(tick_ns / 2);   /* fallback: never spin */
        if (gallery && !shot_path) {            /* static sheet: wait for input instead of redrawing */
            SDL_WaitEvent(NULL);
        }
    }
    SDL_Quit();
    return 0;
}
