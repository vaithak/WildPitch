/* main.c - platform loop: window, input -> WpInput, fixed 60 Hz sim step, interpolated render.
 * The loop never spins: vsync blocks (interval chosen so we present ~60 fps even on 120/144 Hz screens),
 * and if vsync is unavailable we sleep to the frame cap.
 *
 * Flags (for humans and for agents doing visual checks):
 *   --demo                 8 scripted players run/kick/jump (avatar motion check; test harness only)
 *   --gallery              pose sheet of the avatar at close-up and match scale
 *   --style N              avatar preset (0 Modern, 1 Arcade, 2 Retro pixel); P cycles in game
 *   --zoom N               camera zoom preset (0 wide, 1 medium, 2 close); Z cycles in game
 *   --autoplay             seat 0 is scripted: chase ball, dribble at goal, charged shot (visual checks)
 *   --autoplay-chip / --autoplay-dribble   variants: chip instead of shot / sprint zigzag dribble (no shot)
 *   --check-kick-pose      numeric check: kicking instep at the ball on the contact tick (no window)
 *   --screenshot F.bmp     save frame number --frames N (default 60) to F.bmp and exit
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim/sim.h"
#include "sim/tuning.h"
#include "game/render.h"
#include "game/dedication.h"

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
    if (k[SDL_SCANCODE_Q]) in.buttons |= WP_BTN_SWITCH;
    if (k[SDL_SCANCODE_I]) in.buttons |= WP_BTN_CHIP;
    if (k[SDL_SCANCODE_L]) in.buttons |= WP_BTN_LOB;
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
        if ((t + (uint32_t)k * 53) % 170 == 0 && p->z == 0) {
            WpKick kk = {FX(20.0), FX(2.0), 0, p->facing, (uint8_t)(k & 1), WP_KICK_SHOT, 6, 0, 0};
            sim_player_kick(s, i, &kk);
        }
    }
}

/* ---- autoplay harness (visual checks): seat 0 chases the ball, dribbles at the goal, charges and shoots ---- */
static bool autoplay_chip = false, autoplay_dribble = false;
static float autoplay_header = 0;   /* --autoplay-header H: a cross arriving at height H, K pressed (harness) */

/* Harness only (writes the sim directly, like --demo): striker near goal, ball crossed in from the right. */
static void header_cross_setup(SimState *s, float h) {
    int me = s->seat_player[0];
    WpPlayer *p = &s->players[me];
    p->pos = fv2_make(FX(14.0), FX(1.0)); p->vel = fv2_make(0, 0); p->facing = 0;
    WpBall *b = &s->ball;
    fx T = FX(0.7), z0 = FX(1.0);
    b->pos = fv2_make(FX(14.0), FX(11.0)); b->z = z0; b->owner = -1; b->spin = 0;
    b->vel = fv2_scale(fv2_sub(p->pos, b->pos), fx_div(FX_ONE, T));
    b->vz = fx_div((fx)(h * 65536.0f) - z0, T) + fx_mul(T_GRAVITY, T) / 2;
}
static void autoplay_input(const SimState *s, WpInput *in) {
    static int charging = 0;
    int me = s->seat_player[0];
    if (me < 0) return;
    const WpPlayer *p = &s->players[me];
    float px = (float)p->pos.x / 65536.0f, py = (float)p->pos.y / 65536.0f;
    float bx = (float)s->ball.pos.x / 65536.0f, by = (float)s->ball.pos.y / 65536.0f;
    float tx = bx, ty = by;
    if (s->ball.owner == me) { tx = 22.0f; ty = 1.6f; }   /* far side of the goal, past the keeper */
    if (autoplay_dribble && s->ball.owner == me) {        /* sprint zigzag: 90-degree cuts every 0.75 s */
        int leg = (int)(s->tick / 45) % 4;
        float dirs[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
        in->seat[0].move_x = (int8_t)(dirs[leg][0] * 127); in->seat[0].move_y = (int8_t)(dirs[leg][1] * 127);
        in->seat[0].buttons = WP_BTN_SPRINT;
        return;
    }
    float dx = tx - px, dy = ty - py, l = sqrtf(dx * dx + dy * dy) + 1e-4f;
    in->seat[0].move_x = (int8_t)(dx / l * 127.0f);
    in->seat[0].move_y = (int8_t)(dy / l * 127.0f);
    in->seat[0].buttons = 0;
    if (s->ball.owner == me && px > 7.0f && charging < 22) { charging++; in->seat[0].buttons = autoplay_chip ? WP_BTN_CHIP : WP_BTN_SHOOT; }
    else charging = s->ball.owner == me ? charging : 0;
    if (s->phase != WP_PHASE_PLAY) charging = 0;
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
        {"kick back", -PI / 6, 0, 0, 0, WP_ACT_KICK, 0.04f, false},
        {"kick contact", -PI / 6, 0, 0, 0, WP_ACT_KICK, 0.10f, false},
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
            int f = st->pixel_factor;
            WpCamera cam = {0, 0, (pass < 2 ? (float)cellh / 3.1f : (float)w / view) / (float)f, 0.62f, 0.0f, cellw / f, cellh / f};
            WpAvatarLook look;
            int look_id = pi < 4 ? 1 : pi * 3 + 1;           /* idle views share one look (front vs back) */
            wp_avatar_make_look(&look, 7, look_id, pi < 4 ? 0 : (pi & 1), p->keeper);
            WpAvatarPose pose = {.facing = p->facing, .speed = p->speed, .phase = p->phase, .z = p->z,
                                 .action = p->action, .action_t = p->at, .keeper = p->keeper,
                                 .kick_foot = 1, .kick_kind = WP_KICK_SHOT, .kick_windup = 0.1f, .kick_power01 = 0.7f};
            float yoff = -(float)cam.h * 0.36f / (cam.ppm * cam.tilt);   /* feet near the bottom: headroom for jumps */
            if (f > 1) {
                if (!g->lowres || g->lowres_w != cam.w || g->lowres_h != cam.h) {
                    if (g->lowres) SDL_DestroyTexture(g->lowres);
                    g->lowres = SDL_CreateTexture(g->ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, cam.w, cam.h);
                    SDL_SetTextureScaleMode(g->lowres, SDL_SCALEMODE_NEAREST);
                    g->lowres_w = cam.w; g->lowres_h = cam.h;
                }
                SDL_SetRenderViewport(g->ren, NULL);
                SDL_SetRenderTarget(g->ren, g->lowres);
                SDL_SetRenderDrawColor(g->ren, 64, 140, 70, 255);
                SDL_RenderClear(g->ren);
            }
            WpScreenPt sh = wp_project(&cam, 0.1f, yoff - 0.06f, 0);
            wp_soft_disc(g->ren, g->rctx.soft, sh.x, sh.y, 0.4f * cam.ppm, 0.4f * cam.ppm * cam.tilt, (SDL_FColor){0, 0, 0, 0.3f});
            wp_avatar_draw(g->ren, g->rctx.soft, &cam, 0, yoff, &pose, &look, st);
            if (f > 1) {
                SDL_SetRenderTarget(g->ren, NULL);
                SDL_SetRenderViewport(g->ren, &vp);
                SDL_RenderTexture(g->ren, g->lowres, NULL, NULL);
            }
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

/* --check-kick-pose: numeric check that the avatar's kicking toe is at the ball on the sim's contact tick,
 * for both feet and every kick kind. Prints distances; exit code 1 if any is too far. No window needed. */
static int check_kick_pose(void) {
    int bad = 0;
    const char *kinds[3] = {"pass", "shot", "chip"};
    for (int kind = 0; kind < 3; kind++)
        for (int foot = 0; foot < 2; foot++) {
            static SimState s;
            WpMatchConfig cfg = sim_default_config();
            sim_init(&s, &cfg);
            int me = s.seat_player[0];
            s.players[me].pos = fv2_make(-FX(10.0), 0);
            s.ball.pos = fv2_make(-FX(9.6), 0);
            WpInput in; memset(&in, 0, sizeof in);
            for (int t = 0; t < 5; t++) sim_step(&s, &in);
            uint8_t windup = kind == 0 ? 4 : 6;
            WpKick k = {kind == 0 ? FX(12.0) : FX(22.0), kind == 2 ? FX(6.0) : 0, 0, 0, (uint8_t)foot, (uint8_t)kind, windup, 0, 0};
            sim_player_kick(&s, me, &k);
            for (int t = 0; t < 20; t++) { sim_step(&s, &in); if (s.events & WP_EV_KICK) break; }
            const WpPlayer *p = &s.players[me];
            float px = (float)p->pos.x / 65536.0f, py = (float)p->pos.y / 65536.0f;
            float face = (float)p->facing * (6.2831853f / 65536.0f);
            float ballx = px + cosf(face) * 0.45f, bally = py + sinf(face) * 0.45f, ballz = 0.11f;
            WpAvatarLook look; wp_avatar_make_look(&look, 1, me, 0, false);
            WpAvatarPose pose = {.facing = face, .speed = 0, .phase = 0, .z = 0, .action = WP_ACT_KICK,
                                 .action_t = (float)p->action_timer / 60.0f, .kick_foot = (uint8_t)foot,
                                 .kick_kind = (uint8_t)kind, .kick_windup = (float)windup / 60.0f, .kick_power01 = 0.5f};
            float toe[3];
            wp_avatar_toe_pos(&pose, &look, &wp_avatar_styles[0], px, py, foot, toe);
            float dx = toe[0] - ballx, dy = toe[1] - bally, dz = toe[2] - ballz;
            float d = sqrtf(dx * dx + dy * dy + dz * dz);
            printf("%s %s foot: instep->ball %.2f m (fwd %.2f, side %.2f, up %.2f)\n", kinds[kind], foot ? "right" : "left ",
                   (double)d, (double)(dx * cosf(face) + dy * sinf(face)), (double)(-dx * sinf(face) + dy * cosf(face)), (double)dz);
            if (d > 0.16f) bad = 1;   /* ball radius 0.11 + a little */
        }
    printf(bad ? "FAIL: a kicking foot is not at the ball on the contact tick\n" : "OK\n");
    return bad;
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
    int shot_frame = 60, start_style = 0, start_zoom = 0;   /* owner: Modern look, wide zoom (D-011) */
    bool gallery = false, demo = false, autoplay = false, force_card = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shot_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--style") && i + 1 < argc) start_style = atoi(argv[++i]) % WP_AVATAR_STYLE_COUNT;
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) start_zoom = atoi(argv[++i]) % 3;
        else if (!strcmp(argv[i], "--gallery")) gallery = true;
        else if (!strcmp(argv[i], "--dedication")) force_card = true;   /* with --screenshot: capture the card */
        else if (!strcmp(argv[i], "--demo")) demo = true;
        else if (!strcmp(argv[i], "--autoplay")) autoplay = true;
        else if (!strcmp(argv[i], "--autoplay-chip")) { autoplay = true; autoplay_chip = true; }
        else if (!strcmp(argv[i], "--autoplay-dribble")) { autoplay = true; autoplay_dribble = true; }
        else if (!strcmp(argv[i], "--autoplay-header") && i + 1 < argc) { autoplay = true; autoplay_header = (float)atof(argv[++i]); }
        else if (!strcmp(argv[i], "--check-kick-pose")) return check_kick_pose();
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
    if (force_card && shot_path) {                       /* visual check of the dedication card (mid-hold) */
        int w, h;
        SDL_GetRenderOutputSize(g.ren, &w, &h);
        wp_dedication_draw(g.ren, w, h, 2.0f);
        SDL_Surface *surf = SDL_RenderReadPixels(g.ren, NULL);
        if (surf) { SDL_SaveBMP(surf, shot_path); SDL_DestroySurface(surf); }
        SDL_Quit();
        return 0;
    }
    /* the owner's dedication (D-018): on a normal launch only, never in harness/capture modes */
    if ((force_card || !(shot_path || gallery || demo || autoplay)) && !wp_dedication_run(g.ren)) { SDL_Quit(); return 0; }
    int vs = 0;
    bool vsync = SDL_GetRenderVSync(g.ren, &vs) && vs != 0;

    WpMatchConfig cfg = sim_default_config();
    sim_init(&g.cur, &cfg);
    g.demo = demo;
    if (demo) demo_setup(&g.cur);
    if (autoplay)   /* harness only: clear the statues out of the striker's path */
        for (int i = g.cur.n_per_side + 1; i < 2 * g.cur.n_per_side; i++)
            g.cur.players[i].pos = fv2_make(FX_INT(i * 3 - 30), (i & 1) ? FX(11.5) : -FX(11.5));
    g.prev = g.cur;
    wp_render_ctx_init(&g.rctx, g.ren, cfg.seed);
    g.rctx.style = start_style;
    g.rctx.hide_markers = demo;
    g.zoom = start_zoom;
    g.cam = (WpCamera){0, -2.0f, 30.0f, 0.62f, 0.010f, 1280, 720};

    const Uint64 tick_ns = SDL_NS_PER_SECOND / WP_TICK_HZ;
    Uint64 last = SDL_GetTicksNS(), acc = 0;
    int frames = 0;
    long total_frames = 0;
    Uint64 start_ns = SDL_GetTicksNS();
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
            uint16_t frame_ev = 0;
            while (acc >= tick_ns) {
                if (g.demo) demo_input(&g.cur, &in);
                else if (autoplay_header > 0) {
                    if (g.cur.tick == 20) header_cross_setup(&g.cur, autoplay_header);
                    in.seat[0] = (WpSeatInput){0, 0, (uint16_t)(g.cur.tick == 21 ? WP_BTN_SHOOT : 0)};
                }
                else if (autoplay) autoplay_input(&g.cur, &in);
                else in.seat[0] = read_keyboard();
                Uint64 t0 = SDL_GetTicksNS();
                g.prev = g.cur;
                sim_step(&g.cur, &in);
                frame_ev |= g.cur.events;
                if (autoplay && (g.cur.events & (WP_EV_KICK | WP_EV_GOAL | WP_EV_POST)))
                    fprintf(stderr, "tick %u events %04x\n", g.cur.tick, g.cur.events);
                if (g.cur.events & WP_EV_RESET) {     /* teleport: don't interpolate or animate across it */
                    g.prev = g.cur;
                    wp_render_reset_anim(&g.rctx);
                }
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
            float fx_ = me >= 0 ? (float)g.cur.players[me].pos.x / 65536.0f * 0.6f + bx * 0.4f : bx;
            float fy_ = me >= 0 ? (float)g.cur.players[me].pos.y / 65536.0f * 0.6f + by * 0.4f : by;
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
            g.rctx.frame_events = frame_ev;
            wp_render_match(g.ren, &g.rctx, &cam, &g.prev, &g.cur, alpha, dts);
            if (f > 1) {
                SDL_SetRenderTarget(g.ren, NULL);
                SDL_RenderTexture(g.ren, g.lowres, NULL, NULL);
            }
            SDL_SetRenderDrawColor(g.ren, 255, 255, 255, 255);
            char buf[160];
            unsigned secs = g.cur.tick / WP_TICK_HZ;
            snprintf(buf, sizeof buf, "WILDPITCH  %d - %d   %02u:%02u", g.cur.score[0], g.cur.score[1], secs / 60, secs % 60);
            SDL_RenderDebugText(g.ren, 12, 12, buf);
            snprintf(buf, sizeof buf, "WASD move  Shift sprint  J pass (hold: through)  L lob  Q switch  K shoot  I chip  (J/K at a high ball: header)   [P] %s  [Z] %s  F3",
                     st->name, ZOOM_NAME[g.zoom]);
            SDL_RenderDebugText(g.ren, 12, (float)h - 20, buf);
            if (g.cur.phase == WP_PHASE_GOAL) {
                const char *msg = "G O A L !";
                float sc = 4.0f;
                SDL_SetRenderScale(g.ren, sc, sc);
                SDL_SetRenderDrawColor(g.ren, 255, 235, 90, 255);
                SDL_RenderDebugText(g.ren, ((float)w / sc - 8.0f * (float)strlen(msg)) * 0.5f, (float)h / sc * 0.22f, msg);
                SDL_SetRenderScale(g.ren, 1, 1);
                SDL_SetRenderDrawColor(g.ren, 255, 255, 255, 255);
            } else if (g.cur.phase == WP_PHASE_OUT) {
                SDL_RenderDebugText(g.ren, (float)w * 0.5f - 12, 40, "OUT");
            }
            if (g.show_debug) {
                const WpBall *b = &g.cur.ball;
                float bs = sqrtf((float)b->vel.x * (float)b->vel.x + (float)b->vel.y * (float)b->vel.y) / 65536.0f;
                snprintf(buf, sizeof buf, "tick %u  sim %.3f ms  vsync %s  frame %.1f ms", g.cur.tick, g.sim_ms_avg,
                         vsync ? "on" : "off", (double)dts * 1000.0);
                SDL_RenderDebugText(g.ren, 12, 28, buf);
                snprintf(buf, sizeof buf, "ball %.1f m/s  h %.2f m  vz %.1f  spin %.1f  owner %d  phase %d  events %04x",
                         (double)bs, (double)b->z / 65536.0, (double)b->vz / 65536.0, (double)b->spin / 65536.0,
                         b->owner, g.cur.phase, frame_ev);
                SDL_RenderDebugText(g.ren, 12, 40, buf);
            }
        }
        if (shot_path && ++frames == shot_frame) {
            SDL_Surface *surf = SDL_RenderReadPixels(g.ren, NULL);
            if (surf) { SDL_SaveBMP(surf, shot_path); SDL_DestroySurface(surf); }
            int me = g.cur.seat_player[0];       /* for agents cropping a player out of the frame */
            if (me >= 0 && !gallery) {
                WpCamera c = g.cam; c.w = w; c.h = h; c.ppm = (float)w / ZOOM_VIEW_M[g.zoom];
                WpScreenPt sp = wp_project(&c, (float)g.cur.players[me].pos.x / 65536.0f, (float)g.cur.players[me].pos.y / 65536.0f, 0);
                printf("seat0_screen %d %d\n", (int)sp.x, (int)sp.y);
            }
            running = false;
        }
        SDL_RenderPresent(g.ren);
        total_frames++;
        if (!vsync) SDL_DelayNS(tick_ns / 2);   /* fallback: never spin */
        if (gallery && !shot_path) {            /* static sheet: wait for input instead of redrawing */
            SDL_WaitEvent(NULL);
        }
    }
    {   /* frame pacing report (thermal budget: ~60 fps even on high-refresh screens) */
        double secs = (double)(SDL_GetTicksNS() - start_ns) / 1e9;
        const SDL_DisplayMode *m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(g.win));
        if (!shot_path && secs > 1.0)
            printf("avg %.1f fps over %.1f s (display %.0f Hz, vsync %s)\n", (double)total_frames / secs, secs,
                   m ? (double)m->refresh_rate : 0.0, vsync ? "on" : "off");
    }
    SDL_Quit();
    return 0;
}
