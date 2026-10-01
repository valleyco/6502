#include "cyd_sd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"

static const char *TAG = "a2e_cyd_sd";
static sdmmc_card_t *s_card;
static char s_mount[64];

int a2e_cyd_sd_mount(const char *mount_point) {
    if (!mount_point) mount_point = "/sdcard";
    strncpy(s_mount, mount_point, sizeof s_mount - 1);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = A2E_CYD_SD_MOSI,
        .miso_io_num = A2E_CYD_SD_MISO,
        .sclk_io_num = A2E_CYD_SD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(ret));
        return -1;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = A2E_CYD_SD_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(s_mount, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mount fail: %s", esp_err_to_name(ret));
        return -1;
    }
    ESP_LOGI(TAG, "SD mounted at %s", s_mount);
    return 0;
}

void a2e_cyd_sd_unmount(void) {
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(s_mount, s_card);
        s_card = NULL;
    }
}

#else /* desktop: plain fopen from host paths */

int a2e_cyd_sd_mount(const char *mount_point) {
    (void)mount_point;
    return 0;
}

void a2e_cyd_sd_unmount(void) {}

#endif

u8 *a2e_cyd_sd_load(const char *path, size_t *out_len) {
    if (!path || !out_len) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    rewind(f);
    u8 *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_len = (size_t)n;
    return buf;
}
