/* lob.c - lofted passes (D-045 meter, D-046 solve). One trajectory family: vz = T_LOB_VZ_BASE + T_LOB_VZ_PER_M * D
 * (no clamp), D = where it first lands on the grass. A 64-entry table indexed by D (0.75 m steps) holds the
 * horizontal launch speed, and where / when the ball comes DOWN through T_LOB_RECV_Z (the receiving height); it is
 * filled once (sim_init) from the fixed-point forward model ball_flight_to, so it is identical on every compiler,
 * immutable afterwards and not part of SimState (a pure function of the tuning constants).
 *  - Meter (human hold h): D(h) = T_LOB_MIN_D .. lob_max_d (pitch-scaled) along the aim. A team-mate whose PREDICTED
 *    point P_j = q_j + v_j * t_c (t_c = his ball's time to receiving height) is within T_LOB_SNAP + T_LOB_SNAP_K * D
 *    of the landing (+ T_LOB_ANGLE_M * angle/pi off the aim) gets it: dropped on P_j, the meter's surplus / shortfall
 *    as a depth bias along his run (<= T_LOB_BIAS_MAX). Nobody: it lands D along the aim, kept inside the pitch.
 *  - Tap / AI: to the receiver's predicted point (AI: D clamped to the meter range; a ball to a mate in the box,
 *    T_AI_CROSS_ZONE from the goal, is met at head height - crosses and corners are for heading: since D-051 a
 *    driven cross (its own flatter family, cross_solve) through T_CROSS_Z; T_CROSS_DRIVEN 0 = the stage-1 stopgap,
 *    the lob family met at T_LOB_HEAD_Z).
 *  Every landing / meeting point stays T_THROUGH_MARGIN_X / _Y inside the goal lines / touchlines. */
#include "sim/sim_internal.h"

#define LT_N    64
#define LT_STEP FX(0.75)
static fx LT_VH[LT_N];                              /* launch vh */
static fx LT_DC[2][LT_N], LT_TC[2][LT_N];          /* distance + time to come down through T_LOB_RECV_Z / T_LOB_HEAD_Z */
static int lt_ready;
static void cross_table_init(void);

static fx lob_vz(fx D) { return T_LOB_VZ_BASE + fx_mul(T_LOB_VZ_PER_M, D); }

void lob_table_init(void) {
    if (lt_ready) return;
    for (int k = 0; k < LT_N; k++) {
        fx D = LT_STEP * k, vz = lob_vz(D), vh = ball_lob_speed_for(D, vz);
        LT_VH[k] = vh;
        for (int z = 0; z < 2; z++) {
            fx dc;
            int ticks;
            ball_flight_to(vh, vz, 0, z ? T_LOB_HEAD_Z : T_LOB_RECV_Z, &dc, &ticks);
            LT_DC[z][k] = dc;
            LT_TC[z][k] = FX_INT(ticks) / WP_TICK_HZ;
        }
    }
    lt_ready = 1;
    cross_table_init();
}

static fx lt_at(const fx *tab, fx D) {
    if (!lt_ready) lob_table_init();
    D = fx_clamp(D, 0, LT_STEP * (LT_N - 1));
    int i = (int)(D / LT_STEP);
    if (i >= LT_N - 1) i = LT_N - 2;
    fx frac = fx_div(D - LT_STEP * i, LT_STEP);
    return tab[i] + fx_mul(tab[i + 1] - tab[i], frac);
}

/* driven crosses (D-051): a flatter family, vz = T_CROSS_VZ_BASE + T_CROSS_VZ_PER_M * d, indexed by d = where it
 * comes DOWN through T_CROSS_Z (the receiver's head): launch vh and the time to get there. Same fill rules as LT_*. */
static fx CT_VH[LT_N], CT_T[LT_N];
static int ct_ready;
static fx cross_vz(fx d) { return T_CROSS_VZ_BASE + fx_mul(T_CROSS_VZ_PER_M, d); }
static void cross_table_init(void) {
    for (int k = 0; k < LT_N; k++) {
        fx d = LT_STEP * k, vz = cross_vz(d), lo = 0, hi = FX(40.0), dist;
        int ticks = 0;
        for (int it = 0; it < 16; it++) {
            fx mid = (lo + hi) / 2;
            ball_flight_to(mid, vz, 0, T_CROSS_Z, &dist, &ticks);
            if (dist < d) lo = mid; else hi = mid;
        }
        ball_flight_to(hi, vz, 0, T_CROSS_Z, &dist, &ticks);
        CT_VH[k] = hi;
        CT_T[k] = FX_INT(ticks) / WP_TICK_HZ;
    }
    ct_ready = 1;
}
static fx ct_at(const fx *tab, fx d) {
    if (!ct_ready) cross_table_init();
    return lt_at(tab, d);
}

fx lob_recv_time(fx D, int head) { return lt_at(LT_TC[head != 0], D); }
fx lob_recv_dist(fx D, int head) { return lt_at(LT_DC[head != 0], D); }

/* inverse: the D whose ball comes down through the receiving height (head: T_LOB_HEAD_Z) d away */
static fx lob_d_for_recv(fx d, int head) {
    if (!lt_ready) lob_table_init();
    const fx *dc = LT_DC[head != 0];
    if (d <= dc[0]) return 0;
    if (d >= dc[LT_N - 1]) return LT_STEP * (LT_N - 1);
    int lo = 0, hi = LT_N - 1;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (dc[mid] < d) lo = mid; else hi = mid; }
    fx span = dc[hi] - dc[lo];
    return LT_STEP * lo + (span > 0 ? fx_mul(LT_STEP, fx_div(d - dc[lo], span)) : 0);
}

fx lob_max_d(const SimState *s) { return fx_min(T_LOB_MAX_D, fx_mul(s->pitch.length, T_LOB_MAX_D_PITCH)); }
/* how long player i's lob meter takes to fill: slower for the taker of a dead-ball restart, who has the time to
 * weigh it (D-053: a corner into the box needs a hold window wider than open play's 0.4 s ramp) */
int lob_ramp_ticks(const SimState *s, int i) {
    int dead = s->restart_kind && s->restart_kind != WP_RS_KICKOFF && s->restart_taker == i && s->ball.owner == i;
    return dead ? T_LOB_SET_RAMP : T_CHARGE_MAX_TICKS - T_LOB_TAP_TICKS;
}

fx lob_meter_dist(const SimState *s, fx h) { return fx_lerp(T_LOB_MIN_D, lob_max_d(s), fx_clamp(h, 0, FX_ONE)); }

static fv2 in_margins(const SimState *s, fv2 p) {
    fx hl = s->pitch.length / 2 - T_THROUGH_MARGIN_X, hw = s->pitch.width / 2 - T_THROUGH_MARGIN_Y;
    return fv2_make(fx_clamp(p.x, -hl, hl), fx_clamp(p.y, -hw, hw));
}

/* the direction a receiver's depth bias is applied in: along his run, else straight on from the ball */
static fv2 run_dir(const WpPlayer *q, fv2 from) {
    fx sp = fv2_len(q->vel);
    if (sp > FX(1.0)) return fv2_scale(q->vel, fx_div(FX_ONE, sp));
    fv2 d = fv2_sub(q->pos, from);
    return fx_abs(d.x) + fx_abs(d.y) > FX(0.05) ? fv2_norm(d) : fv2_make(FX_ONE, 0);
}

/* team-mate j's predicted receiving point for a lob from `from` (3 iterations of t_c), plus `bias` along his run */
static fv2 lead_point(const SimState *s, int j, fv2 from, fx bias, int head, fx *tc) {
    const WpPlayer *q = &s->players[j];
    fv2 u = run_dir(q, from), P = q->pos;
    fx t = lob_recv_time(lob_d_for_recv(fv2_len(fv2_sub(q->pos, from)), head), head);
    for (int it = 0; it < 3; it++) {
        P = in_margins(s, fv2_add(fv2_add(q->pos, fv2_scale(q->vel, t)), fv2_scale(u, bias)));
        t = lob_recv_time(lob_d_for_recv(fv2_len(fv2_sub(P, from)), head), head);
    }
    *tc = t;
    return P;
}

/* the meter's landing spot: D(h) along dir, shortened so it stays inside the margins (never bends the aim) */
static fx meter_land(const SimState *s, fv2 from, wp_angle dir, fx D) {
    fv2 u = fv2_from_angle(dir, FX_ONE);
    fx room = ray_exit_dist(from, u, s->pitch.length / 2 - T_THROUGH_MARGIN_X, s->pitch.width / 2 - T_THROUGH_MARGIN_Y);
    return fx_min(D, room);
}

/* at the release: which team-mate does the meter (h > 0) snap to? -1 = nobody, it drops into space */
int lob_snap(const SimState *s, int i, wp_angle dir, fx h) {
    const WpPlayer *p = &s->players[i];
    fv2 from = s->ball.pos;
    fx D = lob_meter_dist(s, h);
    fv2 land = fv2_add(from, fv2_from_angle(dir, meter_land(s, from, dir, D)));
    fx tol = T_LOB_SNAP + fx_mul(D, T_LOB_SNAP_K), best = 0;
    int r = -1;
    for (int j = p->team * s->n_per_side; j < (p->team + 1) * s->n_per_side; j++) {
        if (j == i || s->players[j].role == WP_ROLE_KEEPER) continue;
        fx tc;
        fv2 P = lead_point(s, j, from, 0, 0, &tc);
        fv2 d = fv2_sub(P, from);
        int32_t a = fx_abs(d.x) + fx_abs(d.y) > FX(0.05) ? ang_diff(fx_atan2(d.y, d.x), dir) : 0;
        if (a < 0) a = -a;
        fx miss = fv2_len(fv2_sub(P, land)) + fx_mul(T_LOB_ANGLE_M, (fx)(a * 2));   /* a/32768 = angle/pi */
        if (miss <= tol && (r < 0 || miss < best)) { r = j; best = miss; }
    }
    return r;
}

/* At contact: the launch of a lofted ball from the ball's spot. r >= 0: to team-mate r's predicted point (h > 0: with
 * the meter's depth bias), met at T_LOB_RECV_Z (head: at T_LOB_HEAD_Z - the AI's balls into the box), D clamped to
 * [d_min, lob_max_d]; r < 0: lands D(h) along dir (h = 0: T_LOB_TAP_D). The meeting point / time for the receive
 * hand-off come back in *meet / *t_meet. */
fv2 lob_solve(const SimState *s, int r, wp_angle dir, fx h, fx d_min, int head, fx *vz, fv2 *meet, fx *t_meet) {
    fv2 from = s->ball.pos, u;
    fx D, d_max = lob_max_d(s);
    if (r >= 0) {
        fx bias = 0, tc;
        if (h > 0 && !head) {                         /* (a ball to his head in the box: the meter only picks him, D-053) */
            fv2 P0 = lead_point(s, r, from, 0, head, &tc);
            bias = fx_clamp(lob_meter_dist(s, h) - fv2_len(fv2_sub(P0, from)), -T_LOB_BIAS_MAX, T_LOB_BIAS_MAX);
        }
        fv2 P = lead_point(s, r, from, bias, head, &tc);
        fv2 d = fv2_sub(P, from);
        fx dl = fv2_len(d);
        u = dl > FX(0.05) ? fv2_scale(d, fx_div(FX_ONE, dl)) : fv2_from_angle(dir, FX_ONE);
        D = fx_clamp(lob_d_for_recv(dl, head), fx_min(d_min, d_max), d_max);
    } else {
        u = fv2_from_angle(dir, FX_ONE);
        head = 0;
        D = meter_land(s, from, dir, h > 0 ? lob_meter_dist(s, h) : fx_min(T_LOB_TAP_D, d_max));
    }
    *vz = lob_vz(D);
    *meet = fv2_add(from, fv2_scale(u, lob_recv_dist(D, head)));
    *t_meet = lob_recv_time(D, head);
    return fv2_scale(u, lt_at(LT_VH, D));
}

/* a driven cross (D-051) from the ball's spot to team-mate r's predicted point, coming down through his head
 * (T_CROSS_Z) there; d clamped to [T_LOB_AI_MIN_D, lob_max_d]. Meeting point / time for the receive hand-off.
 * Returns a zero velocity when an opponent stands under its path (the caller lofts it instead). */
fv2 cross_solve(const SimState *s, int r, fx *vz, fv2 *meet, fx *t_meet) {
    const WpPlayer *q = &s->players[r];
    fv2 from = s->ball.pos, P = q->pos;
    fx d_max = lob_max_d(s), t = ct_at(CT_T, fv2_len(fv2_sub(q->pos, from))), d = 0;
    for (int it = 0; it < 3; it++) {
        P = in_margins(s, fv2_add(q->pos, fv2_scale(q->vel, t)));
        d = fx_clamp(fv2_len(fv2_sub(P, from)), fx_min(T_LOB_AI_MIN_D, d_max), d_max);
        t = ct_at(CT_T, d);
    }
    fv2 u = fv2_sub(P, from);
    fx ul = fv2_len(u);
    u = ul > FX(0.05) ? fv2_scale(u, fx_div(FX_ONE, ul)) : fv2_make(FX_ONE, 0);
    /* the first man: an opponent standing under the driven path where it is still at a height he can play
     * (before the last T_CROSS_CLEAR_END metres, by the drag-free arc) - then it isn't on, return 0 */
    fx vzd = cross_vz(d), vh = ct_at(CT_VH, d);
    for (int j = (1 - q->team) * s->n_per_side; j < (2 - q->team) * s->n_per_side; j++) {
        fv2 o = fv2_sub(s->players[j].pos, from);
        fx along = fv2_dot(o, u);
        if (along <= 0 || along >= d - T_CROSS_CLEAR_END) continue;
        if (fx_abs(fx_mul(o.x, u.y) - fx_mul(o.y, u.x)) > T_CROSS_CLEAR_W) continue;   /* off its line */
        fx tt = fx_div(along, vh), z = fx_mul(vzd, tt) - fx_mul(fx_mul(T_GRAVITY, tt), tt) / 2;
        if (z < T_CB_JUMP) return fv2_make(0, 0);
    }
    *vz = cross_vz(d);
    *meet = fv2_add(from, fv2_scale(u, d));
    *t_meet = t;
    return fv2_scale(u, ct_at(CT_VH, d));
}
