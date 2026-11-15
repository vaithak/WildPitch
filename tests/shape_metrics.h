/* shape_metrics.h - team-shape metrics for AI-vs-AI tests and harnesses (floats are fine here: tests only).
 * Per team, split by phase (IN = we have the ball, OUT = they have it): length/width/hull area of the outfielders,
 * outfielders within 10 m of the ball, goal-side count, defenders ahead of the ball, rest defence, line gap.
 * Defenders = formation slots k1,k2; outfield = k >= 1. Attack direction: team 0 +x. */
#ifndef WP_SHAPE_METRICS_H
#define WP_SHAPE_METRICS_H
#include <math.h>
#include "sim/sim.h"

typedef struct { double n, s; } ShM;
static inline void shm_add(ShM *a, double v) { a->n += 1; a->s += v; }
static inline double shm_mean(const ShM *a) { return a->n ? a->s / a->n : 0; }

typedef struct {
    ShM len[2], wid[2], area[2], near10[2];       /* [0] = IN, [1] = OUT */
    ShM goalside_out, def_ahead_out, no_rest_in, nobody_ahead_in, gap_dm_in;
} ShapeAcc;

static inline double shm_cross(double ox, double oy, double ax, double ay, double bx, double by) {
    return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox);
}
static inline double shm_hull(const double *x, const double *y, int n) {
    int idx[16], h[34], k = 0;
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (x[idx[j]] < x[idx[i]] || (x[idx[j]] == x[idx[i]] && y[idx[j]] < y[idx[i]])) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    for (int i = 0; i < n; i++) { while (k >= 2 && shm_cross(x[h[k-2]], y[h[k-2]], x[h[k-1]], y[h[k-1]], x[idx[i]], y[idx[i]]) <= 0) k--; h[k++] = idx[i]; }
    for (int i = n - 2, t = k + 1; i >= 0; i--) { while (k >= t && shm_cross(x[h[k-2]], y[h[k-2]], x[h[k-1]], y[h[k-1]], x[idx[i]], y[idx[i]]) <= 0) k--; h[k++] = idx[i]; }
    double a = 0;
    for (int i = 0; i < k - 1; i++) a += x[h[i]] * y[h[i + 1]] - x[h[i + 1]] * y[h[i]];
    return fabs(a) / 2;
}

/* sample team t (call every few ticks in open play) */
static inline void shape_sample(ShapeAcc *a, const SimState *s, int t) {
    const WpBall *b = &s->ball;
    if (s->phase != WP_PHASE_PLAY || b->owner < 0 || s->restart_kind) return;
    int phase = s->players[b->owner].team == t ? 0 : 1, n = s->n_per_side;
    double sg = t == 0 ? 1.0 : -1.0, bx = b->pos.x / 65536.0 * sg, by = b->pos.y / 65536.0;
    double x[16], y[16], minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9, defx = 1e9, midx = 1e9;
    int m = 0, near = 0, goalside = 0, def_ahead = 0, rest = 0, ahead = 0, defs = 0;
    for (int k = 1; k < n; k++) {
        const WpPlayer *p = &s->players[t * n + k];
        double px = p->pos.x / 65536.0 * sg, py = p->pos.y / 65536.0;
        x[m] = px; y[m] = py; m++;
        if (px < minx) minx = px;
        if (px > maxx) maxx = px;
        if (py < miny) miny = py;
        if (py > maxy) maxy = py;
        if (hypot(px - bx, py - by) <= 10.0) near++;
        if (px < bx) goalside++;
        if (px < bx - 5.0) rest++;
        if (px > bx + 3.0) ahead++;
        int is_def = n == 7 ? k <= 2 : k <= 2;
        if (is_def) { defs++; if (px > bx + 1.0) def_ahead++; if (px < defx) defx = px; }
        else if (k < n - 1 && px < midx) midx = px;
    }
    shm_add(&a->len[phase], maxx - minx);
    shm_add(&a->wid[phase], maxy - miny);
    shm_add(&a->area[phase], shm_hull(x, y, m));
    shm_add(&a->near10[phase], near);
    if (phase == 1) {
        shm_add(&a->goalside_out, goalside);
        shm_add(&a->def_ahead_out, defs ? (double)def_ahead / defs : 0);
    } else {
        shm_add(&a->no_rest_in, rest == 0);
        shm_add(&a->nobody_ahead_in, ahead == 0);
        if (midx < 1e8 && defx < 1e8) shm_add(&a->gap_dm_in, midx - defx);
    }
}

static inline void shape_print(const ShapeAcc *a, const char *label) {
    printf("  shape %s: length IN %.1f / OUT %.1f m, width %.1f / %.1f m, area %.0f / %.0f m2, near-10m IN %.2f OUT %.2f,\n"
           "    goal-side OUT %.2f, defenders ahead OUT %.0f%%, no rest IN %.0f%%, nobody ahead IN %.0f%%, gap def-mid IN %.1f m\n",
           label, shm_mean(&a->len[0]), shm_mean(&a->len[1]), shm_mean(&a->wid[0]), shm_mean(&a->wid[1]),
           shm_mean(&a->area[0]), shm_mean(&a->area[1]), shm_mean(&a->near10[0]), shm_mean(&a->near10[1]),
           shm_mean(&a->goalside_out), 100 * shm_mean(&a->def_ahead_out), 100 * shm_mean(&a->no_rest_in),
           100 * shm_mean(&a->nobody_ahead_in), shm_mean(&a->gap_dm_in));
}
#endif
