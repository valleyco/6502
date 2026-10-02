CC ?= gcc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra
CORE := ../../core
CPPFLAGS := -I$(CORE) -I..

CORE_SRCS := \
	$(CORE)/cpu.c \
	$(CORE)/mmu.c \
	$(CORE)/disk.c \
	$(CORE)/video.c \
	$(CORE)/audio.c \
	$(CORE)/machine.c

TEST_BINS := test_disk test_mmu test_disk_rwts test_boot test_disk_latch test_disk_po test_disk_reduced test_video_rgb565 test_video_modes test_audio

.PHONY: all test clean test-boot-dos test-boot-prodos test-boot-invaders

all: test

test_disk: test_disk.c $(CORE)/disk.c ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_disk.c $(CORE)/disk.c

test_disk_po: test_disk_po.c $(CORE)/disk.c ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_disk_po.c $(CORE)/disk.c

test_disk_reduced: test_disk_reduced.c $(CORE)/disk.c ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -DA2E_REDUCED_DISK -o $@ test_disk_reduced.c $(CORE)/disk.c

test_video_rgb565: test_video_rgb565.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_video_rgb565.c $(CORE_SRCS)

test_video_modes: test_video_modes.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_video_modes.c $(CORE_SRCS)

test_audio: test_audio.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_audio.c $(CORE_SRCS)

test_disk_rwts: test_disk_rwts.c $(CORE)/disk.c ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_disk_rwts.c $(CORE)/disk.c

test_disk_latch: test_disk_latch.c $(CORE)/disk.c ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_disk_latch.c $(CORE)/disk.c

test_boot: test_boot.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_boot.c $(CORE_SRCS)

test_boot_dos: test_boot_dos.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_boot_dos.c $(CORE_SRCS)

test_boot_prodos: test_boot_prodos.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_boot_prodos.c $(CORE_SRCS)

test_boot_invaders: test_boot_invaders.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_boot_invaders.c $(CORE_SRCS)

test_mmu: test_mmu.c $(CORE_SRCS) ../test_assert.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_mmu.c $(CORE_SRCS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || exit 1; done

# Optional: needs fixtures from `make fetch-asimov` (skips cleanly if missing)
test-boot-dos: test_boot_dos
	./test_boot_dos

test-boot-prodos: test_boot_prodos
	./test_boot_prodos

test-boot-invaders: test_boot_invaders
	./test_boot_invaders

clean:
	$(RM) $(TEST_BINS) test_boot_dos test_boot_prodos test_boot_invaders
