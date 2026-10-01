#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "bus.h"
#include "test_vectors.h"

typedef struct {
    uint8_t mem[65536];
} bus_t;

static uint8_t mem_read(void *ctx, uint16_t addr) {
    return ((bus_t *)ctx)->mem[addr];
}
static void mem_write(void *ctx, uint16_t addr, uint8_t val) {
    ((bus_t *)ctx)->mem[addr] = val;
}
static void mem_tick(void *ctx) {
    (void)ctx;
}

static void apply_init(a2e_cpu *cpu, const tv_test_t *t) {
    uint32_t m = t->init_mask;
    if (m & TV_MASK_A) cpu->a = t->init.a;
    if (m & TV_MASK_X) cpu->x = t->init.x;
    if (m & TV_MASK_Y) cpu->y = t->init.y;
    if (m & TV_MASK_SP) cpu->sp = t->init.sp;
    if (m & TV_MASK_P) cpu->p = t->init.p;
    if (m & TV_MASK_PC) cpu->pc = t->init.pc;
}

static int check_u8(const char *name, const char *field, uint8_t exp, uint8_t got) {
    if (exp != got) {
        fprintf(stderr, "FAIL %s: expect %s=0x%02X got 0x%02X\n", name, field, exp, got);
        return 1;
    }
    return 0;
}

static int check_u16(const char *name, const char *field, uint16_t exp, uint16_t got) {
    if (exp != got) {
        fprintf(stderr, "FAIL %s: expect %s=0x%04X got 0x%04X\n", name, field, exp, got);
        return 1;
    }
    return 0;
}

static int check_flag(const char *name, const char *field, uint32_t mask,
                      uint16_t flag_bit, uint8_t p, uint16_t flag_bits) {
    int expect = (flag_bits & flag_bit) != 0;
    int got = (p & flag_bit) != 0;
    (void)mask;
    if (expect != got) {
        fprintf(stderr, "FAIL %s: expect %s=%d got %d (p=0x%02X)\n", name, field, expect,
                got, p);
        return 1;
    }
    return 0;
}

static int check_expect(const tv_test_t *t, const a2e_cpu *cpu, const bus_t *bus) {
    const char *name = t->name;
    int fail = 0;
    uint32_t m = t->expect_mask;

    if (m & TV_MASK_A) fail |= check_u8(name, "a", t->expect.a, cpu->a);
    if (m & TV_MASK_X) fail |= check_u8(name, "x", t->expect.x, cpu->x);
    if (m & TV_MASK_Y) fail |= check_u8(name, "y", t->expect.y, cpu->y);
    if (m & TV_MASK_SP) fail |= check_u8(name, "sp", t->expect.sp, cpu->sp);
    if (m & TV_MASK_P) fail |= check_u8(name, "p", t->expect.p, cpu->p);
    if (m & TV_MASK_PC) fail |= check_u16(name, "pc", t->expect.pc, cpu->pc);

    if (m & TV_MASK_FLAG_C)
        fail |= check_flag(name, "flag.c", m, A2E_FLAG_C, cpu->p, t->expect.flag_bits);
    if (m & TV_MASK_FLAG_Z)
        fail |= check_flag(name, "flag.z", m, A2E_FLAG_Z, cpu->p, t->expect.flag_bits);
    if (m & TV_MASK_FLAG_I)
        fail |= check_flag(name, "flag.i", m, A2E_FLAG_I, cpu->p, t->expect.flag_bits);
    if (m & TV_MASK_FLAG_D)
        fail |= check_flag(name, "flag.d", m, A2E_FLAG_D, cpu->p, t->expect.flag_bits);
    if (m & TV_MASK_FLAG_V)
        fail |= check_flag(name, "flag.v", m, A2E_FLAG_V, cpu->p, t->expect.flag_bits);
    if (m & TV_MASK_FLAG_N)
        fail |= check_flag(name, "flag.n", m, A2E_FLAG_N, cpu->p, t->expect.flag_bits);

    if (m & TV_MASK_HALTED) {
        if (!cpu->stopped) {
            fprintf(stderr, "FAIL %s: expect halted\n", name);
            fail = 1;
        }
    }

    for (uint16_t i = 0; i < t->expect_mem_count; i++) {
        uint16_t addr = t->expect_mem[i].addr;
        uint8_t exp = t->expect_mem[i].val;
        uint8_t got = bus->mem[addr];
        if (exp != got) {
            fprintf(stderr, "FAIL %s: expect_mem[$%04X]=0x%02X got 0x%02X\n", name, addr,
                    exp, got);
            fail = 1;
        }
    }
    return fail;
}

static int run_one_test(const tv_file_t *file, const tv_test_t *t) {
    bus_t bus;
    memset(&bus, 0, sizeof bus);

    if (t->code_off + t->code_len > file->code_blob_size) {
        fprintf(stderr, "FAIL %s: invalid code slice\n", t->name);
        return 1;
    }

    a2e_bus bus_ops = {
        .ctx = &bus,
        .read = mem_read,
        .write = mem_write,
        .tick = mem_tick,
    };
    a2e_cpu cpu;
    a2e_cpu_init(&cpu, &bus_ops, t->isa == TV_ISA_65C02);

    /* Defaults before @init overlays */
    cpu.a = 0;
    cpu.x = 0;
    cpu.y = 0;
    cpu.sp = 0xFD;
    cpu.p = (uint8_t)(A2E_FLAG_U | A2E_FLAG_I);
    apply_init(&cpu, t);

    memcpy(&bus.mem[t->start_pc], file->code_blob + t->code_off, t->code_len);
    for (uint16_t i = 0; i < t->mem_count; i++)
        bus.mem[t->mem[i].addr] = t->mem[i].val;

    unsigned steps = 0;
    while (!cpu.stopped && steps < TV_MAX_STEPS) {
        uint16_t before = cpu.pc;
        a2e_cpu_step(&cpu);
        steps++;
        /* Self-loop (JMP *) used as NMOS halt alternative */
        if (!cpu.stopped && cpu.pc == before && !cpu.waiting)
            break;
    }

    if (!(t->expect_mask & TV_MASK_HALTED) && !cpu.stopped && steps >= TV_MAX_STEPS) {
        fprintf(stderr, "FAIL %s: timeout after %u steps (no STP)\n", t->name, TV_MAX_STEPS);
        return 1;
    }

    return check_expect(t, &cpu, &bus);
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "test_vectors.bin";
    tv_file_t file;
    if (tv_load_file(path, &file) != 0) {
        fprintf(stderr, "error: cannot load %s\n", path);
        return 1;
    }

    int passed = 0, failed = 0;
    for (uint16_t i = 0; i < file.count; i++) {
        if (run_one_test(&file, &file.tests[i]) != 0)
            failed++;
        else
            passed++;
    }
    printf("%d passed, %d failed\n", passed, failed);
    tv_free_file(&file);
    return failed ? 1 : 0;
}
