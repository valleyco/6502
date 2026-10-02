#include "test_assert.h"
#include "../../core/audio.h"
#include "../../core/machine.h"
#include <string.h>

static void test_speaker_toggle_makes_pcm(void) {
    a2e_audio a;
    a2e_audio_init(&a);
    a.render_cyc = 1000; /* already synced */

    a2e_audio_speaker_toggle(&a, 1050, true);
    a2e_audio_speaker_toggle(&a, 1100, false);
    a2e_audio_speaker_toggle(&a, 1200, true);

    i16 buf[256];
    int n = a2e_audio_render(&a, 1000 + 23 * 64, buf, 256);
    ASSERT_TRUE(n > 8);
    int nonzero = 0;
    for (int i = 0; i < n; i++)
        if (buf[i] != 0) nonzero++;
    ASSERT_TRUE(nonzero > 0);
}

static void test_disk_seek_click(void) {
    a2e_audio a;
    a2e_audio_init(&a);
    a.disk_fx = true;
    a.render_cyc = 5000;
    a2e_audio_disk_poll(&a, true, 0);
    a2e_audio_disk_poll(&a, true, 1); /* seek */
    ASSERT_TRUE(a.seek_env > 0);

    i16 buf[128];
    int n = a2e_audio_render(&a, 5000 + 23 * 32, buf, 128);
    ASSERT_TRUE(n > 0);
}

static void test_machine_c030_queues_event(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    m.audio.render_cyc = m.cpu.cycles = 2000;
    (void)a2e_mmu_read(&m.mmu, 0xC030);
    ASSERT_TRUE(m.audio.ev_n >= 1);
    ASSERT_TRUE(m.mmu.speaker_level);
}

int main(void) {
    test_speaker_toggle_makes_pcm();
    test_disk_seek_click();
    test_machine_c030_queues_event();
    return test_report();
}
