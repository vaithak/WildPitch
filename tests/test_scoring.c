/* Can an 8-way keyboard player score past the (statue) keeper? Headless sweep over attacking-half positions,
 * stick directions, charge levels, standing vs jogging. Prints outcome maps:
 *   G goal  P post/bar  K blocked by a body (keeper/statue)  W wide/over  . ball still in play
 * Asserts a minimum share of goals from "natural" shooting positions so tuning can't silently break scoring. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"

static char shoot(int x, int y, int8_t sx, int8_t sy, int charge, int jog, int chip) {
    static SimState s;
    WpMatchConfig cfg = sim_default_config();
    sim_init(&s, &cfg);
    s.flags |= WP_SIMF_STATIC_OTHERS;                 /* aim-assist calibration vs statue keeper/defenders */
    int me = s.seat_player[0];
    s.players[me].pos = fv2_make(FX_INT(x), FX_INT(y));
    s.ball.pos = fv2_make(FX_INT(x) + FX(0.4), FX_INT(y));
    run_seat(&s, NOIN, 5);
    if (s.ball.owner != me) return '?';
    uint16_t btn = chip ? WP_BTN_CHIP : WP_BTN_SHOOT;
    WpSeatInput charge_in = jog ? (WpSeatInput){sx, sy, btn} : (WpSeatInput){0, 0, btn};
    uint16_t ev = run_seat(&s, charge_in, charge);
    ev |= run_seat(&s, (WpSeatInput){sx, sy, 0}, 1);                    /* release with the stick held */
    ev |= run_seat(&s, NOIN, 150);
    if (ev & WP_EV_GOAL) return 'G';
    if (ev & WP_EV_POST) return 'P';
    if (ev & WP_EV_BODY) return 'K';
    if (ev & WP_EV_OUT) return 'W';
    return '.';
}

int main(void) {
    static const int8_t dirs[4][2] = {{127, 0}, {127, 127}, {127, -127}, {0, 0}};
    static const char *dname[4] = {"right", "right+up", "right+down", "no stick"};
    int goals_natural = 0, shots_natural = 0;
    for (int chip = 0; chip <= 1; chip++)
    for (int jog = 0; jog <= 1; jog++)
    for (int c = 0; c < 2; c++) {
        int charge = c ? 36 : 18;
        printf("\n%s %s charge %d%%   (rows y=+8..-8, cols x=4..16 step 3; goal at x=20)\n",
               chip ? "CHIP" : "SHOT", jog ? "jogging" : "standing", charge * 100 / 36);
        for (int d = 0; d < 4; d++) {
            printf("  %-11s", dname[d]);
            for (int y = 8; y >= -8; y -= 4) {
                printf(" |");
                for (int x = 4; x <= 16; x += 3) {
                    char r = shoot(x, y, dirs[d][0], dirs[d][1], charge, jog, chip);
                    putchar(r);
                    /* natural: shot, from x>=10, |y|<=4, aiming with the stick toward goal (any 8-way dir) */
                    if (!chip && x >= 10 && y >= -4 && y <= 4 && d < 3) { shots_natural++; goals_natural += r == 'G'; }
                }
            }
            putchar('\n');
        }
    }
    printf("\nnatural shots (x>=10, |y|<=4, stick toward goal): %d/%d goals (%.0f%%)\n",
           goals_natural, shots_natural, 100.0 * goals_natural / shots_natural);
    CHECKF(goals_natural * 100 >= shots_natural * 45, "too hard to score from natural positions");   /* was 52%% at CP0.1 */
    return TEST_DONE();
}
