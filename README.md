# Apple IIe Simulator

Cycle-driven Apple IIe core in portable C11, with a Linux (SDL2) host first and a CYD/ESP32 host stub.

Locked architecture decisions: [`docs/DECISIONS.md`](docs/DECISIONS.md).

## Build

```bash
make test-cpu       # host/cpu (TDD)
make test-machine   # host/machine (TDD)
make test           # both
make fetch-asimov   # optional: ROM+DOS into fixtures/asimov/ (gitignored)
make test-boot-dos  # optional DOS cold-boot smoke (skips if no fixtures)
make linux          # requires SDL2 (pkg-config sdl2)
```

Process: [`PLAN.md`](PLAN.md), decisions: [`docs/DECISIONS.md`](docs/DECISIONS.md) (**D13** TDD).

ROMs/disks: [`docs/RESOURCES.md`](docs/RESOURCES.md) (6502.org, mirrors, `make fetch-asimov` — not bundled in repo).

## Run

```bash
./host/linux/a2e --rom roms/apple2e.rom --disk disks/dos33.dsk
./host/linux/a2e --scale 4 --rom roms/apple2e.rom --disk disks/dos33.dsk
```

`--scale N` sets the window to N×280×192 (1–8, default 3). `+` / `-` change scale while running.

Firmware and disk images are **not** included — see [`roms/README.md`](roms/README.md).

## Layout

| Path | Role |
|------|------|
| `core/` | 6502, IIe MMU/soft switches, Disk II, video state, machine |
| `host/cpu/` | CPU unit tests (no SDL/IDF) |
| `host/machine/` | MMU / Disk II / machine tests |
| `host/linux/` | SDL2 display + keyboard |
| `host/cyd/` | ESP32-2432S028R CYD stub (no PSRAM → reduced RAM/disk profile) |
| `tools/` | Reserved (DSK helpers / language side quest) |

## Status (v1)

- Per-cycle bus tick + instruction-accurate 6502/65C02 core
- IIe banking soft switches (incl. aux / language card for ProDOS path)
- Cycle-driven Disk II with `.dsk` → nibble tracks
- Simplified RGB text/hires present path
- DOS 3.3 / ProDOS boot needs user ROM + Disk II boot PROM + disk image
