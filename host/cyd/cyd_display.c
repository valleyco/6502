#include "cyd_display.h"
#include "video.h"
#include <string.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "a2e_cyd_lcd";
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_lcd_done;
static u16 s_strip[A2E_CYD_PANEL_W];

static bool on_color_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *edata, void *ctx) {
    (void)io; (void)edata; (void)ctx;
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(s_lcd_done, &hpw);
    return hpw == pdTRUE;
}

static void wait_done(void) {
    xSemaphoreTake(s_lcd_done, portMAX_DELAY);
}

int a2e_cyd_display_init(void) {
    s_lcd_done = xSemaphoreCreateBinary();
    if (!s_lcd_done) return -1;

    gpio_config_t bk = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << A2E_CYD_PIN_BL,
    };
    ESP_ERROR_CHECK(gpio_config(&bk));
    gpio_set_level(A2E_CYD_PIN_BL, 0);

    spi_bus_config_t buscfg = {
        .sclk_io_num = A2E_CYD_PIN_SCLK,
        .mosi_io_num = A2E_CYD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = A2E_CYD_PANEL_W * sizeof(u16),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = A2E_CYD_PIN_CS,
        .dc_gpio_num = A2E_CYD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = A2E_CYD_PCLK_HZ,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_color_done,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                             &io_config, &io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = A2E_CYD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_config, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    a2e_cyd_display_fill_black();
    gpio_set_level(A2E_CYD_PIN_BL, 1);
    ESP_LOGI(TAG, "ST7789 ready %dx%d swap_xy RGB invert-off",
             A2E_CYD_PANEL_W, A2E_CYD_PANEL_H);
    return 0;
}

void a2e_cyd_display_fill_black(void) {
    if (!s_panel) return;
    memset(s_strip, 0, sizeof s_strip);
    for (int y = 0; y < A2E_CYD_PANEL_H; y++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
            s_panel, 0, y, A2E_CYD_PANEL_W, y + 1, s_strip));
        wait_done();
    }
}

void a2e_cyd_display_present_row(void *ctx, int y, const u16 *row, int width) {
    (void)ctx;
    if (!s_panel || !row || y < 0 || y >= A2E_CYD_PANEL_H) return;
    int w = width < A2E_CYD_PANEL_W ? width : A2E_CYD_PANEL_W;
    memcpy(s_strip, row, (size_t)w * sizeof(u16));
    if (w < A2E_CYD_PANEL_W)
        memset(s_strip + w, 0, (size_t)(A2E_CYD_PANEL_W - w) * sizeof(u16));
    a2e_rgb565_byteswap(s_strip, (size_t)A2E_CYD_PANEL_W);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        s_panel, 0, y, A2E_CYD_PANEL_W, y + 1, s_strip));
    wait_done();
}

#else /* !ESP_PLATFORM — host stub for desktop smoke */

static int s_rows_seen;
static u16 s_last_mid;

int a2e_cyd_display_init(void) {
    s_rows_seen = 0;
    s_last_mid = 0;
    fprintf(stderr, "a2e_cyd_display: stub init (no SPI)\n");
    return 0;
}

void a2e_cyd_display_fill_black(void) {}

void a2e_cyd_display_present_row(void *ctx, int y, const u16 *row, int width) {
    (void)ctx;
    if (!row || width <= 0) return;
    s_rows_seen++;
    if (y == A2E_CYD_OFF_Y)
        s_last_mid = row[A2E_CYD_OFF_X];
}

/* test hooks */
int a2e_cyd_display_stub_rows(void) { return s_rows_seen; }
u16 a2e_cyd_display_stub_mid(void) { return s_last_mid; }

#endif
