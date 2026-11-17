/* audio.c - see audio.h. */
#include <stdio.h>
#include "game/audio.h"

static void SDLCALL feed(void *ud, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    WpAudio *a = (WpAudio *)ud;                       /* SDL holds the stream lock while we're here */
    int frames = additional / (int)(2 * sizeof(float));
    while (frames > 0) {
        int n = frames < 512 ? frames : 512;
        wp_mixer_render(&a->mix, a->buf, n);
        SDL_PutAudioStreamData(stream, a->buf, n * (int)(2 * sizeof(float)));
        frames -= n;
    }
}

bool wp_audio_open(WpAudio *a) {
    SDL_memset(a, 0, sizeof *a);
    wp_synth_build();
    wp_mixer_init(&a->mix);
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "audio: off (%s)\n", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");    /* ~11 ms buffers: kicks land on the frame */
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, WP_AUDIO_RATE};
    a->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, a);
    if (!a->stream) {
        fprintf(stderr, "audio: no device (%s)\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    a->ok = true;
    SDL_ResumeAudioStreamDevice(a->stream);
    printf("audio: driver %s\n", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?");
    return true;
}

void wp_audio_close(WpAudio *a) {
    if (!a->ok) return;
    SDL_DestroyAudioStream(a->stream);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    a->stream = NULL;
    a->ok = false;
}

void wp_audio_apply(WpAudio *a, const WpSettings *s, bool paused) {
    if (!a->ok) return;
    float master = s->muted ? 0.0f : (float)s->vol_master / 10.0f;
    master *= master;                                  /* the slider feels linear in loudness */
    float fx = (float)s->vol_fx / 10.0f, cr = (float)s->vol_crowd / 10.0f;
    SDL_LockAudioStream(a->stream);
    wp_mixer_set_bus(&a->mix, WP_BUS_FX, master * fx);
    wp_mixer_set_bus(&a->mix, WP_BUS_CROWD, master * cr * (paused ? 0.25f : 1.0f));
    SDL_UnlockAudioStream(a->stream);
}

void wp_audio_play(WpAudio *a, WpSoundId id, float gain, float pan, float rate, float delay_s) {
    if (id >= 0 && id < WP_SND_COUNT) a->plays[id]++;
    if (!a->ok) return;
    int bus = id >= WP_SND_CROWD_BED ? WP_BUS_CROWD : WP_BUS_FX;
    SDL_LockAudioStream(a->stream);
    wp_mixer_play(&a->mix, id, gain, pan, rate, delay_s, bus);
    SDL_UnlockAudioStream(a->stream);
}

void wp_audio_crowd(WpAudio *a, float bed, float excite) {
    if (!a->ok) return;
    SDL_LockAudioStream(a->stream);
    wp_mixer_set_loop(&a->mix, WP_LOOP_BED, bed);
    wp_mixer_set_loop(&a->mix, WP_LOOP_EXCITE, excite);
    SDL_UnlockAudioStream(a->stream);
}
