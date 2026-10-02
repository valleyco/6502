# Apple IIe Simulator — Locked Decisions

Source of truth for architectural decisions. Change only by explicit re-lock.

| ID | Decision |
|----|----------|
| D1 | **Core language:** C11 (no custom language on the critical path) |
| D2 | **Custom multi-target language:** explicit **side quest** — may exist in-repo later, must never block CPU / Disk II / hosts |
| D3 | **Host independence:** portable C core + thin host adapters (video, input, filesystem, time) |
| D4 | **First host:** Linux SDL2 (`host/linux`). Mac / Win later |
| D5 | **ESP32 / CYD:** supported host **after** Linux proves the core; target full IIe machine model on device, with **reduced video fidelity** (RGB framebuffer, not NTSC artifact color) |
| D6 | **CYD hardware (locked to this unit):** **ESP32-2432S028R V3 USB-C** — classic **ESP32-WROOM**, **no PSRAM**; panel **ST7789** 320×240 SPI; touch **XPT2046**; Micro-SD on VSPI. Same board as `../esp32-invaders` (`docs/boards/esp32-2432s028r/BOARD.md`). **Memory profile on CYD:** reduced — **64K main**, aux/LC only if heap allows; do **not** keep full 35-track nibble cache in RAM (stream/decode from SD). Do not fork a second core; gate features with a host compile flag. |
| D7 | **Machine target:** Apple **IIe** (64K + aux bank / soft switches as needed for ProDOS) |
| D8 | **V1 accuracy bar:** correct enough for **DOS 3.3 and ProDOS** disk I/O on `.dsk` / `.po` images — not copy-protection / WOZ perfection |
| D9 | **Disk approach:** **cycle-driven Disk II** (soft switches Q6/Q7, sequencer advancing with CPU cycles) — not high-level “inject sector” cheats. V1 may use simplified nibble tracks derived from DSK; WOZ / protection later |
| D10 | **CPU:** 6502 (and //e 65C02 opcodes as needed for firmware) with **per-cycle** bus model (memory R/W each cycle) so Disk II stays in sync |
| D11 | **Validation order:** CPU → MMU/soft switches → Disk II → boot DOS → ProDOS; UI polish last |
| D12 | **Living plan:** [`PLAN.md`](../PLAN.md) is worked interactively (same style as invaders). Do not start a step until agreed. Update step status after each discussion. |
| D13 | **TDD (locked):** same approach as `../esp32-invaders`. **Red → green → refactor.** New core behavior gets a failing host test **before** implementation. Harness layout: `host/cpu` (CPU), `host/machine` (MMU / Disk II / video / machine). Targets: `make test-cpu`, `make test-machine`, `make test`. No SDL/IDF inside those tests. Assert style mirrors invaders `test_assert.h`. |
| D14 | **CPU ISA vectors (locked):** same method as `../8086pc/i8086` — author tests in `host/cpu/test.asm` with `; @test` / `@init` / `@expect` / `@mem` comment DSL; assemble with **asmx** (`-C 65C02`); `tools/gen_test_vectors.py` → `test_vectors.bin`; C `test_runner`. Halt = STP opcode `$DB` (`DB $DB` in asmx; mnemonic is 65C816-only there). Default profile **65C02** (IIe). Commit generated `test_vectors.bin` so `make test-cpu` runs without asmx; regenerate when `test.asm` changes. |
| D15 | **Audio (locked):** Linux host uses **SDL2 audio**. Primary path = Apple **1-bit speaker** (`$C030` toggles → PCM). **Disk II drive sound:** deferred — placeholder synth (noise + clicks) is not realistic; `disk_fx` stays **off** unless we add samples or proper modeling later. No Mockingboard in v1. |

## Out of scope for v1

- Copy protection, WOZ, 3.5" / SmartPort
- NTSC artifact color, Mockingboard fidelity
- Custom language backends (Java/Node/etc.)
- Mac / Windows hosts

## ROM and disk images

Apple firmware ROMs and commercial disk images are **not** redistributed. Place user-supplied files under `roms/` and `disks/` (see `roms/README.md`).
