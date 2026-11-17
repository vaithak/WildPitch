/* audio_check.c - --check-audio (also a ctest): every synthesised sound is non-silent, within full scale, the
 * expected length, free of DC, starts and ends at zero (one-shots) or wraps seamlessly (loops); the mixer's
 * worst-case mix stays under its ceiling, pans and mutes. No device, no window. */
#include <math.h>
#include <stdio.h>
#include "game/mixer.h"

static float out[2 * WP_AUDIO_RATE * 3];

static void render_stats(WpMixer *m, int frames, float *pk_l, float *pk_r, float *rms_l, float *rms_r) {
    wp_mixer_render(m, out, frames);
    double sl = 0, sr = 0;
    *pk_l = *pk_r = 0;
    for (int i = 0; i < frames; i++) {
        float l = out[2 * i], r = out[2 * i + 1];
        *pk_l = fmaxf(*pk_l, fabsf(l)); *pk_r = fmaxf(*pk_r, fabsf(r));
        sl += (double)(l * l); sr += (double)(r * r);
    }
    *rms_l = (float)sqrt(sl / frames); *rms_r = (float)sqrt(sr / frames);
}

int wp_check_audio(void) {
    wp_synth_build();
    int bad = 0;
    for (int id = 0; id < WP_SND_COUNT; id++) {
        const WpSound *s = wp_sound((WpSoundId)id);
        float pk = 0, dc = 0, rms = 0;
        for (int i = 0; i < s->len; i++) { pk = fmaxf(pk, fabsf(s->data[i])); dc += s->data[i]; rms += s->data[i] * s->data[i]; }
        dc /= (float)s->len; rms = sqrtf(rms / (float)s->len);
        float secs = (float)s->len / (float)WP_AUDIO_RATE;
        float a = fabsf(s->data[0]), z = fabsf(s->data[s->len - 1]);
        float wrap = fabsf(s->data[s->len - 1] - s->data[0]);
        bool ok = pk > 0.1f && pk <= 1.0f && rms > 0.005f && secs >= 0.08f && secs <= 8.0f && fabsf(dc) < 0.01f &&
                  (s->loop ? wrap < 0.05f : (a < 0.02f && z < 0.02f));
        printf("%-14s %5.2f s  peak %.2f  rms %.3f  dc %+.4f  %s %.3f  %s\n", s->name, (double)secs, (double)pk, (double)rms,
               (double)dc, s->loop ? "wrap" : "ends", (double)(s->loop ? wrap : fmaxf(a, z)), ok ? "ok" : "FAIL");
        bad += !ok;
    }
    static WpMixer m;
    float pl, pr, rl, rr;
    /* worst case: everything at full volume at once, loops up, a pile of kicks on top */
    wp_mixer_init(&m);
    m.bus_gain[0] = m.bus_target[0] = 1.0f; m.bus_gain[1] = m.bus_target[1] = 1.0f;
    m.loop_gain[0] = m.loop_target[0] = 1.0f; m.loop_gain[1] = m.loop_target[1] = 1.0f;
    wp_mixer_play(&m, WP_SND_CROWD_ROAR, 1.5f, 0, 1, 0, WP_BUS_CROWD);
    wp_mixer_play(&m, WP_SND_CROWD_GROAN, 1.5f, 0, 1, 0, WP_BUS_CROWD);
    wp_mixer_play(&m, WP_SND_WHISTLE_LONG, 1.5f, 0, 1, 0.1f, WP_BUS_FX);
    wp_mixer_play(&m, WP_SND_POST, 1.5f, 0, 1, 0.2f, WP_BUS_FX);
    for (int k = 0; k < 8; k++) wp_mixer_play(&m, WP_SND_KICK_SHOT, 1.5f, (float)k / 4 - 1, 1, 0.3f, WP_BUS_FX);
    render_stats(&m, 2 * WP_AUDIO_RATE, &pl, &pr, &rl, &rr);
    bool ok = pl <= WP_MIX_CEIL + 1e-4f && pr <= WP_MIX_CEIL + 1e-4f && pl > 0.3f && pr > 0.3f;
    printf("worst-case mix: peak %.3f / %.3f (ceiling %.2f)  %s\n", (double)pl, (double)pr, (double)WP_MIX_CEIL, ok ? "ok" : "FAIL");
    bad += !ok;
    /* pan: a kick hard left is much louder in the left ear */
    wp_mixer_init(&m);
    m.bus_gain[0] = m.bus_target[0] = 1.0f;
    wp_mixer_play(&m, WP_SND_KICK_SHOT, 1.0f, -1.0f, 1, 0, WP_BUS_FX);
    render_stats(&m, WP_AUDIO_RATE / 2, &pl, &pr, &rl, &rr);
    ok = rl > 0.01f && rr < rl * 0.05f;
    printf("pan left: rms L %.3f R %.4f  %s\n", (double)rl, (double)rr, ok ? "ok" : "FAIL");
    bad += !ok;
    /* mute: bus targets to 0 -> silence after the glide, while sounds keep "playing" */
    wp_mixer_init(&m);
    m.bus_gain[0] = m.bus_target[0] = 1.0f; m.bus_gain[1] = m.bus_target[1] = 1.0f;
    m.loop_gain[0] = m.loop_target[0] = 1.0f;
    wp_mixer_play(&m, WP_SND_WHISTLE_LONG, 1.0f, 0, 1, 0, WP_BUS_FX);
    wp_mixer_set_bus(&m, WP_BUS_FX, 0); wp_mixer_set_bus(&m, WP_BUS_CROWD, 0);
    render_stats(&m, WP_AUDIO_RATE / 4, &pl, &pr, &rl, &rr);             /* the glide (~20 ms) */
    render_stats(&m, WP_AUDIO_RATE / 4, &pl, &pr, &rl, &rr);
    ok = pl < 1e-3f && pr < 1e-3f;
    printf("mute: peak %.5f  %s\n", (double)fmaxf(pl, pr), ok ? "ok" : "FAIL");
    bad += !ok;
    /* voice pool: more sounds than voices steals, never overruns */
    wp_mixer_init(&m);
    m.bus_gain[0] = m.bus_target[0] = 1.0f;
    for (int k = 0; k < 3 * WP_MIX_VOICES; k++) wp_mixer_play(&m, WP_SND_BOUNCE, 0.5f, 0, 1, 0, WP_BUS_FX);
    render_stats(&m, WP_AUDIO_RATE, &pl, &pr, &rl, &rr);
    int busy = 0;
    for (int i = 0; i < WP_MIX_VOICES; i++) busy += m.voice[i].snd >= 0;
    ok = busy == 0 && pl <= WP_MIX_CEIL + 1e-4f;
    printf("voice stealing: %d voices still busy after 1 s, peak %.3f  %s\n", busy, (double)pl, ok ? "ok" : "FAIL");
    bad += !ok;
    printf(bad ? "audio check: FAIL\n" : "audio check: ok\n");
    return bad ? 1 : 0;
}
