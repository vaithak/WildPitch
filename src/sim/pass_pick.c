/* pass_pick.c - who receives a pass (D-021; research: reports/Pass receiver selection.md).
 * Structure shared by every football game (SWOS, YSoccer, GRF...): a cone around the stick plus one score,
 *   points = W_ANGLE * degrees off + W_DIST * metres (+ W_FAR per metre beyond PENALTY_M) + lane penalty
 *            (- STICKY for the receiver already previewed); lowest wins; ties -> fewer degrees off -> lower index.
 * - Two bearings: a team-mate counts as "where the stick points" if it points at him NOW or where he'll be when the
 *   ball arrives (flight ~ distance / T_PICK_FLIGHT_V, kept on the pitch) - the smaller angle is used.
 * - Stage A: outfield mates within T_PASS_CONE_DEG. Stage B (only if A is empty): keyboard input also takes outfield
 *   mates up to T_PASS_CONE_WIDE_DEG (never one behind the passer on a key with a forward/back component), and the
 *   keeper within T_PASS_KEEPER_CONE_DEG (unless excluded: through balls, lobs). Nobody -> a ball into space.
 * - Lane danger: an opponent inside T_LANE_CLEAR of the lane costs up to 150 points (a near-tie breaker, never an
 *   override of the stick) and tints the preview ring.
 * - The direction: the stick, a keyboard diagonal latched for a few ticks, the last stick for 0.5 s after release,
 *   then the facing. The preview (pick_recv) is recomputed every tick the player could pass; a pass reads it, so
 *   the ring on screen is what happens. */
#include "sim/sim_internal.h"

static int is_digital(int8_t x, int8_t y) {
    return (x == 0 || x == 127 || x == -127) && (y == 0 || y == 127 || y == -127);
}

/* nearest opponent clearance from the lane ball -> target (opponents advanced a little along their run) */
fx pass_lane_clear(const SimState *s, int team, fv2 a, fv2 t) {
    fv2 seg = fv2_sub(t, a);
    fx len2 = fv2_dot(seg, seg), len = fv2_len(seg), best = FX(99.0);
    if (len2 <= 0) return best;
    int o0 = (1 - team) * s->n_per_side;
    for (int j = o0; j < o0 + s->n_per_side; j++) {
        const WpPlayer *o = &s->players[j];
        fx u = fx_div(fv2_dot(fv2_sub(o->pos, a), seg), len2);
        if (u < 0 || u > FX_ONE) continue;
        fx look = fx_min(fx_div(fx_mul(u, len), T_PICK_FLIGHT_V), T_LANE_LOOK);
        fv2 op = fv2_add(o->pos, fv2_scale(o->vel, look));
        fx uu = fx_clamp(fx_div(fv2_dot(fv2_sub(op, a), seg), len2), 0, FX_ONE);
        fx c = fv2_len(fv2_sub(op, fv2_add(a, fv2_scale(seg, uu))));
        if (c < best) best = c;
    }
    return best;
}

int pass_pick_ex(const SimState *s, int i, wp_angle dir, int flags, int prev, uint8_t *danger) {
    const WpPlayer *p = &s->players[i];
    int best = -1; int32_t best_pts = 0, best_off = 0; fx best_clear = FX(99.0);
    fx hl = s->pitch.length / 2 - FX(1.0), hw = s->pitch.width / 2 - FX(1.0);
    fx sx = fx_cos(dir);
    for (int stage = 0; stage < 2 && best < 0; stage++)
    for (int j = p->team * s->n_per_side; j < (p->team + 1) * s->n_per_side; j++) {
        const WpPlayer *q = &s->players[j];
        int keeper = q->role == WP_ROLE_KEEPER;
        if (j == i || (keeper && (stage == 0 || !(flags & PICK_KEEPER)))) continue;
        fx d0 = fv2_len(fv2_sub(q->pos, p->pos));
        if (d0 > T_PASS_MAX_DIST + T_PASS_PREFILTER_M) continue;
        fv2 qp = fv2_add(q->pos, fv2_scale(q->vel, fx_div(d0, T_PICK_FLIGHT_V)));
        qp = fv2_make(fx_clamp(qp.x, -hl, hl), fx_clamp(qp.y, -hw, hw));
        fv2 d = fv2_sub(qp, p->pos), dn = fv2_sub(q->pos, p->pos);
        fx dist = fv2_len(d);
        if (dist < T_PASS_MIN_DIST || dist > T_PASS_MAX_DIST) continue;
        int32_t off = ang_diff(fx_atan2(d.y, d.x), dir), offn = ang_diff(fx_atan2(dn.y, dn.x), dir);
        if (off < 0) off = -off;
        if (offn < 0) offn = -offn;
        if (offn < off) off = offn;                     /* at him now, or where he's going */
        if (stage == 0) {
            if (off > DEG_TO_ANG(T_PASS_CONE_DEG)) continue;
        } else if (keeper) {
            if (off > DEG_TO_ANG(T_PASS_KEEPER_CONE_DEG)) continue;
        } else {
            if (!(flags & PICK_DIGITAL) || off > DEG_TO_ANG(T_PASS_CONE_WIDE_DEG)) continue;
            if (fx_abs(sx) > FX(0.3) && fx_mul(d.x, sx > 0 ? FX_ONE : -FX_ONE) < -T_PASS_WIDE_BACK_M) continue;
        }
        int32_t pts = (int32_t)(((int64_t)off * 360 * T_PASS_W_ANGLE) / 65536) + (int32_t)(dist / (FX_ONE / T_PASS_W_DIST));
        if (dist > T_PASS_PENALTY_M) pts += (int32_t)((dist - T_PASS_PENALTY_M) / (FX_ONE / T_PASS_W_FAR));
        fx clear = pass_lane_clear(s, p->team, s->ball.pos, qp);
        if (clear < T_LANE_CLEAR) pts += (int32_t)(((int64_t)(T_LANE_CLEAR - clear) * T_PASS_W_LANE_M) >> FX_SHIFT);
        if (j == prev) pts -= T_PASS_STICKY;
        if (best < 0 || pts < best_pts || (pts == best_pts && off < best_off)) {
            best = j; best_pts = pts; best_off = off; best_clear = clear;
        }
    }
    if (danger) {
        fx dg = best >= 0 && best_clear < T_LANE_CLEAR ? fx_div(T_LANE_CLEAR - best_clear, T_LANE_CLEAR) : 0;
        *danger = (uint8_t)fx_clamp(dg * 255 / FX_ONE, 0, 255);
    }
    return best;
}

int pass_pick(const SimState *s, int i, wp_angle dir) {
    return pass_pick_ex(s, i, dir, PICK_DIGITAL | PICK_KEEPER, -1, 0);
}

/* the stick memory for a seat (diagonal latch, last direction) -> the effective pass direction */
static wp_angle effective_dir(SimState *s, int seat, const WpPlayer *p, const WpSeatInput *in, int *digital) {
    int8_t x = in->move_x, y = in->move_y;
    if (s->pick_diag_age[seat] < 255) s->pick_diag_age[seat]++;
    if (s->pick_stick_age[seat] < 255) s->pick_stick_age[seat]++;
    if (x || y) {
        if (is_digital(x, y) && x && y) {               /* a keyboard diagonal: remember it */
            s->pick_diag[seat][0] = x; s->pick_diag[seat][1] = y; s->pick_diag_age[seat] = 0;
        } else if (is_digital(x, y) && s->pick_diag_age[seat] <= T_PICK_DIAG_LATCH &&
                   ((x && x == s->pick_diag[seat][0]) || (y && y == s->pick_diag[seat][1]))) {
            x = s->pick_diag[seat][0]; y = s->pick_diag[seat][1];   /* one key let go a tick early */
        }
        s->pick_stick[seat][0] = x; s->pick_stick[seat][1] = y; s->pick_stick_age[seat] = 0;
    } else if (s->pick_stick_age[seat] <= T_PICK_STICK_MEM) {
        x = s->pick_stick[seat][0]; y = s->pick_stick[seat][1];
    }
    *digital = is_digital(x, y);
    if (!x && !y) return p->facing;
    return fx_atan2(FX_INT(y), FX_INT(x));
}

void pass_pick_update(SimState *s, int seat, int i, const WpSeatInput *in) {
    const WpPlayer *p = &s->players[i];
    int digital;
    wp_angle dir = effective_dir(s, seat, p, in, &digital);
    s->pick_dir[seat] = dir;
    const WpBall *b = &s->ball;
    int could_pass = s->phase == WP_PHASE_PLAY && p->action == WP_ACT_NONE &&
                     (b->owner == i || (b->owner < 0 && b->z <= WP_BAND_FEET_MAX &&
                                        fv2_len(fv2_sub(b->pos, p->pos)) <= FX(1.6)));
    if (!could_pass) { s->pick_recv[seat] = -1; s->pick_danger[seat] = 0; return; }
    s->pick_recv[seat] = (int8_t)pass_pick_ex(s, i, dir, (digital ? PICK_DIGITAL : 0) | PICK_KEEPER,
                                              s->pick_recv[seat], &s->pick_danger[seat]);
}

void pass_pick_reset(SimState *s, int seat) {
    s->pick_recv[seat] = -1;
    s->pick_danger[seat] = 0;
    s->pick_stick_age[seat] = s->pick_diag_age[seat] = 255;
}

/* distance along `dir` from `from` until it leaves the rectangle |x| <= X, |y| <= Y (0 if it never is inside) */
fx ray_exit_dist(fv2 from, fv2 dir, fx X, fx Y) {
    fx tmin = -FX(1000.0), tmax = FX(1000.0);
    fx o[2] = {from.x, from.y}, u[2] = {dir.x, dir.y}, lim[2] = {X, Y};
    for (int k = 0; k < 2; k++) {
        if (fx_abs(u[k]) < FX(0.001)) {
            if (o[k] < -lim[k] || o[k] > lim[k]) return 0;
            continue;
        }
        fx t1 = fx_div(-lim[k] - o[k], u[k]), t2 = fx_div(lim[k] - o[k], u[k]);
        if (t1 > t2) { fx tt = t1; t1 = t2; t2 = tt; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
    }
    if (tmax < fx_max(tmin, 0)) return 0;
    return tmax;
}
