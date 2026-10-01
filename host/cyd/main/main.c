/*
 * CYD host (ESP32-2432S028R V3) — reduced profile (D5/D6).
 * Display: ST7789 RGB565 row blit. Disk/ROM: SD (or host paths in stub).
 */
#include "machine.h"
#include "disk.h"
#include "video.h"
#include "cyd_display.h"
#include "cyd_sd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
static const char *TAG = "a2e_cyd";
#endif

/* Keep image alive for A2E_REDUCED_DISK track re-encode. */
static u8 *s_rom;
static u8 *s_boot;
static u8 *s_disk;
static size_t s_rom_len, s_boot_len, s_disk_len;

static int load_assets(a2e_machine *m) {
#ifdef ESP_PLATFORM
    const char *rom_path = "/sdcard/roms/apple2e.rom";
    const char *boot_path = "/sdcard/roms/diskii_boot.bin";
    const char *disk_path = "/sdcard/disks/dos33.dsk";
    if (a2e_cyd_sd_mount("/sdcard") != 0) {
        ESP_LOGW(TAG, "SD mount failed — core runs without disk");
        return -1;
    }
#else
    const char *rom_path = "roms/apple2e.rom";
    const char *boot_path = "roms/diskii_boot.bin";
    const char *disk_path = "disks/dos33.dsk";
    (void)a2e_cyd_sd_mount(NULL);
#endif

    s_rom = a2e_cyd_sd_load(rom_path, &s_rom_len);
    s_boot = a2e_cyd_sd_load(boot_path, &s_boot_len);
    s_disk = a2e_cyd_sd_load(disk_path, &s_disk_len);

    if (s_rom && s_rom_len > 0) {
        if (a2e_machine_load_rom(m, s_rom, s_rom_len) != 0)
            fprintf(stderr, "ROM load failed\n");
    }
    if (s_boot && s_boot_len > 0)
        a2e_disk_set_boot_rom(s_boot, s_boot_len);
    if (s_disk && s_disk_len > 0) {
        if (a2e_machine_load_disk(m, s_disk, s_disk_len) != 0)
            fprintf(stderr, "disk load failed\n");
    }
    return (s_rom && s_disk) ? 0 : -1;
}

void a2e_cyd_app_main(void) {
    a2e_cyd_display_init();

    a2e_host_ops ops = {
        .present = NULL,
        .present_rgb565_row = a2e_cyd_display_present_row,
        .ctx = NULL,
    };

    a2e_machine mach;
    a2e_machine_init(&mach, &ops);
    mach.mmu.intcxrom = false;

#ifdef A2E_REDUCED_DISK
    fprintf(stderr, "a2e CYD: reduced disk sizeof=%zu\n", sizeof mach.disk);
#endif
#ifndef A2E_REDUCED_VIDEO
    fprintf(stderr, "a2e CYD: tip — build with -DA2E_REDUCED_VIDEO to drop RGB888 FB\n");
#endif

    (void)load_assets(&mach);
    a2e_machine_reset(&mach);

#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "running frames");
    for (;;) {
        a2e_machine_run_frame(&mach);
        vTaskDelay(pdMS_TO_TICKS(16));
    }
#else
    /* Desktop smoke: a few frames through the RGB565 row path */
    for (int i = 0; i < 3; i++)
        a2e_machine_run_frame(&mach);
    fprintf(stderr, "a2e CYD stub: presented frames via RGB565 rows\n");
#endif
}

#ifdef ESP_PLATFORM
void app_main(void) {
    a2e_cyd_app_main();
}
#else
int main(void) {
    a2e_cyd_app_main();
    return 0;
}
#endif
