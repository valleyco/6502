#ifndef A2E_MMU_H
#define A2E_MMU_H

#include "types.h"

/*
 * Apple IIe memory map + soft switches (D7, phase 2/4).
 * Main 64K + aux 64K; language card / bank switches for ProDOS.
 */

typedef struct a2e_disk a2e_disk;
typedef struct a2e_video a2e_video;
typedef struct a2e_machine a2e_machine;

typedef struct a2e_mmu {
    u8 *main;          /* 64K main RAM (owned by machine) */
    u8 *aux;           /* 64K aux RAM */
    u8 *rom_d000;      /* 12K ROM mapped $D000-$FFFF (actually we store 16K $C000-$FFFF style) */
    u8 *rom_c100;      /* optional CX ROM $C100-$CFFF */

    /* Soft-switch state */
    bool store80;      /* 80STORE */
    bool ramrd;        /* RAMRD: read aux $0200-$BFFF */
    bool ramwrt;       /* RAMWRT: write aux $0200-$BFFF */
    bool altzp;        /* ALTZP: aux zero page / stack / LC */
    bool intcxrom;     /* INTCXROM: internal $C100-$CFFF */
    bool slotc3rom;    /* SLOTC3ROM */
    bool page2;        /* PAGE2 */
    bool hires;        /* HIRES */
    bool text_mode;    /* TEXT (true) vs GR (false) */
    bool mixed;        /* MIXED: bottom 4 text rows over graphics */

    /* Language card */
    bool lcram;        /* read RAM $D000-$FFFF */
    bool lcram2;       /* bank 2 of $D000-$DFFF */
    bool lcwrite;      /* write-enable LC RAM */
    int  lc_prewrite;  /* need two reads to enable write */

    a2e_disk *disk;
    a2e_video *video;
    a2e_machine *mach;

    u8 kbd;            /* last key | 0x80 when pending */
    u8 strobe;
    bool speaker_level;

    /* Paddle / button (Step 10 graphic games); VBL from mach->cpu.cycles */
    u8  paddle[4];     /* 0..255 positions; default centered */
    u8  paddle_btn;    /* bits 0..2 = button pressed */
    u64 paddle_reset_cyc; /* cpu cycles at last $C070 */
} a2e_mmu;

void a2e_mmu_init(a2e_mmu *m, u8 *main, u8 *aux, u8 *rom_c000);
u8   a2e_mmu_read(a2e_mmu *m, u16 addr);
void a2e_mmu_write(a2e_mmu *m, u16 addr, u8 val);
void a2e_mmu_key_press(a2e_mmu *m, u8 ascii);
/* Paddle 0..3 (0-255) and button bitmask (bit0=PB0 pressed). */
void a2e_mmu_set_paddles(a2e_mmu *m, u8 p0, u8 p1, u8 p2, u8 p3, u8 btn_mask);

#endif /* A2E_MMU_H */
