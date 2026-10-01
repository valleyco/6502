#include "test_assert.h"
#include "../../core/disk.h"
#include <stdlib.h>
#include <string.h>

/* TDD: unread nibble must survive LDA abs,X leading ticks (hold until Q6L). */

static void test_latch_holds_until_read(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    free(dsk);
    d.motor_on = true;
    a2e_disk_softswitch_read(&d, 0xC0EE);

    for (int i = 0; i < 64; i++)
        a2e_disk_tick(&d);
    ASSERT_TRUE(d.latch_ready);
    u8 held = d.latch;

    for (int i = 0; i < 200; i++)
        a2e_disk_tick(&d);
    ASSERT_TRUE(d.latch_ready);
    ASSERT_EQ_INT(held, d.latch);

    u8 got = a2e_disk_softswitch_read(&d, 0xC0EC);
    ASSERT_TRUE(got & 0x80);
    ASSERT_EQ_INT(held, got);
    ASSERT_TRUE(!d.latch_ready);

    int saw = 0;
    for (int i = 0; i < 40; i++) {
        a2e_disk_tick(&d);
        if (d.latch_ready) {
            saw = 1;
            ASSERT_TRUE(i >= 31);
            break;
        }
    }
    ASSERT_TRUE(saw);
}

static void test_mid_instruction_hold(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    free(dsk);
    d.motor_on = true;
    a2e_disk_softswitch_read(&d, 0xC0EE);

    while (!d.latch_ready)
        a2e_disk_tick(&d);
    u8 first = d.latch;

    for (int i = 0; i < 4; i++)
        a2e_disk_tick(&d);
    ASSERT_EQ_INT(first, d.latch);
    ASSERT_TRUE(d.latch_ready);

    u8 got = a2e_disk_softswitch_read(&d, 0xC0EC);
    ASSERT_EQ_INT(first, got);
}

static void test_poll_loop_reads_sequential_nibbles(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    free(dsk);
    d.motor_on = true;
    a2e_disk_softswitch_read(&d, 0xC0EE);

    int matched = 0;
    u8 seq[] = { 0xD5, 0xAA, 0x96 };
    for (int tries = 0; tries < 20000; tries++) {
        a2e_disk_tick(&d);
        u8 v = a2e_disk_softswitch_read(&d, 0xC0EC);
        if (!(v & 0x80))
            continue;
        if (v == seq[matched]) {
            matched++;
            if (matched == 3)
                break;
        } else {
            matched = (v == seq[0]) ? 1 : 0;
        }
    }
    ASSERT_EQ_INT(3, matched);
}

int main(void) {
    test_latch_holds_until_read();
    test_mid_instruction_hold();
    test_poll_loop_reads_sequential_nibbles();
    return test_report();
}
