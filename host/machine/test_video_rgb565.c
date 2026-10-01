#include "test_assert.h"
#include "../../core/machine.h"
#include "../../core/video.h"
#include <string.h>

static void test_rgb_to_565(void) {
    ASSERT_EQ_INT(0x0000, a2e_rgb_to_565(0, 0, 0));
    ASSERT_EQ_INT(0xFFFF, a2e_rgb_to_565(255, 255, 255));
    /* green phosphor on-pixel from text renderer */
    ASSERT_EQ_INT(a2e_rgb_to_565(0x33, 0xFF, 0x33),
                  (u16)(((0x33 & 0xF8) << 8) | ((0xFF & 0xFC) << 3) | (0x33 >> 3)));
}

static void test_byteswap(void) {
    u16 buf[2] = {0x1234, 0xABCD};
    a2e_rgb565_byteswap(buf, 2);
    ASSERT_EQ_INT(0x3412, buf[0]);
    ASSERT_EQ_INT(0xCDAB, buf[1]);
    a2e_rgb565_byteswap(buf, 2);
    ASSERT_EQ_INT(0x1234, buf[0]);
}

static void test_text_row_and_cyd_panel(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    /* Put ASCII 'A' (0xC1 = normal 'A' with high bit) at text (0,0) */
    m.main_ram[0x400] = 0xC1;

    u16 row[A2E_VIDEO_W];
    memset(m.main_ram + 0x400, 0xA0, 0x400); /* spaces */
    m.main_ram[0x400] = 0xC1; /* 'A' */
    a2e_video_render_row_rgb565(&m.mmu, 0, row);
    /* Space/background black; some glyph pixels green */
    int lit = 0;
    for (int x = 0; x < 7; x++) {
        if (row[x] != 0) lit++;
    }
    ASSERT_TRUE(lit > 0);
    ASSERT_EQ_INT(0, row[7]); /* next cell is space → black */

    u16 panel[A2E_CYD_PANEL_W];
    a2e_video_cyd_panel_row(&m.mmu, 0, panel); /* above Apple area */
    for (int x = 0; x < A2E_CYD_PANEL_W; x++)
        ASSERT_EQ_INT(0, panel[x]);

    a2e_video_cyd_panel_row(&m.mmu, A2E_CYD_OFF_Y, panel);
    for (int x = 0; x < A2E_CYD_OFF_X; x++)
        ASSERT_EQ_INT(0, panel[x]);
    ASSERT_EQ_INT(row[0], panel[A2E_CYD_OFF_X]);
    ASSERT_EQ_INT(row[6], panel[A2E_CYD_OFF_X + 6]);
}

int main(void) {
    test_rgb_to_565();
    test_byteswap();
    test_text_row_and_cyd_panel();
    return test_report();
}
