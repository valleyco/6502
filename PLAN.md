# Apple IIe Simulator — living plan

**This file:** repo root `PLAN.md`.

Work it interactively (same process as `../esp32-invaders/PLAN.md`).
**Do not start a step until we agree it.** After each discussion, update
[`docs/DECISIONS.md`](docs/DECISIONS.md) and the step status here.

Statuses: `todo` · `discuss` · `agreed` · `in progress` · `done`

TDD rule (**D13**): for each step, write/extend `host/cpu` or `host/machine`
tests first; keep them green before moving on.

---

## Hosts (mirrors invaders)

```text
host/cpu/        CPU unit tests (no SDL, no IDF)
host/machine/    MMU / Disk II / video / machine tests
host/linux/      SDL2 playable host
host/cyd/        ESP32-2432S028R stub (after Linux DOS boot)
core/            portable C11 — linked by all hosts
```

---

## Steps

### Step 0 — Lock decisions + TDD harness
**Status:** `done`

D1–D13 locked. Harness: `make test-cpu` / `make test-machine` / `make test`.

### Step 1 — Cycle 6502/65C02 baseline
**Status:** `done` (initial C smoke) · ISA vectors in Step 8

`core/cpu.c` + `host/cpu` C smoke tests. Broader opcode coverage via
asmx-driven vectors (**D14**, Step 8) — same method as `8086pc`.

### Step 2 — IIe MMU / soft switches
**Status:** `done` (initial) · harden under TDD for ProDOS banks

### Step 3 — Cycle-driven Disk II + DSK
**Status:** `done` (encode, latch, **RWTS-level decode**, softswitch sector read)

TDD coverage in `host/machine/test_disk*.c`:
- address prolog find, encode↔decode round-trip, stepper, softswitch sector read
- stage-0 boot: T0S0 → `$0800` → execute (`test_boot`)

Still needs user Disk II PROM + Autostart ROM for real slot-6 cold boot.

### Step 4 — Linux SDL host
**Status:** `done` (skeleton) · polish after DOS boot proven in tests where possible

### Step 5 — DOS 3.3 boot (user ROM/disk)
**Status:** `done` · `make test-boot-dos` green (`$3D0=4C`, ~60M cycles)

Without Apple firmware we have:
- **Clean-room slot-6 path:** `a2e_machine_install_cleanroom_boot()` (`test_boot`)
- RWTS-level disk I/O in `test_disk_rwts`

With Asimov fixtures (gitignored `fixtures/asimov/`):
```bash
make fetch-asimov   # merges IIe monitor + CX ROM banks
make test-boot-dos
./host/linux/a2e --rom roms/apple2e.rom --disk disks/dos33.dsk
```

### Step 6 — ProDOS path
**Status:** `done` · `.po` load + LC bank TDD; `make test-boot-prodos` green (`$BF00=4C`)

- `a2e_disk_load_po()` — ProDOS-order sector interleave
- MMU: LC bank1/bank2 isolation + prewrite gating (`test_mmu`)
- Fixtures: `make fetch-asimov` also pulls `prodos.dsk` (ProDOS 2.4.1)

```bash
make fetch-asimov
make test-boot-prodos
```

### Step 7 — CYD reduced profile
**Status:** `done` (host bring-up) · flash/verify on device still manual

- `A2E_REDUCED_DISK` + `A2E_REDUCED_VIDEO` (CMake on `host/cyd`)
- RGB565 row path: `a2e_video_render_row_rgb565` / `a2e_video_cyd_panel_row` + `test_video_rgb565`
- ST7789 driver (`cyd_display.c`) — invaders-verified pins/settings; SPI byteswap
- SD load (`cyd_sd.c`) — mount `/sdcard`, load `roms/` + `disks/`
- Desktop smoke: `make cyd-stub`

```bash
make test-machine   # includes test_video_rgb565 + test_disk_reduced
make cyd-stub       # no IDF; exercises RGB565 present path
# On device (ESP-IDF project wrapping host/cyd): copy ROM/disk to SD first
```

### Step 8 — asmx ISA vector harness (D14)
**Status:** `done` · 50 vectors green (`make test-cpu`)

8086pc-style asm vectors for 6502/65C02:

```text
host/cpu/test.asm  (; @test / @init / @expect / @mem)
    → asmx -C 65C02 -l -o -b
    → test.bin + test.lst
    → tools/gen_test_vectors.py
    → host/cpu/test_vectors.bin
    → host/cpu/test_runner
```

```bash
make test-cpu              # C smoke + ISA vectors
make -C host/cpu vectors   # regenerate (needs asmx on PATH)
```

Docs: [`docs/asm_test_harness.md`](docs/asm_test_harness.md).

### Step 9 — Video modes (TEXT / GR / MIXED / lores)
**Status:** `done` · `test_video_modes` green

Soft-switch–driven display modes so BASIC `GR` / `HGR` / `TEXT` work:

- Track **TEXT**, **MIXED**, **PAGE2**, **HIRES** (IIe status at `$C01A`–`$C01D`)
- Wire `$C050`–`$C057` on read and write
- **Lores** RGB renderer + mixed bottom-4 text rows
- TDD: `test_video_modes` + MMU status asserts

```bash
make test-machine   # includes test_video_modes
```

### Step 10 — Graphic game smoke (Asimov invaders)
**Status:** `done` · boots to HIRES playfield (`make test-boot-invaders`)

Keyboard invaders compilation from Asimov (`disks/invaders.dsk`, gitignored).

```bash
make fetch-asimov
./host/linux/a2e --rom roms/apple2e.rom --disk disks/invaders.dsk
# CAT menu → E → RUN KEYBOARD APPLE INVADERS → Space
make test-boot-invaders
```

Also: paddle position timers + VBL `$C019`; button soft-switches still
open-bus `0` (returning released `0x80` broke this disk’s menu).

### Step 11 — SDL audio (speaker + disk FX)
**Status:** `done`

**D15:** `$C030` speaker → PCM on Linux SDL2. Disk drive FX code remains but **`disk_fx` default off** (synth not faithful).

```bash
make linux
./host/linux/a2e --rom roms/apple2e.rom --disk disks/invaders.dsk
# speaker beeps; no drive whir unless disk_fx enabled in code
```

---

## Make cheat sheet

```bash
make test-cpu
make test-machine
make test          # both
make linux         # SDL2 host
```
