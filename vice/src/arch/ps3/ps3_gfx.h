#ifndef PS3_GFX_H
#define PS3_GFX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int SceGxmTextureFormat;
typedef unsigned int SceGxmTextureFilter;

#define SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB 1u
#define SCE_GXM_TEXTURE_FORMAT_A8B8G8R8 2u
#define SCE_GXM_TEXTURE_BASE_FORMAT_P8 3u
#define SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 2u

#define SCE_GXM_TEXTURE_FILTER_POINT 0u
#define SCE_GXM_TEXTURE_FILTER_LINEAR 1u

#define RGBA8(r, g, b, a) ((((uint32_t)(a) & 0xFFu) << 24) | (((uint32_t)(b) & 0xFFu) << 16) | (((uint32_t)(g) & 0xFFu) << 8) | ((uint32_t)(r) & 0xFFu))

#define PS3_GFX_SCREEN_WIDTH 960
#define PS3_GFX_SCREEN_HEIGHT 544
#define PS3_GFX_FB_WIDTH 1024

typedef struct ps3_gfx_texture ps3_gfx_texture;
typedef struct ps3_gfx_font ps3_gfx_font;
typedef struct ps3_gfx_pgf ps3_gfx_pgf;

void ps3_gfx_init(void);
void ps3_gfx_boot_test(void);
void ps3_gfx_log_write(const char *text);
void ps3_gfx_start_drawing(void);
void ps3_gfx_end_drawing(void);
void ps3_gfx_swap_buffers(void);
void ps3_gfx_wait_rendering_done(void);
void ps3_gfx_clear_screen(void);
void ps3_gfx_set_clear_color(unsigned int color);
void *ps3_gfx_get_current_fb(void);
void *ps3_gfx_get_front_fb(void);

ps3_gfx_texture *ps3_gfx_create_empty_texture_format(unsigned int width, unsigned int height, SceGxmTextureFormat format);
ps3_gfx_texture *ps3_gfx_load_PNG_buffer(const void *buffer);
ps3_gfx_texture *ps3_gfx_load_image_file(const char *path);
void ps3_gfx_free_texture(ps3_gfx_texture *texture);
void *ps3_gfx_texture_get_datap(const ps3_gfx_texture *texture);
unsigned int ps3_gfx_texture_get_width(const ps3_gfx_texture *texture);
unsigned int ps3_gfx_texture_get_height(const ps3_gfx_texture *texture);
void *ps3_gfx_texture_get_palette(const ps3_gfx_texture *texture);
void ps3_gfx_texture_set_filters(ps3_gfx_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter);

void ps3_gfx_draw_texture(const ps3_gfx_texture *texture, int x, int y);
void ps3_gfx_draw_texture_scale(const ps3_gfx_texture *texture, int x, int y, float x_scale, float y_scale);
void ps3_gfx_draw_texture_part(const ps3_gfx_texture *texture, int x, int y, int tex_x, int tex_y, int tex_w, int tex_h);
void ps3_gfx_draw_texture_part_scale(const ps3_gfx_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale);
void ps3_gfx_draw_line(int x0, int y0, int x1, int y1, unsigned int color);
void ps3_gfx_draw_rectangle(int x, int y, int width, int height, unsigned int color);

ps3_gfx_pgf *ps3_gfx_load_default_pgf(void);
void ps3_gfx_free_pgf(ps3_gfx_pgf *font);
void ps3_gfx_pgf_draw_text(ps3_gfx_pgf *font, int x, int y, unsigned int color, float scale, const char *text);
int ps3_gfx_pgf_text_width(ps3_gfx_pgf *font, float scale, const char *text);
int ps3_gfx_pgf_text_height(ps3_gfx_pgf *font, float scale, const char *text);

ps3_gfx_font *ps3_gfx_load_font_file(const char *path);
ps3_gfx_font *ps3_gfx_load_font_mem(const void *buffer, int size);
void ps3_gfx_free_font(ps3_gfx_font *font);
void ps3_gfx_font_draw_text(ps3_gfx_font *font, int x, int y, unsigned int color, float scale, const char *text);
int ps3_gfx_font_text_width(ps3_gfx_font *font, float scale, const char *text);
int ps3_gfx_font_text_height(ps3_gfx_font *font, float scale, const char *text);

#ifdef __cplusplus
}
#endif

#endif
