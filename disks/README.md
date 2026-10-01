# Disk images (not redistributed)

Place `.dsk` or `.po` images here for the Linux host:

```bash
./host/linux/a2e --rom ../roms/apple2e.rom --disk disks/dos33.dsk
```

V1 expects DOS 3.3 / ProDOS layout (140K per side, 143360 bytes for a full `.dsk`).

## Where to get images

See **[docs/RESOURCES.md](../docs/RESOURCES.md)** — Asimov mirrors, ADTPro (dump your own floppies), and MIT test disks like [AppleII-test](https://github.com/markadev/AppleII-test).

We do **not** download or commit Apple DOS masters or commercial software into this repository.

## Synthetic / test

`make test` builds disk images in memory for unit tests; no files required under `disks/` for CI-style runs.
