#ifndef A2E_CYD_SD_H
#define A2E_CYD_SD_H

#include "types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Micro-SD on VSPI — invaders CYD V3 */
#define A2E_CYD_SD_MOSI 23
#define A2E_CYD_SD_MISO 19
#define A2E_CYD_SD_SCLK 18
#define A2E_CYD_SD_CS    5

int a2e_cyd_sd_mount(const char *mount_point);
void a2e_cyd_sd_unmount(void);

/* Load entire file into malloc'd buffer; caller frees. Returns NULL on fail. */
u8 *a2e_cyd_sd_load(const char *path, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* A2E_CYD_SD_H */
