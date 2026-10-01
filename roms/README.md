# ROM and firmware images (not redistributed)

Apple firmware is copyrighted. Place your own dumps here (or use clean-room boot below).

**Where to look:** [docs/RESOURCES.md](../docs/RESOURCES.md) — [6502.org forum](https://forum.6502.org/), [Asimov mirrors](https://www.apple.asimov.net/), ADTPro, and legal test ROMs/disks.

| File | Size | Description |
|------|------|-------------|
| `apple2e.rom` | 16384 (`$C000-$FFFF`) or 12288 (`$D000-$FFFF`) | Apple IIe (enhanced preferred) firmware |
| `diskii_boot.bin` | 256 | Disk II P5 boot PROM (slot 6 `$C600`) — optional if using clean-room boot |
| `diskii_boot_cleanroom.bin` | 256 | Generated trampoline (`python3 tools/asm_boot_prom.py`) |

Clean-room boot (no Apple PROM): call `a2e_machine_install_cleanroom_boot()` — installs trampoline + page-3 loader that uses assist softswitch `$C0F0` (cycle-driven T0S0 → `$0800`). The Linux host uses this when `diskii_boot.bin` is missing.

Optional full 64K dumps are also accepted by the loader (bytes `$C000-$FFFF` used).

## Disk images

Place `.dsk` / `.po` under `disks/` — see [disks/README.md](../disks/README.md).

## Example

```bash
make linux
./host/linux/a2e --rom roms/apple2e.rom --disk disks/dos33.dsk --bootrom roms/diskii_boot.bin
```
