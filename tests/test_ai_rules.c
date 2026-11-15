/* CP0.3 possession rules: tackles, closest player wins a loose ball, control follows the ball to our team-mate,
 * the ball never passes through anybody. */
#include <stdio.h>
#include "test_util.h"
#include "sim_test_helpers.h"
#include "sim/sim_internal.h"

static SimState s;
static uint16_t step1(WpSeatInput a) { WpInput in; memset(&in, 0, sizeof in); in.seat[0] = a; sim_step(&s, &in); return s.events; }
static uint16_t step2(WpSeatInput a, WpSeatInput b) { WpInput in; memset(&in, 0, sizeof in); in.seat[0] = a; in.seat[1] = b; sim_step(&s, &in); return s.events; }

int main(void) {
    WpMatchConfig cfg = sim_default_config();
    /* 1. tackle: an AI opponent dribbles at us; J when he's close and in front knocks it loose; J from too far misses */
    for (int far = 0; far <= 1; far++) {
        sim_init(&s, &cfg);
        s.seat_player[0] = 2;                                   /* our defender */
        for (int j = 0; j < 10; j++) if (j != 2 && j != 9) s.players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
        s.players[2].pos = fv2_make(-FX(8.0), 0);
        s.players[9].pos = fv2_make(FX(0.0), 0);               /* their striker on the ball, carrying it at us (-x) */
        s.ball.pos = fv2_make(-FX(0.5), 0); s.ball.owner = 9;
        s.seat_player[1] = 9;                                   /* a second human carries it straight at us */
        int tackled = 0, started = 0;
        for (int t = 0; t < 120 && !tackled; t++) {
            for (int j = 0; j < 10; j++) if (j != 2 && j != 9) s.players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
            fx d = fv2_len(fv2_sub(s.ball.pos, s.players[2].pos));
            int press = !started && d < (far ? FX(1.6) : FX(1.0));
            if (press) started = 1;
            uint16_t ev = step2((WpSeatInput){0, 0, (uint16_t)(press ? WP_BTN_PASS : 0)}, (WpSeatInput){-127, 0, 0});
            if (ev & WP_EV_TACKLE) tackled = 1;
        }
        printf("tackle pressed at %s: %s\n", far ? "1.6 m (early)" : "1.0 m", tackled ? "won the ball" : "missed");
        if (!far) CHECKF(tackled, "a well-timed tackle didn't win the ball");
    }
    /* 2. a loose ball rolling between two players: the closer one gets it, whatever his team / index */
    for (int side = 0; side <= 1; side++) {
        sim_init(&s, &cfg);
        s.seat_player[0] = -1;
        for (int j = 0; j < 10; j++) s.players[j].pos = fv2_make(FX_INT(j * 3), FX(40.0));
        s.players[3].pos = fv2_make(0, side ? FX(0.9) : FX(0.6));     /* ours */
        s.players[8].pos = fv2_make(0, side ? -FX(0.6) : -FX(0.9));   /* theirs */
        s.ball.pos = fv2_make(-FX(0.2), 0); s.ball.vel = fv2_make(FX(4.0), 0); s.ball.owner = -1;
        for (int t = 0; t < 60 && s.ball.owner < 0; t++) {
            s.players[3].vel = s.players[8].vel = fv2_make(0, 0);
            step1(NOIN);
        }
        int want = side ? 8 : 3;
        printf("contest (%s nearer): won by %d\n", side ? "theirs" : "ours", s.ball.owner);
        CHECKF(s.ball.owner == want, "contest side %d: won by %d, want %d", side, s.ball.owner, want);
    }
    /* 3. our AI team-mate wins a loose ball: control jumps to him, and the held stick doesn't carry him off */
    {
        sim_init(&s, &cfg);
        int me = s.seat_player[0];
        s.players[3].pos = fv2_make(-FX(5.0), FX(5.0));
        s.ball.pos = fv2_make(-FX(5.0), FX(3.0)); s.ball.vel = fv2_make(0, FX(3.0)); s.ball.owner = -1;
        for (int t = 0; t < 40 && s.ball.owner != 3; t++) step1((WpSeatInput){127, 0, 0});
        fv2 at = s.players[3].pos;
        for (int t = 0; t < 10; t++) step1((WpSeatInput){127, 0, 0});
        fx moved = fv2_len(fv2_sub(s.players[3].pos, at));
        printf("team-mate won it: seat now on %d (was %d), moved %.2f m in the next 1/6 s with the old stick held\n",
               s.seat_player[0], me, tod(moved));
        CHECK(s.ball.owner == 3 && s.seat_player[0] == 3);
        CHECKF(moved < FX(0.6), "inherited stick carried him %.2f m", tod(moved));
    }
    /* 4. live play (no statues): the ball never passes through a non-owner's body; players never overlap */
    {
        sim_init(&s, &cfg);
        fx min_ball = FX(9.0), min_gap = FX(9.0);
        for (int t = 0; t < 60 * 90; t++) {
            int me = s.seat_player[0];
            fv2 d = fv2_sub(s.ball.pos, s.players[me].pos);
            WpSeatInput in = {(int8_t)(d.x > FX(0.3) ? 127 : d.x < -FX(0.3) ? -127 : 0),
                              (int8_t)(d.y > FX(0.3) ? 127 : d.y < -FX(0.3) ? -127 : 0),
                              (uint16_t)((t % 50) < 2 ? WP_BTN_PASS : 0)};
            step1(in);
            if (s.phase != WP_PHASE_PLAY) continue;
            for (int i = 0; i < 10; i++) {
                /* skip: the owner, the kicker's own foot just after a kick (immune), and a player about to take a
                 * loose ball he may control (the body steps aside for control - he takes it next tick) */
                int taking = s.ball.owner < 0 && s.players[i].touch_cooldown == 0;
                if (i != s.ball.owner && s.ball.z < FX(1.5) && s.players[i].action == WP_ACT_NONE && !s.players[i].immune && !taking) {
                    fx db = fv2_len(fv2_sub(s.ball.pos, s.players[i].pos));
                    if (db < min_ball) min_ball = db;
                }
                for (int j = i + 1; j < 10; j++) {
                    fx g = fv2_len(fv2_sub(s.players[i].pos, s.players[j].pos));
                    if (g < min_gap) min_gap = g;
                }
            }
        }
        printf("live 90 s: ball closest to a non-owner's centre %.2f m, closest two players %.2f m\n", tod(min_ball), tod(min_gap));
        /* > 0.15 (not 0.41): two players shoulder to shoulder (0.6 m) can squeeze a ball between them for a tick or two
         * - geometrically it can't clear both bodies; it still never passes THROUGH one */
        CHECKF(min_ball > FX(0.15), "ball %.2f m from a body centre", tod(min_ball));
        CHECKF(min_gap > FX(0.5), "players overlapped (%.2f m)", tod(min_gap));
    }
    /* 5. keeper dives (7v7 goal): a medium shot 1.6 m to his side from 11 m -> he dives and keeps it out; a hard shot
     *    into the corner beats him; a soft one at him is caught */
    {
        static const struct { fx speed, aim_y; int want_goal; const char *name; } cases[3] = {
            {FX(18.0), FX(1.6), 0, "medium, 1.6 m to his side"}, {FX(30.0), FX(2.1), 1, "hard, into the corner"},
            {FX(12.0), FX(0.3), 0, "soft, at him"}};
        for (int c = 0; c < 3; c++) {
            WpMatchConfig c7 = sim_config_7v7();
            sim_init(&s, &c7);
            for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
            int kp = s.n_per_side;                                       /* their keeper */
            for (int j = 0; j < 14; j++) if (j != kp) s.players[j].pos = fv2_make(FX_INT(j * 2 - 14), FX(40.0));
            for (int t = 0; t < 30; t++) {                               /* let him settle on his line */
                for (int j = 0; j < 14; j++) if (j != kp) s.players[j].pos = fv2_make(FX_INT(j * 2 - 14), FX(40.0));
                s.ball.pos = fv2_make(FX(16.5), 0); s.ball.vel = fv2_make(0, 0); s.ball.owner = -1;
                step1(NOIN);
            }
            fv2 from = fv2_make(FX(16.5), 0), to = fv2_make(FX(27.6), cases[c].aim_y);
            s.ball.pos = from; s.ball.z = FX(0.2); s.ball.vz = FX(1.0); s.ball.owner = -1; s.ball.last_touch = 3;
            s.ball.vel = fv2_scale(fv2_norm(fv2_sub(to, from)), cases[c].speed);
            int dived = 0; uint16_t ev = 0;
            for (int t = 0; t < 90; t++) {
                for (int j = 0; j < 14; j++) if (j != kp) s.players[j].pos = fv2_make(FX_INT(j * 2 - 14), FX(40.0));
                ev |= step1(NOIN);
                dived |= s.players[kp].action == WP_ACT_KEEPER_DIVE;
                if (ev & WP_EV_GOAL) break;
            }
            printf("shot %s: %s%s\n", cases[c].name, (ev & WP_EV_GOAL) ? "GOAL" : "kept out", dived ? " (dived)" : "");
            CHECKF(((ev & WP_EV_GOAL) != 0) == cases[c].want_goal, "keeper case %d", c);
            if (c == 0) CHECK(dived);
        }
    }
    return TEST_DONE();
}
