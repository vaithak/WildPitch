/* Laws of the game in the sim (D-040 fouls / free kicks / penalties / advantage, D-041 offside). 7v7 config, rules on.
 * Scenes are built by hand: everyone parked in their own half unless placed. */
#include <stdio.h>
#include <stdlib.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

static SimState s;
static WpInput NO;

static void scene(int half) {
    WpMatchConfig c = sim_config_7v7();
    c.half_ticks = 0;                                     /* endless: no whistle in the middle of a test */
    sim_init(&s, &c);
    s.half = (uint8_t)half;
    for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
    s.flags |= WP_SIMF_STATIC_OTHERS;
    int n = s.n_per_side;
    for (int j = 0; j < 2 * n; j++) {                     /* own half, spread down the middle */
        int t = j / n, k = j % n;
        s.players[j].pos = fv2_make(FX_INT(-team_dir(&s, t) * (8 + 4 * k)), FX_INT(k * 5 - 15));
        s.players[j].vel = fv2_make(0, 0);
    }
    s.ball.owner = -1; s.ball.pos = fv2_make(0, 0); s.ball.vel = fv2_make(0, 0);
    memset(&NO, 0, sizeof NO);
}
static void steps(int n) { for (int t = 0; t < n; t++) sim_step(&s, &NO); }
static void wait_restart(void) { for (int t = 0; t < 200 && !(s.phase == WP_PHASE_PLAY && s.restart_taker >= 0); t++) steps(1); }

/* tackler i lunges now at foot position toward `to` (the tackle's foot arrives next tick) */
static void lunge(int i, fv2 to) {
    WpPlayer *p = &s.players[i];
    fv2 d = fv2_norm(fv2_sub(to, p->pos));
    p->action = WP_ACT_TACKLE; p->action_timer = T_TACKLE_WINDUP - 1; p->kick.done = 0;
    p->facing = fx_atan2(d.y, d.x);
}

int main(void) {
    int n = 7, red_st = 6, blue_cb = n + 1, blue_st = 2 * n - 1;
    /* 1. a tackle that misses the ball but takes the man (ball loose): direct free kick to them */
    scene(0);
    s.players[blue_st].pos = fv2_make(-FX(5.0), 0);
    s.players[1].pos = fv2_make(-FX(5.0), FX(0.8));
    s.ball.pos = fv2_make(-FX(5.0), FX(6.0));             /* nowhere near his foot */
    lunge(1, s.players[blue_st].pos);
    steps(1);
    printf("miss + man: events %x kind %d team %d\n", s.events, s.restart_kind, s.restart_team);
    CHECK((s.events & WP_EV_FOUL) && s.restart_kind == WP_RS_FREEKICK && s.restart_team == 1 && !s.restart_indirect);
    wait_restart();
    fx near = FX(99.0);
    steps(5);
    for (int j = 0; j < n; j++) { fx d = fv2_len(fv2_sub(s.players[j].pos, s.ball.pos)); if (d < near) near = d; }
    printf("  free kick: taker %d (team %d), nearest red %.1f m\n", s.restart_taker, s.players[s.restart_taker].team, tod(near));
    CHECK(s.players[s.restart_taker].team == 1);
    CHECK(near >= fx_mul(s.pitch.length, T_CORNER_DIST_FRAC) - FX(0.1));
    /* 2. advantage: they still have it after the foul -> play on; lose it within the window -> back for the free kick */
    for (int lose = 0; lose < 2; lose++) {
        scene(0);
        s.players[blue_st].pos = fv2_make(-FX(5.0), 0);
        s.players[blue_cb].pos = fv2_make(-FX(2.0), FX(4.0));
        s.players[1].pos = fv2_make(-FX(5.0), FX(0.8));
        s.ball.owner = (int8_t)blue_cb; s.ball.pos = fv2_add(s.players[blue_cb].pos, fv2_make(-FX(0.4), 0));
        lunge(1, s.players[blue_st].pos);
        steps(1);
        CHECK(s.adv_ticks > 0 && s.restart_kind == 0);
        if (lose) { s.ball.owner = 3; s.players[3].pos = s.ball.pos; }   /* red takes it */
        steps(T_ADVANTAGE_TICKS + 5);
        printf("advantage (%s): restart kind %d team %d\n", lose ? "then lost it" : "kept it", s.restart_kind, s.restart_team);
        if (lose) CHECK(s.restart_kind == WP_RS_FREEKICK && s.restart_team == 1);
        else CHECK(s.restart_kind == 0 && s.adv_ticks == 0);
    }
    /* 3. won the ball, but through a running man from behind: foul */
    scene(0);
    WpPlayer *v = &s.players[blue_st];
    v->pos = fv2_make(-FX(5.0), 0); v->vel = fv2_make(-FX(5.0), 0);   /* blue attacks -x: running at our goal */
    s.ball.owner = (int8_t)blue_st; s.ball.pos = fv2_make(-FX(5.5), 0);
    s.players[1].pos = fv2_make(-FX(4.3), 0);             /* right behind him */
    lunge(1, s.ball.pos);
    steps(1);
    printf("from behind: events %x kind %d\n", s.events, s.restart_kind);
    CHECK((s.events & WP_EV_FOUL) && s.restart_kind == WP_RS_FREEKICK);
    /* 4. a foul in the box: penalty - from the mark, everyone else out of the box and the D, keeper on his line */
    for (int half = 0; half < 2; half++) {
        scene(half);
        int sg = team_dir(&s, 0);                         /* red attacks +sg */
        fx hl = s.pitch.length / 2;
        s.players[red_st].pos = fv2_make(fx_mul(FX_INT(sg), hl - FX(8.0)), FX(1.0));
        s.ball.owner = (int8_t)red_st; s.ball.pos = fv2_add(s.players[red_st].pos, fv2_make(0, FX(0.4)));
        s.players[blue_cb].pos = fv2_make(fx_mul(FX_INT(sg), hl - FX(8.0)), FX(1.8));
        s.ball.pos = fv2_make(fx_mul(FX_INT(sg), hl - FX(14.0)), -FX(6.0)); s.ball.owner = -1;   /* missed */
        lunge(blue_cb, s.players[red_st].pos);
        steps(1);
        CHECK(s.restart_kind == WP_RS_PENALTY && s.restart_team == 0);
        s.flags = 0;                                      /* let them walk to the line-up */
        wait_restart();
        steps(10);
        fx mark = fx_mul(FX_INT(sg), hl - fx_mul(s.pitch.length, T_PENALTY_FRAC));
        int inside = 0;
        for (int j = 0; j < 2 * n; j++) {
            if (j == s.restart_taker || j == n) continue;
            if (in_own_box(&s, 1, s.players[j].pos)) inside++;
        }
        fx kx = fx_abs(s.players[n].pos.x);
        printf("penalty (half %d): ball x %.1f (mark %.1f), taker %d shooting %d, others in the box %d, keeper x %.1f\n", half + 1,
               tod(s.ball.pos.x), tod(mark), s.restart_taker, s.stats[s.restart_taker][WP_STAT_SHOOTING], inside, tod(s.players[n].pos.x));
        CHECK(fx_abs(s.ball.pos.x - mark) < FX(0.4) && fx_abs(s.ball.pos.y) < FX(0.4));
        CHECK(inside == 0);
        CHECK(kx > hl - FX(0.5));
    }
    /* 5. an indirect free kick straight into the goal is no goal (goal kick) */
    scene(0);
    restart_foul(&s, 0, fv2_make(s.pitch.length / 2 - FX(12.0), 0), 1);
    wait_restart();
    int tk = s.restart_taker;
    WpKick k = {0};
    k.kind = WP_KICK_SHOT; k.power = FX(22.0); k.dir = 0; k.windup = 6; k.foot = 1;
    for (int j = n; j < 2 * n; j++) s.players[j].pos = fv2_make(-FX(30.0), FX_INT(j - n));   /* nobody in the way */
    sim_player_kick(&s, tk, &k);
    uint16_t ev = 0;
    for (int t = 0; t < 120; t++) { steps(1); ev |= s.events; }
    printf("indirect straight in: goal %d out %d score %d-%d restart %d\n", (ev & WP_EV_GOAL) != 0, (ev & WP_EV_OUT) != 0, s.score[0], s.score[1], s.restart_kind);
    CHECK(!(ev & WP_EV_GOAL) && s.score[0] == 0 && s.restart_kind == WP_RS_GOALKICK);
    /* 6. offside, both halves */
    for (int half = 0; half < 2; half++) {
        int sg;
        /* a) beyond the second-last defender and the ball, he receives: offside -> indirect free kick to them */
        scene(half); sg = team_dir(&s, 0);
        s.players[3].pos = fv2_make(FX_INT(sg * 5), 0);      /* passer */
        s.ball.owner = 3; s.ball.pos = fv2_add(s.players[3].pos, fv2_make(FX_INT(sg) * 4 / 10, 0));
        s.players[n + 1].pos = fv2_make(FX_INT(sg * 15), FX(5.0));   /* their two deepest outfield men at 15 / 16 m */
        s.players[n + 2].pos = fv2_make(FX_INT(sg * 16), -FX(5.0));
        for (int j = n + 3; j < 2 * n; j++) s.players[j].pos = fv2_make(-FX_INT(sg * 10), FX_INT(j * 3 - 30));   /* out of the way */
        s.players[n].pos = fv2_make(FX_INT(sg * 38), 0);     /* keeper */
        s.players[red_st].pos = fv2_make(FX_INT(sg * 22), FX(2.0));   /* beyond them */
        s.seat_player[0] = 3; s.seat_player[1] = (int8_t)red_st;   /* statues can't play the ball: seats */
        fv2 d = fv2_sub(s.players[red_st].pos, s.ball.pos);
        WpKick pk = {0};
        pk.kind = WP_KICK_PASS; pk.dir = fx_atan2(d.y, d.x); pk.to = (uint8_t)(red_st + 1); pk.windup = T_KICK_WINDUP_PASS; pk.foot = 1;
        sim_player_kick(&s, 3, &pk);
        ev = 0;
        for (int t = 0; t < 200 && !(ev & WP_EV_OFFSIDE); t++) { steps(1); ev |= s.events;
            if (getenv("DBG") && (t < 12 || t % 20 == 0)) printf("    t%d ev %x owner %d last %d mask %x line %.1f st x %.1f\n", t, s.events, s.ball.owner, s.ball.last_touch, s.offside_mask, tod(offside_line(&s, 0)), tod(s.players[red_st].pos.x)); }
        printf("offside (half %d): called %d, restart %d team %d indirect %d\n", half + 1, (ev & WP_EV_OFFSIDE) != 0, s.restart_kind,
               s.restart_team, s.restart_indirect);
        CHECK((ev & WP_EV_OFFSIDE) && s.restart_kind == WP_RS_FREEKICK && s.restart_team == 1 && s.restart_indirect);
        /* b) level with the second-last defender is onside; c) in his own half is onside */
        for (int c = 0; c < 2; c++) {
            scene(half); sg = team_dir(&s, 0);
            s.players[3].pos = fv2_make(c ? -FX_INT(sg * 12) : FX_INT(sg * 5), 0);
            s.ball.owner = 3; s.ball.pos = fv2_add(s.players[3].pos, fv2_make(FX_INT(sg) * 4 / 10, 0));
            s.players[n + 1].pos = fv2_make(c ? -FX_INT(sg * 2) : FX_INT(sg * 15), FX(5.0));
            s.players[n + 2].pos = fv2_make(c ? -FX_INT(sg * 1) : FX_INT(sg * 16), -FX(5.0));
            for (int j = n + 3; j < 2 * n; j++) s.players[j].pos = fv2_make(FX_INT(sg * 30), FX_INT(j * 3 - 30));
            s.players[n].pos = fv2_make(FX_INT(sg * 38), 0);
            s.players[red_st].pos = fv2_make(c ? -FX_INT(sg * 4) : FX_INT(sg * 15), FX(2.0));   /* own half / level */
            s.seat_player[0] = 3; s.seat_player[1] = (int8_t)red_st;
            d = fv2_sub(s.players[red_st].pos, s.ball.pos);
            pk.dir = fx_atan2(d.y, d.x);
            sim_player_kick(&s, 3, &pk);
            ev = 0;
            int got = 0;
            for (int t = 0; t < 200 && !got; t++) { steps(1); ev |= s.events; got = s.ball.owner == red_st; }
            printf("  %s: received %d, offside %d\n", c ? "own half" : "level", got, (ev & WP_EV_OFFSIDE) != 0);
            CHECK(got && !(ev & WP_EV_OFFSIDE));
        }
    }
    /* d) no offside from a throw-in */
    scene(0);
    s.restart_kind = WP_RS_THROW; s.restart_team = 0; s.restart_taker = 3;
    s.players[red_st].pos = fv2_make(FX(25.0), 0);
    s.ball.owner = 3;
    offside_on_kick(&s, 3);
    CHECK(s.offside_mask == 0);
    /* 7. sliding tackle (D-043): the lob key with their carrier close by */
    for (int c = 0; c < 3; c++) {
        scene(0);
        s.flags = 0;
        for (int j = 0; j < 2 * n; j++) if (j != 1 && j != blue_st) s.players[j].pos = fv2_make(FX_INT(j * 4 - 28), FX(18.0));
        s.seat_player[0] = 1;
        WpPlayer *q = &s.players[blue_st];
        q->pos = fv2_make(-FX(5.0), 0); q->vel = fv2_make(-FX(5.0), 0);    /* running at our goal */
        s.ball.owner = (int8_t)blue_st; s.ball.pos = fv2_make(-FX(5.5), 0);
        if (c == 0) s.players[1].pos = fv2_make(-FX(6.2), -FX(2.0));      /* from the side, at the ball */
        if (c == 1) s.players[1].pos = fv2_make(-FX(3.4), 0);              /* from behind */
        if (c == 2) s.players[1].pos = fv2_make(-FX(5.0), -FX(6.0));       /* too far */
        WpInput in; memset(&in, 0, sizeof in);
        in.seat[0].buttons = WP_BTN_LOB;
        sim_step(&s, &in);
        int sliding = s.players[1].action == WP_ACT_SLIDE;
        memset(&in, 0, sizeof in);
        uint16_t e = 0;
        for (int t = 0; t < 40; t++) { sim_step(&s, &in); e |= s.events; }
        printf("slide %s: sliding %d, won %d, foul %d (restart %d)\n", c == 0 ? "from the side" : c == 1 ? "from behind" : "out of range",
               sliding, (e & WP_EV_TACKLE) != 0, (e & WP_EV_FOUL) != 0, s.restart_kind);
        if (c == 0) CHECK(sliding && (e & WP_EV_TACKLE) && !(e & WP_EV_FOUL));
        if (c == 1) CHECK(sliding && (e & WP_EV_FOUL));
        if (c == 2) CHECK(!sliding);
    }
    return TEST_DONE();
}
