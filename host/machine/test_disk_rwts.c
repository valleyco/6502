#include "test_assert.h"
#include "../../core/disk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --- helpers that exercise the controller like RWTS (softswitch poll) --- */

static u8 wait_nibble(a2e_disk *d, int max_ticks) {
    for (int i = 0; i < max_ticks; i++) {
        a2e_disk_tick(d);
        u8 v = a2e_disk_softswitch_read(d, 0xC0EC); /* Q6L */
        if (v & 0x80)
            return v;
    }
    return 0;
}

static int find_seq(a2e_disk *d, const u8 *seq, int n, int max_nibbles) {
    int matched = 0;
    for (int i = 0; i < max_nibbles; i++) {
        u8 v = wait_nibble(d, 64);
        if (!(v & 0x80))
            return -1;
        if ((v & 0x7F) == (seq[matched] & 0x7F) || v == seq[matched]) {
            /* disk nibbles are stored with high bit; compare full byte */
        }
        if (v == seq[matched] || (v | 0x80) == (seq[matched] | 0x80)) {
            /* exact match on stored nibble value (already has high bits) */
        }
        if (v == seq[matched]) {
            matched++;
            if (matched == n)
                return 0;
        } else {
            matched = (v == seq[0]) ? 1 : 0;
        }
    }
    return -1;
}

static void test_stepper_to_track_5(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    free(dsk);
    d.motor_on = true;
    /* Phase ON sequence advancing inward: 0,1,2,3,... two half-tracks per track */
    ASSERT_EQ_INT(0, d.track);
    for (int ht = 0; ht < 10; ht++) { /* 10 half-tracks → track 5 */
        int ph = (ht + 1) & 3;
        a2e_disk_softswitch_read(&d, (u16)(0xC0E1 + ph * 2)); /* phase ON only */
    }
    ASSERT_EQ_INT(5, d.track);
}

static void test_roundtrip_sector_bytes(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    for (int i = 0; i < 256; i++)
        dsk[i] = (u8)(i ^ 0xA5); /* T0 logical S0 payload */
    ASSERT_EQ_INT(0, a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256));

    u8 out[256];
    memset(out, 0, sizeof out);
    /* Pure decode from nibble track (no softswitches) — API under test */
    int rc = a2e_disk_decode_sector(a2e_disk_track_nib(&d, 0), A2E_DISK_NIB_LEN, 0 /*track*/, 0 /*phys sec*/, out);
    ASSERT_EQ_INT(0, rc);
    int mism = 0;
    for (int i = 0; i < 256; i++)
        if (out[i] != dsk[i])
            mism++;
    ASSERT_EQ_INT(0, mism);
    free(dsk);
}

static void test_softswitch_find_address_prolog(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    free(dsk);
    d.motor_on = true;
    a2e_disk_softswitch_read(&d, 0xC0EE); /* Q7L read mode */
    a2e_disk_softswitch_read(&d, 0xC0EC); /* Q6L */
    u8 seq[] = { 0xD5, 0xAA, 0x96 };
    ASSERT_EQ_INT(0, find_seq(&d, seq, 3, 8000));
}

static void test_softswitch_read_sector0(void) {
    a2e_disk d;
    a2e_disk_init(&d);
    u8 *dsk = calloc(35 * 16 * 256, 1);
    for (int i = 0; i < 256; i++)
        dsk[i] = (u8)i;
    a2e_disk_load_dsk(&d, dsk, 35 * 16 * 256);
    d.motor_on = true;
    a2e_disk_softswitch_read(&d, 0xC0EE);
    a2e_disk_softswitch_read(&d, 0xC0EC);

    u8 out[256];
    int rc = a2e_disk_softswitch_read_sector(&d, 0, 0, out, 200000);
    ASSERT_EQ_INT(0, rc);
    int mism = 0;
    for (int i = 0; i < 256; i++)
        if (out[i] != dsk[i])
            mism++;
    ASSERT_EQ_INT(0, mism);
    free(dsk);
}

int main(void) {
    test_stepper_to_track_5();
    test_roundtrip_sector_bytes();
    test_softswitch_find_address_prolog();
    test_softswitch_read_sector0();
    return test_report();
}
