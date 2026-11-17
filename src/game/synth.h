/* synth.h - every sound in the game, synthesised once at startup from oscillators, noise and filters (D-055): no
 * sound files, no audio library beyond SDL's device. Pure C (no SDL), mono float samples at WP_AUDIO_RATE. The
 * crowd loops are seamless (cross-faded ends); one-shots start and end at zero (no clicks). */
#ifndef WP_SYNTH_H
#define WP_SYNTH_H
#include <stdbool.h>

#define WP_AUDIO_RATE 48000

typedef enum {
    WP_SND_KICK_PASS,      /* firm side-foot: short thump + soft click */
    WP_SND_KICK_SHOT,      /* laces: deeper, louder thump + sharp leather slap */
    WP_SND_KICK_LOB,       /* scooped: rounder thump + a little air */
    WP_SND_HEADER,         /* dull forehead thud */
    WP_SND_BOUNCE,         /* ball on grass */
    WP_SND_POST,           /* metallic ping (inharmonic partials) + thud */
    WP_SND_NET,            /* swish of the net taking the ball */
    WP_SND_WHISTLE_SHORT,  /* pea whistle, one blast (fouls, offside, kick-offs, goals) */
    WP_SND_WHISTLE_LONG,   /* long blast (half time; the last of the three at full time) */
    WP_SND_TACKLE,         /* boot-on-grass scuff (slides, tackles) */
    WP_SND_CROWD_BED,      /* loop: soft murmur */
    WP_SND_CROWD_EXCITE,   /* loop: a brighter, louder crowd, faded in with danger near a goal */
    WP_SND_CROWD_ROAR,     /* one-shot: a goal */
    WP_SND_CROWD_GROAN,    /* one-shot: "ooh" at a miss / the post */
    WP_SND_COUNT
} WpSoundId;

typedef struct {
    const float *data;
    int          len;      /* samples */
    bool         loop;
    const char  *name;
} WpSound;

void           wp_synth_build(void);            /* idempotent; ~20 s of audio into static storage, a few ms */
const WpSound *wp_sound(WpSoundId id);          /* after wp_synth_build */
#endif
