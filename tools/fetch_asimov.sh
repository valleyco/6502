#!/usr/bin/env bash
# Fetch Apple IIe ROM + Disk II P5 + DOS 3.3 master from an Asimov HTTP mirror
# into a gitignored fixtures/ directory for local testing.
#
# Copyright: Apple firmware and DOS remain copyrighted. This script only
# downloads for personal/local use; binaries are NOT committed to the repo.
# Prefer dumps from hardware you own when possible (see docs/RESOURCES.md).
#
# Usage:
#   ./tools/fetch_asimov.sh
#   ASIMOV_MIRROR=https://www.apple.asimov.net ./tools/fetch_asimov.sh
#   ASIMOV_FORCE=1 ./tools/fetch_asimov.sh   # re-download even if present
#   make fetch-asimov

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ASIMOV_DIR:-$ROOT/fixtures/asimov}"
MIRROR="${ASIMOV_MIRROR:-https://mirrors.apple2.org.za/ftp.apple.asimov.net}"
FORCE="${ASIMOV_FORCE:-0}"
WORKDIR="$OUT/.download"
mkdir -p "$OUT" "$WORKDIR"
export OUT WORKDIR

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: need '$1' on PATH" >&2
    exit 1
  }
}
need curl
need unzip
need python3

# Skip download when dest exists (non-empty) unless ASIMOV_FORCE=1.
fetch() {
  local url="$1" dest="$2"
  if [[ "$FORCE" != "1" && -s "$dest" ]]; then
    echo "skip (exists): $dest"
    return 0
  fi
  echo "GET $url"
  curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 180 \
    -o "$dest" "$url"
}

have_rom() {
  [[ "$FORCE" != "1" && -s "$OUT/apple2e.rom" && $(wc -c <"$OUT/apple2e.rom") -eq 16384 ]]
}

echo "Asimov mirror: $MIRROR"
echo "Output:        $OUT"
[[ "$FORCE" == "1" ]] && echo "Force:         re-download enabled"
echo

# --- IIe firmware (32K dump → first 16K = $C000-$FFFF) ---
if have_rom; then
  echo "skip (exists): $OUT/apple2e.rom"
else
  fetch "$MIRROR/emulators/rom_images/apple_iie_rom.zip" "$WORKDIR/apple_iie_rom.zip"
  unzip -qo "$WORKDIR/apple_iie_rom.zip" -d "$WORKDIR/iie_rom"
  python3 - <<'PY'
from pathlib import Path
import os
workdir = os.environ["WORKDIR"]
out_dir = os.environ["OUT"]
src = Path(workdir) / "iie_rom"
cands = list(src.rglob("*.ROM")) + list(src.rglob("*.rom")) + list(src.rglob("*.bin"))
if not cands:
    raise SystemExit("no ROM file in apple_iie_rom.zip")
data = cands[0].read_bytes()
out = Path(out_dir) / "apple2e.rom"
if len(data) == 32768:
    # Bank0 = monitor; bank1 holds CX firmware at $C100-$CFFF. Merge both.
    merged = bytearray(data[:16384])
    merged[0x100:0x1000] = data[16384 + 0x100 : 16384 + 0x1000]
    out.write_bytes(merged)
elif len(data) in (16384, 12288):
    out.write_bytes(data)
elif len(data) >= 65536:
    out.write_bytes(data[0xC000:0x10000])
else:
    raise SystemExit(f"unexpected ROM size {len(data)} from {cands[0].name}")
cx = out.read_bytes()[0x100]
print(f"wrote {out} ({out.stat().st_size} bytes) from {cands[0].name} (C100={cx:02X})")
PY
fi

# --- Disk II 16-sector P5 boot PROM ($C600, 256 bytes) ---
# Prefer the "D4-D7 bits swapped" dump — that one is code-ordered (starts A2 20).
# The plain 341-0027.bin on Asimov is bit-scrambled and will not boot.
P5_URL="$MIRROR/emulators/rom_images/Apple%20Disk%20II%2016%20Sector%20Interface%20Card%20ROM%20P5%20-%20341-0027.bin-with-D4-D7%20data%20bits%20swapped.bin"
fetch "$P5_URL" "$OUT/diskii_boot.bin"
python3 - <<'PY'
from pathlib import Path
import os
p = Path(os.environ["OUT"]) / "diskii_boot.bin"
data = p.read_bytes()
if len(data) != 256:
    raise SystemExit(f"diskii_boot.bin expected 256 bytes, got {len(data)}")
if data[:2] != bytes([0xA2, 0x20]):
    raise SystemExit(f"diskii_boot.bin does not look like P5 code (got {data[:4].hex()})")
print(f"ok {p} ({len(data)} bytes) P5 starts {data[:4].hex()}")
PY

# --- DOS 3.3 System Master ---
fetch "$MIRROR/images/masters/DOS%203.3%20System%20Master%20-%20680-0210-A%20%281982%29.dsk" \
  "$OUT/dos33.dsk"
python3 - <<'PY'
from pathlib import Path
import os
p = Path(os.environ["OUT"]) / "dos33.dsk"
n = p.stat().st_size
if n < 35 * 16 * 256:
    raise SystemExit(f"dos33.dsk too small: {n}")
print(f"ok {p} ({n} bytes)")
PY

# --- ProDOS system disk (DOS-order .dsk; bootable via Disk II) ---
fetch "$MIRROR/images/masters/prodos/ProDOS_2_4_1.dsk" "$OUT/prodos.dsk"
python3 - <<'PY'
from pathlib import Path
import os
p = Path(os.environ["OUT"]) / "prodos.dsk"
n = p.stat().st_size
if n < 35 * 16 * 256:
    raise SystemExit(f"prodos.dsk too small: {n}")
print(f"ok {p} ({n} bytes)")
PY

# --- Keyboard invaders compilation (graphic-game smoke, Step 10) ---
INVADERS_URL="$MIRROR/images/games/file_based/appleinvaders_keyboardappleinvaders_galaxywars_invasionforce_stellarinvaders_superinvader.dsk"
fetch "$INVADERS_URL" "$OUT/invaders.dsk"
python3 - <<'PY'
from pathlib import Path
import os
p = Path(os.environ["OUT"]) / "invaders.dsk"
n = p.stat().st_size
if n < 35 * 16 * 256:
    raise SystemExit(f"invaders.dsk too small: {n}")
print(f"ok {p} ({n} bytes)")
PY

# Convenience copies into roms/ and disks/ (also gitignored by pattern)
mkdir -p "$ROOT/roms" "$ROOT/disks"
cp -f "$OUT/apple2e.rom" "$ROOT/roms/apple2e.rom"
cp -f "$OUT/diskii_boot.bin" "$ROOT/roms/diskii_boot.bin"
cp -f "$OUT/dos33.dsk" "$ROOT/disks/dos33.dsk"
cp -f "$OUT/prodos.dsk" "$ROOT/disks/prodos.dsk"
cp -f "$OUT/invaders.dsk" "$ROOT/disks/invaders.dsk"

echo
echo "Also copied to roms/ and disks/ for the Linux host."
echo
echo "Done. Optional boot tests:"
echo "  make test-boot-dos"
echo "  make test-boot-prodos"
echo "  make test-boot-invaders"
echo "Run emulator:"
echo "  ./host/linux/a2e --rom roms/apple2e.rom --disk disks/dos33.dsk"
echo "  ./host/linux/a2e --rom roms/apple2e.rom --disk disks/invaders.dsk"
echo "Re-download everything: ASIMOV_FORCE=1 make fetch-asimov"
