#include "test_assert.h"
#include "../../core/machine.h"
#include "../../core/disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Optional invaders graphic-game smoke (Step 10).
 * Requires: make fetch-asimov (pulls invaders.dsk). Skips if missing.
 */

static int file_ok(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && st.st_size > 0;
}

static u8 *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    rewind(f);
    u8 *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_len = (size_t)n;
    return buf;
}

static const char *pick_existing(const char **paths) {
    for (int i = 0; paths[i]; i++) {
        if (file_ok(paths[i]))
            return paths[i];
    }
    return NULL;
}

static void type_str(a2e_machine *m, const char *s, u64 gap) {
    for (; *s; s++) {
        u8 ch = (u8)*s;
        if (ch == '\n') ch = '\r';
        a2e_machine_key(m, ch);
        a2e_machine_run_cycles(m, gap);
    }
}

static int text_has(const a2e_machine *m, const char *needle) {
    char page[0x400 + 1];
    for (int i = 0; i < 0x400; i++) {
        u8 c = m->main_ram[0x400 + i] & 0x7F;
        page[i] = (c >= 0x20 && c < 0x7F) ? (char)c : ' ';
    }
    page[0x400] = 0;
    return strstr(page, needle) != NULL;
}

int main(void) {
    const char *rom_paths[] = {
        "fixtures/asimov/apple2e.rom", "../../fixtures/asimov/apple2e.rom",
        "roms/apple2e.rom", "../../roms/apple2e.rom", NULL
    };
    const char *boot_paths[] = {
        "fixtures/asimov/diskii_boot.bin", "../../fixtures/asimov/diskii_boot.bin",
        "roms/diskii_boot.bin", "../../roms/diskii_boot.bin", NULL
    };
    const char *disk_paths[] = {
        "fixtures/asimov/invaders.dsk", "../../fixtures/asimov/invaders.dsk",
        "disks/invaders.dsk", "../../disks/invaders.dsk", NULL
    };

    const char *rom_path = pick_existing(rom_paths);
    const char *boot_path = pick_existing(boot_paths);
    const char *disk_path = pick_existing(disk_paths);

    if (!rom_path || !boot_path || !disk_path) {
        printf("SKIP: invaders fixtures missing (run: make fetch-asimov)\n");
        return 0;
    }

    printf("Using ROM  %s\n", rom_path);
    printf("Using boot %s\n", boot_path);
    printf("Using disk %s\n", disk_path);

    size_t rom_len = 0, boot_len = 0, dsk_len = 0;
    u8 *rom = load_file(rom_path, &rom_len);
    u8 *boot = load_file(boot_path, &boot_len);
    u8 *dsk = load_file(disk_path, &dsk_len);
    ASSERT_TRUE(rom && boot && dsk);

    a2e_machine m;
    a2e_machine_init(&m, NULL);
    ASSERT_EQ_INT(0, a2e_machine_load_rom(&m, rom, rom_len));
    a2e_disk_set_boot_rom(boot, boot_len);
    ASSERT_EQ_INT(0, a2e_machine_load_disk(&m, dsk, dsk_len));
    free(rom);
    free(boot);
    free(dsk);

    m.mmu.intcxrom = false;
    a2e_machine_reset(&m);
    a2e_machine_run_cycles(&m, 60000000ull);

    printf("after boot: PC=%04X $3D0=%02X\n", m.cpu.pc, m.main_ram[0x3D0]);
    ASSERT_TRUE(m.main_ram[0x3D0] == 0x4C || text_has(&m, "APPLE"));

    /* Exit CAT menu to BASIC, run keyboard invaders */
    type_str(&m, "E", 100000);
    a2e_machine_run_cycles(&m, 8000000ull);
    type_str(&m, "RUN KEYBOARD APPLE INVADERS\r", 120000);
    a2e_machine_run_cycles(&m, 80000000ull);

    ASSERT_TRUE(text_has(&m, "INVADER") || text_has(&m, "SPACEBAR") ||
                text_has(&m, "CONTROL"));

    for (int k = 0; k < 50; k++) {
        a2e_machine_key(&m, ' ');
        a2e_machine_run_cycles(&m, 400000ull);
        if (!m.mmu.text_mode && m.mmu.hires)
            break;
    }

    printf("playfield: text=%d hires=%d PC=%04X\n",
           m.mmu.text_mode, m.mmu.hires, m.cpu.pc);
    ASSERT_TRUE(m.mmu.hires);
    ASSERT_TRUE(!m.mmu.text_mode);

    return test_report();
}
