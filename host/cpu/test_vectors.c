#include "test_vectors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_u16(const uint8_t **p, const uint8_t *end, uint16_t *out) {
    if ((size_t)(end - *p) < 2) return -1;
    *out = (uint16_t)(*p)[0] | ((uint16_t)(*p)[1] << 8);
    *p += 2;
    return 0;
}

static int read_state(const uint8_t **p, const uint8_t *end, tv_cpu_state_t *st) {
    if ((size_t)(end - *p) < TV_STATE_SIZE) return -1;
    memcpy(st, *p, TV_STATE_SIZE);
    *p += TV_STATE_SIZE;
    return 0;
}

static int read_mem_pairs(const uint8_t **p, const uint8_t *end, tv_mem_t **out,
                          uint16_t count) {
    *out = NULL;
    if (count == 0) return 0;
    *out = calloc(count, sizeof(tv_mem_t));
    if (!*out) return -1;
    for (uint16_t i = 0; i < count; i++) {
        if ((size_t)(end - *p) < 3) {
            free(*out);
            *out = NULL;
            return -1;
        }
        (*out)[i].addr = (uint16_t)((*p)[0] | ((*p)[1] << 8));
        (*out)[i].val = (*p)[2];
        *p += 3;
    }
    return 0;
}

int tv_load_file(const char *path, tv_file_t *out) {
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    uint8_t *data = malloc((size_t)sz);
    if (!data) {
        fclose(f);
        return -1;
    }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    if (sz < 11 || memcmp(data, TV_MAGIC, 4) != 0) {
        free(data);
        return -1;
    }
    if (data[4] != TV_VERSION) {
        free(data);
        return -1;
    }

    uint16_t count = (uint16_t)(data[5] | (data[6] << 8));
    uint32_t blob_size = (uint32_t)data[7] | ((uint32_t)data[8] << 8) |
                         ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 24);

    const uint8_t *p = data + 11;
    const uint8_t *end = data + sz;

    tv_test_t *tests = calloc(count, sizeof(tv_test_t));
    if (!tests) {
        free(data);
        return -1;
    }

    for (uint16_t i = 0; i < count; i++) {
        tv_test_t *t = &tests[i];
        uint16_t name_len;
        if (read_u16(&p, end, &name_len) != 0) goto fail;
        if ((size_t)(end - p) < name_len) goto fail;
        t->name = malloc(name_len + 1);
        if (!t->name) goto fail;
        memcpy(t->name, p, name_len);
        t->name[name_len] = '\0';
        p += name_len;

        if ((size_t)(end - p) < 22) goto fail;
        t->start_pc = (uint16_t)(p[0] | (p[1] << 8));
        t->end_pc = (uint16_t)(p[2] | (p[3] << 8));
        t->code_off = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) |
                      ((uint32_t)p[7] << 24);
        t->code_len = (uint32_t)p[8] | ((uint32_t)p[9] << 8) | ((uint32_t)p[10] << 16) |
                      ((uint32_t)p[11] << 24);
        t->init_mask = (uint32_t)p[12] | ((uint32_t)p[13] << 8) | ((uint32_t)p[14] << 16) |
                       ((uint32_t)p[15] << 24);
        t->expect_mask = (uint32_t)p[16] | ((uint32_t)p[17] << 8) | ((uint32_t)p[18] << 16) |
                         ((uint32_t)p[19] << 24);
        t->isa = (uint16_t)(p[20] | (p[21] << 8));
        p += 22;

        if (read_state(&p, end, &t->init) != 0) goto fail;
        if (read_state(&p, end, &t->expect) != 0) goto fail;
        if (read_u16(&p, end, &t->mem_count) != 0) goto fail;
        if (read_mem_pairs(&p, end, &t->mem, t->mem_count) != 0) goto fail;
        if (read_u16(&p, end, &t->expect_mem_count) != 0) goto fail;
        if (read_mem_pairs(&p, end, &t->expect_mem, t->expect_mem_count) != 0) goto fail;
    }

    if ((size_t)(end - p) < blob_size) goto fail;
    out->code_blob = malloc(blob_size ? blob_size : 1);
    if (!out->code_blob) goto fail;
    memcpy(out->code_blob, p, blob_size);
    out->code_blob_size = blob_size;
    out->tests = tests;
    out->count = count;
    free(data);
    return 0;

fail:
    for (uint16_t i = 0; i < count; i++) {
        free(tests[i].name);
        free(tests[i].mem);
        free(tests[i].expect_mem);
    }
    free(tests);
    free(data);
    return -1;
}

void tv_free_file(tv_file_t *f) {
    if (!f) return;
    if (f->tests) {
        for (uint16_t i = 0; i < f->count; i++) {
            free(f->tests[i].name);
            free(f->tests[i].mem);
            free(f->tests[i].expect_mem);
        }
        free(f->tests);
    }
    free(f->code_blob);
    memset(f, 0, sizeof(*f));
}
