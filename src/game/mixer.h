/* mixer.h - the software mixer (D-055), independent of any audio device so --check-audio can run it headless.
 * A fixed pool of one-shot voices (gain, constant-power pan, playback rate, start delay) on two buses (effects,
 * crowd), plus the two crowd loops with their own smoothed gains. Bus gains (master x bus x mute x pause) glide per
 * sample, so changes never click; a peak limiter on the sum keeps the output within +-WP_MIX_CEIL.
 * Not thread-safe by itself: audio.c serialises the device callback and the game thread with the stream lock. */
#ifndef WP_MIXER_H
#define WP_MIXER_H
#include <stdbool.h>
#include <stdint.h>
#include "game/synth.h"

#define WP_MIX_VOICES 24
#define WP_MIX_CEIL   0.95f
enum { WP_BUS_FX = 0, WP_BUS_CROWD = 1, WP_BUS_COUNT };
enum { WP_LOOP_BED = 0, WP_LOOP_EXCITE = 1, WP_LOOP_COUNT };

typedef struct {
    int16_t snd;          /* WpSoundId, -1 = free */
    uint8_t bus;
    float   pos, rate;    /* sample position (fractional: rate != 1 pitches it) */
    int     delay;        /* samples before it starts */
    float   gl, gr;       /* per-channel gain (gain x pan) */
} WpVoice;

typedef struct {
    WpVoice voice[WP_MIX_VOICES];
    float   bus_target[WP_BUS_COUNT], bus_gain[WP_BUS_COUNT];
    float   loop_target[WP_LOOP_COUNT], loop_gain[WP_LOOP_COUNT];
    float   loop_pos[WP_LOOP_COUNT];
    float   env;          /* limiter envelope */
    uint32_t age;         /* voice stealing: oldest first */
    uint32_t started[WP_MIX_VOICES];
} WpMixer;

void wp_mixer_init(WpMixer *m);
/* gain 0..1+, pan -1 (left) .. 1 (right), rate 1 = as built, delay in seconds */
void wp_mixer_play(WpMixer *m, WpSoundId id, float gain, float pan, float rate, float delay_s, int bus);
void wp_mixer_set_bus(WpMixer *m, int bus, float gain);          /* target; glides ~20 ms */
void wp_mixer_set_loop(WpMixer *m, int loop, float gain);        /* target; glides ~0.4 s */
/* interleaved stereo float, frames samples per channel */
void wp_mixer_render(WpMixer *m, float *out, int frames);

int  wp_check_audio(void);   /* --check-audio: every sound + the mixer (worst-case mix, pan, mute), no device */
#endif
