/* pacer.c - see pacer.h. */
#define _POSIX_C_SOURCE 200809L   /* sysconf */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "game/pacer.h"

void wp_pacer_set(WpPacer *p, int fps, bool vsync_ok) {
    p->fps = fps > 0 ? fps : 60;
    p->active = !vsync_ok;
    p->next = SDL_GetTicksNS();
    p->win_start = p->next;
    p->win_frames = 0;
    p->strikes = 0;
}

void wp_pacer_frame(WpPacer *p) {
    Uint64 period = SDL_NS_PER_SECOND / (Uint64)p->fps, now = SDL_GetTicksNS();
    p->win_frames++;
    if (now - p->win_start >= SDL_NS_PER_SECOND) {                        /* watchdog: vsync not pacing us */
        /* two windows in a row over 1.2x the target (the first frames after a window maps can run unpaced) */
        p->strikes = p->win_frames * 5 > p->fps * 6 ? p->strikes + 1 : 0;
        if (p->strikes >= 2 && !p->active) {
            if (getenv("WP_PACER_DEBUG")) fprintf(stderr, "pacer on: %d frames in the last second\n", p->win_frames);
            p->active = true;
        }
        p->win_start = now; p->win_frames = 0;
    }
    if (!p->active) { p->next = now; return; }
    p->next += period;
    if (p->next + period < now) p->next = now;                            /* fell behind: don't catch up */
    if (p->next > now) SDL_DelayPrecise(p->next - now);
}

int wp_pacer_vsync_interval(SDL_Window *w, int fps) {
    const SDL_DisplayMode *m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w));
    if (!m || m->refresh_rate <= 0.0f) return 1;
    int k = (int)(m->refresh_rate / (float)fps + 0.5f);
    return k < 1 ? 1 : k;
}

double wp_cpu_seconds(void) {
#ifdef __linux__
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return -1;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    char *p = buf;
    for (char *q = buf; *q; q++) if (*q == ')') p = q;                     /* comm may contain spaces */
    unsigned long ut = 0, st = 0;
    /* after ") ": state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime */
    if (sscanf(p + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &st) != 2) return -1;
    return (double)(ut + st) / (double)sysconf(_SC_CLK_TCK);
#else
    return -1;
#endif
}
