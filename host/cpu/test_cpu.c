#include "test_assert.h"
#include "../../core/cpu.h"
#include "../../core/bus.h"
#include <string.h>

static u8 mem[65536];
static u64 ticks;

static u8 test_read(void *ctx, u16 addr) {
    (void)ctx;
    return mem[addr];
}
static void test_write(void *ctx, u16 addr, u8 val) {
    (void)ctx;
    mem[addr] = val;
}
static void test_tick(void *ctx) {
    (void)ctx;
    ticks++;
}

static void load_prog(u16 addr, const u8 *bytes, size_t n) {
    memcpy(mem + addr, bytes, n);
}

static void test_lda_imm(void) {
    memset(mem, 0, sizeof mem);
    ticks = 0;
    a2e_bus bus = { .ctx = NULL, .read = test_read, .write = test_write, .tick = test_tick };
    a2e_cpu cpu;
    a2e_cpu_init(&cpu, &bus, true);
    u8 prog[] = { 0xA9, 0x42, 0x85, 0x00, 0x00 };
    load_prog(0x8000, prog, sizeof prog);
    mem[0xFFFC] = 0x00; mem[0xFFFD] = 0x80;
    mem[0xFFFE] = 0x00; mem[0xFFFF] = 0x80;
    a2e_cpu_reset(&cpu);
    a2e_cpu_step(&cpu);
    ASSERT_EQ_INT(0x42, cpu.a);
    ASSERT_TRUE(!(cpu.p & A2E_FLAG_Z));
    a2e_cpu_step(&cpu);
    ASSERT_EQ_INT(0x42, mem[0x00]);
}

static void test_adc_flags(void) {
    memset(mem, 0, sizeof mem);
    a2e_bus bus = { .ctx = NULL, .read = test_read, .write = test_write, .tick = test_tick };
    a2e_cpu cpu;
    a2e_cpu_init(&cpu, &bus, true);
    u8 prog[] = { 0xA9, 0xFF, 0x69, 0x01, 0x00 };
    load_prog(0x0200, prog, sizeof prog);
    mem[0xFFFC] = 0x00; mem[0xFFFD] = 0x02;
    mem[0xFFFE] = 0x00; mem[0xFFFF] = 0x02;
    a2e_cpu_reset(&cpu);
    a2e_cpu_step(&cpu);
    a2e_cpu_step(&cpu);
    ASSERT_EQ_INT(0, cpu.a);
    ASSERT_TRUE(cpu.p & A2E_FLAG_C);
    ASSERT_TRUE(cpu.p & A2E_FLAG_Z);
}

static void test_cycles_lda(void) {
    memset(mem, 0, sizeof mem);
    a2e_bus bus = { .ctx = NULL, .read = test_read, .write = test_write, .tick = test_tick };
    a2e_cpu cpu;
    a2e_cpu_init(&cpu, &bus, true);
    u8 prog[] = { 0xA9, 0x00 };
    load_prog(0x1000, prog, sizeof prog);
    mem[0xFFFC] = 0x00; mem[0xFFFD] = 0x10;
    a2e_cpu_reset(&cpu);
    u64 before = cpu.cycles;
    int c = a2e_cpu_step(&cpu);
    ASSERT_EQ_INT(2, c);
    ASSERT_EQ_INT(2, (int)(cpu.cycles - before));
}

static void test_lda_abs_x_bpl_loop(void) {
    /* Disk-poll shaped loop: LDA $C0EC,X / BPL *-5  with X=0 */
    memset(mem, 0, sizeof mem);
    a2e_bus bus = { .ctx = NULL, .read = test_read, .write = test_write, .tick = test_tick };
    a2e_cpu cpu;
    a2e_cpu_init(&cpu, &bus, true);

    /* At $1000:
     * A9 00      LDA #0
     * AA         TAX
     * BD EC C0   LDA $C0EC,X
     * 10 FB      BPL *-5   (branch to LDA abs,X)
     * 00         BRK
     * Make $C0EC return 0x80 on first abs,X read after a few tries via counter
     */
    u8 prog[] = {
        0xA9, 0x00,
        0xAA,
        0xBD, 0xEC, 0xC0,
        0x10, 0xFB,
        0x00
    };
    load_prog(0x1000, prog, sizeof prog);
    mem[0xFFFC] = 0x00; mem[0xFFFD] = 0x10;
    mem[0xFFFE] = 0x00; mem[0xFFFF] = 0x10;

    /* Floating bus at C0EC: start clear, set ready after PC enters loop */
    mem[0xC0EC] = 0x00;
    a2e_cpu_reset(&cpu);
    a2e_cpu_step(&cpu); /* LDA #0 */
    a2e_cpu_step(&cpu); /* TAX */
    /* First LDA abs,X sees 0 → BPL taken */
    a2e_cpu_step(&cpu);
    ASSERT_TRUE(!(cpu.a & 0x80));
    a2e_cpu_step(&cpu); /* BPL taken back */
    mem[0xC0EC] = 0x80;
    a2e_cpu_step(&cpu); /* LDA sees ready */
    ASSERT_EQ_INT(0x80, cpu.a);
    a2e_cpu_step(&cpu); /* BPL not taken */
    ASSERT_TRUE(cpu.pc == 0x1008 || cpu.pc == 0x1009);
}

int main(void) {
    test_lda_imm();
    test_adc_flags();
    test_cycles_lda();
    test_lda_abs_x_bpl_loop();
    return test_report();
}
