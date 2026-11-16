/* sim.c - orchestration: config, init, kickoff reset, per-tick order, match flow, hashing.
 * Tick order: clear events -> players (input, movement, kicks, control) -> ball (substepped physics,
 * collisions, goal/out detection) -> match flow (goal/out pauses, resets) -> bookkeeping. */
#include "sim/sim_internal.h"
#include <string.h>

#define WP_SIMSTATE_SIZE_EXPECTED 2240  /* x86-64 gcc/clang; see _Static_assert near sim_hash */

/* 7-a-side (owner requests): keeper + 2-3-1 on 80 x 52 m - a real pitch's 105:68 (1.54) proportion - with
 * 5.6 x 2.1 m goals (7.32/68 of the width, like a real pitch) */
WpMatchConfig sim_config_7v7(void) {
    WpMatchConfig c = sim_default_config();
    c.n_per_side = 7;
    c.pitch.length = FX(80.0);
    c.pitch.width = FX(52.0);
    c.pitch.goal_width = FX(5.6);
    c.pitch.goal_height = FX(2.1);
    c.pitch.goal_depth = FX(1.5);
    c.stats_mode = WP_STATS_OWN;                      /* generated squads, equal totals (D-034) */
    c.half_ticks = T_HALF_TICKS;                      /* two 5-minute halves, 90' at 9x (D-036) */
    c.assist_level = WP_ASSIST_NORMAL;                /* movement assist toward the ball (D-037) */
    c.rules = WP_RULE_FOULS | WP_RULE_OFFSIDE;        /* a real match (D-040/D-041) */
    return c;
}

/* the game's 5-a-side: 60 x 39 m (1.54), 4.2 x 2 m goals (real goal-to-width ratio). sim_default_config() keeps the 40 x 25 m regression format. */
WpMatchConfig sim_config_5v5(void) {
    WpMatchConfig c = sim_default_config();
    c.pitch.length = FX(60.0);
    c.pitch.width = FX(39.0);
    c.pitch.goal_width = FX(4.2);
    c.pitch.goal_height = FX(2.0);
    c.pitch.goal_depth = FX(1.3);
    c.stats_mode = WP_STATS_OWN;
    c.half_ticks = T_HALF_TICKS;
    c.assist_level = WP_ASSIST_NORMAL;
    c.rules = WP_RULE_FOULS | WP_RULE_OFFSIDE;
    return c;
}

/* Formation spot k of n (fractions of half-length / half-width, team 0 orientation, kick-off shape):
 * 5 = keeper + 2-1-1, 7 = keeper + 2-3-1; others: a placeholder spread. Shared by kick-offs and the AI's shape. */
void formation_frac(int n, int k, fx *fxl, fx *fyl) {
    static const fx form5[5][2] = {
        {FX(-0.92), FX(0.0)}, {FX(-0.55), FX(-0.45)}, {FX(-0.55), FX(0.45)}, {FX(-0.30), FX(0.0)}, {FX(-0.06), FX(0.0)},
    };
    static const fx form7[7][2] = {
        {FX(-0.93), FX(0.0)}, {FX(-0.62), FX(-0.40)}, {FX(-0.62), FX(0.40)},
        {FX(-0.34), FX(-0.62)}, {FX(-0.36), FX(0.0)}, {FX(-0.34), FX(0.62)}, {FX(-0.06), FX(0.0)},
    };
    if (n == 7 && k < 7) { *fxl = form7[k][0]; *fyl = form7[k][1]; return; }
    if (k < 5) { *fxl = form5[k][0]; *fyl = form5[k][1]; return; }
    *fxl = FX(-0.5) + (fx)(k - 5) * FX(0.08); *fyl = (fx)((k % 3) - 1) * FX(0.5);
}

WpMatchConfig sim_default_config(void) {
    WpMatchConfig c;
    memset(&c, 0, sizeof c);
    c.seed = 1;
    c.ai_level[0] = c.ai_level[1] = WP_AI_NORMAL;
    c.n_per_side = 5;
    c.pitch.length = FX(40.0);
    c.pitch.width = FX(25.0);
    c.pitch.goal_width = FX(4.0);
    c.pitch.goal_height = FX(2.0);
    c.pitch.goal_depth = FX(1.2);
    return c;
}

/* Kickoff formation for 5-a-side (keeper + 2-1-1), as fractions of half-length/width. Team 0 defends -x.
 * Larger sides fall back to a placeholder spread until per-format formations exist (ROADMAP "Format support").
 * Resets ALL transient player state so nothing (a jump, a kick) survives a reset. */
void match_place_kickoff(SimState *s) {
    fx hl = s->pitch.length / 2, hw = s->pitch.width / 2;
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        WpPlayer *p = &s->players[i];
        int team = i / s->n_per_side, k = i % s->n_per_side;
        fx fxl, fyl;
        formation_frac(s->n_per_side, k, &fxl, &fyl);
        fx px = fx_mul(fxl, hl), py = fx_mul(fyl, hw);
        if (team_dir(s, team) < 0) { px = -px; py = -py; }
        if (k == s->n_per_side - 1 && s->n_per_side >= 5 && team != s->kickoff_team)   /* striker: outside the circle */
            px = team_dir(s, team) > 0 ? -(kickoff_r(s) + FX(0.5)) : kickoff_r(s) + FX(0.5);
        fx stamina = p->stamina;
        memset(p, 0, sizeof *p);
        p->stamina = stamina;
        p->pos = fv2_make(px, py);
        p->facing = team_dir(s, team) > 0 ? 0 : 32768;
        p->team = (uint8_t)team;
        p->role = k == 0 ? WP_ROLE_KEEPER : WP_ROLE_OUTFIELD;
        p->charge_kind = 0xFF;
    }
    s->pass_target = -1;
    s->pass_from = -1;
    s->restart_kind = 0;
    s->restart_taker = -1;
    team_plan_reset(s);
    memset(&s->ball, 0, sizeof s->ball);
    s->ball.owner = -1;
    s->ball.last_touch = -1;
    s->events |= WP_EV_RESET;
}

/* the kick-off as a set piece (D-032): the kicking team's striker stands on the ball and must play it (no dribbling
 * off); everyone stays in his own half and the other team outside the centre circle until then. Applied after every
 * goal; the game also calls it for the opening kick-off (sim_init leaves a free ball for tests and drills). */
void sim_kickoff_setpiece(SimState *s) {
    int taker = s->kickoff_team * s->n_per_side + s->n_per_side - 1;
    WpPlayer *tp = &s->players[taker];
    tp->pos = fv2_make(team_dir(s, s->kickoff_team) > 0 ? -FX(0.45) : FX(0.45), 0);
    s->ball.owner = (int8_t)taker;
    s->ball.last_touch = (int8_t)taker;
    s->restart_kind = WP_RS_KICKOFF;
    s->restart_team = s->kickoff_team;
    s->restart_taker = (int8_t)taker;
    s->restart_ticks = 0;
    for (int k = 0; k < WP_MAX_SEATS; k++)                /* a seat on the kicking team takes it */
        if (s->seat_player[k] >= 0 && s->players[s->seat_player[k]].team == s->kickoff_team) s->seat_player[k] = (int8_t)taker;
}

void sim_init(SimState *s, const WpMatchConfig *cfg) {
    memset(s, 0, sizeof *s);
    wp_rng_seed(&s->rng, cfg->seed, 0x5157u);
    s->pitch = cfg->pitch;
    for (int t = 0; t < 2; t++) s->ai_level[t] = cfg->ai_level[t] < WP_AI_LEVELS ? cfg->ai_level[t] : WP_AI_NORMAL;
    s->n_per_side = cfg->n_per_side < 1 ? 1 : (cfg->n_per_side > WP_MAX_PER_SIDE ? WP_MAX_PER_SIDE : cfg->n_per_side);
    for (int i = 0; i < WP_MAX_PLAYERS; i++) s->players[i].stamina = FX_ONE;
    for (int i = 0; i < WP_MAX_SEATS; i++) {
        s->seat_player[i] = -1;
        s->pick_recv[i] = -1;
        s->pick_stick_age[i] = s->pick_diag_age[i] = 255;
    }
    s->seat_player[0] = (int8_t)(s->n_per_side - 1); /* seat 0 = team 0 striker */
    s->half_ticks = cfg->half_ticks;                 /* 0 = endless (D-036) */
    s->first_kickoff = 0;
    s->rules = cfg->rules;
    for (int k = 0; k < WP_MAX_SEATS; k++) s->assist_level[k] = cfg->assist_level < WP_ASSIST_LEVELS ? cfg->assist_level : WP_ASSIST_NORMAL;
    stats_init(s, cfg->stats_mode);                  /* draws from s->rng only when stats are on (D-034) */
    s->kickoff_team = 0;
    match_place_kickoff(s);
    s->phase = WP_PHASE_PLAY;
    s->events = 0;
}

/* Goal celebration -> kickoff reset; temporary out-of-play rule: ball placed just inside the line. */
static void match_flow(SimState *s, const WpInput *in) {
    if (s->phase == WP_PHASE_SETUP) { restart_setup_step(s, in); return; }   /* D-044 */
    if (s->phase == WP_PHASE_PLAY || s->phase == WP_PHASE_HALFTIME || s->phase == WP_PHASE_FULLTIME || s->phase_timer == 0)
        return;                                           /* clock.c owns HT/FT */
    if (--s->phase_timer) return;
    if (s->phase == WP_PHASE_GOAL) {
        match_place_kickoff(s);
        sim_kickoff_setpiece(s);
    } else if (s->phase == WP_PHASE_OUT) {
        restart_setup(s, in);                             /* walk into place, then the restart (D-029/D-044) */
        return;
    }
    s->phase = WP_PHASE_PLAY;
}

void sim_step(SimState *s, const WpInput *in) {
    s->events = 0;
    players_step(s, in);
    ball_step(s);
    offside_step(s);                                  /* a flagged man played it: offside (D-041) */
    if (s->adv_ticks) {                               /* advantage (D-040): they lose it -> back for the free kick */
        int o = s->ball.owner;
        if (s->phase != WP_PHASE_PLAY) s->adv_ticks = 0;
        else if (o >= 0 && s->players[o].team != s->adv_team) { s->adv_ticks = 0; restart_foul(s, s->adv_team, s->adv_spot, 0); }
        else s->adv_ticks--;
    }
    match_flow(s, in);
    clock_step(s);
    if (s->indirect_live && (s->phase != WP_PHASE_PLAY ||          /* someone else touched it: an ordinary ball again */
                             (s->ball.last_touch >= 0 && s->ball.last_touch != s->indirect_live - 1))) s->indirect_live = 0;
    for (int k = 0; k < WP_MAX_SEATS; k++) s->prev_buttons[k] = in->seat[k].buttons;
    s->tick++;
}

/* ---- hashing: explicit fields so struct padding never leaks in ----
 * If this assert fires you changed SimState/WpPlayer/WpBall: hash the new fields in sim_hash(), then update the size. */
_Static_assert(sizeof(SimState) == WP_SIMSTATE_SIZE_EXPECTED, "SimState changed: update sim_hash() and the size");
typedef struct { uint64_t h; } Fnv;
static void fnv_u64(Fnv *f, uint64_t v) {
    for (int i = 0; i < 8; i++) { f->h ^= (v >> (i * 8)) & 0xFF; f->h *= 1099511628211ULL; }
}
static void fnv_i(Fnv *f, int64_t v) { fnv_u64(f, (uint64_t)v); }

uint64_t sim_hash(const SimState *s) {
    Fnv f = {1469598103934665603ULL};
    fnv_i(&f, s->tick); fnv_u64(&f, s->rng.state); fnv_u64(&f, s->rng.inc);
    fnv_i(&f, s->pitch.length); fnv_i(&f, s->pitch.width);
    fnv_i(&f, s->pitch.goal_width); fnv_i(&f, s->pitch.goal_height); fnv_i(&f, s->pitch.goal_depth);
    const WpBall *b = &s->ball;
    fnv_i(&f, b->pos.x); fnv_i(&f, b->pos.y); fnv_i(&f, b->z);
    fnv_i(&f, b->vel.x); fnv_i(&f, b->vel.y); fnv_i(&f, b->vz); fnv_i(&f, b->spin);
    fnv_i(&f, b->owner); fnv_i(&f, b->last_touch); fnv_i(&f, b->held); fnv_i(&f, b->held_ticks); fnv_i(&f, b->no_hands);
    fnv_i(&f, s->n_per_side); fnv_i(&f, s->phase); fnv_i(&f, s->phase_timer);
    fnv_i(&f, s->score[0]); fnv_i(&f, s->score[1]); fnv_i(&f, s->kickoff_team); fnv_i(&f, s->flags); fnv_i(&f, s->ai_level[0]); fnv_i(&f, s->ai_level[1]);
    fnv_i(&f, s->restart_kind); fnv_i(&f, s->restart_team); fnv_i(&f, s->restart_taker); fnv_i(&f, s->restart_ticks);
    fnv_i(&f, s->restart_spot.x); fnv_i(&f, s->restart_spot.y);
    fnv_i(&f, s->restart_indirect); fnv_i(&f, s->indirect_live); fnv_i(&f, s->offside_mask); fnv_i(&f, s->offside_team); if (s->rules) fnv_i(&f, s->rules);
    fnv_i(&f, s->offside_from); fnv_i(&f, s->offside_x); fnv_i(&f, s->adv_ticks); fnv_i(&f, s->adv_team); fnv_i(&f, s->adv_spot.x); fnv_i(&f, s->adv_spot.y);
    for (int t = 0; t < 2; t++) {
        const WpTeamPlan *pl = &s->plan[t];
        fnv_i(&f, pl->focus.x); fnv_i(&f, pl->focus.y); fnv_i(&f, pl->poss_fade); fnv_i(&f, pl->trans_ticks);
        fnv_i(&f, pl->last_poss); fnv_i(&f, pl->winner); fnv_i(&f, pl->cover); fnv_i(&f, pl->near_sup);
        fnv_i(&f, pl->runner); fnv_i(&f, pl->sup_timer);
        for (int k = 0; k < WP_MAX_PER_SIDE; k++) { fnv_i(&f, pl->role[k]); fnv_i(&f, pl->mark[k]); }
    } fnv_i(&f, s->events);
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        fnv_i(&f, s->seat_player[k]); fnv_i(&f, s->prev_buttons[k]); fnv_i(&f, s->switch_cool[k]);
    }
    fnv_i(&f, s->pass_target); fnv_i(&f, s->pass_from); fnv_i(&f, s->pass_ticks);
    fnv_i(&f, s->pass_stick[0]); fnv_i(&f, s->pass_stick[1]);
    fnv_i(&f, s->pass_meet.x); fnv_i(&f, s->pass_meet.y); fnv_i(&f, s->pass_meet_tick); fnv_i(&f, s->pass_plan);
    fnv_i(&f, s->recv_away); fnv_i(&f, s->recv_gone);
    fnv_i(&f, s->recv_goal.x); fnv_i(&f, s->recv_goal.y); fnv_i(&f, s->recv_speed);
    for (int k = 0; k < WP_MAX_SEATS; k++) {
        fnv_i(&f, s->latch_stick[k][0]); fnv_i(&f, s->latch_stick[k][1]); fnv_i(&f, s->latch_ticks[k]);
        fnv_i(&f, s->pick_recv[k]); fnv_i(&f, s->pick_danger[k]); fnv_i(&f, s->pick_dir[k]);
        fnv_i(&f, s->pick_stick[k][0]); fnv_i(&f, s->pick_stick[k][1]); fnv_i(&f, s->pick_stick_age[k]);
        fnv_i(&f, s->pick_diag[k][0]); fnv_i(&f, s->pick_diag[k][1]); fnv_i(&f, s->pick_diag_age[k]);
    }
    {   /* assist levels: all OFF carries nothing (the pre-D-037 hash) */
        int any = 0;
        for (int k = 0; k < WP_MAX_SEATS; k++) any |= s->assist_level[k];
        if (any) for (int k = 0; k < WP_MAX_SEATS; k++) fnv_i(&f, s->assist_level[k]);
    }
    if (s->half_ticks) {                                  /* endless matches carry no clock: unhashed (D-036) */
        fnv_i(&f, s->half_ticks); fnv_i(&f, s->clock); fnv_i(&f, s->stoppage); fnv_i(&f, s->stop_acc);
        fnv_i(&f, s->half); fnv_i(&f, s->first_kickoff);
    }
    if (s->stats_mode) {                                  /* neutral stats carry no information: unhashed (D-034) */
        fnv_i(&f, s->stats_mode);
        for (int i = 0; i < 2 * s->n_per_side; i++) for (int k = 0; k < WP_STAT_N; k++) fnv_i(&f, s->stats[i][k]);
    }
    for (int i = 0; i < 2 * s->n_per_side; i++) {
        const WpPlayer *p = &s->players[i];
        fnv_i(&f, p->pos.x); fnv_i(&f, p->pos.y); fnv_i(&f, p->vel.x); fnv_i(&f, p->vel.y);
        fnv_i(&f, p->z); fnv_i(&f, p->vz); fnv_i(&f, p->action); fnv_i(&f, p->action_timer);
        fnv_i(&f, p->facing); fnv_i(&f, p->team); fnv_i(&f, p->role);
        fnv_i(&f, p->kick_charge); fnv_i(&f, p->charge_kind); fnv_i(&f, p->immune);
        fnv_i(&f, p->touch_cooldown); fnv_i(&f, p->stamina);
        fnv_i(&f, p->ai_act); fnv_i(&f, p->ai_mate); fnv_i(&f, p->ai_wait); fnv_i(&f, p->ai_hold);
        fnv_i(&f, p->ai_goal.x); fnv_i(&f, p->ai_goal.y);
        const WpKick *k = &p->kick;
        fnv_i(&f, k->power); fnv_i(&f, k->vz); fnv_i(&f, k->spin); fnv_i(&f, k->dir);
        fnv_i(&f, k->foot); fnv_i(&f, k->kind); fnv_i(&f, k->windup); fnv_i(&f, k->done); fnv_i(&f, k->to);
    }
    return f.h;
}
