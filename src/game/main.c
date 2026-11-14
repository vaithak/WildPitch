/* main.c - platform loop: window, input -> WpInput, fixed 60 Hz sim step, interpolated render.
 * The loop never spins: vsync blocks, and if vsync is unavailable we sleep to the frame cap. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <string.h>
#include "sim/sim.h"
#include "game/render.h"

typedef struct {
    SDL_Window   *win;
    SDL_Renderer *ren;
    SimState      prev, cur;
    WpCamera      cam;
    bool          show_debug;
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

int main(int argc, char **argv) {
    /* --screenshot <file.bmp>: run ~1 s with no input, save a frame, exit (used by agents for visual checks). */
    const char *shot_path = NULL;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot_path = argv[++i];
    int frames = 0;
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
    bool vsync = SDL_SetRenderVSync(g.ren, 1);

    WpMatchConfig cfg = sim_default_config();
    sim_init(&g.cur, &cfg);
    g.prev = g.cur;
    g.cam = (WpCamera){0, 0, 26.0f, 0.62f, 0.010f, 1280, 720};

    const Uint64 tick_ns = SDL_NS_PER_SECOND / WP_TICK_HZ;
    Uint64 last = SDL_GetTicksNS(), acc = 0;
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F3) g.show_debug = !g.show_debug;
        }
        Uint64 now = SDL_GetTicksNS();
        Uint64 dt = now - last; last = now;
        if (dt > 250 * SDL_NS_PER_MS) dt = 250 * SDL_NS_PER_MS;   /* after suspend: don't fast-forward */
        acc += dt;
        WpInput in;
        memset(&in, 0, sizeof in);
        in.seat[0] = read_keyboard();
        while (acc >= tick_ns) {
            Uint64 t0 = SDL_GetTicksNS();
            g.prev = g.cur;
            sim_step(&g.cur, &in);
            double ms = (double)(SDL_GetTicksNS() - t0) / 1e6;
            g.sim_ms_avg = g.sim_ms_avg * 0.98 + ms * 0.02;
            acc -= tick_ns;
        }
        float alpha = (float)acc / (float)tick_ns;

        int w, h;
        SDL_GetRenderOutputSize(g.ren, &w, &h);
        g.cam.w = w; g.cam.h = h;
        g.cam.ppm = (float)w / 52.0f;
        /* camera eases toward the ball */
        float bx = (float)g.cur.ball.pos.x / 65536.0f, by = (float)g.cur.ball.pos.y / 65536.0f;
        g.cam.cx += (bx * 0.5f - g.cam.cx) * 0.05f;
        g.cam.cy += (by * 0.4f - 2.0f - g.cam.cy) * 0.05f;

        SDL_SetRenderDrawColor(g.ren, 18, 40, 22, 255);
        SDL_RenderClear(g.ren);
        wp_render_match(g.ren, &g.cam, &g.prev, &g.cur, alpha);
        SDL_SetRenderDrawColor(g.ren, 255, 255, 255, 255);
        char buf[128];
        snprintf(buf, sizeof buf, "WILDPITCH  %d - %d", g.cur.score[0], g.cur.score[1]);
        SDL_RenderDebugText(g.ren, 12, 12, buf);
        if (g.show_debug) {
            snprintf(buf, sizeof buf, "tick %u  sim %.3f ms  vsync %s", g.cur.tick, g.sim_ms_avg, vsync ? "on" : "off");
            SDL_RenderDebugText(g.ren, 12, 28, buf);
        }
        if (shot_path && ++frames == 60) {
            SDL_Surface *surf = SDL_RenderReadPixels(g.ren, NULL);
            if (surf) { SDL_SaveBMP(surf, shot_path); SDL_DestroySurface(surf); }
            running = false;
        }
        SDL_RenderPresent(g.ren);
        if (!vsync) SDL_DelayNS(tick_ns / 2);   /* fallback: never spin */
    }
    SDL_Quit();
    return 0;
}
