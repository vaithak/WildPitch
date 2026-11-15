#include <stdlib.h>
/* passfail.c - AI vs AI pass outcomes (CP0.3c baseline): for every pass / through / lob / cross, what happened first:
 * a team-mate controlled it (complete), an opponent cut it out (split: before / after it reached the target's
 * 1.5 m), it went out, or it was still loose when someone else... Tallied by pass length.  usage: passfail [7|5] */
#include <stdio.h>
#include <string.h>
#include "sim/sim.h"
static double tod(fx v) { return (double)v / 65536.0; }
int main(int argc, char **argv) {
    int seven = argc > 1 ? argv[1][0] == '7' : 1;
    static SimState s;
    long n[3] = {0}, ok[3] = {0}, cut_early[3] = {0}, cut_late[3] = {0}, out[3] = {0}, other[3] = {0}, kind_n[16] = {0}, kind_ok[16] = {0};
    long tackles[2] = {0}, goals[2] = {0}, shots[2] = {0}, src_n[3] = {0}, src_ok[3] = {0};   /* long passes: open play / set piece / keeper */
    for (int m = 0; m < 12; m++) {
        WpMatchConfig cfg = seven ? sim_config_7v7() : sim_config_5v5();
    if (getenv("WP_STATS")) cfg.stats_mode = (uint8_t)atoi(getenv("WP_STATS"));   /* 0 neutral, 1 mirrored, 2 own */
        sim_init(&s, &cfg);
        for (int k = 0; k < WP_MAX_SEATS; k++) s.seat_player[k] = -1;
        s.ball.pos = fv2_make(FX_INT((m % 5) - 2), FX_INT((m % 3) - 1));
        WpInput in; memset(&in, 0, sizeof in);
        int live = 0, kicker = -1, to = -1, kind = 0, band = 0, reached = 0, src = 0, prev_rs = 0;
        for (long t = 0; t < 60L * 60 * 3; t++) {
            prev_rs = s.restart_kind;
            sim_step(&s, &in);
            if (s.events & WP_EV_TACKLE) tackles[s.players[s.ball.last_touch].team]++;
            if (s.events & WP_EV_GOAL) goals[s.ball.pos.x > 0 ? 0 : 1]++;
            if (live) {
                if (to >= 0 && fv2_len(fv2_sub(s.ball.pos, s.players[to].pos)) < FX(1.5)) reached = 1;
                int o = s.ball.owner, done = -1;
                if (s.events & (WP_EV_OUT | WP_EV_GOAL)) done = 3;
                else if (o >= 0 && o != kicker) done = s.players[o].team == s.players[kicker].team ? 0 : (reached ? 2 : 1);
                else if (o == kicker || (s.events & WP_EV_RESET)) done = 4;
                if (done >= 0) {
                    live = 0;
                    if (done == 0) { ok[band]++; kind_ok[kind]++; if (band == 2) src_ok[src]++; }
                    else if (done == 1) cut_early[band]++;
                    else if (done == 2) cut_late[band]++;
                    else if (done == 3) out[band]++;
                    else other[band]++;
                }
            }
            if (s.events & WP_EV_KICK) {
                int k = s.ball.last_touch, kd = s.players[k].kick.kind;
                if (kd == WP_KICK_SHOT) shots[s.players[k].team]++;
                if (kd == WP_KICK_PASS || kd == WP_KICK_THROUGH || kd == WP_KICK_LOB) {
                    if (live) other[band]++;
                    live = 1; kicker = k; kind = kd; reached = 0;
                    to = s.players[k].kick.to ? s.players[k].kick.to - 1 : -1;
                    double d = to >= 0 ? tod(fv2_len(fv2_sub(s.players[to].pos, s.ball.pos))) : 15.0;
                    band = d < 10 ? 0 : d < 20 ? 1 : 2;
                    n[band]++; kind_n[kd]++;
                    src = prev_rs ? 1 : s.players[k].role == WP_ROLE_KEEPER ? 2 : 0;
                    if (band == 2) src_n[src]++;
                } else live = 0;
            }
        }
    }
    const char *bn[3] = {"<10 m", "10-20 m", ">20 m"};
    long N = 0, OK = 0;
    for (int b = 0; b < 3; b++) {
        N += n[b]; OK += ok[b];
        printf("%-8s passes %5ld: complete %4.0f%%  cut out in flight %4.0f%%  cut out at the receiver %4.0f%%  out %3.0f%%  other %3.0f%%\n",
               bn[b], n[b], 100.0 * ok[b] / (n[b] ? n[b] : 1), 100.0 * cut_early[b] / (n[b] ? n[b] : 1),
               100.0 * cut_late[b] / (n[b] ? n[b] : 1), 100.0 * out[b] / (n[b] ? n[b] : 1), 100.0 * other[b] / (n[b] ? n[b] : 1));
    }
    printf("all: %ld passes, %.0f%% complete | pass %.0f%% (n %ld), through %.0f%% (n %ld), lob/cross %.0f%% (n %ld)\n", N, 100.0 * OK / (N ? N : 1),
           100.0 * kind_ok[WP_KICK_PASS] / (kind_n[WP_KICK_PASS] ? kind_n[WP_KICK_PASS] : 1), kind_n[WP_KICK_PASS],
           100.0 * kind_ok[WP_KICK_THROUGH] / (kind_n[WP_KICK_THROUGH] ? kind_n[WP_KICK_THROUGH] : 1), kind_n[WP_KICK_THROUGH],
           100.0 * kind_ok[WP_KICK_LOB] / (kind_n[WP_KICK_LOB] ? kind_n[WP_KICK_LOB] : 1), kind_n[WP_KICK_LOB]);
    printf(">20 m by source: open play %ld (%.0f%% complete), set piece %ld (%.0f%%), keeper %ld (%.0f%%)\n", src_n[0], 100.0 * src_ok[0] / (src_n[0] ? src_n[0] : 1), src_n[1], 100.0 * src_ok[1] / (src_n[1] ? src_n[1] : 1), src_n[2], 100.0 * src_ok[2] / (src_n[2] ? src_n[2] : 1));
    printf("tackles %ld-%ld, goals %ld-%ld, shots %ld-%ld\n", tackles[0], tackles[1], goals[0], goals[1], shots[0], shots[1]);
}
