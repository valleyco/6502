#include "disk.h"
#include <string.h>
#include <stdlib.h>

/* 6-and-2 translation table (DOS 3.3) */
static const u8 ENC_6_2[64] = {
    0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
    0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
    0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
    0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
    0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
    0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
    0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
    0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

static u8 DEC_6_2[256];
static int dec_ready;

static void ensure_dec(void) {
    if (dec_ready) return;
    memset(DEC_6_2, 0xFF, sizeof DEC_6_2);
    for (int i = 0; i < 64; i++)
        DEC_6_2[ENC_6_2[i]] = (u8)i;
    dec_ready = 1;
}

/* Logical (DOS/.dsk) sector → physical address-field sector (BOOT1 interleave). */
static const int DOS33_LOG_TO_PHYS[16] = {
    0x0, 0xD, 0xB, 0x9, 0x7, 0x5, 0x3, 0x1,
    0xE, 0xC, 0xA, 0x8, 0x6, 0x4, 0x2, 0xF
};

/* ProDOS-order (.po) file sector index → physical address-field sector. */
static const int PRODOS_LOG_TO_PHYS[16] = {
    0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE,
    0x1, 0x3, 0x5, 0x7, 0x9, 0xB, 0xD, 0xF
};

/* Built-in minimal Disk II boot PROM stub (not Apple firmware).
 * Overwritten by a2e_disk_install_cleanroom_boot() or user file. */
u8 a2e_disk_boot_rom_rw[256];
static const u8 *a2e_disk_boot_rom_ptr = a2e_disk_boot_rom_rw;
static u8 a2e_disk_boot_rom_default[256];

void a2e_disk_set_boot_rom(const u8 *data, size_t len) {
    memset(a2e_disk_boot_rom_rw, 0x60, sizeof a2e_disk_boot_rom_rw);
    if (data && len) {
        if (len > 256) len = 256;
        memcpy(a2e_disk_boot_rom_rw, data, len);
    }
    a2e_disk_boot_rom_ptr = a2e_disk_boot_rom_rw;
}

const u8 *a2e_disk_boot_rom_get(void) {
    return a2e_disk_boot_rom_ptr;
}

void a2e_disk_init(a2e_disk *d) {
    memset(d, 0, sizeof(*d));
    d->write_protect = true;
#ifdef A2E_REDUCED_DISK
    d->cached_track = -1;
#endif
    if (a2e_disk_boot_rom_default[0] == 0)
        memset(a2e_disk_boot_rom_rw, 0x60, sizeof a2e_disk_boot_rom_rw);
    a2e_disk_boot_rom_ptr = a2e_disk_boot_rom_rw;
}

static void write_4n4(u8 *dst, int *p, u8 val) {
    dst[(*p)++] = (u8)(0xAA | ((val >> 1) & 0x55));
    dst[(*p)++] = (u8)(0xAA | (val & 0x55));
}

static u8 read_4n4(u8 a, u8 b) {
    return (u8)(((a & 0x55) << 1) | (b & 0x55));
}

static void encode_sector(u8 *track, int *pos, int vol, int trk, int sector, const u8 data[256]);

static void encode_one_track(u8 *dst, const u8 *img, int t, const int *log_to_phys) {
    int pos = 0;
    for (int i = 0; i < 48; i++) dst[pos++] = 0xFF;
    for (int log_sec = 0; log_sec < 16; log_sec++) {
        int phys = log_to_phys[log_sec];
        const u8 *sec = img + (size_t)(t * 16 + log_sec) * 256;
        encode_sector(dst, &pos, 254, t, phys, sec);
        if (pos >= A2E_DISK_NIB_LEN - 400) break;
    }
    while (pos < A2E_DISK_NIB_LEN) dst[pos++] = 0xFF;
}

static const int *order_table(int po) {
    return po ? PRODOS_LOG_TO_PHYS : DOS33_LOG_TO_PHYS;
}

static int load_nibblized(a2e_disk *d, const u8 *img, size_t len, int po) {
    if (!img || len < 35 * 16 * 256) return -1;
    d->img = img;
    d->img_len = len;
    d->img_po = po;
    const int *map = order_table(po);
#ifdef A2E_REDUCED_DISK
    encode_one_track(d->nib_cache, img, 0, map);
    d->cached_track = 0;
#else
    memset(d->nib, 0xFF, sizeof d->nib);
    for (int t = 0; t < 35; t++)
        encode_one_track(d->nib[t], img, t, map);
#endif
    d->loaded = true;
    d->track = 0;
    d->half_track = 0;
    d->nib_pos = 0;
    d->bit_pos = 0;
    d->latch_ready = false;
    d->write_protect = false;
    return 0;
}

int a2e_disk_load_dsk(a2e_disk *d, const u8 *dsk, size_t len) {
    return load_nibblized(d, dsk, len, 0);
}

int a2e_disk_load_po(a2e_disk *d, const u8 *po, size_t len) {
    return load_nibblized(d, po, len, 1);
}

u8 *a2e_disk_track_nib(a2e_disk *d, int track) {
    if (track < 0) track = 0;
    if (track > 34) track = 34;
#ifdef A2E_REDUCED_DISK
    if (!d->img || d->img_len < 35 * 16 * 256) return d->nib_cache;
    if (d->cached_track != track) {
        encode_one_track(d->nib_cache, d->img, track, order_table(d->img_po));
        d->cached_track = track;
        d->nib_pos = 0;
        d->latch_ready = false;
        d->nib_timer = 0;
    }
    return d->nib_cache;
#else
    return d->nib[track];
#endif
}

static void encode_sector(u8 *track, int *pos, int vol, int trk, int sector, const u8 data[256]) {
    track[(*pos)++] = 0xD5;
    track[(*pos)++] = 0xAA;
    track[(*pos)++] = 0x96;
    write_4n4(track, pos, (u8)vol);
    write_4n4(track, pos, (u8)trk);
    write_4n4(track, pos, (u8)sector);
    write_4n4(track, pos, (u8)(vol ^ trk ^ sector));
    track[(*pos)++] = 0xDE;
    track[(*pos)++] = 0xAA;
    track[(*pos)++] = 0xEB;

    for (int i = 0; i < 6; i++) track[(*pos)++] = 0xFF;

    track[(*pos)++] = 0xD5;
    track[(*pos)++] = 0xAA;
    track[(*pos)++] = 0xAD;

    {
        u8 primary[256], secondary[86];
        memset(secondary, 0, sizeof secondary);
        for (int i = 0; i < 84; i++) {
            u8 d0 = data[i];
            u8 d1 = data[i + 86];
            u8 d2 = data[i + 172];
            secondary[i] = (u8)(
                ((d0 & 1) << 1) | ((d0 & 2) >> 1) |
                ((((d1 & 1) << 1) | ((d1 & 2) >> 1)) << 2) |
                ((((d2 & 1) << 1) | ((d2 & 2) >> 1)) << 4)
            );
            primary[i] = (u8)(d0 >> 2);
            primary[i + 86] = (u8)(d1 >> 2);
            primary[i + 172] = (u8)(d2 >> 2);
        }
        for (int i = 84; i < 86; i++) {
            u8 d0 = data[i];
            u8 d1 = data[i + 86];
            secondary[i] = (u8)(
                ((d0 & 1) << 1) | ((d0 & 2) >> 1) |
                ((((d1 & 1) << 1) | ((d1 & 2) >> 1)) << 2)
            );
            primary[i] = (u8)(d0 >> 2);
            primary[i + 86] = (u8)(d1 >> 2);
        }

        u8 chk = 0;
        /* Secondary then primary, ascending — matches Disk II P5 / AppleWin order */
        for (int i = 0; i < 86; i++) {
            u8 n = (u8)(secondary[i] ^ chk);
            track[(*pos)++] = ENC_6_2[n & 0x3F];
            chk = secondary[i];
        }
        for (int i = 0; i < 256; i++) {
            u8 n = (u8)(primary[i] ^ chk);
            track[(*pos)++] = ENC_6_2[n & 0x3F];
            chk = primary[i];
        }
        track[(*pos)++] = ENC_6_2[chk & 0x3F];
    }

    track[(*pos)++] = 0xDE;
    track[(*pos)++] = 0xAA;
    track[(*pos)++] = 0xEB;

    for (int i = 0; i < 14; i++) track[(*pos)++] = 0xFF;
}

static int decode_data_field(const u8 *nib, int len, int pos, u8 out[256]) {
    ensure_dec();
    if (pos + 3 + 343 > len) return -1;
    if (nib[pos] != 0xD5 || nib[pos + 1] != 0xAA || nib[pos + 2] != 0xAD)
        return -1;
    pos += 3;
    u8 secondary[86], primary[256];
    u8 chk = 0;
    u8 last = 0;
    for (int i = 0; i < 86; i++) {
        u8 enc = nib[pos++];
        u8 val = DEC_6_2[enc];
        if (val == 0xFF) return -1;
        last = (u8)(chk ^ val);
        secondary[i] = last;
        chk = last;
    }
    for (int i = 0; i < 256; i++) {
        u8 enc = nib[pos++];
        u8 val = DEC_6_2[enc];
        if (val == 0xFF) return -1;
        last = (u8)(chk ^ val);
        primary[i] = last;
        chk = last;
    }
    u8 enc = nib[pos++];
    u8 val = DEC_6_2[enc];
    if (val == 0xFF) return -1;
    if ((u8)(chk ^ val) != 0) {
        /* checksum nibble should xor to 0 against running chk — accept if matches chk */
        if (val != chk) return -1;
    }

    for (int i = 0; i < 256; i++)
        out[i] = (u8)(primary[i] << 2);
    for (int i = 0; i < 84; i++) {
        u8 s = secondary[i];
        out[i] |= (u8)(((s >> 1) & 1) | ((s & 1) << 1));
        out[i + 86] |= (u8)((((s >> 3) & 1) | (((s >> 2) & 1) << 1)));
        out[i + 172] |= (u8)((((s >> 5) & 1) | (((s >> 4) & 1) << 1)));
    }
    for (int i = 84; i < 86; i++) {
        u8 s = secondary[i];
        out[i] |= (u8)(((s >> 1) & 1) | ((s & 1) << 1));
        out[i + 86] |= (u8)((((s >> 3) & 1) | (((s >> 2) & 1) << 1)));
    }
    return 0;
}

int a2e_disk_decode_sector(const u8 *track, int track_len, int want_track, int want_phys_sec, u8 out[256]) {
    for (int i = 0; i < track_len - 20; i++) {
        if (track[i] != 0xD5 || track[i + 1] != 0xAA || track[i + 2] != 0x96)
            continue;
        u8 vol = read_4n4(track[i + 3], track[i + 4]);
        u8 trk = read_4n4(track[i + 5], track[i + 6]);
        u8 sec = read_4n4(track[i + 7], track[i + 8]);
        (void)vol;
        if (trk != (u8)want_track || sec != (u8)want_phys_sec)
            continue;
        /* find following data field */
        for (int j = i + 12; j < track_len - 350 && j < i + 80; j++) {
            if (track[j] == 0xD5 && track[j + 1] == 0xAA && track[j + 2] == 0xAD) {
                if (decode_data_field(track, track_len, j, out) == 0)
                    return 0;
            }
        }
    }
    return -1;
}

void a2e_disk_tick(a2e_disk *d) {
    if (!d->motor_on || !d->loaded) return;
    d->spin_cycle++;
    /*
     * Hold a ready nibble until Q6L is read so LDA abs,X's leading ticks
     * cannot overwrite it (required for Disk II P5).
     */
    if (d->latch_ready)
        return;
    d->nib_timer++;
    if (d->nib_timer >= 32) {
        d->nib_timer = 0;
        d->nib_pos++;
        if (d->nib_pos >= A2E_DISK_NIB_LEN) d->nib_pos = 0;
        if (!d->write_mode) {
            u8 *trk = a2e_disk_track_nib(d, d->track);
            d->latch = trk[d->nib_pos];
            d->latch_ready = true;
            d->hold_age = 0;
        }
    }
}

static void step_phase(a2e_disk *d, int phase_on) {
    int delta = (phase_on - d->phase) & 3;
    if (delta == 1) {
        d->phase = phase_on;
        if (d->half_track < 68) d->half_track++;
    } else if (delta == 3) {
        d->phase = phase_on;
        if (d->half_track > 0) d->half_track--;
    } else {
        d->phase = phase_on;
    }
    int prev = d->track;
    d->track = d->half_track / 2;
    if (d->track > 34) d->track = 34;
    /* Changing tracks invalidates the current shift-register byte. */
    d->latch_ready = false;
    d->nib_timer = 0;
    if (d->track != prev)
        (void)a2e_disk_track_nib(d, d->track);
}

u8 a2e_disk_softswitch_read(a2e_disk *d, u16 addr) {
    int n = addr & 0xF;
    switch (n) {
    case 0x0: case 0x2: case 0x4: case 0x6:
        /* Phase off — no stepper motion (only energizing a phase steps). */
        return 0;
    case 0x1: case 0x3: case 0x5: case 0x7:
        step_phase(d, n >> 1);
        return 0;
    case 0x8: d->motor_on = false; return 0;
    case 0x9: d->motor_on = true; return 0;
    case 0xA: d->drive = 0; return 0;
    case 0xB: d->drive = 1; return 0;
    case 0xC: /* Q6L read data */
        d->q6 = 0;
        if (d->write_mode)
            return 0;
        if (d->latch_ready) {
            d->latch_ready = false;
            d->nib_timer = 0;
            d->hold_age = 0;
            return d->latch; /* nibbles already have bit7 set when valid */
        }
        return (u8)(d->latch & 0x7F);
    case 0xD: /* Q6H */
        d->q6 = 1;
        return (u8)(d->write_protect ? 0x80 : 0x00);
    case 0xE: /* Q7L */
        d->q7 = 0;
        d->write_mode = false;
        return d->latch_ready ? (u8)(d->latch | 0x80) : (u8)(d->latch & 0x7F);
    case 0xF: /* Q7H */
        d->q7 = 1;
        d->write_mode = true;
        return 0;
    }
    return 0;
}

void a2e_disk_softswitch_write(a2e_disk *d, u16 addr, u8 val) {
    (void)val;
    (void)a2e_disk_softswitch_read(d, addr);
}

static u8 ss_wait_nibble(a2e_disk *d, int *budget) {
    while (*budget > 0) {
        a2e_disk_tick(d);
        (*budget)--;
        u8 v = a2e_disk_softswitch_read(d, 0xC0EC);
        if (v & 0x80)
            return v;
    }
    return 0;
}

int a2e_disk_softswitch_read_sector(a2e_disk *d, int want_track, int want_phys_sec,
                                    u8 out[256], int max_ticks) {
    ensure_dec();
    int budget = max_ticks;
    /* Ensure read mode */
    a2e_disk_softswitch_read(d, 0xC0EE);
    a2e_disk_softswitch_read(d, 0xC0EC);

    while (budget > 0) {
        /* Find address prolog D5 AA 96 */
        u8 a = ss_wait_nibble(d, &budget);
        if (!(a & 0x80) || a != 0xD5) continue;
        u8 b = ss_wait_nibble(d, &budget);
        if (b != 0xAA) continue;
        u8 c = ss_wait_nibble(d, &budget);
        if (c != 0x96) continue;

        u8 v0 = ss_wait_nibble(d, &budget);
        u8 v1 = ss_wait_nibble(d, &budget);
        u8 t0 = ss_wait_nibble(d, &budget);
        u8 t1 = ss_wait_nibble(d, &budget);
        u8 s0 = ss_wait_nibble(d, &budget);
        u8 s1 = ss_wait_nibble(d, &budget);
        (void)ss_wait_nibble(d, &budget);
        (void)ss_wait_nibble(d, &budget);
        u8 vol = read_4n4(v0, v1);
        u8 trk = read_4n4(t0, t1);
        u8 sec = read_4n4(s0, s1);
        (void)vol;
        if (trk != (u8)want_track || sec != (u8)want_phys_sec)
            continue;

        /* Find data prolog */
        for (;;) {
            if (budget <= 0) return -1;
            u8 x = ss_wait_nibble(d, &budget);
            if (x != 0xD5) continue;
            u8 y = ss_wait_nibble(d, &budget);
            if (y != 0xAA) continue;
            u8 z = ss_wait_nibble(d, &budget);
            if (z != 0xAD) continue;
            break;
        }

        u8 secondary[86], primary[256];
        u8 chk = 0;
        for (int i = 0; i < 86; i++) {
            u8 enc = ss_wait_nibble(d, &budget);
            u8 val = DEC_6_2[enc];
            if (val == 0xFF) return -1;
            secondary[i] = (u8)(chk ^ val);
            chk = secondary[i];
        }
        for (int i = 0; i < 256; i++) {
            u8 enc = ss_wait_nibble(d, &budget);
            u8 val = DEC_6_2[enc];
            if (val == 0xFF) return -1;
            primary[i] = (u8)(chk ^ val);
            chk = primary[i];
        }
        u8 cenc = ss_wait_nibble(d, &budget);
        u8 cval = DEC_6_2[cenc];
        if (cval == 0xFF || cval != chk) return -1;

        for (int i = 0; i < 256; i++)
            out[i] = (u8)(primary[i] << 2);
        for (int i = 0; i < 84; i++) {
            u8 s = secondary[i];
            out[i] |= (u8)(((s >> 1) & 1) | ((s & 1) << 1));
            out[i + 86] |= (u8)((((s >> 3) & 1) | (((s >> 2) & 1) << 1)));
            out[i + 172] |= (u8)((((s >> 5) & 1) | (((s >> 4) & 1) << 1)));
        }
        for (int i = 84; i < 86; i++) {
            u8 s = secondary[i];
            out[i] |= (u8)(((s >> 1) & 1) | ((s & 1) << 1));
            out[i + 86] |= (u8)((((s >> 3) & 1) | (((s >> 2) & 1) << 1)));
        }
        return 0;
    }
    return -1;
}
