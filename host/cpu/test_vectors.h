#ifndef A2E_TEST_VECTORS_H
#define A2E_TEST_VECTORS_H

#include <stdint.h>

#define TV_MAGIC "A2ET"
#define TV_VERSION 1

#define TV_MASK_A (1u << 0)
#define TV_MASK_X (1u << 1)
#define TV_MASK_Y (1u << 2)
#define TV_MASK_SP (1u << 3)
#define TV_MASK_P (1u << 4)
#define TV_MASK_PC (1u << 5)
#define TV_MASK_HALTED (1u << 6)

#define TV_MASK_FLAG_C (1u << 8)
#define TV_MASK_FLAG_Z (1u << 9)
#define TV_MASK_FLAG_I (1u << 10)
#define TV_MASK_FLAG_D (1u << 11)
#define TV_MASK_FLAG_V (1u << 12)
#define TV_MASK_FLAG_N (1u << 13)

#define TV_STATE_SIZE 12
#define TV_MAX_STEPS 100000u

#define TV_ISA_6502 0
#define TV_ISA_65C02 1

typedef struct {
    uint8_t a, x, y, sp, p;
    uint8_t _pad[3];
    uint16_t pc;
    uint16_t flag_bits;
} tv_cpu_state_t;

typedef struct {
    uint16_t addr;
    uint8_t val;
} tv_mem_t;

typedef struct {
    char *name;
    uint16_t start_pc;
    uint16_t end_pc;
    uint32_t code_off;
    uint32_t code_len;
    uint32_t init_mask;
    uint32_t expect_mask;
    uint16_t isa;
    tv_cpu_state_t init;
    tv_cpu_state_t expect;
    tv_mem_t *mem;
    uint16_t mem_count;
    tv_mem_t *expect_mem;
    uint16_t expect_mem_count;
} tv_test_t;

typedef struct {
    tv_test_t *tests;
    uint16_t count;
    uint8_t *code_blob;
    uint32_t code_blob_size;
} tv_file_t;

int tv_load_file(const char *path, tv_file_t *out);
void tv_free_file(tv_file_t *f);

#endif /* A2E_TEST_VECTORS_H */
