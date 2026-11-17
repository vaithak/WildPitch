/* pacer.h - frame-rate cap (runs cool, D-056). Vsync paces frames when it works (apply the interval for the target
 * rate); the pacer is the safety net: a deadline-based sleep that switches itself on when vsync is off or isn't
 * honoured (more than 1.2x the target frames in a second), so the loop never spins. Also the CPU-time probe the
 * --perf-seconds measurement uses. */
#ifndef WP_PACER_H
#define WP_PACER_H
#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct {
    int    fps;            /* target: 60, or 30 (battery) */
    bool   active;         /* sleeping to the deadline (vsync absent / ignored) */
    Uint64 next;           /* deadline of the next frame */
    Uint64 win_start;      /* watchdog window */
    int    win_frames;
    int    strikes;        /* watchdog windows in a row over budget */
} WpPacer;

void wp_pacer_set(WpPacer *p, int fps, bool vsync_ok);
void wp_pacer_frame(WpPacer *p);          /* after SDL_RenderPresent */
/* vsync interval that gives ~fps on this display (1 if unknown); SDL_SetRenderVSync with it */
int  wp_pacer_vsync_interval(SDL_Window *w, int fps);
/* process CPU seconds (user + system) from /proc/self/stat; < 0 where unavailable */
double wp_cpu_seconds(void);
#endif
