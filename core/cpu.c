#include "cpu.h"
#include "bus.h"
#include <string.h>

static void set_zn(a2e_cpu *c, u8 v) {
    c->p = (u8)((c->p & (u8)~(A2E_FLAG_Z | A2E_FLAG_N))
                | (v == 0 ? A2E_FLAG_Z : 0)
                | (v & 0x80 ? A2E_FLAG_N : 0));
}

static u8 rd(a2e_cpu *c, u16 a) {
    a2e_bus_tick(c->bus);
    c->cycles++;
    return a2e_bus_read(c->bus, a);
}

static void wr(a2e_cpu *c, u16 a, u8 v) {
    a2e_bus_tick(c->bus);
    c->cycles++;
    a2e_bus_write(c->bus, a, v);
}

/* Dead cycle: tick + count, optional dummy read */
static void tick1(a2e_cpu *c) {
    a2e_bus_tick(c->bus);
    c->cycles++;
}

static void dummy_rd(a2e_cpu *c, u16 a) {
    (void)rd(c, a);
}

void a2e_cpu_init(a2e_cpu *cpu, a2e_bus *bus, bool is_65c02) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->bus = bus;
    cpu->is_65c02 = is_65c02;
    cpu->p = A2E_FLAG_U | A2E_FLAG_I;
    cpu->sp = 0xFD;
}

void a2e_cpu_reset(a2e_cpu *cpu) {
    cpu->sp = (u8)(cpu->sp - 3);
    cpu->p |= A2E_FLAG_I;
    cpu->stopped = false;
    cpu->waiting = false;
    u8 lo = rd(cpu, 0xFFFC);
    u8 hi = rd(cpu, 0xFFFD);
    cpu->pc = (u16)(lo | (hi << 8));
    /* Reset takes 7 cycles total; we already did 2 reads — add 5 more ticks */
    for (int i = 0; i < 5; i++) tick1(cpu);
}

static void push(a2e_cpu *c, u8 v) {
    wr(c, (u16)(0x0100 | c->sp), v);
    c->sp--;
}

static u8 pull(a2e_cpu *c) {
    c->sp++;
    return rd(c, (u16)(0x0100 | c->sp));
}

void a2e_cpu_nmi(a2e_cpu *cpu) {
    tick1(cpu);
    tick1(cpu);
    push(cpu, (u8)(cpu->pc >> 8));
    push(cpu, (u8)(cpu->pc & 0xFF));
    push(cpu, (u8)(cpu->p & (u8)~A2E_FLAG_B));
    cpu->p |= A2E_FLAG_I;
    u8 lo = rd(cpu, 0xFFFA);
    u8 hi = rd(cpu, 0xFFFB);
    cpu->pc = (u16)(lo | (hi << 8));
}

void a2e_cpu_irq(a2e_cpu *cpu) {
    if (cpu->p & A2E_FLAG_I) return;
    tick1(cpu);
    tick1(cpu);
    push(cpu, (u8)(cpu->pc >> 8));
    push(cpu, (u8)(cpu->pc & 0xFF));
    push(cpu, (u8)(cpu->p & (u8)~A2E_FLAG_B));
    cpu->p |= A2E_FLAG_I;
    if (cpu->is_65c02) cpu->p &= (u8)~A2E_FLAG_D;
    u8 lo = rd(cpu, 0xFFFE);
    u8 hi = rd(cpu, 0xFFFF);
    cpu->pc = (u16)(lo | (hi << 8));
}

/* ---- addressing helpers; each returns effective address and burns cycles ---- */

static u16 imm(a2e_cpu *c) {
    return c->pc++;
}

static u16 zp(a2e_cpu *c) {
    return rd(c, c->pc++);
}

static u16 zp_x(a2e_cpu *c) {
    u8 a = rd(c, c->pc++);
    tick1(c);
    return (u8)(a + c->x);
}

static u16 zp_y(a2e_cpu *c) {
    u8 a = rd(c, c->pc++);
    tick1(c);
    return (u8)(a + c->y);
}

static u16 abs_(a2e_cpu *c) {
    u8 lo = rd(c, c->pc++);
    u8 hi = rd(c, c->pc++);
    return (u16)(lo | (hi << 8));
}

static u16 abs_x(a2e_cpu *c, bool write_or_rmw) {
    u8 lo = rd(c, c->pc++);
    u8 hi = rd(c, c->pc++);
    u16 base = (u16)(lo | (hi << 8));
    u16 addr = (u16)(base + c->x);
    if (write_or_rmw || ((base & 0xFF00) != (addr & 0xFF00))) {
        dummy_rd(c, (u16)((hi << 8) | (u8)(lo + c->x))); /* page-cross / RMW dummy */
    }
    return addr;
}

static u16 abs_y(a2e_cpu *c, bool write_or_rmw) {
    u8 lo = rd(c, c->pc++);
    u8 hi = rd(c, c->pc++);
    u16 base = (u16)(lo | (hi << 8));
    u16 addr = (u16)(base + c->y);
    if (write_or_rmw || ((base & 0xFF00) != (addr & 0xFF00))) {
        dummy_rd(c, (u16)((hi << 8) | (u8)(lo + c->y)));
    }
    return addr;
}

static u16 ind_x(a2e_cpu *c) {
    u8 z = rd(c, c->pc++);
    tick1(c);
    u8 ptr = (u8)(z + c->x);
    u8 lo = rd(c, ptr);
    u8 hi = rd(c, (u8)(ptr + 1));
    return (u16)(lo | (hi << 8));
}

static u16 ind_y(a2e_cpu *c, bool write_or_rmw) {
    u8 z = rd(c, c->pc++);
    u8 lo = rd(c, z);
    u8 hi = rd(c, (u8)(z + 1));
    u16 base = (u16)(lo | (hi << 8));
    u16 addr = (u16)(base + c->y);
    if (write_or_rmw || ((base & 0xFF00) != (addr & 0xFF00))) {
        dummy_rd(c, (u16)((hi << 8) | (u8)(lo + c->y)));
    }
    return addr;
}

static u16 ind_zp(a2e_cpu *c) { /* 65C02 (zp) */
    u8 z = rd(c, c->pc++);
    u8 lo = rd(c, z);
    u8 hi = rd(c, (u8)(z + 1));
    return (u16)(lo | (hi << 8));
}

static u16 ind_abs(a2e_cpu *c) {
    u8 lo = rd(c, c->pc++);
    u8 hi = rd(c, c->pc++);
    u16 ptr = (u16)(lo | (hi << 8));
    if (!c->is_65c02) {
        /* 6502 JMP (abs) page-wrap bug */
        u8 a = rd(c, ptr);
        u8 b = rd(c, (u16)((ptr & 0xFF00) | (u8)(ptr + 1)));
        return (u16)(a | (b << 8));
    }
    u8 a = rd(c, ptr);
    u8 b = rd(c, (u16)(ptr + 1));
    return (u16)(a | (b << 8));
}

static u16 ind_abs_x(a2e_cpu *c) { /* 65C02 JMP (abs,X) */
    u8 lo = rd(c, c->pc++);
    u8 hi = rd(c, c->pc++);
    tick1(c);
    u16 ptr = (u16)((lo | (hi << 8)) + c->x);
    u8 a = rd(c, ptr);
    u8 b = rd(c, (u16)(ptr + 1));
    return (u16)(a | (b << 8));
}

/* ---- ALU ---- */

static void op_adc(a2e_cpu *c, u8 v) {
    if (c->p & A2E_FLAG_D) {
        /* BCD */
        u8 c0 = (c->p & A2E_FLAG_C) ? 1 : 0;
        int al = (c->a & 0x0F) + (v & 0x0F) + c0;
        int ah = (c->a >> 4) + (v >> 4);
        if (al > 9) { al -= 10; ah++; }
        u8 r = (u8)(((ah > 9) ? (ah - 10) : ah) << 4 | (al & 0x0F));
        int bin = c->a + v + c0;
        c->p = (u8)((c->p & (u8)~(A2E_FLAG_C | A2E_FLAG_Z | A2E_FLAG_V | A2E_FLAG_N))
                    | (ah > 9 ? A2E_FLAG_C : 0)
                    | (((~(c->a ^ v) & (c->a ^ bin)) & 0x80) ? A2E_FLAG_V : 0));
        c->a = r;
        if (c->is_65c02) {
            tick1(c);
            set_zn(c, c->a);
        } else {
            /* NMOS: Z/N from binary intermediate quirks — approximate with result */
            set_zn(c, c->a);
        }
        return;
    }
    int t = c->a + v + ((c->p & A2E_FLAG_C) ? 1 : 0);
    c->p = (u8)((c->p & (u8)~(A2E_FLAG_C | A2E_FLAG_Z | A2E_FLAG_V | A2E_FLAG_N))
                | (t > 0xFF ? A2E_FLAG_C : 0)
                | (((~(c->a ^ v) & (c->a ^ t)) & 0x80) ? A2E_FLAG_V : 0)
                | ((t & 0xFF) == 0 ? A2E_FLAG_Z : 0)
                | (t & 0x80 ? A2E_FLAG_N : 0));
    c->a = (u8)t;
}

static void op_sbc(a2e_cpu *c, u8 v) {
    if (c->p & A2E_FLAG_D) {
        u8 c0 = (c->p & A2E_FLAG_C) ? 1 : 0;
        int al = (c->a & 0x0F) - (v & 0x0F) - (1 - c0);
        int ah = (c->a >> 4) - (v >> 4);
        if (al < 0) { al += 10; ah--; }
        if (ah < 0) ah += 10;
        int bin = c->a - v - (1 - c0);
        c->p = (u8)((c->p & (u8)~(A2E_FLAG_C | A2E_FLAG_Z | A2E_FLAG_V | A2E_FLAG_N))
                    | (bin >= 0 ? A2E_FLAG_C : 0)
                    | ((((c->a ^ v) & (c->a ^ bin)) & 0x80) ? A2E_FLAG_V : 0));
        c->a = (u8)((ah << 4) | (al & 0x0F));
        if (c->is_65c02) tick1(c);
        set_zn(c, c->a);
        return;
    }
    op_adc(c, (u8)~v);
}

static void op_cmp(a2e_cpu *c, u8 reg, u8 v) {
    u16 t = (u16)(reg - v);
    c->p = (u8)((c->p & (u8)~(A2E_FLAG_C | A2E_FLAG_Z | A2E_FLAG_N))
                | (reg >= v ? A2E_FLAG_C : 0)
                | ((t & 0xFF) == 0 ? A2E_FLAG_Z : 0)
                | (t & 0x80 ? A2E_FLAG_N : 0));
}

static u8 op_asl(a2e_cpu *c, u8 v) {
    c->p = (u8)((c->p & (u8)~A2E_FLAG_C) | (v & 0x80 ? A2E_FLAG_C : 0));
    v <<= 1;
    set_zn(c, v);
    return v;
}

static u8 op_lsr(a2e_cpu *c, u8 v) {
    c->p = (u8)((c->p & (u8)~A2E_FLAG_C) | (v & 0x01 ? A2E_FLAG_C : 0));
    v >>= 1;
    set_zn(c, v);
    return v;
}

static u8 op_rol(a2e_cpu *c, u8 v) {
    u8 carry = (c->p & A2E_FLAG_C) ? 1 : 0;
    c->p = (u8)((c->p & (u8)~A2E_FLAG_C) | (v & 0x80 ? A2E_FLAG_C : 0));
    v = (u8)((v << 1) | carry);
    set_zn(c, v);
    return v;
}

static u8 op_ror(a2e_cpu *c, u8 v) {
    u8 carry = (c->p & A2E_FLAG_C) ? 0x80 : 0;
    c->p = (u8)((c->p & (u8)~A2E_FLAG_C) | (v & 0x01 ? A2E_FLAG_C : 0));
    v = (u8)((v >> 1) | carry);
    set_zn(c, v);
    return v;
}

static void branch(a2e_cpu *c, bool take) {
    i8 off = (i8)rd(c, c->pc++);
    if (take) {
        tick1(c);
        u16 old = c->pc;
        c->pc = (u16)(c->pc + off);
        if ((old & 0xFF00) != (c->pc & 0xFF00)) tick1(c);
    }
}

int a2e_cpu_step(a2e_cpu *cpu) {
    if (cpu->stopped) return 0;
    if (cpu->waiting) {
        tick1(cpu);
        return 1;
    }

    u64 start = cpu->cycles;
    u8 op = rd(cpu, cpu->pc++);

    switch (op) {
    /* ---- ADC ---- */
    case 0x69: { u8 v = rd(cpu, imm(cpu)); op_adc(cpu, v); break; }
    case 0x65: { u8 v = rd(cpu, zp(cpu)); op_adc(cpu, v); break; }
    case 0x75: { u8 v = rd(cpu, zp_x(cpu)); op_adc(cpu, v); break; }
    case 0x6D: { u8 v = rd(cpu, abs_(cpu)); op_adc(cpu, v); break; }
    case 0x7D: { u8 v = rd(cpu, abs_x(cpu, false)); op_adc(cpu, v); break; }
    case 0x79: { u8 v = rd(cpu, abs_y(cpu, false)); op_adc(cpu, v); break; }
    case 0x61: { u8 v = rd(cpu, ind_x(cpu)); op_adc(cpu, v); break; }
    case 0x71: { u8 v = rd(cpu, ind_y(cpu, false)); op_adc(cpu, v); break; }
    case 0x72: if (cpu->is_65c02) { u8 v = rd(cpu, ind_zp(cpu)); op_adc(cpu, v); } break;

    /* ---- AND ---- */
    case 0x29: { u8 v = rd(cpu, imm(cpu)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x25: { u8 v = rd(cpu, zp(cpu)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x35: { u8 v = rd(cpu, zp_x(cpu)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x2D: { u8 v = rd(cpu, abs_(cpu)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x3D: { u8 v = rd(cpu, abs_x(cpu, false)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x39: { u8 v = rd(cpu, abs_y(cpu, false)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x21: { u8 v = rd(cpu, ind_x(cpu)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x31: { u8 v = rd(cpu, ind_y(cpu, false)); cpu->a &= v; set_zn(cpu, cpu->a); break; }
    case 0x32: if (cpu->is_65c02) { u8 v = rd(cpu, ind_zp(cpu)); cpu->a &= v; set_zn(cpu, cpu->a); } break;

    /* ---- ASL ---- */
    case 0x0A: { tick1(cpu); cpu->a = op_asl(cpu, cpu->a); break; }
    case 0x06: case 0x16: case 0x0E: case 0x1E: {
        u16 a = (op == 0x06) ? zp(cpu) : (op == 0x16) ? zp_x(cpu) :
                (op == 0x0E) ? abs_(cpu) : abs_x(cpu, true);
        u8 v = rd(cpu, a);
        tick1(cpu);
        v = op_asl(cpu, v);
        wr(cpu, a, v);
        break;
    }

    /* ---- branches ---- */
    case 0x10: branch(cpu, !(cpu->p & A2E_FLAG_N)); break;
    case 0x30: branch(cpu, cpu->p & A2E_FLAG_N); break;
    case 0x50: branch(cpu, !(cpu->p & A2E_FLAG_V)); break;
    case 0x70: branch(cpu, cpu->p & A2E_FLAG_V); break;
    case 0x90: branch(cpu, !(cpu->p & A2E_FLAG_C)); break;
    case 0xB0: branch(cpu, cpu->p & A2E_FLAG_C); break;
    case 0xD0: branch(cpu, !(cpu->p & A2E_FLAG_Z)); break;
    case 0xF0: branch(cpu, cpu->p & A2E_FLAG_Z); break;
    case 0x80: if (cpu->is_65c02) branch(cpu, true); break;

    /* ---- BIT ---- */
    case 0x24: case 0x2C: {
        u16 a = (op == 0x24) ? zp(cpu) : abs_(cpu);
        u8 v = rd(cpu, a);
        cpu->p = (u8)((cpu->p & (u8)~(A2E_FLAG_Z | A2E_FLAG_V | A2E_FLAG_N))
                      | ((cpu->a & v) == 0 ? A2E_FLAG_Z : 0)
                      | (v & 0x40 ? A2E_FLAG_V : 0)
                      | (v & 0x80 ? A2E_FLAG_N : 0));
        break;
    }
    case 0x89: if (cpu->is_65c02) { /* BIT imm */
        u8 v = rd(cpu, imm(cpu));
        cpu->p = (u8)((cpu->p & (u8)~A2E_FLAG_Z) | ((cpu->a & v) == 0 ? A2E_FLAG_Z : 0));
    }
    break;
    case 0x34: case 0x3C: if (cpu->is_65c02) {
        u16 a = (op == 0x34) ? zp_x(cpu) : abs_x(cpu, false);
        u8 v = rd(cpu, a);
        cpu->p = (u8)((cpu->p & (u8)~(A2E_FLAG_Z | A2E_FLAG_V | A2E_FLAG_N))
                      | ((cpu->a & v) == 0 ? A2E_FLAG_Z : 0)
                      | (v & 0x40 ? A2E_FLAG_V : 0)
                      | (v & 0x80 ? A2E_FLAG_N : 0));
    }
    break;

    /* ---- flag ---- */
    case 0x18: tick1(cpu); cpu->p &= (u8)~A2E_FLAG_C; break;
    case 0x38: tick1(cpu); cpu->p |= A2E_FLAG_C; break;
    case 0x58: tick1(cpu); cpu->p &= (u8)~A2E_FLAG_I; break;
    case 0x78: tick1(cpu); cpu->p |= A2E_FLAG_I; break;
    case 0xB8: tick1(cpu); cpu->p &= (u8)~A2E_FLAG_V; break;
    case 0xD8: tick1(cpu); cpu->p &= (u8)~A2E_FLAG_D; break;
    case 0xF8: tick1(cpu); cpu->p |= A2E_FLAG_D; break;

    /* ---- BRK ---- */
    case 0x00: {
        rd(cpu, cpu->pc); /* padding */
        cpu->pc++;
        push(cpu, (u8)(cpu->pc >> 8));
        push(cpu, (u8)(cpu->pc & 0xFF));
        push(cpu, (u8)(cpu->p | A2E_FLAG_B | A2E_FLAG_U));
        cpu->p |= A2E_FLAG_I;
        if (cpu->is_65c02) cpu->p &= (u8)~A2E_FLAG_D;
        u8 lo = rd(cpu, 0xFFFE);
        u8 hi = rd(cpu, 0xFFFF);
        cpu->pc = (u16)(lo | (hi << 8));
        break;
    }

    /* ---- CMP/CPX/CPY ---- */
    case 0xC9: op_cmp(cpu, cpu->a, rd(cpu, imm(cpu))); break;
    case 0xC5: op_cmp(cpu, cpu->a, rd(cpu, zp(cpu))); break;
    case 0xD5: op_cmp(cpu, cpu->a, rd(cpu, zp_x(cpu))); break;
    case 0xCD: op_cmp(cpu, cpu->a, rd(cpu, abs_(cpu))); break;
    case 0xDD: op_cmp(cpu, cpu->a, rd(cpu, abs_x(cpu, false))); break;
    case 0xD9: op_cmp(cpu, cpu->a, rd(cpu, abs_y(cpu, false))); break;
    case 0xC1: op_cmp(cpu, cpu->a, rd(cpu, ind_x(cpu))); break;
    case 0xD1: op_cmp(cpu, cpu->a, rd(cpu, ind_y(cpu, false))); break;
    case 0xD2: if (cpu->is_65c02) op_cmp(cpu, cpu->a, rd(cpu, ind_zp(cpu))); break;
    case 0xE0: op_cmp(cpu, cpu->x, rd(cpu, imm(cpu))); break;
    case 0xE4: op_cmp(cpu, cpu->x, rd(cpu, zp(cpu))); break;
    case 0xEC: op_cmp(cpu, cpu->x, rd(cpu, abs_(cpu))); break;
    case 0xC0: op_cmp(cpu, cpu->y, rd(cpu, imm(cpu))); break;
    case 0xC4: op_cmp(cpu, cpu->y, rd(cpu, zp(cpu))); break;
    case 0xCC: op_cmp(cpu, cpu->y, rd(cpu, abs_(cpu))); break;

    /* ---- DEC/DEX/DEY ---- */
    case 0xCA: tick1(cpu); cpu->x--; set_zn(cpu, cpu->x); break;
    case 0x88: tick1(cpu); cpu->y--; set_zn(cpu, cpu->y); break;
    case 0x3A: if (cpu->is_65c02) { tick1(cpu); cpu->a--; set_zn(cpu, cpu->a); } break;
    case 0xC6: case 0xD6: case 0xCE: case 0xDE: {
        u16 a = (op == 0xC6) ? zp(cpu) : (op == 0xD6) ? zp_x(cpu) :
                (op == 0xCE) ? abs_(cpu) : abs_x(cpu, true);
        u8 v = rd(cpu, a);
        tick1(cpu);
        v--;
        set_zn(cpu, v);
        wr(cpu, a, v);
        break;
    }

    /* ---- EOR ---- */
    case 0x49: cpu->a ^= rd(cpu, imm(cpu)); set_zn(cpu, cpu->a); break;
    case 0x45: cpu->a ^= rd(cpu, zp(cpu)); set_zn(cpu, cpu->a); break;
    case 0x55: cpu->a ^= rd(cpu, zp_x(cpu)); set_zn(cpu, cpu->a); break;
    case 0x4D: cpu->a ^= rd(cpu, abs_(cpu)); set_zn(cpu, cpu->a); break;
    case 0x5D: cpu->a ^= rd(cpu, abs_x(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0x59: cpu->a ^= rd(cpu, abs_y(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0x41: cpu->a ^= rd(cpu, ind_x(cpu)); set_zn(cpu, cpu->a); break;
    case 0x51: cpu->a ^= rd(cpu, ind_y(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0x52: if (cpu->is_65c02) { cpu->a ^= rd(cpu, ind_zp(cpu)); set_zn(cpu, cpu->a); } break;

    /* ---- INC/INX/INY ---- */
    case 0xE8: tick1(cpu); cpu->x++; set_zn(cpu, cpu->x); break;
    case 0xC8: tick1(cpu); cpu->y++; set_zn(cpu, cpu->y); break;
    case 0x1A: if (cpu->is_65c02) { tick1(cpu); cpu->a++; set_zn(cpu, cpu->a); } break;
    case 0xE6: case 0xF6: case 0xEE: case 0xFE: {
        u16 a = (op == 0xE6) ? zp(cpu) : (op == 0xF6) ? zp_x(cpu) :
                (op == 0xEE) ? abs_(cpu) : abs_x(cpu, true);
        u8 v = rd(cpu, a);
        tick1(cpu);
        v++;
        set_zn(cpu, v);
        wr(cpu, a, v);
        break;
    }

    /* ---- JMP/JSR/RTS/RTI ---- */
    case 0x4C: cpu->pc = abs_(cpu); break;
    case 0x6C: cpu->pc = ind_abs(cpu); break;
    case 0x7C: if (cpu->is_65c02) cpu->pc = ind_abs_x(cpu); break;
    case 0x20: {
        u8 lo = rd(cpu, cpu->pc++);
        tick1(cpu);
        push(cpu, (u8)(cpu->pc >> 8));
        push(cpu, (u8)(cpu->pc & 0xFF));
        u8 hi = rd(cpu, cpu->pc);
        cpu->pc = (u16)(lo | (hi << 8));
        break;
    }
    case 0x60: {
        tick1(cpu);
        tick1(cpu);
        u8 lo = pull(cpu);
        u8 hi = pull(cpu);
        tick1(cpu);
        cpu->pc = (u16)((lo | (hi << 8)) + 1);
        break;
    }
    case 0x40: {
        tick1(cpu);
        tick1(cpu);
        cpu->p = (u8)(pull(cpu) | A2E_FLAG_U);
        u8 lo = pull(cpu);
        u8 hi = pull(cpu);
        cpu->pc = (u16)(lo | (hi << 8));
        break;
    }

    /* ---- LDA ---- */
    case 0xA9: cpu->a = rd(cpu, imm(cpu)); set_zn(cpu, cpu->a); break;
    case 0xA5: cpu->a = rd(cpu, zp(cpu)); set_zn(cpu, cpu->a); break;
    case 0xB5: cpu->a = rd(cpu, zp_x(cpu)); set_zn(cpu, cpu->a); break;
    case 0xAD: cpu->a = rd(cpu, abs_(cpu)); set_zn(cpu, cpu->a); break;
    case 0xBD: cpu->a = rd(cpu, abs_x(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0xB9: cpu->a = rd(cpu, abs_y(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0xA1: cpu->a = rd(cpu, ind_x(cpu)); set_zn(cpu, cpu->a); break;
    case 0xB1: cpu->a = rd(cpu, ind_y(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0xB2: if (cpu->is_65c02) { cpu->a = rd(cpu, ind_zp(cpu)); set_zn(cpu, cpu->a); } break;

    /* ---- LDX ---- */
    case 0xA2: cpu->x = rd(cpu, imm(cpu)); set_zn(cpu, cpu->x); break;
    case 0xA6: cpu->x = rd(cpu, zp(cpu)); set_zn(cpu, cpu->x); break;
    case 0xB6: cpu->x = rd(cpu, zp_y(cpu)); set_zn(cpu, cpu->x); break;
    case 0xAE: cpu->x = rd(cpu, abs_(cpu)); set_zn(cpu, cpu->x); break;
    case 0xBE: cpu->x = rd(cpu, abs_y(cpu, false)); set_zn(cpu, cpu->x); break;

    /* ---- LDY ---- */
    case 0xA0: cpu->y = rd(cpu, imm(cpu)); set_zn(cpu, cpu->y); break;
    case 0xA4: cpu->y = rd(cpu, zp(cpu)); set_zn(cpu, cpu->y); break;
    case 0xB4: cpu->y = rd(cpu, zp_x(cpu)); set_zn(cpu, cpu->y); break;
    case 0xAC: cpu->y = rd(cpu, abs_(cpu)); set_zn(cpu, cpu->y); break;
    case 0xBC: cpu->y = rd(cpu, abs_x(cpu, false)); set_zn(cpu, cpu->y); break;

    /* ---- LSR ---- */
    case 0x4A: tick1(cpu); cpu->a = op_lsr(cpu, cpu->a); break;
    case 0x46: case 0x56: case 0x4E: case 0x5E: {
        u16 a = (op == 0x46) ? zp(cpu) : (op == 0x56) ? zp_x(cpu) :
                (op == 0x4E) ? abs_(cpu) : abs_x(cpu, true);
        u8 v = rd(cpu, a); tick1(cpu); v = op_lsr(cpu, v); wr(cpu, a, v); break;
    }

    /* ---- NOP ---- */
    case 0xEA: tick1(cpu); break;
    case 0x44: /* NOP zp (65C02) */
        if (cpu->is_65c02) { (void)rd(cpu, zp(cpu)); }
        break;
    case 0x54: case 0xD4: case 0xF4:
        if (cpu->is_65c02) { (void)rd(cpu, zp_x(cpu)); }
        break;
    case 0x5C: case 0xDC: case 0xFC:
        if (cpu->is_65c02) { (void)rd(cpu, abs_(cpu)); tick1(cpu); tick1(cpu); }
        break;

    /* ---- ORA ---- */
    case 0x09: cpu->a |= rd(cpu, imm(cpu)); set_zn(cpu, cpu->a); break;
    case 0x05: cpu->a |= rd(cpu, zp(cpu)); set_zn(cpu, cpu->a); break;
    case 0x15: cpu->a |= rd(cpu, zp_x(cpu)); set_zn(cpu, cpu->a); break;
    case 0x0D: cpu->a |= rd(cpu, abs_(cpu)); set_zn(cpu, cpu->a); break;
    case 0x1D: cpu->a |= rd(cpu, abs_x(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0x19: cpu->a |= rd(cpu, abs_y(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0x01: cpu->a |= rd(cpu, ind_x(cpu)); set_zn(cpu, cpu->a); break;
    case 0x11: cpu->a |= rd(cpu, ind_y(cpu, false)); set_zn(cpu, cpu->a); break;
    case 0x12: if (cpu->is_65c02) { cpu->a |= rd(cpu, ind_zp(cpu)); set_zn(cpu, cpu->a); } break;

    /* ---- PHA/PHP/PLA/PLP/PHX/PHY/PLX/PLY ---- */
    case 0x48: tick1(cpu); push(cpu, cpu->a); break;
    case 0x08: tick1(cpu); push(cpu, (u8)(cpu->p | A2E_FLAG_B | A2E_FLAG_U)); break;
    case 0x68: tick1(cpu); tick1(cpu); cpu->a = pull(cpu); set_zn(cpu, cpu->a); break;
    case 0x28: tick1(cpu); tick1(cpu); cpu->p = (u8)(pull(cpu) | A2E_FLAG_U); break;
    case 0xDA: if (cpu->is_65c02) { tick1(cpu); push(cpu, cpu->x); } break;
    case 0x5A: if (cpu->is_65c02) { tick1(cpu); push(cpu, cpu->y); } break;
    case 0xFA: if (cpu->is_65c02) { tick1(cpu); tick1(cpu); cpu->x = pull(cpu); set_zn(cpu, cpu->x); } break;
    case 0x7A: if (cpu->is_65c02) { tick1(cpu); tick1(cpu); cpu->y = pull(cpu); set_zn(cpu, cpu->y); } break;

    /* ---- ROL/ROR ---- */
    case 0x2A: tick1(cpu); cpu->a = op_rol(cpu, cpu->a); break;
    case 0x26: case 0x36: case 0x2E: case 0x3E: {
        u16 a = (op == 0x26) ? zp(cpu) : (op == 0x36) ? zp_x(cpu) :
                (op == 0x2E) ? abs_(cpu) : abs_x(cpu, true);
        u8 v = rd(cpu, a); tick1(cpu); v = op_rol(cpu, v); wr(cpu, a, v); break;
    }
    case 0x6A: tick1(cpu); cpu->a = op_ror(cpu, cpu->a); break;
    case 0x66: case 0x76: case 0x6E: case 0x7E: {
        u16 a = (op == 0x66) ? zp(cpu) : (op == 0x76) ? zp_x(cpu) :
                (op == 0x6E) ? abs_(cpu) : abs_x(cpu, true);
        u8 v = rd(cpu, a); tick1(cpu); v = op_ror(cpu, v); wr(cpu, a, v); break;
    }

    /* ---- SBC ---- */
    case 0xE9: op_sbc(cpu, rd(cpu, imm(cpu))); break;
    case 0xE5: op_sbc(cpu, rd(cpu, zp(cpu))); break;
    case 0xF5: op_sbc(cpu, rd(cpu, zp_x(cpu))); break;
    case 0xED: op_sbc(cpu, rd(cpu, abs_(cpu))); break;
    case 0xFD: op_sbc(cpu, rd(cpu, abs_x(cpu, false))); break;
    case 0xF9: op_sbc(cpu, rd(cpu, abs_y(cpu, false))); break;
    case 0xE1: op_sbc(cpu, rd(cpu, ind_x(cpu))); break;
    case 0xF1: op_sbc(cpu, rd(cpu, ind_y(cpu, false))); break;
    case 0xF2: if (cpu->is_65c02) op_sbc(cpu, rd(cpu, ind_zp(cpu))); break;

    /* ---- STA ---- */
    case 0x85: wr(cpu, zp(cpu), cpu->a); break;
    case 0x95: wr(cpu, zp_x(cpu), cpu->a); break;
    case 0x8D: wr(cpu, abs_(cpu), cpu->a); break;
    case 0x9D: wr(cpu, abs_x(cpu, true), cpu->a); break;
    case 0x99: wr(cpu, abs_y(cpu, true), cpu->a); break;
    case 0x81: wr(cpu, ind_x(cpu), cpu->a); break;
    case 0x91: wr(cpu, ind_y(cpu, true), cpu->a); break;
    case 0x92: if (cpu->is_65c02) wr(cpu, ind_zp(cpu), cpu->a); break;

    /* ---- STX/STY/STZ ---- */
    case 0x86: wr(cpu, zp(cpu), cpu->x); break;
    case 0x96: wr(cpu, zp_y(cpu), cpu->x); break;
    case 0x8E: wr(cpu, abs_(cpu), cpu->x); break;
    case 0x84: wr(cpu, zp(cpu), cpu->y); break;
    case 0x94: wr(cpu, zp_x(cpu), cpu->y); break;
    case 0x8C: wr(cpu, abs_(cpu), cpu->y); break;
    case 0x64: if (cpu->is_65c02) wr(cpu, zp(cpu), 0); break; /* STZ zp */
    case 0x74: if (cpu->is_65c02) wr(cpu, zp_x(cpu), 0); break; /* STZ zp,X */
    case 0x9C: if (cpu->is_65c02) wr(cpu, abs_(cpu), 0); break;
    case 0x9E: if (cpu->is_65c02) wr(cpu, abs_x(cpu, true), 0); break;

    /* ---- transfers ---- */
    case 0xAA: tick1(cpu); cpu->x = cpu->a; set_zn(cpu, cpu->x); break;
    case 0xA8: tick1(cpu); cpu->y = cpu->a; set_zn(cpu, cpu->y); break;
    case 0xBA: tick1(cpu); cpu->x = cpu->sp; set_zn(cpu, cpu->x); break;
    case 0x8A: tick1(cpu); cpu->a = cpu->x; set_zn(cpu, cpu->a); break;
    case 0x9A: tick1(cpu); cpu->sp = cpu->x; break;
    case 0x98: tick1(cpu); cpu->a = cpu->y; set_zn(cpu, cpu->a); break;

    /* ---- TSB/TRB (65C02) ---- */
    case 0x04: /* TSB zp */ if (cpu->is_65c02) {
        u16 a = zp(cpu);
        u8 v = rd(cpu, a);
        tick1(cpu);
        cpu->p = (u8)((cpu->p & (u8)~A2E_FLAG_Z) | ((v & cpu->a) == 0 ? A2E_FLAG_Z : 0));
        wr(cpu, a, (u8)(v | cpu->a));
    }
    break;
    case 0x0C: /* TSB abs */ if (cpu->is_65c02) {
        u16 a = abs_(cpu);
        u8 v = rd(cpu, a);
        tick1(cpu);
        cpu->p = (u8)((cpu->p & (u8)~A2E_FLAG_Z) | ((v & cpu->a) == 0 ? A2E_FLAG_Z : 0));
        wr(cpu, a, (u8)(v | cpu->a));
    }
    break;
    case 0x14: /* TRB zp */ if (cpu->is_65c02) {
        u16 a = zp(cpu);
        u8 v = rd(cpu, a);
        tick1(cpu);
        cpu->p = (u8)((cpu->p & (u8)~A2E_FLAG_Z) | ((v & cpu->a) == 0 ? A2E_FLAG_Z : 0));
        wr(cpu, a, (u8)(v & (u8)~cpu->a));
    }
    break;
    case 0x1C: /* TRB abs */ if (cpu->is_65c02) {
        u16 a = abs_(cpu);
        u8 v = rd(cpu, a);
        tick1(cpu);
        cpu->p = (u8)((cpu->p & (u8)~A2E_FLAG_Z) | ((v & cpu->a) == 0 ? A2E_FLAG_Z : 0));
        wr(cpu, a, (u8)(v & (u8)~cpu->a));
    }
    break;

    default:
        /* Unofficial / unhandled: treat as NOP tick */
        tick1(cpu);
        break;
    }

    return (int)(cpu->cycles - start);
}

u64 a2e_cpu_run(a2e_cpu *cpu, u64 n) {
    u64 start = cpu->cycles;
    while (cpu->cycles - start < n && !cpu->stopped)
        a2e_cpu_step(cpu);
    return cpu->cycles - start;
}
