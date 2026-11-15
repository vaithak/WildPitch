/* avatar.h - procedural 3D-skeleton footballers (DECISIONS D-009).
 *
 * A player is 17 joints posed procedurally (run cycle, kick, header, keeper stance...) and drawn as
 * soft-edged outlined capsules through the tabletop camera. Everything here is PRESENTATION ONLY:
 *  - WpAvatarPose is filled from SimState by an adapter (render.c) or by hand (the --gallery).
 *  - Stride phase / smoothed facing live in WpAvatarAnim (per player, presentation state).
 *  - Appearance (WpAvatarLook) is derived from a hash of (match seed, player index), never sim RNG.
 */
#ifndef WP_AVATAR_H
#define WP_AVATAR_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include "game/camera.h"

typedef struct {
    float facing;        /* radians, 0 = +x */
    float speed;         /* m/s horizontal */
    float phase;         /* stride phase, radians (advances with distance travelled) */
    float z;             /* feet height above grass (jump) */
    uint8_t action;      /* WP_ACT_* from sim.h */
    float action_t;      /* seconds since action start */
    float time;          /* global seconds, for idle breathing */
    bool  keeper;
    /* kick parameters (from the sim's WpKick, D-012) so the strike lands on the contact tick */
    uint8_t kick_foot;   /* 0 left, 1 right */
    uint8_t kick_kind;   /* WP_KICK_* */
    float kick_windup;   /* seconds from action start to ball contact (0 -> default) */
    float kick_power01;  /* 0..1 strength of the strike */
    float charge01;      /* 0..1 while charging a shot/chip near the ball (leg drawn back), else 0 */
} WpAvatarPose;


typedef struct {
    SDL_FColor skin, hair, shirt, sleeve, shorts, socks, boots, gloves;
    uint8_t hair_style;  /* 0 short, 1 buzz, 2 long, 3 curly/afro, 4 bald, 5 bun */
    float height;        /* metres (1.68 .. 1.92) */
    float build;         /* width multiplier (0.92 .. 1.10) */
} WpAvatarLook;

typedef struct {
    const char *name;
    float head_scale;    /* 1 = realistic; >1 = readable "arcade" heads */
    float limb_scale;    /* limb thickness multiplier */
    float outline_px;    /* outline thickness in (render-target) pixels; 0 = none */
    int   pixel_factor;  /* 1 = full res; 3 = render match at 1/3 res, upscale nearest (retro) */
} WpAvatarStyle;

#define WP_AVATAR_STYLE_COUNT 3
extern const WpAvatarStyle wp_avatar_styles[WP_AVATAR_STYLE_COUNT];

typedef struct {
    float phase, facing;
    float last_x, last_y;
    bool  init;
} WpAvatarAnim;

/* Presentation-side per-frame update: advance stride by distance, smooth facing toward sim facing. */
void wp_avatar_anim_update(WpAvatarAnim *a, float x, float y, float sim_facing, float speed, float dt);

void wp_avatar_make_look(WpAvatarLook *look, uint64_t seed, int player_index, int team, bool keeper);

/* soft disc texture used for antialiased shapes (create once per renderer) */
SDL_Texture *wp_soft_disc_create(SDL_Renderer *r);

void wp_avatar_draw(SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam, float x, float y,
                    const WpAvatarPose *pose, const WpAvatarLook *look, const WpAvatarStyle *style);

/* soft-edged primitives (exposed for ball/shadows) */
void wp_soft_disc(SDL_Renderer *r, SDL_Texture *soft, float cx, float cy, float rx, float ry, SDL_FColor c);
void wp_soft_capsule(SDL_Renderer *r, SDL_Texture *soft, float ax, float ay, float ar,
                     float bx, float by, float br, SDL_FColor c);

/* Joint query for checks: world position of the instep (mid-foot; foot 0 = left, 1 = right) for a pose at (x, y). */
void wp_avatar_toe_pos(const WpAvatarPose *pose, const WpAvatarLook *look, const WpAvatarStyle *style,
                       float x, float y, int foot, float out[3]);

#endif
