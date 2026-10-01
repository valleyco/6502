CC ?= gcc
CFLAGS ?= -std=c11 -O0 -g -Wall -Wextra
CORE := ../../core
CPPFLAGS := -I$(CORE) -I..

CPU_SRCS := $(CORE)/cpu.c

.PHONY: all test clean

all: test

test_cpu: test_cpu.c $(CPU_SRCS) ../test_assert.h $(CORE)/cpu.h $(CORE)/bus.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ test_cpu.c $(CPU_SRCS)

test: test_cpu
	./test_cpu

clean:
	$(RM) test_cpu
