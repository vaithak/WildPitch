/* mixer.c - see mixer.h. */
#include <math.h>
#include <string.h>
#include "game/mixer.h"

void wp_mixer_init(WpMixer *m) {
    memset(m, 0, sizeof *m);
    for (int i = 0; i < WP_MIX_VOICES; i++) m->voice[i].snd = -1;
}

void wp_mixer_play(WpMixer *m, WpSoundId id, float gain, float pan, float rate, float delay_s, int bus) {
    if (id < 0 || id >= WP_SND_COUNT || gain <= 0.001f || !wp_sound(id)->data) return;
    int slot = -1;
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < WP_MIX_VOICES; i++) {
        if (m->voice[i].snd < 0) { slot = i; break; }
        if (m->started[i] < oldest) { oldest = m->started[i]; slot = i; }   /* none free: steal the oldest */
    }
    pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
    float a = (pan + 1.0f) * 0.25f * 3.14159265f;                          /* constant power */
    WpVoice *v = &m->voice[slot];
    v->snd = (int16_t)id;
    v->bus = (uint8_t)(bus == WP_BUS_CROWD ? WP_BUS_CROWD : WP_BUS_FX);
    v->pos = 0;
    v->rate = rate > 0.25f ? (rate < 4.0f ? rate : 4.0f) : 0.25f;
    v->delay = delay_s > 0 ? (int)(delay_s * (float)WP_AUDIO_RATE) : 0;
    v->gl = gain * cosf(a) * 1.41421356f;
    v->gr = gain * sinf(a) * 1.41421356f;
    m->started[slot] = ++m->age;
}

void wp_mixer_set_bus(WpMixer *m, int bus, float gain) {
    if (bus >= 0 && bus < WP_BUS_COUNT) m->bus_target[bus] = gain < 0 ? 0 : gain;
}
void wp_mixer_set_loop(WpMixer *m, int loop, float gain) {
    if (loop >= 0 && loop < WP_LOOP_COUNT) m->loop_target[loop] = gain < 0 ? 0 : gain;
}

void wp_mixer_render(WpMixer *m, float *out, int frames) {
    const float kb = 1.0f / (0.02f * (float)WP_AUDIO_RATE), kl = 1.0f / (0.4f * (float)WP_AUDIO_RATE);
    const float rel = 1.0f - 1.0f / (0.15f * (float)WP_AUDIO_RATE);         /* limiter release ~150 ms */
    static const WpSoundId LOOP_SND[WP_LOOP_COUNT] = {WP_SND_CROWD_BED, WP_SND_CROWD_EXCITE};
    for (int f = 0; f < frames; f++) {
        for (int b = 0; b < WP_BUS_COUNT; b++) m->bus_gain[b] += (m->bus_target[b] - m->bus_gain[b]) * kb;
        float L = 0, Rr = 0, crowd = m->bus_gain[WP_BUS_CROWD];
        for (int k = 0; k < WP_LOOP_COUNT; k++) {
            m->loop_gain[k] += (m->loop_target[k] - m->loop_gain[k]) * kl;
            const WpSound *s = wp_sound(LOOP_SND[k]);
            if (!s->data || s->len < 2) continue;
            int i = (int)m->loop_pos[k], j = (i + s->len / 2) % s->len;   /* right ear: half a loop later (wide) */
            float g = m->loop_gain[k] * crowd;
            L += s->data[i] * g; Rr += s->data[j] * g;
            if (++m->loop_pos[k] >= (float)s->len) m->loop_pos[k] = 0;
        }
        for (int i = 0; i < WP_MIX_VOICES; i++) {
            WpVoice *v = &m->voice[i];
            if (v->snd < 0) continue;
            if (v->delay > 0) { v->delay--; continue; }
            const WpSound *s = wp_sound((WpSoundId)v->snd);
            int p = (int)v->pos;
            if (p >= s->len - 1) { v->snd = -1; continue; }
            float fr = v->pos - (float)p, x = s->data[p] + (s->data[p + 1] - s->data[p]) * fr;
            float g = m->bus_gain[v->bus];
            L += x * v->gl * g; Rr += x * v->gr * g;
            v->pos += v->rate;
        }
        float pk = fmaxf(fabsf(L), fabsf(Rr));
        m->env = pk > m->env * rel ? pk : m->env * rel;
        float lim = m->env > WP_MIX_CEIL ? WP_MIX_CEIL / m->env : 1.0f;
        out[2 * f] = L * lim;
        out[2 * f + 1] = Rr * lim;
    }
}
