#ifndef A2E_CPU_H
#define A2E_CPU_H

#include "types.h"

typedef struct a2e_bus a2e_bus;

typedef struct a2e_cpu {
    u8  a, x, y, sp, p;
    u16 pc;
    u64 cycles;          /* total cycles executed */
    bool stopped;        /* STP (65C02) or fatal */
    bool waiting;        /* WAI (65C02) */
    bool is_65c02;       /* Apple IIe enhanced */
    a2e_bus *bus;
} a2e_cpu;

/* NV-BDIZC */
enum {
    A2E_FLAG_C = 0x01,
    A2E_FLAG_Z = 0x02,
    A2E_FLAG_I = 0x04,
    A2E_FLAG_D = 0x08,
    A2E_FLAG_B = 0x10,
    A2E_FLAG_U = 0x20,
    A2E_FLAG_V = 0x40,
    A2E_FLAG_N = 0x80
};

void a2e_cpu_init(a2e_cpu *cpu, a2e_bus *bus, bool is_65c02);
void a2e_cpu_reset(a2e_cpu *cpu);
void a2e_cpu_irq(a2e_cpu *cpu);
void a2e_cpu_nmi(a2e_cpu *cpu);

/* Execute one instruction; returns cycles consumed. Advances bus each cycle. */
int a2e_cpu_step(a2e_cpu *cpu);

/* Run until at least `n` cycles have been added; returns cycles actually run. */
u64 a2e_cpu_run(a2e_cpu *cpu, u64 n);

#endif /* A2E_CPU_H */
