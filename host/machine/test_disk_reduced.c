#include "test_assert.h"
#include "../../core/disk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Built with -DA2E_REDUCED_DISK (CYD / D6 profile): one nibble track cache.
 * Image bytes must remain valid for track switches.
 */

#ifndef A2E_REDUCED_DISK
#error "test_disk_reduced requires -DA2E_REDUCED_DISK"
#endif

static void test_reduced_size(void) {
    /* Full cache is 35 * 6656 ≈ 228 KiB; reduced is one track + pointers. */
    ASSERT_TRUE(sizeof(a2e_disk) < 20000);
    printf("sizeof(a2e_disk) reduced = %zu\n", sizeof(a2e_disk));
}

static void test_track_switch_renibblizes(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *img = calloc(35 * 16 * 256, 1);
    ASSERT_TRUE(img != NULL);
    img[0] = 0x11;
    img[1 * 16 * 256] = 0x22; /* track 1 sector 0 */

    ASSERT_EQ_INT(0, a2e_disk_load_dsk(&d, img, 35 * 16 * 256));
    ASSERT_EQ_INT(0, d.cached_track);

    u8 out[256];
    ASSERT_EQ_INT(0, a2e_disk_decode_sector(a2e_disk_track_nib(&d, 0),
                                            A2E_DISK_NIB_LEN, 0, 0, out));
    ASSERT_EQ_INT(0x11, out[0]);

    ASSERT_EQ_INT(0, a2e_disk_decode_sector(a2e_disk_track_nib(&d, 1),
                                            A2E_DISK_NIB_LEN, 1, 0, out));
    ASSERT_EQ_INT(1, d.cached_track);
    ASSERT_EQ_INT(0x22, out[0]);

    /* Stepper: two phase advances → one full track */
    d.half_track = 0;
    d.track = 0;
    d.phase = 0;
    d.cached_track = 0;
    a2e_disk_softswitch_read(&d, 0xC0E3); /* phase 1 — delta from 0 */
    a2e_disk_softswitch_read(&d, 0xC0E5); /* phase 2 */
    ASSERT_EQ_INT(2, d.half_track);
    ASSERT_EQ_INT(1, d.track);
    ASSERT_EQ_INT(1, d.cached_track);

    free(img);
}

int main(void) {
    test_reduced_size();
    test_track_switch_renibblizes();
    return test_report();
}
