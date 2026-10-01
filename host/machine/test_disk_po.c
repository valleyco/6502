#include "test_assert.h"
#include "../../core/disk.h"
#include <stdlib.h>
#include <string.h>

/*
 * ProDOS-order (.po) images store track sectors in PO interleave, not DOS 3.3
 * logical order. Physical address-field numbers still use the PO→phys map.
 */

static void fill_sec(u8 *dsk, int trk, int idx, u8 tag) {
    u8 *p = dsk + (size_t)(trk * 16 + idx) * 256;
    memset(p, tag, 256);
    p[0] = tag;
    p[1] = (u8)trk;
    p[2] = (u8)idx;
}

static void test_po_load_and_decode(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *img = calloc(35 * 16 * 256, 1);
    ASSERT_TRUE(img != NULL);

    /* PO index 0 → phys 0; PO index 1 → phys 2 */
    fill_sec(img, 0, 0, 0xA0);
    fill_sec(img, 0, 1, 0xA1);

    ASSERT_EQ_INT(0, a2e_disk_load_po(&d, img, 35 * 16 * 256));
    ASSERT_TRUE(d.loaded);

    u8 out[256];
    ASSERT_EQ_INT(0, a2e_disk_decode_sector(a2e_disk_track_nib(&d, 0), A2E_DISK_NIB_LEN, 0, 0, out));
    ASSERT_EQ_INT(0xA0, out[0]);
    ASSERT_EQ_INT(0, out[1]);
    ASSERT_EQ_INT(0, out[2]);

    ASSERT_EQ_INT(0, a2e_disk_decode_sector(a2e_disk_track_nib(&d, 0), A2E_DISK_NIB_LEN, 0, 2, out));
    ASSERT_EQ_INT(0xA1, out[0]);
    ASSERT_EQ_INT(0, out[1]);
    ASSERT_EQ_INT(1, out[2]);

    free(img);
}

static void test_po_not_confused_with_dsk(void) {
    a2e_disk d_po, d_do;
    a2e_disk_init(&d_po);
    a2e_disk_init(&d_do);
    u8 *img = calloc(35 * 16 * 256, 1);
    ASSERT_TRUE(img != NULL);
    fill_sec(img, 1, 1, 0x5A);

    a2e_disk_load_po(&d_po, img, 35 * 16 * 256);
    a2e_disk_load_dsk(&d_do, img, 35 * 16 * 256);

    u8 po_out[256], do_out[256];
    /* PO idx 1 → phys 2; DO idx 1 → phys 0xD */
    ASSERT_EQ_INT(0, a2e_disk_decode_sector(a2e_disk_track_nib(&d_po, 1), A2E_DISK_NIB_LEN, 1, 2, po_out));
    ASSERT_EQ_INT(0x5A, po_out[0]);

    ASSERT_EQ_INT(0, a2e_disk_decode_sector(a2e_disk_track_nib(&d_do, 1), A2E_DISK_NIB_LEN, 1, 0xD, do_out));
    ASSERT_EQ_INT(0x5A, do_out[0]);

    /* Same phys sector number must not both hold the marker */
    int po_on_d = a2e_disk_decode_sector(a2e_disk_track_nib(&d_po, 1), A2E_DISK_NIB_LEN, 1, 0xD, po_out);
    if (po_on_d == 0)
        ASSERT_TRUE(po_out[0] != 0x5A);

    free(img);
}

int main(void) {
    test_po_load_and_decode();
    test_po_not_confused_with_dsk();
    return test_report();
}
