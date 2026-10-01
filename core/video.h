#ifndef A2E_VIDEO_INCLUDED
#define A2E_VIDEO_INCLUDED

#include "types.h"

#define A2E_VIDEO_W 280
#define A2E_VIDEO_H 192

/* CYD panel (ST7789 landscape) — Apple frame is centered inside */
#define A2E_CYD_PANEL_W 320
#define A2E_CYD_PANEL_H 240
#define A2E_CYD_OFF_X   ((A2E_CYD_PANEL_W - A2E_VIDEO_W) / 2)  /* 20 */
#define A2E_CYD_OFF_Y   ((A2E_CYD_PANEL_H - A2E_VIDEO_H) / 2)  /* 24 */

typedef struct a2e_mmu a2e_mmu;

typedef struct a2e_video {
#ifndef A2E_REDUCED_VIDEO
    u8 fb[A2E_VIDEO_H][A2E_VIDEO_W][3]; /* RGB888 simplified (D5) */
#endif
    bool dirty;
} a2e_video;

void a2e_video_init(a2e_video *v);
void a2e_video_render(a2e_video *v, a2e_mmu *m);

/* RGB565 helpers (CYD / D6 — no full second framebuffer required). */
u16  a2e_rgb_to_565(u8 r, u8 g, u8 b);
void a2e_rgb565_byteswap(u16 *buf, size_t count);

/* Render one Apple scanline as host-endian RGB565 (A2E_VIDEO_W pixels). */
void a2e_video_render_row_rgb565(a2e_mmu *m, int y, u16 *out);

/*
 * Fill a CYD panel-width strip: black borders + centered Apple row.
 * out has A2E_CYD_PANEL_W pixels; apple_y is 0..A2E_VIDEO_H-1.
 */
void a2e_video_cyd_panel_row(a2e_mmu *m, int panel_y, u16 *out);

#endif /* A2E_VIDEO_INCLUDED */
