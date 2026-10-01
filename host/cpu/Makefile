CC ?= gcc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra
CORE := ../../core
CPPFLAGS := -I$(CORE) -I..
ASMX ?= asmx
ORG ?= 0x200
PYTHON ?= python3
GEN := ../../tools/gen_test_vectors.py

CPU_SRCS := $(CORE)/cpu.c
ASM := test.asm
ASM_LST := test.lst
ASM_BIN := test.bin
VECTORS := test_vectors.bin

.PHONY: all test test-smoke test-asm vectors dump-vectors clean

all: test

test_cpu: test_cpu.c $(CPU_SRCS) ../test_assert.h $(CORE)/cpu.h $(CORE)/bus.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_cpu.c $(CPU_SRCS)

test_runner: test_runner.c test_vectors.c test_vectors.h $(CPU_SRCS) $(CORE)/cpu.h $(CORE)/bus.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_runner.c test_vectors.c $(CPU_SRCS)

$(ASM_LST) $(ASM_BIN): $(ASM)
	$(ASMX) -C 65C02 -e -w -l $(ASM_LST) -o $(ASM_BIN) -b $(ORG) $(ASM)

$(VECTORS): $(ASM) $(ASM_LST) $(ASM_BIN) $(GEN)
	$(PYTHON) $(GEN) $(ASM) -o $(VECTORS)

vectors: $(VECTORS)

dump-vectors: $(VECTORS)
	$(PYTHON) -c "import struct,sys; d=open('$(VECTORS)','rb').read();\
print('magic',d[:4],'ver',d[4],'count',d[5]|d[6]<<8,'blob',int.from_bytes(d[7:11],'little'))"

test-smoke: test_cpu
	./test_cpu

test-asm: test_runner $(VECTORS)
	./test_runner $(VECTORS)

test: test-smoke test-asm

clean:
	$(RM) test_cpu test_runner $(ASM_LST) $(ASM_BIN)
