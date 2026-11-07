#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/systime.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>

extern "C" {
#include "ps3_gfx.h"
#include "stockfont.h"
#include "sound.h"
}

extern "C" void PS3_ExitProcessNow(void);




static volatile int g_exit_requested = 0;

static void ps3_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;

    switch (status) {
    case SYSUTIL_MENU_OPEN:
        
        sound_suspend();
        break;
    case SYSUTIL_MENU_CLOSE:
        sound_resume();
        break;
    case SYSUTIL_EXIT_GAME:
        
        g_exit_requested = 1;
        break;
    default:
        break;
    }
}

struct ps3_gfx_texture {
    unsigned int width;
    unsigned int height;
    unsigned int format;
    unsigned char *data;
    uint32_t palette[256];
    int min_filter;
    int mag_filter;
};

struct ps3_gfx_font {
    TTF_Font *ttf;
};

struct ps3_gfx_pgf {
    TTF_Font *ttf;
};

typedef struct ps3_gfx_text_entry_s {
    int width;
    int height;
    std::vector<unsigned char> pixels;
} ps3_gfx_text_entry_t;

static unsigned char *g_fb = NULL;
static unsigned char *g_front_fb = NULL;
static unsigned char *g_upload = NULL;
static unsigned int g_clear_color = 0xff000000u;
static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static int g_initialized = 0;
static std::map<std::string, ps3_gfx_text_entry_t> g_text_cache;

static unsigned int ps3_color_r(unsigned int color)
{
    return color & 0xffu;
}

static unsigned int ps3_color_g(unsigned int color)
{
    return (color >> 8) & 0xffu;
}

static unsigned int ps3_color_b(unsigned int color)
{
    return (color >> 16) & 0xffu;
}

static unsigned int ps3_color_a(unsigned int color)
{
    return (color >> 24) & 0xffu;
}

static void ps3_blend_pixel(int x, int y, unsigned int r, unsigned int g, unsigned int b, unsigned int a)
{
    unsigned char *dst;

    if (a == 0) {
        return;
    }
    if (x < 0 || x >= PS3_GFX_SCREEN_WIDTH || y < 0 || y >= PS3_GFX_SCREEN_HEIGHT) {
        return;
    }

    dst = g_fb + ((size_t)y * PS3_GFX_FB_WIDTH + (size_t)x) * 4;
    if (a >= 255) {
        dst[0] = (unsigned char)r;
        dst[1] = (unsigned char)g;
        dst[2] = (unsigned char)b;
        dst[3] = 255;
        return;
    }

    dst[0] = (unsigned char)((r * a + dst[0] * (255 - a)) / 255);
    dst[1] = (unsigned char)((g * a + dst[1] * (255 - a)) / 255);
    dst[2] = (unsigned char)((b * a + dst[2] * (255 - a)) / 255);
    dst[3] = 255;
}

static void ps3_texture_pixel(const ps3_gfx_texture *texture, int x, int y,
                              unsigned int *r, unsigned int *g, unsigned int *b, unsigned int *a)
{
    size_t offset;
    unsigned int value;
    unsigned int index;

    if (x < 0 || x >= (int)texture->width || y < 0 || y >= (int)texture->height) {
        *r = 0;
        *g = 0;
        *b = 0;
        *a = 0;
        return;
    }

    offset = (size_t)y * texture->width + (size_t)x;
    if (texture->format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
        index = texture->data[offset];
        value = texture->palette[index & 0xff];
        *r = ps3_color_r(value);
        *g = ps3_color_g(value);
        *b = ps3_color_b(value);
        *a = ps3_color_a(value);
    } else if (texture->format == SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB) {
        value = ((unsigned int)texture->data[offset * 2] << 8) | texture->data[offset * 2 + 1];
        *r = ((value >> 11) & 0x1f) * 255 / 31;
        *g = ((value >> 5) & 0x3f) * 255 / 63;
        *b = (value & 0x1f) * 255 / 31;
        *a = 255;
    } else {
        *r = texture->data[offset * 4 + 0];
        *g = texture->data[offset * 4 + 1];
        *b = texture->data[offset * 4 + 2];
        *a = texture->data[offset * 4 + 3];
    }
}

static int ps3_palette_is_opaque(const ps3_gfx_texture *texture)
{
    int i;

    for (i = 0; i < 256; i++) {
        if (ps3_color_a(texture->palette[i]) != 255u) {
            return 0;
        }
    }
    return 1;
}

static void ps3_blit_indexed_opaque(const ps3_gfx_texture *texture, int dst_x, int dst_y, int dst_w, int dst_h,
                                    float src_x, float src_y, float src_w, float src_h)
{
    static int col_src[PS3_GFX_SCREEN_WIDTH];
    uint32_t pal32[256];
    float step_x = src_w / (float)dst_w;
    float step_y = src_h / (float)dst_h;
    int x0 = dst_x < 0 ? -dst_x : 0;
    int x1 = dst_w;
    int y0 = dst_y < 0 ? -dst_y : 0;
    int y1 = dst_h;
    int xs;
    int xe;
    int x;
    int y;
    int i;

    if (x1 > PS3_GFX_SCREEN_WIDTH - dst_x) {
        x1 = PS3_GFX_SCREEN_WIDTH - dst_x;
    }
    if (y1 > PS3_GFX_SCREEN_HEIGHT - dst_y) {
        y1 = PS3_GFX_SCREEN_HEIGHT - dst_y;
    }
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (i = 0; i < 256; i++) {
        unsigned char bytes[4];

        bytes[0] = (unsigned char)ps3_color_r(texture->palette[i]);
        bytes[1] = (unsigned char)ps3_color_g(texture->palette[i]);
        bytes[2] = (unsigned char)ps3_color_b(texture->palette[i]);
        bytes[3] = 255;
        memcpy(&pal32[i], bytes, sizeof(uint32_t));
    }

    xs = x1;
    xe = x0;
    for (x = x0; x < x1; x++) {
        int tx = (int)(src_x + (float)x * step_x);

        col_src[x - x0] = tx;
        if (tx >= 0 && tx < (int)texture->width) {
            if (x < xs) {
                xs = x;
            }
            xe = x + 1;
        }
    }
    if (xs >= xe) {
        return;
    }

    for (y = y0; y < y1; y++) {
        int ty = (int)(src_y + (float)y * step_y);
        const unsigned char *row;
        uint32_t *out;

        if (ty < 0 || ty >= (int)texture->height) {
            continue;
        }
        row = texture->data + (size_t)ty * texture->width;
        out = (uint32_t *)(g_fb + ((size_t)(dst_y + y) * PS3_GFX_FB_WIDTH + (size_t)(dst_x + xs)) * 4);
        for (x = xs; x < xe; x++) {
            *out++ = pal32[row[col_src[x - x0]]];
        }
    }
}

static void ps3_blit_texture(const ps3_gfx_texture *texture, int dst_x, int dst_y, int dst_w, int dst_h,
                             float src_x, float src_y, float src_w, float src_h)
{
    int x;
    int y;
    float step_x;
    float step_y;

    if (texture == NULL || texture->data == NULL || dst_w <= 0 || dst_h <= 0 || src_w <= 0.0f || src_h <= 0.0f) {
        return;
    }

    if (texture->format == SCE_GXM_TEXTURE_BASE_FORMAT_P8 && ps3_palette_is_opaque(texture)) {
        ps3_blit_indexed_opaque(texture, dst_x, dst_y, dst_w, dst_h, src_x, src_y, src_w, src_h);
        return;
    }

    step_x = src_w / (float)dst_w;
    step_y = src_h / (float)dst_h;

    for (y = 0; y < dst_h; y++) {
        int out_y = dst_y + y;
        if (out_y < 0 || out_y >= PS3_GFX_SCREEN_HEIGHT) {
            continue;
        }
        for (x = 0; x < dst_w; x++) {
            int out_x = dst_x + x;
            unsigned int r;
            unsigned int g;
            unsigned int b;
            unsigned int a;
            int tex_x;
            int tex_y;

            if (out_x < 0 || out_x >= PS3_GFX_SCREEN_WIDTH) {
                continue;
            }

            tex_x = (int)(src_x + (float)x * step_x);
            tex_y = (int)(src_y + (float)y * step_y);
            ps3_texture_pixel(texture, tex_x, tex_y, &r, &g, &b, &a);
            ps3_blend_pixel(out_x, out_y, r, g, b, a);
        }
    }
}

static size_t ps3_png_buffer_size(const unsigned char *buffer)
{
    size_t offset = 8;
    int guard = 0;

    if (buffer == NULL || memcmp(buffer, "\x89PNG\r\n\x1a\n", 8) != 0) {
        return 0;
    }

    while (guard++ < 4096) {
        unsigned int length;

        length = ((unsigned int)buffer[offset] << 24) | ((unsigned int)buffer[offset + 1] << 16) |
                 ((unsigned int)buffer[offset + 2] << 8) | (unsigned int)buffer[offset + 3];
        offset += 12 + length;

        if (memcmp(buffer + offset - length - 8, "IEND", 4) == 0) {
            break;
        }
    }

    return offset;
}

void ps3_gfx_log_write(const char *text)
{
    FILE *f;

    if (text == NULL) {
        return;
    }

    f = fopen("/dev_hdd0/vice/vice_log.txt", "a");
    if (f != NULL) {
        fputs(text, f);
        fputc('\n', f);
        fclose(f);
    }
}

void ps3_gfx_init(void)
{
    static const Uint32 window_flags[] = { 0, SDL_WINDOW_SHOWN };
    static const Uint32 renderer_flags[] = {
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
        SDL_RENDERER_ACCELERATED,
        SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE,
        SDL_RENDERER_SOFTWARE,
        0
    };
    SDL_DisplayMode display_mode;
    char message[256];
    int window_width = PS3_GFX_SCREEN_WIDTH;
    int window_height = PS3_GFX_SCREEN_HEIGHT;
    int actual_width = 0;
    int actual_height = 0;
    int i;

    if (g_initialized) {
        return;
    }
    g_initialized = 1;

    ps3_gfx_log_write("video: init begin");

    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, ps3_sysutil_callback, NULL);

    ps3_gfx_log_write("video: settle begin");
    {
        videoState video_state;
        int tries = 0;

        memset(&video_state, 0, sizeof(video_state));
        while (tries < 500) {
            if (videoGetState(0, 0, &video_state) == 0 && video_state.state == 0) {
                break;
            }
            sysUsleep(10000);
            sysUtilCheckCallback();
            tries++;
        }
        snprintf(message, sizeof(message), "video: settle state=%d tries=%d",
                 (int)video_state.state, tries);
        ps3_gfx_log_write(message);
    }

    {
        FILE *sdl_log = fopen("/dev_hdd0/vice/sdl_log.txt", "w");
        if (sdl_log != NULL) {
            fputs("sdl: run begin rev 3\n", sdl_log);
            fclose(sdl_log);
        }
    }
    ps3_gfx_log_write("video: sdl init begin");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        snprintf(message, sizeof(message), "video: SDL_Init failed %s", SDL_GetError());
        ps3_gfx_log_write(message);
        return;
    }
    ps3_gfx_log_write("video: SDL_Init ok");

    g_fb = (unsigned char *)calloc(1, (size_t)PS3_GFX_FB_WIDTH * PS3_GFX_SCREEN_HEIGHT * 4);
    g_front_fb = (unsigned char *)calloc(1, (size_t)PS3_GFX_FB_WIDTH * PS3_GFX_SCREEN_HEIGHT * 4);
    g_upload = (unsigned char *)calloc(1, (size_t)PS3_GFX_SCREEN_WIDTH * PS3_GFX_SCREEN_HEIGHT * 4);
    ps3_gfx_log_write("video: buffers ok");

    ps3_gfx_log_write("video: ttf begin");
    TTF_Init();
    ps3_gfx_log_write("video: ttf ok");

    ps3_gfx_log_write("video: img begin");
    IMG_Init(IMG_INIT_PNG);
    ps3_gfx_log_write("video: img ok");

    SDL_ShowCursor(SDL_DISABLE);

    if (SDL_GetDesktopDisplayMode(0, &display_mode) == 0 && display_mode.w >= 640 && display_mode.h >= 480) {
        window_width = display_mode.w;
        window_height = display_mode.h;
    }

    for (i = 0; i < 2 && g_window == NULL; i++) {
        g_window = SDL_CreateWindow("VICE4PS3", 0, 0, window_width, window_height, window_flags[i]);
    }
    if (g_window == NULL) {
        g_window = SDL_CreateWindow("VICE4PS3", 0, 0, PS3_GFX_SCREEN_WIDTH, PS3_GFX_SCREEN_HEIGHT, 0);
    }
    if (g_window == NULL) {
        snprintf(message, sizeof(message), "video: CreateWindow failed %s", SDL_GetError());
        ps3_gfx_log_write(message);
        return;
    }
    snprintf(message, sizeof(message), "video: window %dx%d", window_width, window_height);
    ps3_gfx_log_write(message);

    SDL_GetWindowSize(g_window, &actual_width, &actual_height);
    if (actual_width >= 640 && actual_height >= 480) {
        window_width = actual_width;
        window_height = actual_height;
    }

    for (i = 0; i < 5 && g_renderer == NULL; i++) {
        g_renderer = SDL_CreateRenderer(g_window, -1, renderer_flags[i]);
    }
    if (g_renderer == NULL) {
        snprintf(message, sizeof(message), "video: CreateRenderer failed %s", SDL_GetError());
        ps3_gfx_log_write(message);
        return;
    }
    ps3_gfx_log_write("video: renderer ok");

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  PS3_GFX_SCREEN_WIDTH, PS3_GFX_SCREEN_HEIGHT);
    if (g_texture == NULL) {
        snprintf(message, sizeof(message), "video: CreateTexture failed %s", SDL_GetError());
        ps3_gfx_log_write(message);
    } else {
        SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_NONE);
        ps3_gfx_log_write("video: texture ok");
    }
}

static ps3_gfx_texture *ps3_gfx_load_splash(void)
{
    static const char *paths[] = {
        "data/splash.png",
        "resources/data/splash.png",
        "/dev_hdd0/game/VICEP3001/USRDIR/data/splash.png",
        "/dev_hdd0/game/VICEP3001/USRDIR/resources/data/splash.png",
        NULL
    };
    char message[256];
    int i;

    for (i = 0; paths[i] != NULL; i++) {
        ps3_gfx_texture *texture = ps3_gfx_load_image_file(paths[i]);
        if (texture != NULL) {
            snprintf(message, sizeof(message), "video: splash loaded %s", paths[i]);
            ps3_gfx_log_write(message);
            return texture;
        }
    }

    ps3_gfx_log_write("video: splash image missing");
    return NULL;
}

static int ps3_gfx_render_splash_hd(void)
{
    static const char *paths[] = {
        "data/splash_hd.png",
        "resources/data/splash_hd.png",
        "/dev_hdd0/game/VICEP3001/USRDIR/data/splash_hd.png",
        "/dev_hdd0/game/VICEP3001/USRDIR/resources/data/splash_hd.png",
        NULL
    };
    char message[256];
    int i;

    if (g_renderer == NULL) {
        return 0;
    }

    for (i = 0; paths[i] != NULL; i++) {
        SDL_Surface *surface;
        SDL_Texture *texture;
        SDL_Rect destination;
        int output_width = 0;
        int output_height = 0;
        int image_width;
        int image_height;
        float scale_x;
        float scale_y;
        float scale;

        surface = IMG_Load(paths[i]);
        if (surface == NULL) {
            continue;
        }

        image_width = surface->w;
        image_height = surface->h;
        if (image_width <= 0 || image_height <= 0) {
            SDL_FreeSurface(surface);
            continue;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        texture = SDL_CreateTextureFromSurface(g_renderer, surface);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        SDL_FreeSurface(surface);
        if (texture == NULL) {
            continue;
        }

        if (SDL_GetRendererOutputSize(g_renderer, &output_width, &output_height) != 0 ||
            output_width <= 0 || output_height <= 0) {
            output_width = PS3_GFX_SCREEN_WIDTH;
            output_height = PS3_GFX_SCREEN_HEIGHT;
        }

        scale_x = (float)output_width / (float)image_width;
        scale_y = (float)output_height / (float)image_height;
        scale = scale_x < scale_y ? scale_x : scale_y;
        destination.w = (int)((float)image_width * scale);
        destination.h = (int)((float)image_height * scale);
        if (destination.w <= 0 || destination.h <= 0) {
            SDL_DestroyTexture(texture);
            continue;
        }
        destination.x = (output_width - destination.w) / 2;
        destination.y = (output_height - destination.h) / 2;

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, texture, NULL, &destination);
        SDL_RenderPresent(g_renderer);

        snprintf(message, sizeof(message), "video: splash hd %s %dx%d", paths[i],
                 destination.w, destination.h);
        ps3_gfx_log_write(message);
        SDL_DestroyTexture(texture);
        return 1;
    }

    ps3_gfx_log_write("video: splash hd missing");
    return 0;
}

void ps3_gfx_boot_test(void)
{
    ps3_gfx_texture *splash;
    int hd_splash;
    int i;

    ps3_gfx_log_write("video: boot test begin");
    ps3_gfx_init();

    if (g_fb == NULL) {
        return;
    }

    ps3_gfx_set_clear_color(RGBA8(10, 8, 20, 255));
    ps3_gfx_clear_screen();

    hd_splash = ps3_gfx_render_splash_hd();
    if (!hd_splash) {
        splash = ps3_gfx_load_splash();
        if (splash != NULL) {
            int width = (int)ps3_gfx_texture_get_width(splash);
            int height = (int)ps3_gfx_texture_get_height(splash);

            if (width > 0 && height > 0) {
                float scale_x = (float)PS3_GFX_SCREEN_WIDTH / (float)width;
                float scale_y = (float)PS3_GFX_SCREEN_HEIGHT / (float)height;
                float scale = scale_x < scale_y ? scale_x : scale_y;
                int draw_width = (int)(width * scale);
                int draw_height = (int)(height * scale);

                ps3_gfx_draw_texture_scale(splash,
                                           (PS3_GFX_SCREEN_WIDTH - draw_width) / 2,
                                           (PS3_GFX_SCREEN_HEIGHT - draw_height) / 2,
                                           scale, scale);
            }
            ps3_gfx_free_texture(splash);
        } else {
            ps3_gfx_font *font;

            ps3_gfx_set_clear_color(RGBA8(32, 24, 75, 255));
            ps3_gfx_clear_screen();

            ps3_gfx_draw_rectangle(0, 0, 160, 4, RGBA8(220, 60, 50, 255));
            ps3_gfx_draw_rectangle(160, 0, 160, 4, RGBA8(235, 130, 40, 255));
            ps3_gfx_draw_rectangle(320, 0, 160, 4, RGBA8(240, 210, 50, 255));
            ps3_gfx_draw_rectangle(480, 0, 160, 4, RGBA8(60, 190, 80, 255));
            ps3_gfx_draw_rectangle(640, 0, 160, 4, RGBA8(60, 180, 230, 255));
            ps3_gfx_draw_rectangle(800, 0, 160, 4, RGBA8(80, 100, 220, 255));
            ps3_gfx_draw_rectangle(0, 54, PS3_GFX_SCREEN_WIDTH, 2, RGBA8(85, 69, 176, 255));

            font = ps3_gfx_load_font_file("data/font.ttf");
            if (font != NULL) {
                ps3_gfx_font_draw_text(font, 40, 78, RGBA8(255, 255, 255, 255), 26.0f, "VICE4PS3");
                ps3_gfx_font_draw_text(font, 40, 120, RGBA8(112, 164, 215, 255), 16.0f, "Commodore 64");
                ps3_gfx_font_draw_text(font, 40, 180, RGBA8(138, 135, 176, 255), 14.0f, "Starting up...");
                ps3_gfx_free_font(font);
            }

            ps3_gfx_draw_rectangle(40, 230, 880, 12, RGBA8(27, 20, 62, 255));
            ps3_gfx_draw_rectangle(40, 230, 520, 12, RGBA8(112, 164, 215, 255));
        }

        ps3_gfx_swap_buffers();
        ps3_gfx_log_write("video: splash presented");
    } else {
        ps3_gfx_log_write("video: splash presented hd");
    }

    for (i = 0; i < 120; i++) {
        SDL_PumpEvents();
        SDL_Delay(16);
    }

    ps3_gfx_log_write("video: splash done");
}

void ps3_gfx_start_drawing(void)
{
}

void ps3_gfx_end_drawing(void)
{
}

void ps3_gfx_wait_rendering_done(void)
{
}

void ps3_gfx_set_clear_color(unsigned int color)
{
    g_clear_color = color;
}

void ps3_gfx_clear_screen(void)
{
    uint32_t word;
    uint64_t word64;
    uint64_t *dst64;
    size_t i;
    size_t count64 = ((size_t)PS3_GFX_FB_WIDTH * PS3_GFX_SCREEN_HEIGHT) / 2;

    if (g_fb == NULL) {
        return;
    }

    word = ((uint32_t)ps3_color_r(g_clear_color) << 24) |
           ((uint32_t)ps3_color_g(g_clear_color) << 16) |
           ((uint32_t)ps3_color_b(g_clear_color) << 8) | 255u;
    word64 = ((uint64_t)word << 32) | (uint64_t)word;

    dst64 = (uint64_t *)g_fb;
    for (i = 0; i < count64; i++) {
        dst64[i] = word64;
    }
}

void ps3_gfx_swap_buffers(void)
{
    size_t i;

    sysUtilCheckCallback();

    if (g_exit_requested) {
        PS3_ExitProcessNow();
    }

    if (g_renderer != NULL && g_texture != NULL && g_fb != NULL && g_upload != NULL) {
        int y;
        size_t row_count64 = (size_t)PS3_GFX_SCREEN_WIDTH / 2;

        for (y = 0; y < PS3_GFX_SCREEN_HEIGHT; y++) {
            const uint64_t *src64 = (const uint64_t *)(g_fb + (size_t)y * PS3_GFX_FB_WIDTH * 4);
            uint64_t *dst64 = (uint64_t *)(g_upload + (size_t)y * PS3_GFX_SCREEN_WIDTH * 4);
            for (i = 0; i < row_count64; i++) {
                dst64[i] = ((src64[i] >> 8) & 0x00FFFFFF00FFFFFFULL) | 0xFF000000FF000000ULL;
            }
        }
        SDL_UpdateTexture(g_texture, NULL, g_upload, PS3_GFX_SCREEN_WIDTH * 4);
        SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
        SDL_RenderPresent(g_renderer);
    }
}

void *ps3_gfx_get_current_fb(void)
{
    return g_fb;
}

void *ps3_gfx_get_front_fb(void)
{
    if (g_front_fb != NULL && g_fb != NULL) {
        memcpy(g_front_fb, g_fb, (size_t)PS3_GFX_FB_WIDTH * PS3_GFX_SCREEN_HEIGHT * 4);
    }
    return g_front_fb;
}

ps3_gfx_texture *ps3_gfx_create_empty_texture_format(unsigned int width, unsigned int height, SceGxmTextureFormat format)
{
    ps3_gfx_texture *texture;
    size_t bytes;
    size_t i;

    if (width == 0 || height == 0) {
        return NULL;
    }

    texture = (ps3_gfx_texture *)calloc(1, sizeof(ps3_gfx_texture));
    if (texture == NULL) {
        return NULL;
    }

    texture->width = width;
    texture->height = height;
    texture->format = format;
    texture->min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
    texture->mag_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;

    if (format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
        bytes = (size_t)width * height;
    } else if (format == SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB) {
        bytes = (size_t)width * height * 2;
    } else {
        bytes = (size_t)width * height * 4;
    }

    texture->data = (unsigned char *)calloc(1, bytes);
    if (texture->data == NULL) {
        free(texture);
        return NULL;
    }

    for (i = 0; i < 256; i++) {
        texture->palette[i] = 0xff000000u;
    }

    return texture;
}

ps3_gfx_texture *ps3_gfx_load_PNG_buffer(const void *buffer)
{
    const unsigned char *png = (const unsigned char *)buffer;
    size_t size;
    SDL_RWops *rw;
    SDL_Surface *surface;
    SDL_Surface *converted;
    ps3_gfx_texture *texture;
    size_t row;

    if (png == NULL) {
        return NULL;
    }

    size = ps3_png_buffer_size(png);
    if (size == 0) {
        return NULL;
    }

    rw = SDL_RWFromConstMem(png, (int)size);
    if (rw == NULL) {
        return NULL;
    }

    surface = IMG_Load_RW(rw, 1);
    if (surface == NULL) {
        return NULL;
    }

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (converted == NULL) {
        return NULL;
    }

    texture = (ps3_gfx_texture *)calloc(1, sizeof(ps3_gfx_texture));
    if (texture == NULL) {
        SDL_FreeSurface(converted);
        return NULL;
    }

    texture->width = (unsigned int)converted->w;
    texture->height = (unsigned int)converted->h;
    texture->format = SCE_GXM_TEXTURE_FORMAT_A8B8G8R8;
    texture->min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
    texture->mag_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
    texture->data = (unsigned char *)malloc((size_t)texture->width * texture->height * 4);
    if (texture->data == NULL) {
        free(texture);
        SDL_FreeSurface(converted);
        return NULL;
    }

    SDL_LockSurface(converted);
    for (row = 0; row < texture->height; row++) {
        memcpy(texture->data + row * texture->width * 4,
               (unsigned char *)converted->pixels + row * converted->pitch,
               (size_t)texture->width * 4);
    }
    SDL_UnlockSurface(converted);
    SDL_FreeSurface(converted);

    return texture;
}

ps3_gfx_texture *ps3_gfx_load_image_file(const char *path)
{
    SDL_Surface *surface;
    SDL_Surface *converted;
    ps3_gfx_texture *texture;
    size_t row;

    if (path == NULL) {
        return NULL;
    }

    surface = IMG_Load(path);
    if (surface == NULL) {
        return NULL;
    }

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (converted == NULL) {
        return NULL;
    }

    texture = (ps3_gfx_texture *)calloc(1, sizeof(ps3_gfx_texture));
    if (texture == NULL) {
        SDL_FreeSurface(converted);
        return NULL;
    }

    texture->width = (unsigned int)converted->w;
    texture->height = (unsigned int)converted->h;
    texture->format = SCE_GXM_TEXTURE_FORMAT_A8B8G8R8;
    texture->min_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
    texture->mag_filter = SCE_GXM_TEXTURE_FILTER_LINEAR;
    texture->data = (unsigned char *)malloc((size_t)texture->width * texture->height * 4);
    if (texture->data == NULL) {
        free(texture);
        SDL_FreeSurface(converted);
        return NULL;
    }

    SDL_LockSurface(converted);
    for (row = 0; row < texture->height; row++) {
        memcpy(texture->data + row * texture->width * 4,
               (unsigned char *)converted->pixels + row * converted->pitch,
               (size_t)texture->width * 4);
    }
    SDL_UnlockSurface(converted);
    SDL_FreeSurface(converted);

    return texture;
}
void ps3_gfx_free_texture(ps3_gfx_texture *texture)
{
    if (texture == NULL) {
        return;
    }
    free(texture->data);
    free(texture);
}

void *ps3_gfx_texture_get_datap(const ps3_gfx_texture *texture)
{
    return texture ? texture->data : NULL;
}

unsigned int ps3_gfx_texture_get_width(const ps3_gfx_texture *texture)
{
    return texture ? texture->width : 0;
}

unsigned int ps3_gfx_texture_get_height(const ps3_gfx_texture *texture)
{
    return texture ? texture->height : 0;
}

void *ps3_gfx_texture_get_palette(const ps3_gfx_texture *texture)
{
    return texture ? (void *)texture->palette : NULL;
}

void ps3_gfx_texture_set_filters(ps3_gfx_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter)
{
    if (texture == NULL) {
        return;
    }
    texture->min_filter = (int)min_filter;
    texture->mag_filter = (int)mag_filter;
}

void ps3_gfx_draw_texture(const ps3_gfx_texture *texture, int x, int y)
{
    if (texture == NULL) {
        return;
    }
    ps3_blit_texture(texture, x, y, (int)texture->width, (int)texture->height, 0.0f, 0.0f,
                     (float)texture->width, (float)texture->height);
}

void ps3_gfx_draw_texture_scale(const ps3_gfx_texture *texture, int x, int y, float x_scale, float y_scale)
{
    if (texture == NULL) {
        return;
    }
    ps3_blit_texture(texture, x, y, (int)(texture->width * x_scale), (int)(texture->height * y_scale),
                     0.0f, 0.0f, (float)texture->width, (float)texture->height);
}

void ps3_gfx_draw_texture_part(const ps3_gfx_texture *texture, int x, int y, int tex_x, int tex_y, int tex_w, int tex_h)
{
    if (texture == NULL) {
        return;
    }
    ps3_blit_texture(texture, x, y, tex_w, tex_h, (float)tex_x, (float)tex_y, (float)tex_w, (float)tex_h);
}

void ps3_gfx_draw_texture_part_scale(const ps3_gfx_texture *texture, float x, float y, float tex_x, float tex_y,
                                     float tex_w, float tex_h, float x_scale, float y_scale)
{
    if (texture == NULL) {
        return;
    }
    ps3_blit_texture(texture, (int)x, (int)y, (int)(tex_w * x_scale), (int)(tex_h * y_scale),
                     tex_x, tex_y, tex_w, tex_h);
}

void ps3_gfx_draw_line(int x0, int y0, int x1, int y1, unsigned int color)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    unsigned int r = ps3_color_r(color);
    unsigned int g = ps3_color_g(color);
    unsigned int b = ps3_color_b(color);
    unsigned int a = ps3_color_a(color);

    for (;;) {
        ps3_blend_pixel(x0, y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        if (2 * err >= dy) {
            err += dy;
            x0 += sx;
        }
        if (2 * err <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ps3_gfx_draw_rectangle(int x, int y, int width, int height, unsigned int color)
{
    int x0;
    int y0;
    int x1;
    int y1;
    int i;
    int j;
    unsigned int r;
    unsigned int g;
    unsigned int b;
    unsigned int a;

    if (g_fb == NULL || width <= 0 || height <= 0) {
        return;
    }

    a = ps3_color_a(color);
    if (a == 0) {
        return;
    }

    x0 = x < 0 ? 0 : x;
    y0 = y < 0 ? 0 : y;
    x1 = (x + width) > PS3_GFX_SCREEN_WIDTH ? PS3_GFX_SCREEN_WIDTH : (x + width);
    y1 = (y + height) > PS3_GFX_SCREEN_HEIGHT ? PS3_GFX_SCREEN_HEIGHT : (y + height);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    r = ps3_color_r(color);
    g = ps3_color_g(color);
    b = ps3_color_b(color);

    if (a >= 255) {
        uint32_t word = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 255u;
        for (j = y0; j < y1; j++) {
            uint32_t *row = (uint32_t *)(g_fb + ((size_t)j * PS3_GFX_FB_WIDTH + (size_t)x0) * 4);
            int span = x1 - x0;
            for (i = 0; i < span; i++) {
                row[i] = word;
            }
        }
        return;
    }

    for (j = y0; j < y1; j++) {
        unsigned char *dst = g_fb + ((size_t)j * PS3_GFX_FB_WIDTH + (size_t)x0) * 4;
        unsigned int inv_a = 255u - a;
        for (i = x0; i < x1; i++) {
            dst[0] = (unsigned char)((r * a + dst[0] * inv_a) / 255u);
            dst[1] = (unsigned char)((g * a + dst[1] * inv_a) / 255u);
            dst[2] = (unsigned char)((b * a + dst[2] * inv_a) / 255u);
            dst[3] = 255;
            dst += 4;
        }
    }
}

static int ps3_font_point_size(float scale)
{
    if (scale <= 2.0f) {
        return 22;
    }
    return (int)(scale + 0.5f);
}

static ps3_gfx_text_entry_t *ps3_get_text_entry(TTF_Font *font, int point_size, unsigned int color, const char *text)
{
    char prefix[64];
    std::string key;
    std::map<std::string, ps3_gfx_text_entry_t>::iterator it;
    SDL_Color text_color;
    SDL_Surface *surface;
    SDL_Surface *converted;
    ps3_gfx_text_entry_t entry;
    size_t row;

    if (font == NULL || text == NULL || text[0] == '\0') {
        return NULL;
    }

    snprintf(prefix, sizeof(prefix), "%d|%u|", point_size, color);
    key = prefix;
    key += text;

    it = g_text_cache.find(key);
    if (it != g_text_cache.end()) {
        return &it->second;
    }

    if (g_text_cache.size() > 512) {
        g_text_cache.clear();
    }

    TTF_SetFontSize(font, point_size);
    text_color.r = (Uint8)ps3_color_r(color);
    text_color.g = (Uint8)ps3_color_g(color);
    text_color.b = (Uint8)ps3_color_b(color);
    text_color.a = 255;

    surface = TTF_RenderUTF8_Blended(font, text, text_color);
    if (surface == NULL) {
        return NULL;
    }

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (converted == NULL) {
        return NULL;
    }

    entry.width = converted->w;
    entry.height = converted->h;
    entry.pixels.resize((size_t)converted->w * converted->h * 4);

    SDL_LockSurface(converted);
    for (row = 0; row < (size_t)converted->h; row++) {
        memcpy(&entry.pixels[row * converted->w * 4],
               (unsigned char *)converted->pixels + row * converted->pitch,
               (size_t)converted->w * 4);
    }
    SDL_UnlockSurface(converted);
    SDL_FreeSurface(converted);

    g_text_cache[key] = entry;
    return &g_text_cache[key];
}

static void ps3_draw_text(TTF_Font *font, int x, int y, unsigned int color, float scale, const char *text)
{
    ps3_gfx_text_entry_t *entry;
    int point_size = ps3_font_point_size(scale);
    int top;
    int i;
    int j;

    if (font == NULL || text == NULL || text[0] == '\0') {
        return;
    }

    entry = ps3_get_text_entry(font, point_size, color, text);
    if (entry == NULL) {
        return;
    }

    TTF_SetFontSize(font, point_size);
    top = y - TTF_FontAscent(font);

    for (j = 0; j < entry->height; j++) {
        for (i = 0; i < entry->width; i++) {
            unsigned char *pixel = &entry->pixels[((size_t)j * entry->width + i) * 4];
            ps3_blend_pixel(x + i, top + j, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
}

ps3_gfx_pgf *ps3_gfx_load_default_pgf(void)
{
    TTF_Font *ttf;
    ps3_gfx_pgf *pgf;

    ttf = TTF_OpenFontRW(SDL_RWFromConstMem(stockfont, stockfont_size), 1, 22);
    if (ttf == NULL) {
        return NULL;
    }

    pgf = (ps3_gfx_pgf *)calloc(1, sizeof(ps3_gfx_pgf));
    if (pgf == NULL) {
        TTF_CloseFont(ttf);
        return NULL;
    }
    pgf->ttf = ttf;
    return pgf;
}

void ps3_gfx_free_pgf(ps3_gfx_pgf *font)
{
    if (font == NULL) {
        return;
    }
    if (font->ttf != NULL) {
        TTF_CloseFont(font->ttf);
    }
    free(font);
}

void ps3_gfx_pgf_draw_text(ps3_gfx_pgf *font, int x, int y, unsigned int color, float scale, const char *text)
{
    if (font == NULL) {
        return;
    }
    ps3_draw_text(font->ttf, x, y, color, scale, text);
}

int ps3_gfx_pgf_text_width(ps3_gfx_pgf *font, float scale, const char *text)
{
    int width = 0;
    int height = 0;

    if (font == NULL || font->ttf == NULL || text == NULL) {
        return 0;
    }
    TTF_SetFontSize(font->ttf, ps3_font_point_size(scale));
    TTF_SizeUTF8(font->ttf, text, &width, &height);
    return width;
}

int ps3_gfx_pgf_text_height(ps3_gfx_pgf *font, float scale, const char *text)
{
    (void)text;
    if (font == NULL || font->ttf == NULL) {
        return 0;
    }
    TTF_SetFontSize(font->ttf, ps3_font_point_size(scale));
    return TTF_FontHeight(font->ttf);
}

ps3_gfx_font *ps3_gfx_load_font_file(const char *path)
{
    TTF_Font *ttf;
    ps3_gfx_font *font;

    if (path == NULL) {
        return NULL;
    }

    ttf = TTF_OpenFont(path, 22);
    if (ttf == NULL) {
        return NULL;
    }

    font = (ps3_gfx_font *)calloc(1, sizeof(ps3_gfx_font));
    if (font == NULL) {
        TTF_CloseFont(ttf);
        return NULL;
    }
    font->ttf = ttf;
    return font;
}

ps3_gfx_font *ps3_gfx_load_font_mem(const void *buffer, int size)
{
    TTF_Font *ttf;
    ps3_gfx_font *font;

    if (buffer == NULL || size <= 0) {
        return NULL;
    }

    ttf = TTF_OpenFontRW(SDL_RWFromConstMem(buffer, size), 1, 22);
    if (ttf == NULL) {
        return NULL;
    }

    font = (ps3_gfx_font *)calloc(1, sizeof(ps3_gfx_font));
    if (font == NULL) {
        TTF_CloseFont(ttf);
        return NULL;
    }
    font->ttf = ttf;
    return font;
}

void ps3_gfx_free_font(ps3_gfx_font *font)
{
    if (font == NULL) {
        return;
    }
    if (font->ttf != NULL) {
        TTF_CloseFont(font->ttf);
    }
    free(font);
}

void ps3_gfx_font_draw_text(ps3_gfx_font *font, int x, int y, unsigned int color, float scale, const char *text)
{
    if (font == NULL) {
        return;
    }
    ps3_draw_text(font->ttf, x, y, color, scale, text);
}

int ps3_gfx_font_text_width(ps3_gfx_font *font, float scale, const char *text)
{
    int width = 0;
    int height = 0;

    if (font == NULL || font->ttf == NULL || text == NULL) {
        return 0;
    }
    TTF_SetFontSize(font->ttf, ps3_font_point_size(scale));
    TTF_SizeUTF8(font->ttf, text, &width, &height);
    return width;
}

int ps3_gfx_font_text_height(ps3_gfx_font *font, float scale, const char *text)
{
    (void)text;
    if (font == NULL || font->ttf == NULL) {
        return 0;
    }
    TTF_SetFontSize(font->ttf, ps3_font_point_size(scale));
    return TTF_FontHeight(font->ttf);
}
