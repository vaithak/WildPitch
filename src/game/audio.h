/* audio.h - the audio device (D-055): SDL's audio subsystem, one float stereo stream at WP_AUDIO_RATE whose
 * callback pulls from the mixer. Everything here is a silent no-op when there is no device (headless, CI, harness
 * modes): wp_audio_open just returns false and every call checks `ok`. The game thread changes the mixer only
 * under the stream lock, so the callback never sees a half-written voice. No allocation after open. */
#ifndef WP_AUDIO_H
#define WP_AUDIO_H
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "game/mixer.h"
#include "game/settings.h"

typedef struct {
    SDL_AudioStream *stream;
    bool             ok;
    WpMixer          mix;
    float            buf[2 * 512];   /* callback scratch (interleaved stereo) */
} WpAudio;

bool wp_audio_open(WpAudio *a);                /* builds the sounds, opens the default device; false = no audio */
void wp_audio_close(WpAudio *a);
/* volumes from the settings (master x bus, mute); paused (pause menu / title): the crowd drops to a quarter */
void wp_audio_apply(WpAudio *a, const WpSettings *s, bool paused);
void wp_audio_play(WpAudio *a, WpSoundId id, float gain, float pan, float rate, float delay_s);
void wp_audio_crowd(WpAudio *a, float bed, float excite);    /* crowd loop targets 0..1 */
#endif
