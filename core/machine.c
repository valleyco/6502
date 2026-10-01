#include "machine.h"
#include <string.h>

static u8 mach_read(void *ctx, u16 addr) {
    a2e_machine *m = ctx;
    /* Slot 6 Disk II PROM when internal CX ROM is disabled */
    if (!m->mmu.intcxrom && addr >= 0xC600 && addr < 0xC700) {
        const u8 *rom = a2e_disk_boot_rom_get();
        return rom[addr - 0xC600];
    }
    return a2e_mmu_read(&m->mmu, addr);
}

static void mach_write(void *ctx, u16 addr, u8 val) {
    a2e_machine *m = ctx;
    a2e_mmu_write(&m->mmu, addr, val);
}

static void mach_tick(void *ctx) {
    a2e_machine *m = ctx;
    a2e_disk_tick(&m->disk);
}

void a2e_machine_init(a2e_machine *m, const a2e_host_ops *host) {
    memset(m, 0, sizeof(*m));
    if (host) m->host = *host;
    a2e_disk_init(&m->disk);
    a2e_video_init(&m->video);
    a2e_mmu_init(&m->mmu, m->main_ram, m->aux_ram, m->rom);
    m->mmu.disk = &m->disk;
    m->mmu.video = &m->video;
    m->mmu.mach = m;
    m->bus.ctx = m;
    m->bus.read = mach_read;
    m->bus.write = mach_write;
    m->bus.tick = mach_tick;
    a2e_cpu_init(&m->cpu, &m->bus, true);
    m->frame_cycles = 17030;
    m->running = true;
}

int a2e_machine_load_rom(a2e_machine *m, const u8 *data, size_t len) {
    if (!data) return -1;
    memset(m->rom, 0, sizeof m->rom);
    if (len == 16384) {
        memcpy(m->rom, data, 16384);
    } else if (len == 32768) {
        /*
         * Asimov apple_iie_rom.zip: two 16K banks.
         * Bank0 = monitor ($D000-$FFFF) with $C100-$CFFF padded $A0.
         * Bank1 = CX video firmware in $C100-$CFFF (needed by FBB4/DOS reloc).
         */
        memcpy(m->rom, data, 16384);
        memcpy(m->rom + 0x100, data + 16384 + 0x100, 0xF00);
    } else if (len == 12288) {
        /* $D000-$FFFF only */
        memcpy(m->rom + 0x1000, data, 12288);
    } else if (len >= 65536) {
        /* full 64K dump: take $C000-$FFFF */
        memcpy(m->rom, data + 0xC000, 16384);
    } else {
        size_t n = len < sizeof m->rom ? len : sizeof m->rom;
        memcpy(m->rom, data, n);
    }
    /*
     * Asimov IIe dumps pad $C100-$CFFF with $A0 (no CX video firmware).
     * SETVID/SETKBD end in JMP $FBB4 → JMP $C100; HOME can fall into
     * $FBB4 too. Real CX firmware rejoins at $F8B7 (STA $C006 / RTS).
     * Without it, relocator JMP $F8B4 → $FBB4 → $C100 never returns and
     * Autostart reboots the slot ROM forever.
     */
    if (m->rom[0x0100] == 0xA0) {
        /* FEAB: JMP $FBB4 → RTS (SETVID/SETKBD shared tail) */
        if (m->rom[0xFEAB - 0xC000] == 0x4C) {
            m->rom[0xFEAB - 0xC000] = 0x60;
            m->rom[0xFEAC - 0xC000] = 0xEA;
            m->rom[0xFEAD - 0xC000] = 0xEA;
        }
        /* FB53: BNE $FBB4 → RTS (HOME/TEXT init tail) */
        if (m->rom[0xFB53 - 0xC000] == 0xD0) {
            m->rom[0xFB53 - 0xC000] = 0x60;
            m->rom[0xFB54 - 0xC000] = 0xEA;
        }
        /* C100: PLP / JMP $F8B7 — FBB4 PHP'd; CX firmware must PLP then
         * rejoin monitor at $F8B7 (STA $C006 … RTS). */
        m->rom[0x0100] = 0x28;
        m->rom[0x0101] = 0x4C;
        m->rom[0x0102] = 0xB7;
        m->rom[0x0103] = 0xF8;
    }
    return 0;
}

int a2e_machine_load_disk(a2e_machine *m, const u8 *dsk, size_t len) {
    return a2e_disk_load_dsk(&m->disk, dsk, len);
}

int a2e_machine_load_disk_po(a2e_machine *m, const u8 *po, size_t len) {
    return a2e_disk_load_po(&m->disk, po, len);
}

void a2e_machine_reset(a2e_machine *m) {
    a2e_cpu_reset(&m->cpu);
}

void a2e_machine_run_cycles(a2e_machine *m, u64 cycles) {
    a2e_cpu_run(&m->cpu, cycles);
}

void a2e_machine_run_frame(a2e_machine *m) {
    a2e_cpu_run(&m->cpu, m->frame_cycles);
    if (m->host.present_rgb565_row) {
        u16 row[A2E_CYD_PANEL_W];
        for (int y = 0; y < A2E_CYD_PANEL_H; y++) {
            a2e_video_cyd_panel_row(&m->mmu, y, row);
            m->host.present_rgb565_row(m->host.ctx, y, row, A2E_CYD_PANEL_W);
        }
        m->video.dirty = false;
    } else if (m->host.present) {
        a2e_video_render(&m->video, &m->mmu);
#ifndef A2E_REDUCED_VIDEO
        m->host.present(m->host.ctx, (const u8 *)m->video.fb,
                        A2E_VIDEO_W, A2E_VIDEO_H, A2E_VIDEO_W * 3);
#endif
    }
}

void a2e_machine_key(a2e_machine *m, u8 ascii) {
    a2e_mmu_key_press(&m->mmu, ascii);
}

#include "disk_boot_rom.inc.c"

void a2e_machine_install_cleanroom_boot(a2e_machine *m) {
    a2e_disk_set_boot_rom(A2E_CLEANROOM_BOOT_TRAMPOLINE, sizeof A2E_CLEANROOM_BOOT_TRAMPOLINE);
    memcpy(m->main_ram + 0x0300, A2E_CLEANROOM_BOOT_PAGE3, sizeof A2E_CLEANROOM_BOOT_PAGE3);
    m->mmu.intcxrom = false;
}
