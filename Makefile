# Apple IIe simulator (D1: C11, D13: TDD like invaders)
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -g -Icore
LDFLAGS ?=

CORE_SRCS = core/cpu.c core/mmu.c core/disk.c core/video.c core/audio.c core/machine.c
CORE_OBJS = $(CORE_SRCS:.c=.o)

.PHONY: all clean test test-cpu test-machine test-boot-dos test-boot-prodos test-boot-invaders fetch-asimov linux cyd-stub

all: test linux

core/%.o: core/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test-cpu:
	$(MAKE) -C host/cpu test

test-machine:
	$(MAKE) -C host/machine test

test: test-cpu test-machine

# Download Apple IIe ROM + Disk II P5 + DOS 3.3 + ProDOS into fixtures/asimov/
fetch-asimov:
	bash tools/fetch_asimov.sh

# Optional cold-boot smoke tests (skip if fixtures missing)
test-boot-dos:
	$(MAKE) -C host/machine test-boot-dos

test-boot-prodos:
	$(MAKE) -C host/machine test-boot-prodos

test-boot-invaders:
	$(MAKE) -C host/machine test-boot-invaders

# CYD host desktop stub (RGB565 rows + reduced disk; no ESP-IDF)
cyd-stub:
	$(MAKE) -C host/cyd

# Linux host (SDL2)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

host/linux/a2e: host/linux/main.c $(CORE_OBJS)
	@if [ -z "$(SDL_LIBS)" ]; then echo "SDL2 not found (pkg-config sdl2); skip host"; exit 1; fi
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ host/linux/main.c $(CORE_OBJS) $(SDL_LIBS) $(LDFLAGS)

linux: host/linux/a2e

clean:
	rm -f $(CORE_OBJS) host/linux/a2e
	$(MAKE) -C host/cpu clean
	$(MAKE) -C host/machine clean
	$(MAKE) -C host/cyd clean
