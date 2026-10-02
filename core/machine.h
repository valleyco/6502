#ifndef A2E_MACHINE_H
#define A2E_MACHINE_H

#include "types.h"
#include "cpu.h"
#include "bus.h"
#include "mmu.h"
#include "disk.h"
#include "video.h"
#include "audio.h"
#include "host.h"

typedef struct a2e_machine {
    a2e_cpu   cpu;
    a2e_bus   bus;
    a2e_mmu   mmu;
    a2e_disk  disk;
    a2e_video video;
    a2e_audio audio;
    u8        main_ram[65536];
    u8        aux_ram[65536];
    u8        rom[16384];      /* $C000-$FFFF image (16K); unused low 256 of CX */
    a2e_host_ops host;
    bool      running;
    u64       frame_cycles;    /* ~17030 cycles per NTSC frame */
} a2e_machine;

void a2e_machine_init(a2e_machine *m, const a2e_host_ops *host);
int  a2e_machine_load_rom(a2e_machine *m, const u8 *data, size_t len);
int  a2e_machine_load_disk(a2e_machine *m, const u8 *dsk, size_t len);
int  a2e_machine_load_disk_po(a2e_machine *m, const u8 *po, size_t len);
void a2e_machine_reset(a2e_machine *m);
void a2e_machine_run_cycles(a2e_machine *m, u64 cycles);
void a2e_machine_run_frame(a2e_machine *m);
void a2e_machine_key(a2e_machine *m, u8 ascii);

/* Install clean-room Disk II boot: trampoline at $C600 + loader at $0300. */
void a2e_machine_install_cleanroom_boot(a2e_machine *m);

#endif /* A2E_MACHINE_H */
