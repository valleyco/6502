# CYD / ESP32 host

Board locked to the same unit as [`../esp32-invaders`](../../esp32-invaders):

**ESP32-2432S028R V3 USB-C** — see invaders
[`docs/boards/esp32-2432s028r/BOARD.md`](../../esp32-invaders/docs/boards/esp32-2432s028r/BOARD.md).

| | |
|---|---|
| MCU | Classic **ESP32-WROOM** (**no PSRAM**) |
| Panel | **ST7789**, landscape **320×240**, SPI2 (MOSI 13, SCLK 14, CS 15, DC 2, BL 21) |
| Touch | **XPT2046** on SPI3 (25/32/39/33, IRQ 36) |
| Storage | Micro-SD (23/19/18/5) — intended for ROM + `.dsk` |
| Serial | `/dev/ttyUSB0` (CH340) |
| Audio | GPIO26 → SC8002B (optional later) |

Decisions: [`docs/DECISIONS.md`](../../docs/DECISIONS.md) **D5–D6**.

## Memory reality (no PSRAM)

Full IIe on this silicon does **not** fit a naive RAM layout:

| Buffer | Size |
|---|---|
| Main 64K + aux 64K | 128 KiB |
| RGB888 280×192 FB | ~158 KiB |
| Nibble cache 35×6656 | ~228 KiB |

So on CYD we use a **reduced profile** (same core, compile/host flags):

- **64K main** always; aux/LC only if free heap allows
- Present as **RGB565** (or render text/hires directly into a line buffer — no full RGB888 FB)
- Disk: **`A2E_REDUCED_DISK`** — one nibblized track + raw image (or SD stream); `test_disk_reduced`
- Reuse invaders pinout / ST7789 bring-up knowledge; do **not** link invaders `components/board` as-is (different ABI)

Build the reduced disk unit test on the host:

```bash
make -C host/machine test_disk_reduced && ./host/machine/test_disk_reduced
```

## Layout (when porting)

```text
host/cyd/
  README.md
  CMakeLists.txt          ESP-IDF component / project hook
  main/main.c             a2e_host_ops → ST7789 + touch/SD
```

Port **after** Linux boots DOS with real ROMs. Copy verified ST7789 settings from
invaders (`swap_xy`, mirrors off, RGB, invert off, ~20 MHz, RGB565 byte-swap on TX).

Desktop smoke (no ESP-IDF):

```bash
make cyd-stub && ./host/cyd/a2e_cyd_stub
```

On device: wrap `host/cyd` as an ESP-IDF component, put `apple2e.rom`,
`diskii_boot.bin`, and a `.dsk` under `/sdcard/roms` and `/sdcard/disks`.

## Input sketch

- Resistive touch zones or PCF8575 on CN1 (SDA=27, SCL=22 @0x20) → `a2e_machine_key`
- Same expander pattern as invaders is fine; map to Apple keys, not Midway `KEY_*`
