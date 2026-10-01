#ifndef A2E_CYD_DISPLAY_H
#define A2E_CYD_DISPLAY_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ST7789 320×240 landscape — pins match invaders CYD V3 BOARD.md */
#define A2E_CYD_LCD_HOST_SPI  2
#define A2E_CYD_PIN_SCLK      14
#define A2E_CYD_PIN_MOSI      13
#define A2E_CYD_PIN_CS        15
#define A2E_CYD_PIN_DC         2
#define A2E_CYD_PIN_BL        21
#define A2E_CYD_PIN_RST       (-1)
#define A2E_CYD_PCLK_HZ       (20 * 1000 * 1000)

int  a2e_cyd_display_init(void);
void a2e_cyd_display_present_row(void *ctx, int y, const u16 *row, int width);
void a2e_cyd_display_fill_black(void);

#ifdef __cplusplus
}
#endif

#endif /* A2E_CYD_DISPLAY_H */
