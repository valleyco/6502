#include "test_assert.h"
#include "../../core/machine.h"
#include "../../core/disk.h"
#include <stdlib.h>
#include <string.h>

/* Existing stage-0 C-path boot */
static void test_boot_t0s0_via_c_decode(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);

    u8 *dsk = calloc(35 * 16 * 256, 1);
    ASSERT_TRUE(dsk != NULL);
    u8 prog[] = { 0xA9, 0x42, 0x85, 0x00, 0x4C, 0x04, 0x08 };
    memcpy(dsk, prog, sizeof prog);
    ASSERT_EQ_INT(0, a2e_machine_load_disk(&m, dsk, 35 * 16 * 256));
    free(dsk);

    m.disk.motor_on = true;
    u8 sector[256];
    ASSERT_EQ_INT(0, a2e_disk_softswitch_read_sector(&m.disk, 0, 0, sector, 500000));
    memcpy(m.main_ram + 0x800, sector, 256);

    memset(m.rom, 0xEA, sizeof m.rom);
    m.rom[0xFFFC - 0xC000] = 0x00;
    m.rom[0xFFFD - 0xC000] = 0x08;
    a2e_machine_reset(&m);
    a2e_machine_run_cycles(&m, 200);
    ASSERT_EQ_INT(0x42, m.main_ram[0x00]);
}

/* Clean-room slot-6 PROM trampoline → page-3 loader → T0S0 at $0801 */
static void test_boot_via_cleanroom_prom(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    a2e_machine_install_cleanroom_boot(&m);

    u8 *dsk = calloc(35 * 16 * 256, 1);
    ASSERT_TRUE(dsk != NULL);
    /* $0800 = unused count byte; entry $0801 */
    u8 prog[256];
    memset(prog, 0xEA, sizeof prog);
    prog[0] = 0x01;                 /* sectors-to-read style nop */
    prog[1] = 0xA9; prog[2] = 0x42; /* LDA #$42 */
    prog[3] = 0x85; prog[4] = 0x00; /* STA $00 */
    prog[5] = 0x4C; prog[6] = 0x05; prog[7] = 0x08; /* JMP $0805 */
    memcpy(dsk, prog, 256);
    ASSERT_EQ_INT(0, a2e_machine_load_disk(&m, dsk, 35 * 16 * 256));
    free(dsk);

    /* Slot ROM visible; reset → $C600 */
    m.mmu.intcxrom = false;
    memset(m.rom, 0xEA, sizeof m.rom);
    m.rom[0xFFFC - 0xC000] = 0x00;
    m.rom[0xFFFD - 0xC000] = 0xC6;

    a2e_machine_reset(&m);
    a2e_machine_run_cycles(&m, 5000000);

    ASSERT_EQ_INT(0x42, m.main_ram[0x00]);
    ASSERT_EQ_INT(0x42, m.cpu.a);
}

int main(void) {
    test_boot_t0s0_via_c_decode();
    test_boot_via_cleanroom_prom();
    return test_report();
}
