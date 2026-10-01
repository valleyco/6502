#include "../../core/machine.h"
#include "../../core/disk.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    int scale;
} host_sdl;

static void present(void *ctx, const u8 *rgb, int w, int h, int stride_bytes) {
    host_sdl *hs = ctx;
    (void)stride_bytes;
    void *pixels;
    int pitch;
    if (SDL_LockTexture(hs->tex, NULL, &pixels, &pitch) != 0) return;
    u8 *dst = pixels;
    for (int y = 0; y < h; y++) {
        u8 *row = dst + y * pitch;
        const u8 *src = rgb + y * w * 3;
        for (int x = 0; x < w; x++) {
            row[x * 4 + 0] = src[x * 3 + 2]; /* B */
            row[x * 4 + 1] = src[x * 3 + 1]; /* G */
            row[x * 4 + 2] = src[x * 3 + 0]; /* R */
            row[x * 4 + 3] = 255;
        }
    }
    SDL_UnlockTexture(hs->tex);
    SDL_RenderClear(hs->ren);
    SDL_RenderCopy(hs->ren, hs->tex, NULL, NULL);
    SDL_RenderPresent(hs->ren);
}

static u8 *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    u8 *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_len = (size_t)n;
    return buf;
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --rom PATH       Apple IIe ROM (default: roms/apple2e.rom)\n"
        "  --disk PATH      .dsk / .po image\n"
        "  --bootrom PATH   Disk II P5 PROM (default: roms/diskii_boot.bin)\n"
        "  --scale N        window scale factor 1..8 (default: 3)\n"
        "  -h, --help       this help\n"
        "\n"
        "ROMs are not redistributed; see roms/README.md\n"
        "Keys: Esc=quit, F5=reset, +/-=scale, printable -> Apple keyboard\n",
        argv0);
}

static int clamp_scale(int s) {
    if (s < 1) return 1;
    if (s > 8) return 8;
    return s;
}

static void apply_scale(host_sdl *hs, int scale) {
    hs->scale = clamp_scale(scale);
    if (hs->win)
        SDL_SetWindowSize(hs->win, A2E_VIDEO_W * hs->scale, A2E_VIDEO_H * hs->scale);
}

int main(int argc, char **argv) {
    const char *rom_path = "roms/apple2e.rom";
    const char *disk_path = NULL;
    const char *bootrom_path = "roms/diskii_boot.bin";
    int scale = 3;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_path = argv[++i];
        else if (!strcmp(argv[i], "--disk") && i + 1 < argc) disk_path = argv[++i];
        else if (!strcmp(argv[i], "--bootrom") && i + 1 < argc) bootrom_path = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) {
            scale = atoi(argv[++i]);
            scale = clamp_scale(scale);
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    /* Integer scale should stay crisp, not bilinear-blurred. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    host_sdl host = {0};
    host.scale = scale;
    host.win = SDL_CreateWindow("Apple IIe",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        A2E_VIDEO_W * host.scale, A2E_VIDEO_H * host.scale,
        SDL_WINDOW_RESIZABLE);
    host.ren = SDL_CreateRenderer(host.win, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(host.ren, A2E_VIDEO_W, A2E_VIDEO_H);
    SDL_RenderSetIntegerScale(host.ren, SDL_TRUE);
    host.tex = SDL_CreateTexture(host.ren, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 A2E_VIDEO_W, A2E_VIDEO_H);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(host.tex, SDL_ScaleModeNearest);
#endif

    a2e_host_ops ops = {
        .present = present,
        .ctx = &host,
    };

    a2e_machine mach;
    a2e_machine_init(&mach, &ops);

    size_t n = 0;
    u8 *rom = load_file(rom_path, &n);
    if (rom) {
        a2e_machine_load_rom(&mach, rom, n);
        free(rom);
        printf("Loaded ROM %s (%zu bytes)\n", rom_path, n);
    } else {
        fprintf(stderr, "Warning: no ROM at %s — running without firmware\n", rom_path);
        /* Minimal demo: fill text screen and spin */
        memset(mach.rom, 0xEA, sizeof mach.rom);
        mach.rom[0xFFFC - 0xC000] = 0x00;
        mach.rom[0xFFFD - 0xC000] = 0x80;
        /* write "NO ROM" to text page via loop at $8000 */
        mach.main_ram[0x8000] = 0x4C;
        mach.main_ram[0x8001] = 0x00;
        mach.main_ram[0x8002] = 0x80;
        const char *msg = "NO ROM - see roms/README.md";
        for (int i = 0; msg[i]; i++)
            mach.main_ram[0x400 + i] = (u8)(msg[i] | 0x80);
    }

    u8 *boot = load_file(bootrom_path, &n);
    if (boot) {
        a2e_disk_set_boot_rom(boot, n);
        free(boot);
        printf("Loaded Disk II boot ROM %s\n", bootrom_path);
    } else {
        a2e_machine_install_cleanroom_boot(&mach);
        printf("No %s — using clean-room Disk II boot assist\n", bootrom_path);
    }

    if (disk_path) {
        u8 *dsk = load_file(disk_path, &n);
        if (dsk) {
            if (a2e_machine_load_disk(&mach, dsk, n) == 0)
                printf("Loaded disk %s\n", disk_path);
            else
                fprintf(stderr, "Failed to parse disk %s\n", disk_path);
            free(dsk);
        } else {
            fprintf(stderr, "Cannot open disk %s\n", disk_path);
        }
    }

    a2e_machine_reset(&mach);

    int running = 1;
    Uint32 frame_ms = 16; /* ~60 Hz present; core runs ~1 NTSC frame worth of cycles */
    while (running) {
        Uint32 t0 = SDL_GetTicks();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
                else if (ev.key.keysym.sym == SDLK_F5) a2e_machine_reset(&mach);
                else if (ev.key.keysym.sym == SDLK_EQUALS ||
                         ev.key.keysym.sym == SDLK_PLUS ||
                         ev.key.keysym.sym == SDLK_KP_PLUS) {
                    apply_scale(&host, host.scale + 1);
                } else if (ev.key.keysym.sym == SDLK_MINUS ||
                           ev.key.keysym.sym == SDLK_KP_MINUS) {
                    apply_scale(&host, host.scale - 1);
                } else {
                    char ch = 0;
                    SDL_Keycode k = ev.key.keysym.sym;
                    if (k >= 32 && k < 127) ch = (char)k;
                    if (k == SDLK_RETURN) ch = '\r';
                    if (k == SDLK_BACKSPACE) ch = 0x08;
                    if (ch) a2e_machine_key(&mach, (u8)ch);
                }
            }
        }
        a2e_machine_run_frame(&mach);
        Uint32 dt = SDL_GetTicks() - t0;
        if (dt < frame_ms) SDL_Delay(frame_ms - dt);
    }

    SDL_DestroyTexture(host.tex);
    SDL_DestroyRenderer(host.ren);
    SDL_DestroyWindow(host.win);
    SDL_Quit();
    return 0;
}
