#ifndef A2E_AUDIO_H
#define A2E_AUDIO_H

#include "types.h"

#define A2E_AUDIO_RATE      44100
#define A2E_AUDIO_CPU_HZ    1020484
#define A2E_AUDIO_EV_MAX    4096

typedef struct a2e_audio {
    bool enabled;
    bool disk_fx;              /* synth motor/seek; off by default (not convincing) */
    bool spk_level;
    u64  render_cyc;           /* last CPU cycle converted to PCM */

    u64  ev_cyc[A2E_AUDIO_EV_MAX];
    u8   ev_lvl[A2E_AUDIO_EV_MAX];
    int  ev_n;

    bool motor_on;
    bool motor_prev;
    int  half_track;
    int  half_prev;
    int  seek_env;             /* decaying click amplitude */
    u32  noise_state;          /* LCG for motor whir */
} a2e_audio;

void a2e_audio_init(a2e_audio *a);
void a2e_audio_speaker_toggle(a2e_audio *a, u64 cycle, bool level);
void a2e_audio_disk_poll(a2e_audio *a, bool motor_on, int half_track);

/*
 * Render PCM from a->render_cyc up to end_cyc at A2E_AUDIO_RATE.
 * Returns sample count written (≤ max_samples).
 */
int a2e_audio_render(a2e_audio *a, u64 end_cyc, i16 *out, int max_samples);

#endif /* A2E_AUDIO_H */
