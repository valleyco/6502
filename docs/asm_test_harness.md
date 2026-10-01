# ASM-driven 6502/65C02 test harness

Assembly-authored ISA tests (**asmx**), comment metadata, binary vectors, C
runner. Same method as `../8086pc/i8086` (see that repo’s
`docs/asm_test_harness.md`). Locked as **D14**.

## Workflow

```text
host/cpu/test.asm  (; @test / @init / @expect / @mem)
    → asmx -C 65C02 -e -w -l test.lst -o test.bin -b 0x200
    → test.bin + test.lst
    → tools/gen_test_vectors.py
    → host/cpu/test_vectors.bin
    → host/cpu/test_runner
```

```bash
make test-cpu                # C smoke + ISA vectors
make -C host/cpu vectors     # regenerate (requires asmx on PATH)
make -C host/cpu dump-vectors
```

Requires **asmx** (multi-CPU assembler; `-C 6502` / `65C02`) to regenerate.
Committed `test_vectors.bin` lets CI / default `make test` run without asmx.

## Comment DSL

| Directive | Purpose |
|-----------|---------|
| `@test name` | Start a test; next label is `start_pc` |
| `@init key=val ...` | CPU state before run (only listed fields). `isa=6502\|65c02` (default 65c02) |
| `@mem addr=val` | Preload 16-bit address (repeatable) |
| `@expect key=val ...` | Post-run checks |
| `@expect_mem addr=val` | Post-run memory check |

Each test should end with halt = **STP** opcode `$DB`. asmx’s `STP` mnemonic is
65C816-only, so write:

```asm
        DB      $DB             ; halt (STP)
```

Vector bounds run through the **next** test’s start (so helpers after an early
halt are still included).

### Registers / flags

- 8-bit: `a x y sp p`
- 16-bit: `pc`
- Flags: `flag.c flag.z flag.i flag.d flag.v flag.n` (6502 P bits)
- Meta: `halted=1`

Values: `$42`, `0x42`, `42h`, decimal.

### Example

```asm
; @test lda_imm
; @expect a=$42 flag.z=0 flag.n=0
lda_imm:
        LDA     #$42
        DB      $DB             ; halt (STP)
```

`ORG $0200` is the default base; asmx listing addresses are absolute.
`-b 0x200` must match the `ORG` so the binary is unpadded from that address.

## Binary format

Magic `A2ET`, version **1**. Per-test records + code blob (see
`host/cpu/test_vectors.h`). Mem pairs are `u16 addr + u8 val`.
