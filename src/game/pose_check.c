/* pose_check.c - part of --check-kick-pose (D-052): for every aerial contact stance, the drawn limb's contact centre
 * (chest / thigh top / forehead + a ball radius, or the instep) is within 0.15 m of the ball's centre on the contact
 * tick. Runs the real sim (a ball flown at a standing seat player at the stance's height, three lateral offsets),
 * builds the pose exactly as the renderer does (stance_vis.c) for the frame that shows the touch, and measures. */
#include "game/pose_check.h"
#include "game/stance_vis.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ME 6                       /* 7v7: team 0 striker */

static float tof(fx v) { return (float)v / 65536.0f; }

typedef struct { const char *name; uint8_t kind; float z, v; uint16_t press; } Case;

/* the header pose's clock, as render.c's header_pose_t at alpha 0 */
static float header_t(const WpPlayer *p) {
    float t = (float)p->action_timer / (float)WP_TICK_HZ;
    if ((p->action != WP_ACT_JUMP_HEADER && p->action != WP_ACT_STAND_HEADER) || p->kick.kind != WP_KICK_HEADER) return t;
    if (!p->kick.windup) return 0.15f;
    float since = ((float)p->action_timer - (float)(p->kick.windup - 1)) / (float)WP_TICK_HZ;
    return p->action == WP_ACT_JUMP_HEADER ? since : 0.28f + since;
}

/* one run: returns the distance (m), or -1 if the touch never came */
static float run_case(const Case *k, float off, float *out_z) {
    static SimState s, prev;
    WpMatchConfig cfg = sim_config_7v7();
    cfg.rules = 0;
    cfg.stats_mode = WP_STATS_NEUTRAL;
    sim_init(&s, &cfg);
    s.flags |= WP_SIMF_STATIC_OTHERS;
    for (int i = 0; i < 2 * s.n_per_side; i++) if (i != ME) s.players[i].pos = fv2_make(FX(-15.0) + i * FX(2.0), FX(80.0));
    if (k->kind == WP_LT_LAYOFF) s.players[ME - 1].pos = fv2_make(FX(6.0), FX(1.0));   /* a mate 4 m in front */
    for (int j = 0; j < WP_MAX_SEATS; j++) s.seat_player[j] = -1;
    s.seat_player[0] = ME;
    s.phase = WP_PHASE_PLAY;
    s.restart_kind = 0;
    s.ball.owner = -1;
    s.players[ME].pos = fv2_make(FX(10.0), 0);
    s.players[ME].vel = fv2_make(0, 0);
    s.players[ME].facing = 32768;                         /* facing the ball, which comes from -x */
    s.ball.pos = fv2_make(FX(7.0), (fx)(off * 65536.0f));
    s.ball.z = (fx)(k->z * 65536.0f) + FX(0.05);
    s.ball.vel = fv2_make((fx)(k->v * 65536.0f), 0);
    s.ball.vz = FX(0.5);
    WpInput in;
    memset(&in, 0, sizeof in);
    for (int t = 0; t < 90; t++) {
        in.seat[0].buttons = t < 3 ? k->press : 0;        /* a press early, released: buffered (D-048) */
        prev = s;
        sim_step(&s, &in);
        const WpPlayer *p = &s.players[ME];
        if (p->last_touch_tick == (uint16_t)(s.tick - 1) && p->last_touch_kind) {
            if (p->last_touch_kind != k->kind) return -2;
            /* the frame whose `cur` holds the touch, at alpha 0: drawn where `prev` has him and the ball */
            WpAvatarLook look;
            wp_avatar_make_look(&look, 1, ME, 0, false);
            float px = tof(prev.players[ME].pos.x), py = tof(prev.players[ME].pos.y);
            WpAvatarPose pose = {.facing = (float)p->facing * (6.2831853f / 65536.0f), .action = p->action,
                                 .kick_foot = p->kick.foot, .kick_kind = p->kick.kind, .z = tof(prev.players[ME].z),
                                 .action_t = header_t(p)};
            WpStanceVis c = {0};
            wp_stance_fill(&prev, &s, ME, 0.0f, px, py, &c, &pose);
            if (!pose.stance) return -3;
            float at[3];
            wp_avatar_stance_point(&pose, &look, &wp_avatar_styles[0], px, py, at);
            if (getenv("WP_POSE_V")) printf("    act %d at %.3f z %.2f stance %d t %.3f tgt %.2f %.2f %.2f turn %.2f -> limb %.2f %.2f %.2f ball %.2f %.2f %.2f me %.2f %.2f\n", pose.action, (double)pose.action_t, (double)pose.z, pose.stance, (double)pose.stance_t, (double)pose.stance_tgt[0], (double)pose.stance_tgt[1], (double)pose.stance_tgt[2], (double)pose.head_turn, (double)at[0], (double)at[1], (double)at[2], (double)tof(prev.ball.pos.x), (double)tof(prev.ball.pos.y), (double)tof(prev.ball.z), (double)px, (double)py);
            float dx = at[0] - tof(prev.ball.pos.x), dy = at[1] - tof(prev.ball.pos.y), dz = at[2] - tof(prev.ball.z);
            *out_z = tof(prev.ball.z);
            return sqrtf(dx * dx + dy * dy + dz * dz);
        }
        if (s.ball.owner >= 0 && s.ball.owner != ME) return -1;
    }
    return -1;
}

int wp_check_stance_pose(void) {
    static const Case cases[] = {
        {"instep    ", WP_LT_INSTEP,     0.42f, 12.5f, 0},
        {"thigh     ", WP_LT_THIGH,      0.80f, 13.0f, 0},
        {"chest     ", WP_LT_CHEST,      1.25f, 15.0f, 0},
        {"head ctl  ", WP_LT_HEADCTL,    1.80f, 10.0f, 0},
        {"volley    ", WP_LT_VOLLEY,     0.80f, 13.0f, WP_BTN_SHOOT},
        {"half-volley", WP_LT_HALFVOLLEY, 0.42f, 12.0f, WP_BTN_PASS},
        {"lay-off   ", WP_LT_LAYOFF,     1.25f, 12.0f, WP_BTN_PASS},
        {"header pass", WP_LT_HEADER,    1.85f, 12.0f, WP_BTN_PASS},
        {"header shot", WP_LT_HEADER,    2.10f, 12.0f, WP_BTN_SHOOT},
    };
    static const float offs[3] = {-0.25f, 0.0f, 0.25f};
    int bad = 0;
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        float worst = 0;
        int n = 0;
        for (int o = 0; o < 3; o++) {
            float z = 0, d = run_case(&cases[k], offs[o], &z);
            if (d < 0) {
                printf("stance %s offset %+.2f: %s\n", cases[k].name, (double)offs[o],
                       d == -2.0f ? "touched with another stance" : d == -3.0f ? "no stance pose" : "no touch");
                bad = 1;
                continue;
            }
            n++;
            if (getenv("WP_POSE_V")) printf("  %s offset %+.2f: ball z %.2f, %.3f m\n", cases[k].name, (double)offs[o], (double)z, (double)d);
            if (d > worst) worst = d;
        }
        printf("stance %s: limb->ball %.3f m (worst of %d)\n", cases[k].name, (double)worst, n);
        if (worst > 0.15f) bad = 1;
    }
    return bad;
}
