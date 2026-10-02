#include "audio.h"
#include <string.h>

#define SPK_AMP    3500
#define MOTOR_AMP   380
#define SEEK_AMP   5500
#define SEEK_DECAY   48

void a2e_audio_init(a2e_audio *a) {
    memset(a, 0, sizeof(*a));
    a->enabled = true;
    a->disk_fx = false;
    a->noise_state = 0xA5A5u;
    a->half_prev = -1;
}

void a2e_audio_speaker_toggle(a2e_audio *a, u64 cycle, bool level) {
    if (!a || !a->enabled) return;
    a->spk_level = level;
    if (a->ev_n >= A2E_AUDIO_EV_MAX) {
        /* Drop oldest half if flooded (rare pathological click loops). */
        int keep = A2E_AUDIO_EV_MAX / 2;
        memmove(a->ev_cyc, a->ev_cyc + (A2E_AUDIO_EV_MAX - keep),
                (size_t)keep * sizeof(a->ev_cyc[0]));
        memmove(a->ev_lvl, a->ev_lvl + (A2E_AUDIO_EV_MAX - keep),
                (size_t)keep * sizeof(a->ev_lvl[0]));
        a->ev_n = keep;
    }
    a->ev_cyc[a->ev_n] = cycle;
    a->ev_lvl[a->ev_n] = level ? 1 : 0;
    a->ev_n++;
}

void a2e_audio_disk_poll(a2e_audio *a, bool motor_on, int half_track) {
    if (!a || !a->enabled) return;
    if (half_track != a->half_prev && a->half_prev >= 0) {
        a->seek_env = SEEK_AMP;
    }
    a->half_prev = half_track;
    a->half_track = half_track;
    a->motor_prev = a->motor_on;
    a->motor_on = motor_on;
}

static i16 noise(a2e_audio *a) {
    a->noise_state = a->noise_state * 1664525u + 1013904223u;
    return (i16)((a->noise_state >> 16) & 0xFFFF);
}

int a2e_audio_render(a2e_audio *a, u64 end_cyc, i16 *out, int max_samples) {
    if (!a || !out || max_samples <= 0) return 0;
    if (!a->enabled) {
        a->render_cyc = end_cyc;
        return 0;
    }
    if (a->render_cyc == 0 && end_cyc > 0)
        a->render_cyc = end_cyc; /* first call: sync, avoid huge catch-up */

    /* Fixed-point cycles per sample: CPU_HZ / RATE ≈ 23.14 → 23 + fraction */
    const u64 cps_num = A2E_AUDIO_CPU_HZ;
    const u64 cps_den = A2E_AUDIO_RATE;
    int ev_i = 0;
    int n = 0;
    u64 frac = 0;

    while (a->render_cyc < end_cyc && n < max_samples) {
        u64 step = cps_num / cps_den;
        frac += cps_num % cps_den;
        if (frac >= cps_den) {
            frac -= cps_den;
            step++;
        }
        u64 next = a->render_cyc + step;
        if (next > end_cyc) next = end_cyc;

        while (ev_i < a->ev_n && a->ev_cyc[ev_i] < next) {
            a->spk_level = a->ev_lvl[ev_i] != 0;
            ev_i++;
        }

        i32 s = a->spk_level ? SPK_AMP : -SPK_AMP;

        if (a->disk_fx) {
            if (a->motor_on) {
                i16 nse = noise(a);
                s += (i32)((nse * MOTOR_AMP) / 32768);
            }
            if (a->seek_env > 0) {
                s += (a->seek_env * ((noise(a) & 1) ? 1 : -1));
                a->seek_env -= SEEK_DECAY;
                if (a->seek_env < 0) a->seek_env = 0;
            }
        }

        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out[n++] = (i16)s;
        a->render_cyc = next;
    }

    /* Discard consumed events */
    if (ev_i > 0) {
        int left = a->ev_n - ev_i;
        if (left > 0) {
            memmove(a->ev_cyc, a->ev_cyc + ev_i, (size_t)left * sizeof(a->ev_cyc[0]));
            memmove(a->ev_lvl, a->ev_lvl + ev_i, (size_t)left * sizeof(a->ev_lvl[0]));
        }
        a->ev_n = left;
    }
    return n;
}
