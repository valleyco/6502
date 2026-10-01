#ifndef A2E_BUS_H
#define A2E_BUS_H

#include "types.h"

/*
 * Per-cycle bus (D10): every CPU memory access goes through here.
 * Soft switches, Disk II, and other devices hook via the machine layer.
 */
typedef struct a2e_bus {
    void *ctx;
    u8  (*read)(void *ctx, u16 addr);
    void (*write)(void *ctx, u16 addr, u8 val);
    /* Called once per CPU cycle (Disk II sequencer, video counters, etc.). */
    void (*tick)(void *ctx);
} a2e_bus;

static inline u8 a2e_bus_read(a2e_bus *b, u16 addr) {
    return b->read(b->ctx, addr);
}

static inline void a2e_bus_write(a2e_bus *b, u16 addr, u8 val) {
    b->write(b->ctx, addr, val);
}

static inline void a2e_bus_tick(a2e_bus *b) {
    if (b->tick) b->tick(b->ctx);
}

#endif /* A2E_BUS_H */
