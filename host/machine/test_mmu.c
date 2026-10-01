#include "test_assert.h"
#include "../../core/machine.h"
#include <string.h>

static void test_mmu_ram(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    a2e_mmu_write(&m.mmu, 0x1234, 0xAB);
    ASSERT_EQ_INT(0xAB, a2e_mmu_read(&m.mmu, 0x1234));
    a2e_mmu_write(&m.mmu, 0xC005, 0); /* RAMWRT on */
    a2e_mmu_write(&m.mmu, 0x2000, 0x55);
    a2e_mmu_write(&m.mmu, 0xC004, 0); /* RAMWRT off */
    ASSERT_EQ_INT(0x55, m.aux_ram[0x2000]);
    a2e_mmu_write(&m.mmu, 0xC003, 0); /* RAMRD on */
    ASSERT_EQ_INT(0x55, a2e_mmu_read(&m.mmu, 0x2000));
}

static void test_prodos_banking(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    a2e_mmu_write(&m.mmu, 0x100, 0x11);
    a2e_mmu_write(&m.mmu, 0xC009, 0); /* ALTZP on */
    a2e_mmu_write(&m.mmu, 0x100, 0x22);
    ASSERT_EQ_INT(0x22, m.aux_ram[0x100]);
    ASSERT_EQ_INT(0x11, m.main_ram[0x100]);
    a2e_mmu_write(&m.mmu, 0xC008, 0); /* ALTZP off */
    ASSERT_EQ_INT(0x11, a2e_mmu_read(&m.mmu, 0x100));
    (void)a2e_mmu_read(&m.mmu, 0xC083);
    (void)a2e_mmu_read(&m.mmu, 0xC083);
    ASSERT_TRUE(m.mmu.lcwrite);
    ASSERT_TRUE(m.mmu.lcram);
    a2e_mmu_write(&m.mmu, 0xE000, 0xA5);
    ASSERT_EQ_INT(0xA5, m.main_ram[0xE000]);
}

/* ProDOS uses both LC $D000 banks; they must not alias. */
static void test_lc_bank1_bank2_isolated(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);

    /* Bank 2 write-enable: C083 twice */
    (void)a2e_mmu_read(&m.mmu, 0xC083);
    (void)a2e_mmu_read(&m.mmu, 0xC083);
    ASSERT_TRUE(m.mmu.lcram2);
    ASSERT_TRUE(m.mmu.lcwrite);
    a2e_mmu_write(&m.mmu, 0xD000, 0xB2);
    ASSERT_EQ_INT(0xB2, m.main_ram[0xC000]); /* bank2 stored at $C000 image */

    /* Bank 1 write-enable: C08B twice */
    (void)a2e_mmu_read(&m.mmu, 0xC08B);
    (void)a2e_mmu_read(&m.mmu, 0xC08B);
    ASSERT_TRUE(!m.mmu.lcram2);
    ASSERT_TRUE(m.mmu.lcwrite);
    a2e_mmu_write(&m.mmu, 0xD000, 0xB1);
    ASSERT_EQ_INT(0xB1, m.main_ram[0xD000]);
    ASSERT_EQ_INT(0xB2, m.main_ram[0xC000]); /* bank2 untouched */

    /* Read bank2 back */
    (void)a2e_mmu_read(&m.mmu, 0xC083);
    ASSERT_TRUE(m.mmu.lcram);
    ASSERT_TRUE(m.mmu.lcram2);
    ASSERT_EQ_INT(0xB2, a2e_mmu_read(&m.mmu, 0xD000));

    /* Read bank1 */
    (void)a2e_mmu_read(&m.mmu, 0xC08B);
    ASSERT_TRUE(m.mmu.lcram);
    ASSERT_TRUE(!m.mmu.lcram2);
    ASSERT_EQ_INT(0xB1, a2e_mmu_read(&m.mmu, 0xD000));
}

static void test_lc_prewrite_needs_two_odds(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    ASSERT_TRUE(!m.mmu.lcwrite);
    (void)a2e_mmu_read(&m.mmu, 0xC081);
    ASSERT_TRUE(!m.mmu.lcwrite);
    (void)a2e_mmu_read(&m.mmu, 0xC080); /* even clears prewrite */
    (void)a2e_mmu_read(&m.mmu, 0xC081);
    ASSERT_TRUE(!m.mmu.lcwrite);
    (void)a2e_mmu_read(&m.mmu, 0xC081);
    ASSERT_TRUE(m.mmu.lcwrite);
}

static void test_machine_reset_vector(void) {
    a2e_machine m;
    a2e_machine_init(&m, NULL);
    memset(m.rom, 0xEA, sizeof m.rom);
    m.rom[0xFFFC - 0xC000] = 0x00;
    m.rom[0xFFFD - 0xC000] = 0x80;
    m.main_ram[0x8000] = 0xA9;
    m.main_ram[0x8001] = 0x01;
    m.main_ram[0x8002] = 0x4C;
    m.main_ram[0x8003] = 0x02;
    m.main_ram[0x8004] = 0x80;
    a2e_machine_reset(&m);
    a2e_machine_run_cycles(&m, 50);
    ASSERT_EQ_INT(0x01, m.cpu.a);
    ASSERT_TRUE(m.cpu.pc == 0x8002 || m.cpu.pc == 0x8003 || m.cpu.pc == 0x8004);
}

int main(void) {
    test_mmu_ram();
    test_prodos_banking();
    test_lc_bank1_bank2_isolated();
    test_lc_prewrite_needs_two_odds();
    test_machine_reset_vector();
    return test_report();
}
