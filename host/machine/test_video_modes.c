#include "test_assert.h"
#include "../../core/machine.h"
#include "../../core/video.h"
#include <string.h>

/* IIe soft-switch status (bit7): $C01A TEXT, $C01B MIXED, $C01C PAGE2, $C01D HIRES */

static void test_video_softswitches_and_status(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);

    ASSERT_TRUE(m.mmu.text_mode);
    ASSERT_TRUE(!m.mmu.mixed);
    ASSERT_TRUE(!m.mmu.hires);
    ASSERT_TRUE(!m.mmu.page2);
    ASSERT_TRUE(a2e_mmu_read(&m.mmu, 0xC01A) & 0x80); /* TEXT */
    ASSERT_TRUE(!(a2e_mmu_read(&m.mmu, 0xC01B) & 0x80));
    ASSERT_TRUE(!(a2e_mmu_read(&m.mmu, 0xC01C) & 0x80));
    ASSERT_TRUE(!(a2e_mmu_read(&m.mmu, 0xC01D) & 0x80));

    (void)a2e_mmu_read(&m.mmu, 0xC050); /* GR */
    ASSERT_TRUE(!m.mmu.text_mode);
    ASSERT_TRUE(!(a2e_mmu_read(&m.mmu, 0xC01A) & 0x80));

    (void)a2e_mmu_read(&m.mmu, 0xC053); /* MIXED */
    ASSERT_TRUE(m.mmu.mixed);
    ASSERT_TRUE(a2e_mmu_read(&m.mmu, 0xC01B) & 0x80);

    (void)a2e_mmu_read(&m.mmu, 0xC052); /* FULL */
    ASSERT_TRUE(!m.mmu.mixed);

    a2e_mmu_write(&m.mmu, 0xC057, 0); /* HIRES */
    ASSERT_TRUE(m.mmu.hires);
    ASSERT_TRUE(a2e_mmu_read(&m.mmu, 0xC01D) & 0x80);

    a2e_mmu_write(&m.mmu, 0xC056, 0); /* LORES */
    ASSERT_TRUE(!m.mmu.hires);

    a2e_mmu_write(&m.mmu, 0xC055, 0); /* PAGE2 */
    ASSERT_TRUE(m.mmu.page2);
    ASSERT_TRUE(a2e_mmu_read(&m.mmu, 0xC01C) & 0x80);

    (void)a2e_mmu_read(&m.mmu, 0xC051); /* TEXT */
    ASSERT_TRUE(m.mmu.text_mode);
    ASSERT_TRUE(a2e_mmu_read(&m.mmu, 0xC01A) & 0x80);
}

static void test_lores_pixel_and_mixed(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    memset(m.main_ram + 0x400, 0, 0x400);

    /* Lores color 1 (magenta-ish) in upper nibble of cell (0,0) */
    m.main_ram[0x400] = 0x01;

    (void)a2e_mmu_read(&m.mmu, 0xC050); /* GR */
    (void)a2e_mmu_read(&m.mmu, 0xC056); /* LORES */

    u16 row[A2E_VIDEO_W];
    a2e_video_render_row_rgb565(&m.mmu, 0, row);
    ASSERT_TRUE(row[0] != 0); /* lit lores block */

    /* Mixed: bottom text rows still draw text glyphs */
    (void)a2e_mmu_read(&m.mmu, 0xC053);
    memset(m.main_ram + 0x400, 0xA0, 0x400);
    /* Text row 20 starts at LINE[20]=0x250 → $650 */
    m.main_ram[0x650] = 0xC1; /* 'A' */
    a2e_video_render_row_rgb565(&m.mmu, 160, row); /* first scanline of text row 20 */
    int lit = 0;
    for (int x = 0; x < 7; x++)
        if (row[x] != 0) lit++;
    ASSERT_TRUE(lit > 0);
}

static void test_hires_pixel(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    memset(m.main_ram + 0x2000, 0, 0x2000);

    (void)a2e_mmu_read(&m.mmu, 0xC050); /* GR */
    (void)a2e_mmu_read(&m.mmu, 0xC057); /* HIRES */

    /* Set bit0 of first hires byte on line 0 → pixel x=0 */
    m.main_ram[0x2000] = 0x01;

    u16 row[A2E_VIDEO_W];
    a2e_video_render_row_rgb565(&m.mmu, 0, row);
    ASSERT_TRUE(row[0] != 0);
    ASSERT_EQ_INT(0, row[1]);
}

int main(void) {
    test_video_softswitches_and_status();
    test_lores_pixel_and_mixed();
    test_hires_pixel();
    return test_report();
}
