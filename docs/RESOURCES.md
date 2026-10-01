# External resources (ROMs, disks, docs)

This project **does not ship** Apple firmware or commercial DOS/ProDOS images.
Use the links below to find **documentation**, **community advice**, and
**legal alternatives**. What you download and how you use it is your
responsibility.

## 6502.org (CPU + community)

| Resource | URL | Use for this project |
|----------|-----|----------------------|
| Forum | [forum.6502.org](https://forum.6502.org/) | Emulator design, Apple II bring-up, ROM naming; threads often reference Asimov paths and AppleWin settings |
| Source snippets | [6502.org/source](http://6502.org/source/) | Algorithms (I/O, xmodem, etc.) — not Apple ROMs |
| 6502asm examples | [6502asm.com/examples](http://6502asm.com/examples) | **CPU regression** without Apple ROM (see also `host/cpu/`) |

The forum is where projects like PPS-2 and inindev’s Apple IIe JS discuss **not** bundling Apple ROMs and loading licensed images locally — same model as this repo.

## Apple II archives (reference / personal dumps)

Large mirrors exist; they contain **copyrighted** Apple and third-party software.
They are useful to **identify filenames** and read docs, not as an implied license to redistribute.

| Mirror | URL | Notes |
|--------|-----|--------|
| Asimov (primary) | [apple.asimov.net](https://www.apple.asimov.net/) | ROM collections, DOS 3.3 masters, `.dsk` / `.woz`; forum posts often cite `ftp.apple.asimov.net/pub/apple_II/...` |
| Asimov mirror | [mirrors.apple2.org.za](https://mirrors.apple2.org.za/ftp.apple.asimov.net/) | Same tree; good for browsing documentation |
| Masters index | [apple.asimov.net/images/masters/](https://www.apple.asimov.net/images/masters/) | e.g. “DOS 3.3 System Master”, “Apple DOS 3.3 August 1980.dsk” (143360 bytes) |

**Typical IIe firmware:** forum threads refer to packs like **II ROMs** (enhanced vs unenhanced matter for 65C02). Match ROM to machine: enhanced //e for our default `is_65c02` core.

**Legal context:** Apple’s ROM and DOS object code were held copyrightable (*Apple v. Franklin*). Apple community FAQ text on mirrors states DOS 3.3 is **not** “legally available online” from Apple’s perspective; many sites host images anyway. Prefer **images you dump from hardware you own** (ADTPro, KryoFlux, etc.) when possible.


## Fetch script (local only)

```bash
make fetch-asimov          # → fixtures/asimov/ (gitignored)
make test-boot-dos         # optional DOS cold-boot smoke; skips if fixtures missing
make test-boot-prodos      # optional ProDOS cold-boot smoke; skips if fixtures missing
```

Downloads from the Asimov HTTP mirror (default `mirrors.apple2.org.za`):

| Local file | Source |
|------------|--------|
| `fixtures/asimov/apple2e.rom` | `emulators/rom_images/apple_iie_rom.zip` (monitor bank + CX `$C100` merged) |
| `fixtures/asimov/diskii_boot.bin` | Disk II 16-sector P5 `341-0027` |
| `fixtures/asimov/dos33.dsk` | `images/masters/DOS 3.3 System Master - 680-0210-A (1982).dsk` |
| `fixtures/asimov/prodos.dsk` | `images/masters/prodos/ProDOS_2_4_1.dsk` |

Also copies into `roms/` / `disks/` for `./host/linux/a2e`. Override mirror with `ASIMOV_MIRROR=...`.

## Tools to create images you own

| Tool | URL | Role |
|------|-----|------|
| ADTPro | [adtpro.com](https://adtpro.com/) | Serial/USB transfer: real disk ↔ `.dsk` |
| WOZ / Applesauce | [applesaucefdc.com](https://applesaucefdc.com/) | Preservation-quality `.woz` (v1 scope is `.dsk`; WOZ later) |
| CiderPress / AppleCommander | various | Inspect/edit `.dsk` on the host |

Place resulting files under `roms/` and `disks/` (see those READMEs).

## Test media without Apple DOS (helpful for TDD / dev)

| Resource | License | Use |
|----------|---------|-----|
| [markadev/AppleII-test](https://github.com/markadev/AppleII-test) | MIT | Diagnostic `.dsk` (MODETEST, BANKTEST) — good for **MMU/banking/video** once you have *some* boot path |
| [6502_ror_bug_test](https://github.com/misterblack1/6502_ror_bug_test) | check repo | Small **test ROM** (not Apple firmware) for NMOS 6502 ROR quirk |
| In-repo tests | — | `make test` — CPU, disk encode/RWTS, clean-room boot (`host/machine/test_boot.c`) |

## Documentation (no ROM required)

- [Apple II History](https://apple2history.org/) — machine variants, ROM generations  
- [Apple II DOS FAQ (mirror)](https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/programming/basic/Apple%20II%20DOS%20%26%20Commands%20FAQ.txt) — DOS 3.3 behavior, RWTS, INIT  
- [Virtual ][ / AppleWin docs](https://github.com/AppleWin/AppleWin) — soft-switch behavior cross-check  

## What this repo does without Apple files

- **`a2e_machine_install_cleanroom_boot()`** — slot-6 path loads T0S0 via cycle-driven disk (`$C0F0` assist); see `roms/README.md`  
- **`make test`** — disk round-trip and boot simulation with synthetic `.dsk` bytes in tests  

When you add real `apple2e.rom` + DOS `.dsk`, run:

```bash
./host/linux/a2e --rom roms/apple2e.rom --disk disks/dos33.dsk
```

Optional: `--bootrom roms/diskii_boot.bin` if you have Apple’s Disk II PROM; otherwise the Linux host installs clean-room boot automatically.
