# Desktop smoke build for CYD host (no ESP-IDF). Uses stub display/SD.
CC ?= gcc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra
ROOT := ../..
CPPFLAGS := -I$(ROOT)/core -I. -DA2E_REDUCED_DISK -DA2E_REDUCED_VIDEO

SRCS := \
	main/main.c \
	cyd_display.c \
	cyd_sd.c \
	$(ROOT)/core/cpu.c \
	$(ROOT)/core/mmu.c \
	$(ROOT)/core/disk.c \
	$(ROOT)/core/video.c \
	$(ROOT)/core/audio.c \
	$(ROOT)/core/machine.c

.PHONY: all clean run

all: a2e_cyd_stub

a2e_cyd_stub: $(SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRCS)

run: a2e_cyd_stub
	./a2e_cyd_stub

clean:
	$(RM) a2e_cyd_stub
