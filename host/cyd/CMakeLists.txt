# CYD / ESP-IDF — reduced memory profile (D6).

idf_component_register(
    SRCS "main/main.c"
         "cyd_display.c"
         "cyd_sd.c"
         "../../core/cpu.c"
         "../../core/mmu.c"
         "../../core/disk.c"
         "../../core/video.c"
         "../../core/machine.c"
    INCLUDE_DIRS "." "main" "../../core"
    REQUIRES driver esp_lcd fatfs
)

# D6: one nibble track; no RGB888 framebuffer on device.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    A2E_REDUCED_DISK=1
    A2E_REDUCED_VIDEO=1
)
