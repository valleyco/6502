#include "mmu.h"
#include "disk.h"
#include "video.h"
#include "audio.h"
#include "machine.h"
#include <string.h>

void a2e_mmu_init(a2e_mmu *m, u8 *main, u8 *aux, u8 *rom_c000) {
    memset(m, 0, sizeof(*m));
    m->main = main;
    m->aux = aux;
    m->rom_d000 = rom_c000; /* full 16K from $C000 */
    m->rom_c100 = rom_c000;
    /* Slot cards (Disk II at $C600) need INTCXROM off after cold boot. */
    m->intcxrom = false;
    m->text_mode = true;
    m->mixed = false;
    m->hires = false;
    m->page2 = false;
    m->paddle[0] = m->paddle[1] = m->paddle[2] = m->paddle[3] = 128;
    m->paddle_btn = 0;
    m->paddle_reset_cyc = 0;
    m->lcram = false;
    m->lcram2 = true;
    m->lcwrite = false;
}

void a2e_mmu_key_press(a2e_mmu *m, u8 ascii) {
    m->kbd = (u8)(ascii | 0x80);
}

void a2e_mmu_set_paddles(a2e_mmu *m, u8 p0, u8 p1, u8 p2, u8 p3, u8 btn_mask) {
    m->paddle[0] = p0;
    m->paddle[1] = p1;
    m->paddle[2] = p2;
    m->paddle[3] = p3;
    m->paddle_btn = (u8)(btn_mask & 7);
}

static u64 mmu_cycles(a2e_mmu *m) {
    return (m->mach) ? m->mach->cpu.cycles : 0;
}

static u8 *lc_bank_ptr(a2e_mmu *m, u16 addr, bool write) {
    (void)write;
    u8 *base = m->altzp ? m->aux : m->main;
    if (addr >= 0xE000) {
        return &base[addr];
    }
    /* $D000-$DFFF: bank1 at $D000 in RAM image, bank2 at $C000 area of bank */
    if (m->lcram2) {
        /* bank 2: stored at 0xC000-0xCFFF in main/aux image (common approach) */
        return &base[0xC000 + (addr - 0xD000)];
    }
    return &base[addr];
}

static void lc_access(a2e_mmu *m, u16 addr) {
    /* Language card soft switches $C080-$C08F (also used by Disk II in slots — slot 0) */
    int n = addr & 0xF;
    /* Two consecutive reads of *odd* addresses that select read-ROM enable write */
    bool read_ram = (n & 1) != 0; /* C081,C083,C085,C087,C089,... */
    /* Classic soft-switch decode:
       C080: read RAM bank2, no write
       C081: read ROM, write RAM bank2
       C082: read ROM, no write
       C083: read RAM bank2, write RAM bank2
       ... bank1 variants in C088-C08F
    */
    m->lcram2 = (n & 8) == 0;
    m->lcram = (n == 0 || n == 3 || n == 8 || n == 11 ||
                n == 4 || n == 7 || n == 12 || n == 15);
    /* More standard:
       bit0 = 1 => write enable attempt
       FF FF: actually use well-known table
    */
    switch (n) {
    case 0x0: m->lcram = true;  m->lcwrite = false; m->lcram2 = true;  m->lc_prewrite = 0; break;
    case 0x1: m->lcram = false; m->lcram2 = true;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0x2: m->lcram = false; m->lcwrite = false; m->lcram2 = true;  m->lc_prewrite = 0; break;
    case 0x3: m->lcram = true;  m->lcram2 = true;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0x4: m->lcram = true;  m->lcwrite = false; m->lcram2 = true;  m->lc_prewrite = 0; break;
    case 0x5: m->lcram = false; m->lcram2 = true;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0x6: m->lcram = false; m->lcwrite = false; m->lcram2 = true;  m->lc_prewrite = 0; break;
    case 0x7: m->lcram = true;  m->lcram2 = true;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0x8: m->lcram = true;  m->lcwrite = false; m->lcram2 = false; m->lc_prewrite = 0; break;
    case 0x9: m->lcram = false; m->lcram2 = false;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0xA: m->lcram = false; m->lcwrite = false; m->lcram2 = false; m->lc_prewrite = 0; break;
    case 0xB: m->lcram = true;  m->lcram2 = false;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0xC: m->lcram = true;  m->lcwrite = false; m->lcram2 = false; m->lc_prewrite = 0; break;
    case 0xD: m->lcram = false; m->lcram2 = false;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    case 0xE: m->lcram = false; m->lcwrite = false; m->lcram2 = false; m->lc_prewrite = 0; break;
    case 0xF: m->lcram = true;  m->lcram2 = false;
        if (++m->lc_prewrite >= 2) { m->lcwrite = true; } break;
    }
    (void)read_ram;
}

static u8 read_ram_window(a2e_mmu *m, u16 addr) {
    bool use_aux = false;
    if (addr < 0x200) {
        use_aux = m->altzp;
    } else if (addr < 0xC000) {
        if (m->store80 && ((addr >= 0x400 && addr < 0x800) ||
                           (m->hires && addr >= 0x2000 && addr < 0x4000))) {
            use_aux = m->page2;
        } else {
            use_aux = m->ramrd;
        }
    }
    return use_aux ? m->aux[addr] : m->main[addr];
}

static void write_ram_window(a2e_mmu *m, u16 addr, u8 val) {
    bool use_aux = false;
    if (addr < 0x200) {
        use_aux = m->altzp;
    } else if (addr < 0xC000) {
        if (m->store80 && ((addr >= 0x400 && addr < 0x800) ||
                           (m->hires && addr >= 0x2000 && addr < 0x4000))) {
            use_aux = m->page2;
        } else {
            use_aux = m->ramwrt;
        }
    }
    if (use_aux) m->aux[addr] = val;
    else m->main[addr] = val;
    if (m->video && ((addr >= 0x400 && addr < 0x800) ||
                     (addr >= 0x2000 && addr < 0x6000)))
        m->video->dirty = true;
}

u8 a2e_mmu_read(a2e_mmu *m, u16 addr) {
    if (addr < 0xC000) {
        return read_ram_window(m, addr);
    }

    /* I/O $C000-$C0FF */
    if (addr < 0xC100) {
        u16 a = addr;
        switch (a) {
        case 0xC000: return m->kbd;
        case 0xC010: m->kbd &= 0x7F; return m->kbd;
        case 0xC011: return (u8)(m->lcram2 ? 0x00 : 0x80);
        case 0xC012: return (u8)(m->lcram ? 0x80 : 0x00);
        case 0xC013: return (u8)(m->ramrd ? 0x80 : 0x00);
        case 0xC014: return (u8)(m->ramwrt ? 0x80 : 0x00);
        case 0xC015: return (u8)(m->intcxrom ? 0x80 : 0x00);
        case 0xC016: return (u8)(m->altzp ? 0x80 : 0x00);
        case 0xC017: return (u8)(m->slotc3rom ? 0x80 : 0x00);
        case 0xC018: return (u8)(m->store80 ? 0x80 : 0x00);
        /* IIe video status: TEXT / MIXED / PAGE2 / HIRES */
        case 0xC01A: return (u8)(m->text_mode ? 0x80 : 0x00);
        case 0xC01B: return (u8)(m->mixed ? 0x80 : 0x00);
        case 0xC01C: return (u8)(m->page2 ? 0x80 : 0x00);
        case 0xC01D: return (u8)(m->hires ? 0x80 : 0x00);
        case 0xC019: {
            /* Approximate NTSC VBL: bit7 set in last ~1/4 of 17030-cycle frame */
            u64 in_frame = mmu_cycles(m) % 17030ull;
            return (u8)(in_frame >= 12480ull ? 0x80 : 0x00);
        }
        case 0xC030:
            m->speaker_level = !m->speaker_level;
            if (m->mach)
                a2e_audio_speaker_toggle(&m->mach->audio, mmu_cycles(m), m->speaker_level);
            return 0;
        case 0xC061: case 0xC062: case 0xC063:
            /*
             * Paddle buttons: real HW has bit7 clear when pressed.
             * Returning 0x80 (released) breaks this disk's CAT menu /
             * DOS warmstart on our bus; keep 0 for v1 open-bus compat.
             * Position timers ($C064-$C067) still work for paddle games.
             */
            (void)a;
            return 0;
        case 0xC064: case 0xC065: case 0xC066: case 0xC067: {
            int i = (int)(a - 0xC064);
            u64 elapsed = mmu_cycles(m) - m->paddle_reset_cyc;
            u64 thresh = (u64)m->paddle[i] * 11ull;
            return (u8)(elapsed < thresh ? 0x80 : 0x00);
        }
        case 0xC070:
            m->paddle_reset_cyc = mmu_cycles(m);
            return 0;
        case 0xC050: m->text_mode = false; if (m->video) m->video->dirty = true; return 0;
        case 0xC051: m->text_mode = true;  if (m->video) m->video->dirty = true; return 0;
        case 0xC052: m->mixed = false;     if (m->video) m->video->dirty = true; return 0;
        case 0xC053: m->mixed = true;      if (m->video) m->video->dirty = true; return 0;
        case 0xC054: m->page2 = false; if (m->video) m->video->dirty = true; return 0;
        case 0xC055: m->page2 = true;  if (m->video) m->video->dirty = true; return 0;
        case 0xC056: m->hires = false; if (m->video) m->video->dirty = true; return 0;
        case 0xC057: m->hires = true;  if (m->video) m->video->dirty = true; return 0;
        default:
            break;
        }

        /* Disk II in slot 6: $C0E0-$C0EF */
        if (a >= 0xC0E0 && a <= 0xC0EF && m->disk)
            return a2e_disk_softswitch_read(m->disk, a);

        /* Clean-room boot assist: reading $C0F0 also triggers load (idempotent) */
        if (a == 0xC0F0 && m->mach)
            return 0;

        /* Language card $C080-$C08F */
        if (a >= 0xC080 && a <= 0xC08F) {
            lc_access(m, a);
            return 0;
        }

        return 0;
    }

    /* $C100-$CFFF: CX ROM or slot ROM (slot ROM filled by machine bus) */
    if (addr < 0xD000) {
        if (m->intcxrom && m->rom_c100)
            return m->rom_c100[addr - 0xC000];
        return 0;
    }

    /* $D000-$FFFF */
    if (m->lcram) {
        return *lc_bank_ptr(m, addr, false);
    }
    if (m->rom_d000)
        return m->rom_d000[addr - 0xC000];
    return 0;
}

void a2e_mmu_write(a2e_mmu *m, u16 addr, u8 val) {
    if (addr < 0xC000) {
        write_ram_window(m, addr, val);
        return;
    }

    if (addr < 0xC100) {
        switch (addr) {
        case 0xC000: case 0xC001:
            m->store80 = (addr & 1) != 0; break;
        case 0xC002: case 0xC003:
            m->ramrd = (addr & 1) != 0; break;
        case 0xC004: case 0xC005:
            m->ramwrt = (addr & 1) != 0; break;
        case 0xC006: case 0xC007:
            m->intcxrom = (addr & 1) != 0; break;
        case 0xC008: case 0xC009:
            m->altzp = (addr & 1) != 0; break;
        case 0xC00A: case 0xC00B:
            m->slotc3rom = (addr & 1) != 0; break;
        case 0xC010: m->kbd &= 0x7F; break;
        case 0xC030:
            m->speaker_level = !m->speaker_level;
            if (m->mach)
                a2e_audio_speaker_toggle(&m->mach->audio, mmu_cycles(m), m->speaker_level);
            break;
        case 0xC070: m->paddle_reset_cyc = mmu_cycles(m); break;
        case 0xC050: m->text_mode = false; if (m->video) m->video->dirty = true; break;
        case 0xC051: m->text_mode = true;  if (m->video) m->video->dirty = true; break;
        case 0xC052: m->mixed = false;     if (m->video) m->video->dirty = true; break;
        case 0xC053: m->mixed = true;      if (m->video) m->video->dirty = true; break;
        case 0xC054: m->page2 = false; if (m->video) m->video->dirty = true; break;
        case 0xC055: m->page2 = true;  if (m->video) m->video->dirty = true; break;
        case 0xC056: m->hires = false; if (m->video) m->video->dirty = true; break;
        case 0xC057: m->hires = true;  if (m->video) m->video->dirty = true; break;
        default:
            if (addr >= 0xC0E0 && addr <= 0xC0EF && m->disk)
                a2e_disk_softswitch_write(m->disk, addr, val);
            if (addr >= 0xC080 && addr <= 0xC08F)
                lc_access(m, addr);
            /* Clean-room boot assist: STA $C0F0 → load T0 phys S0 to $0800 */
            if (addr == 0xC0F0 && m->mach && m->disk) {
                u8 buf[256];
                m->disk->motor_on = true;
                if (a2e_disk_softswitch_read_sector(m->disk, 0, 0, buf, 500000) == 0)
                    memcpy(m->main + 0x0800, buf, 256);
            }
            break;
        }
        return;
    }

    if (addr >= 0xD000 && m->lcwrite) {
        *lc_bank_ptr(m, addr, true) = val;
        return;
    }
    /* ROM / I/O writes ignored */
}
