#include "test_assert.h"
#include "../../core/machine.h"
#include "../../core/disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Optional ProDOS cold-boot smoke test.
 * Requires fixtures from: ./tools/fetch_asimov.sh  (gitignored)
 * Skips (exit 0) when files are missing so default `make test` stays green.
 *
 * Success: ProDOS MLI vector at $BF00 is JMP ($4C), or printable text page.
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

static int ends_with_ci(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    if (n < m) return 0;
    for (size_t i = 0; i < m; i++) {
        char a = s[n - m + i], b = suf[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int text_page_has_printable(const a2e_machine *m) {
    int count = 0;
    for (int a = 0x400; a < 0x800; a++) {
        u8 c = m->main_ram[a] & 0x7F;
        if (c >= 0x20 && c < 0x7F)
            count++;
    }
    return count >= 16;
}

int main(void) {
    const char *rom_paths[] = {
        "fixtures/asimov/apple2e.rom",
        "../../fixtures/asimov/apple2e.rom",
        "roms/apple2e.rom",
        "../../roms/apple2e.rom",
        NULL
    };
    const char *boot_paths[] = {
        "fixtures/asimov/diskii_boot.bin",
        "../../fixtures/asimov/diskii_boot.bin",
        "roms/diskii_boot.bin",
        "../../roms/diskii_boot.bin",
        NULL
    };
    const char *disk_paths[] = {
        "fixtures/asimov/prodos.dsk",
        "../../fixtures/asimov/prodos.dsk",
        "fixtures/asimov/prodos.po",
        "../../fixtures/asimov/prodos.po",
        "disks/prodos.dsk",
        "../../disks/prodos.dsk",
        "disks/prodos.po",
        "../../disks/prodos.po",
        NULL
    };

    const char *rom_path = pick_existing(rom_paths);
    const char *boot_path = pick_existing(boot_paths);
    const char *disk_path = pick_existing(disk_paths);

    if (!rom_path || !boot_path || !disk_path) {
        printf("SKIP: ProDOS boot fixtures missing (run: make fetch-asimov)\n");
        printf("  rom=%s boot=%s disk=%s\n",
               rom_path ? rom_path : "(none)",
               boot_path ? boot_path : "(none)",
               disk_path ? disk_path : "(none)");
        return 0;
    }

    printf("Using ROM  %s\n", rom_path);
    printf("Using boot %s\n", boot_path);
    printf("Using disk %s\n", disk_path);

    size_t rom_len = 0, boot_len = 0, dsk_len = 0;
    u8 *rom = load_file(rom_path, &rom_len);
    u8 *boot = load_file(boot_path, &boot_len);
    u8 *dsk = load_file(disk_path, &dsk_len);
    ASSERT_TRUE(rom != NULL);
    ASSERT_TRUE(boot != NULL);
    ASSERT_TRUE(dsk != NULL);

    a2e_machine m;
    a2e_machine_init(&m, NULL);
    ASSERT_EQ_INT(0, a2e_machine_load_rom(&m, rom, rom_len));
    a2e_disk_set_boot_rom(boot, boot_len);
    if (ends_with_ci(disk_path, ".po"))
        ASSERT_EQ_INT(0, a2e_machine_load_disk_po(&m, dsk, dsk_len));
    else
        ASSERT_EQ_INT(0, a2e_machine_load_disk(&m, dsk, dsk_len));
    free(rom);
    free(boot);
    free(dsk);

    m.mmu.intcxrom = false;
    a2e_machine_reset(&m);

    {
        u8 b0 = a2e_disk_boot_rom_get()[0];
        printf("P5[0]=%02X bus C600=%02X reset PC=%04X\n",
               b0, m.bus.read(m.bus.ctx, 0xC600), m.cpu.pc);
        ASSERT_TRUE(m.cpu.pc == 0xFA62 || m.cpu.pc == 0xC600 || m.cpu.pc > 0xC000);
    }

    /* ProDOS system load is heavier than DOS 3.3 relocator */
    a2e_machine_run_cycles(&m, 80000000ull);

    u8 mli = m.main_ram[0xBF00];
    printf("after boot: PC=%04X A=%02X cycles=%llu text_printable=%d $BF00=%02X "
           "lcr=%d lcw=%d motor=%d trk=%d\n",
           m.cpu.pc, m.cpu.a, (unsigned long long)m.cpu.cycles,
           text_page_has_printable(&m), mli,
           m.mmu.lcram, m.mmu.lcwrite, m.disk.motor_on, m.disk.track);

    int mli_ok = (mli == 0x4C);
    int text_ok = text_page_has_printable(&m);
    ASSERT_TRUE(m.cpu.cycles > 100000);

    if (!(mli_ok || text_ok)) {
        fprintf(stderr,
                "FAIL: ProDOS MLI not resident (PC=%04X $BF00=%02X trk=%d).\n",
                m.cpu.pc, mli, m.disk.track);
    }
    ASSERT_TRUE(mli_ok || text_ok);

    return test_report();
}
