/* The quick-match half lengths (CP0.6: 3 / 5 / 8 real minutes) run a whole AI-vs-AI match to full time: half time
 * and full time come after regulation plus at most the stoppage cap, the broadcast clock reaches 45' and 90', it
 * never runs backwards within a half, and stoppage (uint16 ticks, scaled by half_ticks / 45) never overflows. */
#include <stdio.h>
#include <string.h>
#include "test_util.h"
#include "sim/sim_internal.h"

static SimState s;

static void run(int minutes) {
    WpMatchConfig cfg = sim_config_7v7();
    cfg.half_ticks = (uint32_t)minutes * 60u * WP_TICK_HZ;
    sim_init(&s, &cfg);
    sim_kickoff_setpiece(&s);
    for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;   /* AI vs AI */
    WpInput in;
    memset(&in, 0, sizeof in);
    uint32_t H = cfg.half_ticks, cap = (uint32_t)T_STOPPAGE_MAX_MIN * (H / 45) + 30 * WP_TICK_HZ;   /* + a set piece */
    long ht = -1, ft = -1;
    int prev = -1, mono = 1, max_added = 0, shown_ht = 0, shown_ft = 0;
    for (long t = 0; t < (long)(2 * (H + cap) + T_HALFTIME_TICKS + 600) && ft < 0; t++) {
        sim_step(&s, &in);
        int mm, ss, ad, ex;
        sim_clock_display(&s, &mm, &ss, &ad, &ex);
        int shown = mm * 60 + ss + ex;
        if (ad > max_added) max_added = ad;
        if (s.phase != WP_PHASE_HALFTIME && prev >= 0 && shown < prev && !(s.half == 1 && prev >= 2700 && shown == 2700)) mono = 0;
        prev = shown;
        if (s.phase == WP_PHASE_HALFTIME && ht < 0) { ht = t; shown_ht = mm * 60 + ss; }
        if (s.phase == WP_PHASE_FULLTIME) { ft = t; shown_ft = mm * 60 + ss; }
    }
    printf("%d min halves: half time at tick %ld (%.1f s real), full time at %ld (%.1f s), clock %d:%02d / %d:%02d, "
           "max added %d', score %d-%d\n", minutes, ht, (double)ht / 60, ft, (double)ft / 60, shown_ht / 60, shown_ht % 60,
           shown_ft / 60, shown_ft % 60, max_added, s.score[0], s.score[1]);
    CHECKF(ht >= (long)H && ht <= (long)(H + cap), "%d min: half time at %ld", minutes, ht);
    CHECKF(ft >= ht + (long)H && ft <= ht + T_HALFTIME_TICKS + (long)(H + cap) + 2, "%d min: full time at %ld", minutes, ft);
    CHECKF(shown_ht >= 45 * 60 && shown_ft >= 90 * 60, "%d min: clock %d / %d", minutes, shown_ht, shown_ft);
    CHECKF(mono, "%d min: clock ran backwards", minutes);
    CHECKF(max_added >= 1 && max_added <= T_STOPPAGE_MAX_MIN, "%d min: added %d", minutes, max_added);
}

int main(void) {
    run(3);
    run(5);
    run(8);
    return TEST_DONE();
}
