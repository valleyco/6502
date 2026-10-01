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
**Status:** `done` (initial C coverage) · expand under TDD

`core/cpu.c` + `host/cpu`. Next under TDD: broader opcode/cycle cases
(optional YAML runner later).

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

---

## Make cheat sheet

```bash
make test-cpu
make test-machine
make test          # both
make linux         # SDL2 host
```
