#include "test_assert.h"
#include "../../core/disk.h"
#include <stdlib.h>
#include <string.h>

static void test_disk_dsk_encode(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    ASSERT_TRUE(dsk != NULL);
    dsk[0] = 0x01; dsk[1] = 0x02; dsk[2] = 0x03;
    ASSERT_EQ_INT(0, a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256));
    ASSERT_TRUE(d.loaded);
    int found = 0;
    for (int i = 0; i < A2E_DISK_NIB_LEN - 3; i++) {
        if (a2e_disk_track_nib(&d, 0)[i] == 0xD5 && a2e_disk_track_nib(&d, 0)[i + 1] == 0xAA && a2e_disk_track_nib(&d, 0)[i + 2] == 0x96) {
            found = 1;
            break;
        }
    }
    ASSERT_TRUE(found);
    free(dsk);
}

static void test_disk_spin_latch(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    free(dsk);
    d.motor_on = true;
    a2e_disk_softswitch_read(&d, 0xC0EE);
    a2e_disk_softswitch_read(&d, 0xC0EC);
    u8 saw_ready = 0;
    for (int i = 0; i < 500; i++) {
        a2e_disk_tick(&d);
        if (a2e_disk_softswitch_read(&d, 0xC0EC) & 0x80)
            saw_ready = 1;
    }
    ASSERT_TRUE(saw_ready);
}

int main(void) {
    test_disk_dsk_encode();
    test_disk_spin_latch();
    return test_report();
}
