#!/usr/bin/env python3
"""Generate host/cpu/test_vectors.bin from asmx .asm + .lst + .bin."""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

MEM_PAIR_RE = re.compile(r"(\$?[0-9a-f]+h?)\s*=\s*(\$?[0-9a-f]+h?)", re.I)
ORG_RE = re.compile(r"^\s*ORG\s+(\$?[0-9a-f]+h?|0x[0-9a-f]+)\s*$", re.I)
# asmx listing: "0200                    lda_imm:"
LST_LABEL_RE = re.compile(r"^([0-9A-Fa-f]{4})\s+(\w+):\s*$")
# asmx listing: "0200  A9 42                     LDA     #$42"
LST_CODE_RE = re.compile(
    r"^([0-9A-Fa-f]{4})\s+((?:[0-9A-Fa-f]{2})(?:\s+[0-9A-Fa-f]{2})*)\s{2,}(.*)$"
)

MASK = {
    "a": 1 << 0,
    "x": 1 << 1,
    "y": 1 << 2,
    "sp": 1 << 3,
    "p": 1 << 4,
    "pc": 1 << 5,
    "halted": 1 << 6,
    "flag.c": 1 << 8,
    "flag.z": 1 << 9,
    "flag.i": 1 << 10,
    "flag.d": 1 << 11,
    "flag.v": 1 << 12,
    "flag.n": 1 << 13,
}

# Match A2E_FLAG_* in core/cpu.h
FLAG_BITS = {
    "flag.c": 0x01,
    "flag.z": 0x02,
    "flag.i": 0x04,
    "flag.d": 0x08,
    "flag.v": 0x40,
    "flag.n": 0x80,
}

ISA_NAMES = {
    "6502": 0,
    "65c02": 1,
    "65C02": 1,
}


@dataclass
class CpuState:
    a: int = 0
    x: int = 0
    y: int = 0
    sp: int = 0xFD
    p: int = 0x24  # U|I
    pc: int = 0
    flag_bits: int = 0


@dataclass
class TestCase:
    name: str
    label: str = ""
    start_pc: int = 0
    end_pc: int = 0
    init_mask: int = 0
    expect_mask: int = 0
    isa: int = 1  # default 65C02
    init: CpuState = field(default_factory=CpuState)
    expect: CpuState = field(default_factory=CpuState)
    mem: list[tuple[int, int]] = field(default_factory=list)
    expect_mem: list[tuple[int, int]] = field(default_factory=list)


def parse_int(s: str) -> int:
    s = s.strip().lower()
    if s.endswith("h"):
        return int(s[:-1], 16)
    if s.startswith("0x"):
        return int(s, 16)
    if s.startswith("$"):
        return int(s[1:], 16)
    if s in ("0", "1"):
        return int(s)
    return int(s, 0)


def parse_pairs(text: str) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    for part in re.split(r"\s+", text.strip()):
        if not part or "=" not in part:
            continue
        k, v = part.split("=", 1)
        pairs.append((k.strip().lower(), v.strip()))
    return pairs


def apply_field(state: CpuState, mask: int, key: str, val: int) -> int:
    key = key.lower()
    if key.startswith("flag."):
        if key not in FLAG_BITS:
            raise ValueError(f"unknown flag {key!r}")
        if val:
            state.flag_bits |= FLAG_BITS[key]
        else:
            state.flag_bits &= ~FLAG_BITS[key]
        return mask | MASK[key]
    if key == "halted":
        return mask | MASK["halted"]
    reg_map = {"a", "x", "y", "sp", "p", "pc"}
    if key not in reg_map:
        raise ValueError(f"unknown field {key!r}")
    width = 0xFFFF if key == "pc" else 0xFF
    setattr(state, key, val & width)
    return mask | MASK[key]


def parse_asm(path: Path) -> tuple[list[TestCase], int]:
    tests: list[TestCase] = []
    current: TestCase | None = None
    org = 0x200

    for raw in path.read_text().splitlines():
        code_part = raw.split(";", 1)[0].strip()
        om = ORG_RE.match(code_part)
        if om:
            org = parse_int(om.group(1))

        line = raw.split(";", 1)
        comment = line[1].strip() if len(line) > 1 else ""

        if not comment.startswith("@"):
            if code_part.endswith(":") and not code_part.startswith("."):
                lbl = code_part[:-1].strip().split()[-1]
                if current is not None and not current.label:
                    current.label = lbl
            continue

        body = comment[1:].strip()
        if body.startswith("test "):
            name = body[5:].strip()
            if not name:
                raise ValueError("empty @test name")
            current = TestCase(name=name)
            tests.append(current)
            continue

        if current is None:
            raise ValueError(f"directive before @test: {body}")

        if body.startswith("init "):
            for k, v in parse_pairs(body[5:]):
                if k.lower() == "isa":
                    key = v.strip()
                    key_l = key.lower()
                    if key_l not in ISA_NAMES and key not in ISA_NAMES:
                        raise ValueError(f"unknown isa {v!r}")
                    current.isa = ISA_NAMES.get(key, ISA_NAMES[key_l])
                    continue
                current.init_mask = apply_field(
                    current.init, current.init_mask, k, parse_int(v)
                )
        elif body.startswith("expect "):
            for k, v in parse_pairs(body[7:]):
                current.expect_mask = apply_field(
                    current.expect, current.expect_mask, k, parse_int(v)
                )
        elif body.startswith("mem "):
            pairs = MEM_PAIR_RE.findall(body[4:].strip())
            if not pairs:
                raise ValueError(f"bad @mem line: {body}")
            for addr_s, val_s in pairs:
                current.mem.append((parse_int(addr_s), parse_int(val_s)))
        elif body.startswith("expect_mem "):
            pairs = MEM_PAIR_RE.findall(body[11:].strip())
            if not pairs:
                raise ValueError(f"bad @expect_mem line: {body}")
            for addr_s, val_s in pairs:
                current.expect_mem.append((parse_int(addr_s), parse_int(val_s)))

    return tests, org


def parse_lst_labels(path: Path) -> dict[str, int]:
    """asmx listing addresses are absolute (already include ORG)."""
    labels: dict[str, int] = {}
    for raw in path.read_text().splitlines():
        lm = LST_LABEL_RE.match(raw)
        if lm:
            labels[lm.group(2).lower()] = int(lm.group(1), 16)
    return labels


def find_end_pc(lines: list[str], start: int, next_start: int) -> int:
    """Exclusive end PC for a test (prefer next test start)."""
    if next_start < 0x10000 and next_start > start:
        return next_start
    end = None
    for raw in lines:
        m = LST_CODE_RE.match(raw)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if addr < start:
            continue
        hex_bytes = m.group(2).split()
        asm = m.group(3)
        # Halt = STP opcode $DB (DB $DB in source)
        if hex_bytes == ["DB"] or re.search(r"\bSTP\b", asm, re.I):
            end = addr + len(hex_bytes)
    return end if end is not None else next_start


def pack_state(st: CpuState) -> bytes:
    # a,x,y,sp,p, pad[3], pc, flag_bits  => 12 bytes
    return struct.pack(
        "<5B3xHH",
        st.a & 0xFF,
        st.x & 0xFF,
        st.y & 0xFF,
        st.sp & 0xFF,
        st.p & 0xFF,
        st.pc & 0xFFFF,
        st.flag_bits & 0xFFFF,
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate 6502/65C02 test vectors binary")
    ap.add_argument("asm", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=Path("test_vectors.bin"))
    args = ap.parse_args()

    asm_path = args.asm
    lst_path = asm_path.with_suffix(".lst")
    bin_path = asm_path.with_suffix(".bin")
    if not lst_path.is_file() or not bin_path.is_file():
        print(f"error: need {lst_path} and {bin_path}", file=sys.stderr)
        return 1

    tests, org = parse_asm(asm_path)
    if not tests:
        print("error: no tests found", file=sys.stderr)
        return 1

    labels = parse_lst_labels(lst_path)
    lst_lines = lst_path.read_text().splitlines()
    bin_data = bin_path.read_bytes()

    names_seen: set[str] = set()
    code_blob = bytearray()
    records: list[bytes] = []

    for tc in tests:
        if tc.name in names_seen:
            print(f"error: duplicate test {tc.name!r}", file=sys.stderr)
            return 1
        names_seen.add(tc.name)
        lbl = (tc.label or tc.name).lower()
        if lbl not in labels:
            print(f"error: label {lbl!r} not in listing", file=sys.stderr)
            return 1
        tc.start_pc = labels[lbl]

    for i, tc in enumerate(tests):
        next_start = tests[i + 1].start_pc if i + 1 < len(tests) else org + len(bin_data)
        tc.end_pc = find_end_pc(lst_lines, tc.start_pc, next_start)
        code_len = tc.end_pc - tc.start_pc
        if code_len <= 0:
            print(f"error: test {tc.name}: bad code length", file=sys.stderr)
            return 1
        file_off = tc.start_pc - org
        file_end = tc.end_pc - org
        if file_off < 0 or file_end > len(bin_data):
            print(
                f"error: test {tc.name}: slice outside bin "
                f"(off={file_off}:{file_end} bin={len(bin_data)} org={org:#x})",
                file=sys.stderr,
            )
            return 1
        code_slice = bin_data[file_off:file_end]
        code_off = len(code_blob)
        code_blob.extend(code_slice)

        if not (tc.init_mask & MASK["pc"]):
            tc.init.pc = tc.start_pc
            tc.init_mask |= MASK["pc"]

        name_b = tc.name.encode("utf-8")
        rec = struct.pack("<H", len(name_b)) + name_b
        rec += struct.pack(
            "<HHIIIIH",
            tc.start_pc,
            tc.end_pc,
            code_off,
            code_len,
            tc.init_mask,
            tc.expect_mask,
            tc.isa & 0xFFFF,
        )
        rec += pack_state(tc.init)
        rec += pack_state(tc.expect)
        rec += struct.pack("<H", len(tc.mem))
        for addr, val in tc.mem:
            rec += struct.pack("<HB", addr & 0xFFFF, val & 0xFF)
        rec += struct.pack("<H", len(tc.expect_mem))
        for addr, val in tc.expect_mem:
            rec += struct.pack("<HB", addr & 0xFFFF, val & 0xFF)
        records.append(rec)

    out = bytearray(b"A2ET")
    out += struct.pack("<BHI", 1, len(tests), len(code_blob))
    for rec in records:
        out += rec
    out += code_blob
    args.output.write_bytes(out)
    print(
        f"Wrote {args.output} ({len(tests)} tests, {len(code_blob)} code bytes, org={org:#x})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
