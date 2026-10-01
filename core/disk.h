#ifndef A2E_DISK_H
#define A2E_DISK_H

#include "types.h"

#define A2E_DISK_TRACKS   35
#define A2E_DISK_NIB_LEN  6656   /* typical nibble track length for DOS 3.3 */

/*
 * A2E_REDUCED_DISK (CYD / D6): keep one nibblized track + raw image pointer.
 * Full profile caches all 35 tracks (~228 KiB).
 */
typedef struct a2e_disk {
#ifdef A2E_REDUCED_DISK
    u8  nib_cache[A2E_DISK_NIB_LEN];
    int cached_track;   /* -1 empty; else 0..34 matching nib_cache */
#else
    u8  nib[A2E_DISK_TRACKS][A2E_DISK_NIB_LEN];
#endif
    const u8 *img;      /* raw .dsk/.po bytes (not owned); needed for reduced */
    size_t img_len;
    int    img_po;      /* 0 = DOS order, 1 = ProDOS order */
    int track;          /* 0..34 */
    int phase;          /* stepper phase 0..3 */
    int bit_pos;        /* bit index within track */
    int nib_pos;        /* nibble byte index */
    u8  latch;          /* data register */
    bool motor_on;
    bool write_mode;
    bool write_protect;
    bool loaded;
    u64 spin_cycle;     /* cycles since motor on */
    int nib_timer;      /* cycles since last latch consume; next ready at 32 */
    int hold_age;       /* cycles held unread; overwrite after short window */
    int drive;          /* 0 or 1 */
    u8  q6, q7;         /* mode bits from soft switches */
    int half_track;     /* stepper half-track position 0..68 */
    bool latch_ready;   /* bit7 valid; cleared when data reg read */
} a2e_disk;

void a2e_disk_init(a2e_disk *d);
int  a2e_disk_load_dsk(a2e_disk *d, const u8 *dsk, size_t len);
int  a2e_disk_load_po(a2e_disk *d, const u8 *po, size_t len);
void a2e_disk_tick(a2e_disk *d);          /* one CPU cycle */
u8   a2e_disk_softswitch_read(a2e_disk *d, u16 addr);
void a2e_disk_softswitch_write(a2e_disk *d, u16 addr, u8 val);
void a2e_disk_set_boot_rom(const u8 *data, size_t len);
const u8 *a2e_disk_boot_rom_get(void);

/* Ensure track is nibblized; returns pointer to that track's nibble buffer. */
u8 *a2e_disk_track_nib(a2e_disk *d, int track);

/* Decode one DOS 3.3 sector from a nibble track image (phys sector 0..15). */
int a2e_disk_decode_sector(const u8 *track, int track_len, int want_track, int want_phys_sec, u8 out[256]);

/* Poll softswitches (motor must be on, read mode) until sector found & decoded. */
int a2e_disk_softswitch_read_sector(a2e_disk *d, int want_track, int want_phys_sec,
                                    u8 out[256], int max_ticks);

#endif /* A2E_DISK_H */
