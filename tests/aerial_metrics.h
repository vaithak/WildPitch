/* aerial_metrics.h - aerial counters for AI-vs-AI harnesses (CP0.5a stage 0). Read-only: everything comes from the
 * per-tick events and test-side state (the sim is never written, nothing is added to SimState, the golden hash is
 * untouched). Shared by tools/playtest/aerial.c and tests, so "the same metric" means the same code.
 *
 * Definitions (frozen; change them only together with the baselines in docs/STATUS.md):
 *  - LOB  = a WP_KICK_LOB struck with the feet (not from the hands: keeper throws / long throw-ins excluded).
 *           A CROSS is a lob struck from wide in the attacking third (|y| > 0.40 half-width, x beyond 0.35 half-length
 *           toward the goal attacked) - the brain's cross gate, judged by geometry at the kick.
 *           Distance at the kick = |intended receiver - ball| (untargeted lobs: own bucket).
 *  - LOB COMPLETED = the first player other than the kicker to win the ball (own it) or head it is a team-mate.
 *           Ends unresolved (= not completed) on an out / goal / stoppage, or when the kicker plays it again.
 *  - AERIAL CONTACT = the first contact by an outfield player other than the last kicker with a ball that has been
 *           above the feet band (z > WP_BAND_FEET_MAX) since that kick (shots / chips / header shots excluded). Band = ball height at the END OF THE
 *           PREVIOUS TICK (before the touch moved it): feet <= 0.55 (already dropped), body <= 1.45, head above.
 *           Kinds: GAIN (he owns it), BODY (it bounced off him), HEADER. Keepers are counted apart.
 *           CONTROL SUCCESS (gains and body hits only) = his team owns the ball AERIAL_KEEP_TICKS later
 *           (or one of them has kicked it by then).
 *  - HEADER PASS COMPLETED = after a PASS header (kick.foot == 0), the next player to own / head it is a team-mate.
 *  - per minute = per minute of match ticks (3600 ticks). */
#ifndef WP_AERIAL_METRICS_H
#define WP_AERIAL_METRICS_H
#include <stdio.h>
#include "sim/sim_internal.h"

#define AERIAL_KEEP_TICKS 30
enum { AB_FEET = 0, AB_BODY, AB_HEAD, AB_N };
enum { AK_GAIN = 0, AK_BODY, AK_HEADER, AK_N };
#define AERIAL_DIST_N 8                   /* lob distance buckets: <10, <15, <20, <25, <30, <40, >=40, untargeted */

typedef struct {
    long ticks;
    long lobs, lobs_done, crosses, crosses_done;
    long lob_dist[AERIAL_DIST_N], lob_dist_done[AERIAL_DIST_N];
    long contact[AB_N][AK_N];            /* first aerial contacts by band x kind */
    long ctrl_try[AB_N], ctrl_ok[AB_N];  /* control success (gains + body hits) by band */
    long keeper_contacts;
    long headers, header_shots, header_pass, header_pass_done;
} AerialStats;

typedef struct {
    int lob_from, lob_team, lob_cross, lob_bucket;   /* pending lob (lob_from < 0: none) */
    int hp_from, hp_team;                            /* pending header pass */
    int air_from, air_up;                            /* last kicker; the ball has been above the feet band since */
    int chk_team, chk_band, chk_left;                /* pending control check */
    int prev_owner, prev_held;
    fx prev_z;
} AerialTrack;

static inline void aerial_reset(AerialTrack *t, const SimState *s) {
    t->lob_from = t->hp_from = t->air_from = -1;
    t->air_up = 0;
    t->chk_left = 0;
    t->prev_owner = s->ball.owner;
    t->prev_held = s->ball.held;
    t->prev_z = s->ball.z;
}

static inline int aerial_band(fx z) { return z <= WP_BAND_FEET_MAX ? AB_FEET : z <= WP_BAND_BODY_MAX ? AB_BODY : AB_HEAD; }

/* call once after every sim_step */
static inline void aerial_update(AerialStats *a, AerialTrack *t, const SimState *s) {
    const WpBall *b = &s->ball;
    uint16_t ev = s->events;
    a->ticks++;
    int stop = (ev & (WP_EV_OUT | WP_EV_GOAL | WP_EV_RESET)) || s->phase != WP_PHASE_PLAY;
    int gain = b->owner >= 0 && b->owner != t->prev_owner;           /* someone new owns it */
    int header = (ev & WP_EV_HEADER) != 0, kick = (ev & WP_EV_KICK) != 0;
    int who = gain ? b->owner : header ? b->last_touch : -1;          /* who won / headed it this tick */

    /* pending control check */
    if (t->chk_left > 0) {
        int ok = b->owner >= 0 && s->players[b->owner].team == t->chk_team;
        if (kick && b->last_touch >= 0 && s->players[b->last_touch].team == t->chk_team) { a->ctrl_ok[t->chk_band]++; t->chk_left = 0; }
        else if (stop || (b->owner >= 0 && !ok)) t->chk_left = 0;
        else if (--t->chk_left == 0 && ok) a->ctrl_ok[t->chk_band]++;
    }
    /* pending lob / header pass: who gets it first */
    if (t->lob_from >= 0) {
        if (who >= 0 && who != t->lob_from) {
            if (s->players[who].team == t->lob_team) {
                a->lobs_done++; a->lob_dist_done[t->lob_bucket]++;
                if (t->lob_cross) a->crosses_done++;
            }
            t->lob_from = -1;
        } else if (stop || who == t->lob_from || (kick && !header && b->last_touch == t->lob_from)) t->lob_from = -1;
    }
    if (t->hp_from >= 0 && !(header && b->last_touch == t->hp_from && kick)) {
        if (who >= 0 && who != t->hp_from) {
            if (s->players[who].team == t->hp_team) a->header_pass_done++;
            t->hp_from = -1;
        } else if (stop || who == t->hp_from) t->hp_from = -1;
    }
    /* first aerial contact */
    if (t->air_from >= 0 && t->air_up) {
        int c = -1, kind = -1;
        if (gain && b->owner != t->air_from) { c = b->owner; kind = AK_GAIN; }
        else if (header && b->last_touch != t->air_from) { c = b->last_touch; kind = AK_HEADER; }
        else if ((ev & WP_EV_BODY) && b->last_touch >= 0 && b->last_touch != t->air_from) { c = b->last_touch; kind = AK_BODY; }
        if (c >= 0) {
            if (s->players[c].role == WP_ROLE_KEEPER) a->keeper_contacts++;
            else {
                int band = aerial_band(t->prev_z);
                a->contact[band][kind]++;
                if (kind != AK_HEADER) {
                    a->ctrl_try[band]++;
                    if (kind == AK_GAIN) { t->chk_team = s->players[c].team; t->chk_band = band; t->chk_left = AERIAL_KEEP_TICKS; }
                }
            }
            t->air_from = -1;
        } else if (stop || (b->owner >= 0 && b->owner == t->air_from)) t->air_from = -1;
    }
    /* new kicks */
    if (kick && b->last_touch >= 0) {
        int k = b->last_touch;
        const WpPlayer *p = &s->players[k];
        if (header) {
            a->headers++;
            if (p->kick.foot) a->header_shots++;
            else { a->header_pass++; t->hp_from = k; t->hp_team = p->team; }
        } else if (p->kick.kind == WP_KICK_LOB && !t->prev_held) {
            int sg = team_dir(s, p->team);
            fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
            int cross = fx_abs(b->pos.y) > fx_mul(hw, FX(0.40)) && b->pos.x * sg > fx_mul(hl, FX(0.35));
            int bucket = AERIAL_DIST_N - 1;
            if (p->kick.to) {
                fx d = fv2_len(fv2_sub(s->players[p->kick.to - 1].pos, b->pos));
                bucket = d < FX(10.0) ? 0 : d < FX(15.0) ? 1 : d < FX(20.0) ? 2 : d < FX(25.0) ? 3 : d < FX(30.0) ? 4 : d < FX(40.0) ? 5 : 6;
            }
            a->lobs++; a->lob_dist[bucket]++;
            if (cross) a->crosses++;
            t->lob_from = k; t->lob_team = p->team; t->lob_cross = cross; t->lob_bucket = bucket;
        }
        int shot = header ? p->kick.foot != 0 : (p->kick.kind == WP_KICK_SHOT || p->kick.kind == WP_KICK_CHIP);
        t->air_from = shot ? -1 : k;                 /* shots are blocked, not received: not aerial receptions */
        t->air_up = 0;
    }
    if (t->air_from >= 0 && b->owner < 0 && b->z > WP_BAND_FEET_MAX) t->air_up = 1;
    if (stop) { t->lob_from = t->hp_from = t->air_from = -1; }
    t->prev_owner = b->owner;
    t->prev_held = b->held;
    t->prev_z = b->z;
}

static inline double aerial_pct(long n, long d) { return d ? 100.0 * (double)n / (double)d : 0.0; }

static inline void aerial_print(const AerialStats *a) {
    double mins = (double)a->ticks / 3600.0;
    static const char *bn[AB_N] = {"feet (<=0.55)", "body (<=1.45)", "head (>1.45) "};
    static const char *dn[AERIAL_DIST_N] = {"<10", "10-15", "15-20", "20-25", "25-30", "30-40", ">=40", "untargeted"};
    printf("  lobs %ld (%.2f/min) completed %ld (%.0f%%); crosses %ld (%.2f/min) completed %ld (%.0f%%)\n",
           a->lobs, a->lobs / mins, a->lobs_done, aerial_pct(a->lobs_done, a->lobs),
           a->crosses, a->crosses / mins, a->crosses_done, aerial_pct(a->crosses_done, a->crosses));
    printf("  lob distance at the kick:");
    for (int k = 0; k < AERIAL_DIST_N; k++)
        if (a->lob_dist[k]) printf(" %s m %ld (%.0f%%)", dn[k], a->lob_dist[k], aerial_pct(a->lob_dist_done[k], a->lob_dist[k]));
    printf("\n");
    printf("  first aerial contact (outfield; keepers %ld):\n", a->keeper_contacts);
    for (int k = 0; k < AB_N; k++)
        printf("    %s gain %5ld body %5ld header %5ld | control success %ld/%ld (%.0f%%)\n", bn[k], a->contact[k][AK_GAIN],
               a->contact[k][AK_BODY], a->contact[k][AK_HEADER], a->ctrl_ok[k], a->ctrl_try[k], aerial_pct(a->ctrl_ok[k], a->ctrl_try[k]));
    printf("  headers %ld (%.2f/min): shots %ld, passes %ld completed %ld (%.0f%%)\n", a->headers, a->headers / mins,
           a->header_shots, a->header_pass, a->header_pass_done, aerial_pct(a->header_pass_done, a->header_pass));
}
#endif
