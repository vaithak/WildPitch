/* ball_draw.c - see ball_draw.h. */
#include <math.h>
#include "game/ball_draw.h"
#include "game/avatar.h"   /* wp_soft_disc */

#define BALL_R     0.11f          /* true radius: sets the roll rate */
#define DRAW_SCALE 1.5f           /* drawn slightly larger for readability */

/* 12 pentagon centres of a truncated icosahedron = icosahedron vertices (0, +-1, +-phi) cyclic, normalised */
static const float PANELS[12][3] = {
    {0, 0.5257f, 0.8507f}, {0, -0.5257f, 0.8507f}, {0, 0.5257f, -0.8507f}, {0, -0.5257f, -0.8507f},
    {0.5257f, 0.8507f, 0}, {-0.5257f, 0.8507f, 0}, {0.5257f, -0.8507f, 0}, {-0.5257f, -0.8507f, 0},
    {0.8507f, 0, 0.5257f}, {0.8507f, 0, -0.5257f}, {-0.8507f, 0, 0.5257f}, {-0.8507f, 0, -0.5257f},
};

static void ident(float m[3][3]) {
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = i == j ? 1.0f : 0.0f;
}

void wp_ball_vis_update(WpBallVis *v, float x, float y, float z) {
    (void)z;
    if (!v->init) {
        if (v->m[0][0] == 0 && v->m[1][1] == 0 && v->m[2][2] == 0) ident(v->m);   /* first ever frame */
        v->last_x = x; v->last_y = y; v->init = true;
        return;
    }
    float dx = x - v->last_x, dy = y - v->last_y;
    v->last_x = x; v->last_y = y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 1e-5f || d > 3.0f) return;            /* still, or a jump we shouldn't spin across */
    /* rolling (also in the air, where it reads as carried spin - fine visually) */
    float ax = -dy / d, ay = dx / d;              /* axis = z x motion, horizontal */
    float th = d / BALL_R, c = cosf(th), s = sinf(th), t = 1.0f - c;
    float R[3][3] = {                              /* Rodrigues, axis (ax, ay, 0) */
        {t * ax * ax + c, t * ax * ay,      s * ay},
        {t * ax * ay,     t * ay * ay + c, -s * ax},
        {-s * ay,         s * ax,           c},
    };
    float n[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) n[i][j] = R[i][0] * v->m[0][j] + R[i][1] * v->m[1][j] + R[i][2] * v->m[2][j];
    /* re-orthonormalise columns (Gram-Schmidt) so float drift never skews the ball */
    for (int j = 0; j < 3; j++) {
        for (int k = 0; k < j; k++) {
            float dot = n[0][j] * n[0][k] + n[1][j] * n[1][k] + n[2][j] * n[2][k];
            for (int i = 0; i < 3; i++) n[i][j] -= dot * n[i][k];
        }
        float l = sqrtf(n[0][j] * n[0][j] + n[1][j] * n[1][j] + n[2][j] * n[2][j]);
        for (int i = 0; i < 3; i++) v->m[i][j] = n[i][j] / l;
    }
}

void wp_ball_draw_shadow(SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam, float x, float y, float z) {
    /* light from up-left-behind: the shadow slides out from under a rising ball (height cue) */
    WpScreenPt g = wp_project(cam, x + 0.03f + 0.35f * z, y - 0.02f - 0.25f * z, 0);
    float sh = 1.0f / (1.0f + z * 0.35f);
    float rad = 0.15f * cam->ppm * g.s * (0.6f + 0.4f * sh) * DRAW_SCALE / 1.4f;
    wp_soft_disc(r, soft, g.x, g.y, rad, rad * cam->tilt, (SDL_FColor){0, 0, 0, 0.38f * sh});
}

void wp_ball_draw(SDL_Renderer *r, SDL_Texture *soft, const WpCamera *cam, const WpBallVis *v,
                  float x, float y, float z, float vx, float vy) {
    WpScreenPt b = wp_project(cam, x, y, z + BALL_R);
    float br = BALL_R * cam->ppm * b.s * DRAW_SCALE;
    float spd = sqrtf(vx * vx + vy * vy);

    /* streak for fast balls: fading ghosts over the last ~half frame of travel */
    if (spd > 15.0f) {
        float k = fminf((spd - 15.0f) / 10.0f, 1.0f);
        for (int g = 2; g >= 1; g--) {
            float back = (float)g * 0.33f / 60.0f;
            WpScreenPt q = wp_project(cam, x - vx * back, y - vy * back, z + BALL_R);
            wp_soft_disc(r, soft, q.x, q.y, br * 0.95f, br * 0.95f,
                         (SDL_FColor){0.95f, 0.95f, 0.95f, k * (g == 1 ? 0.35f : 0.18f)});
        }
    }
    /* body: outline, shaded base, lit cap */
    wp_soft_disc(r, soft, b.x, b.y, br + 1.0f, br + 1.0f, (SDL_FColor){0.07f, 0.07f, 0.09f, 1});
    wp_soft_disc(r, soft, b.x, b.y, br, br, (SDL_FColor){0.80f, 0.80f, 0.83f, 1});
    wp_soft_disc(r, soft, b.x - br * 0.12f, b.y - br * 0.14f, br * 0.84f, br * 0.84f, (SDL_FColor){0.98f, 0.98f, 0.98f, 1});

    /* panels: contrast fades out as the drawn spin per frame gets too fast to read (3 -> 9 m/s) */
    float contrast = 1.0f - fminf(fmaxf((spd - 3.0f) / 6.0f, 0.0f), 1.0f);
    if (contrast > 0.02f) {
        /* orthographic view matching the tilt: toward-camera V = (0,-cos e, sin e), screen-up U = (0, sin e, cos e) */
        float e = atanf(cam->tilt), se = sinf(e), ce = cosf(e);
        for (int k = 0; k < 12; k++) {
            const float *p = PANELS[k];
            float wx = v->m[0][0] * p[0] + v->m[0][1] * p[1] + v->m[0][2] * p[2];
            float wy = v->m[1][0] * p[0] + v->m[1][1] * p[1] + v->m[1][2] * p[2];
            float wz = v->m[2][0] * p[0] + v->m[2][1] * p[1] + v->m[2][2] * p[2];
            float facing = -wy * ce + wz * se;
            if (facing < 0.15f) continue;
            float sx = wx, sy = wy * se + wz * ce;    /* on-disc offset (unit sphere) */
            float pr = br * 0.30f * sqrtf(facing);    /* foreshortened toward the rim */
            float a = contrast * fminf((facing - 0.15f) / 0.3f, 1.0f);
            wp_soft_disc(r, soft, b.x + sx * br * 0.82f, b.y - sy * br * 0.82f, pr, pr,
                         (SDL_FColor){0.12f, 0.12f, 0.16f, 0.95f * a});
        }
    }
    /* specular glint (fixed to the light, not the panels) */
    wp_soft_disc(r, soft, b.x - br * 0.35f, b.y - br * 0.38f, br * 0.22f, br * 0.22f, (SDL_FColor){1, 1, 1, 0.7f});
}
