#ifndef A2E_HOST_H
#define A2E_HOST_H

#include "types.h"

/*
 * Thin host ABI (D3). Core owns all timing; host presents frames,
 * feeds input, and loads disk/ROM bytes.
 */

typedef struct a2e_host_ops {
    /* Present RGB888 framebuffer; w/h typically 280x192. May be NULL if using rows. */
    void (*present)(void *ctx, const u8 *rgb, int w, int h, int stride_bytes);

    /*
     * Optional CYD path: one panel scanline of host-endian RGB565
     * (width = A2E_CYD_PANEL_W). Host byteswaps for SPI if needed.
     */
    void (*present_rgb565_row)(void *ctx, int y, const u16 *row, int width);

    /* Optional: sample sink (1-bit speaker toggles converted by host). */
    void (*audio_push)(void *ctx, const i16 *samples, int count);

    /* Wall-clock sleep / yield (ms). May be NULL. */
    void (*sleep_ms)(void *ctx, u32 ms);

    void *ctx;
} a2e_host_ops;

#endif /* A2E_HOST_H */
