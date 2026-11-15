/* stats.c - player stats (D-034): generation per formation slot and the helpers every module reads them through.
 * Stats are 1..99, 50 = the old all-equal player; stat_k() maps a stat to k = (stat - 50) / 50 in [-0.98, 0.98], and
 * every effect is (old value) x (1 + gain * k) or similar, so 50 reproduces the old behaviour. In WP_STATS_NEUTRAL
 * every helper returns the old constant exactly and nothing draws from the RNG: the pre-stats sim, bit for bit. */
#include <string.h>
#include "sim/sim_internal.h"

/* slot templates: keeper / defender / midfielder / striker (pace, control, passing, shooting, tackling, stamina,
 * keeping). Every outfield template sums to the same total, so no slot is "better", only different. */
static const uint8_t TEMPLATE[4][WP_STAT_N] = {
    {46, 50, 52, 25, 30, 55, 70},   /* keeper */
    {56, 52, 56, 36, 70, 64, 10},   /* defender */
    {60, 64, 68, 50, 50, 68, 10},   /* midfielder */
    {68, 64, 54, 72, 34, 58, 10},   /* striker */
};

static int slot_kind(const SimState *s, int k) {
    if (k == 0) return 0;
    fx m = slot_mindset(s, k);
    return m >= FX(0.9) ? 3 : m <= FX(0.1) ? 1 : 2;
}

/* one team's deviations from the templates, zero-sum per stat over the team so both squads total the same */
static void gen_team(SimState *s, int team) {
    int n = s->n_per_side;
    for (int st = 0; st < WP_STAT_N; st++) {
        int dev[WP_MAX_PER_SIDE], sum = 0;
        for (int k = 0; k < n; k++) {
            int d = (int)wp_rng_range(&s->rng, 2 * T_STAT_SPREAD + 1) - T_STAT_SPREAD;   /* one draw per statement */
            dev[k] = d;
            sum += d;
        }
        for (int k = 0; k < n && sum != 0; k++) {          /* spread the residual so the total is exact */
            int fix = sum > 0 ? -1 : 1;
            dev[k] += fix;
            sum += fix;
            if (k == n - 1 && sum != 0) k = -1;
        }
        for (int k = 0; k < n; k++) {
            int v = (int)TEMPLATE[slot_kind(s, k)][st] + dev[k];
            s->stats[team * n + k][st] = (uint8_t)(v < 1 ? 1 : v > 99 ? 99 : v);
        }
    }
}

void stats_init(SimState *s, uint8_t mode) {
    s->stats_mode = mode <= WP_STATS_OWN ? mode : WP_STATS_NEUTRAL;
    memset(s->stats, 50, sizeof s->stats);
    if (s->stats_mode == WP_STATS_NEUTRAL) return;
    gen_team(s, 0);
    if (s->stats_mode == WP_STATS_MIRRORED) memcpy(s->stats[s->n_per_side], s->stats[0], (size_t)s->n_per_side * WP_STAT_N);
    else gen_team(s, 1);
}

fx stat_k(const SimState *s, int i, int which) {
    if (!s->stats_mode) return 0;
    return FX_INT((int)s->stats[i][which] - 50) / 50;
}

fx player_top_speed(const SimState *s, int i, int sprint) {
    fx base = sprint ? T_SPRINT_SPEED : T_WALK_SPEED;
    if (!s->stats_mode) return base;
    return fx_mul(base, FX_ONE + fx_mul(T_STAT_PACE, stat_k(s, i, WP_STAT_PACE)));
}
