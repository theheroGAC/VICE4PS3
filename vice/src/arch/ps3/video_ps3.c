




























#include "vice.h"
#include "video.h"
#include "videoarch.h"
#include "palette.h"
#include "lib.h"
#include "raster.h"
#include "interrupt.h"
#include "sound.h"
#include "vsync.h"
#include "cmdline.h"
#include "resources.h"
#include "controller.h"
#include "debug_ps3.h"
#include "ps3_gfx.h"
#include <string.h>


static struct video_canvas_s*	activeCanvas = NULL;
static float					last_framerate = 0;
static float					last_percent = 0;
static int						last_warp_flag = 0;
static int						drive_led_on = 0, tape_led_on = 0;
static video_canvas_t*			active_canvas = NULL;
static void						pause_trap(uint16_t unused_addr, void *data); 
static void						load_snapshot_trap(uint16_t addr, void *data);
void							video_PS3_menu_show();


static const cmdline_option_t cmdline_options[] = {
    { NULL },
};

static const resource_int_t resources_int[] = {
    { NULL }
};

void video_canvas_resize(struct video_canvas_s *canvas, char resize_canvas)
{
	unsigned int x, y, width, height;
	
	if (!(canvas && canvas->draw_buffer && canvas->videoconfig)) {
        return;
    }
	
    width = canvas->draw_buffer->canvas_width;
    height = canvas->draw_buffer->canvas_height;

    
    if (width == 0 || height == 0 || (canvas->width && height == canvas->height)) {
        return;
    }
	
    canvas->width = canvas->actual_width = width;
    canvas->height = canvas->actual_height = height;
	PS3_GetViewInfo(0,0,0,0,&canvas->depth);

	x = canvas->geometry->extra_offscreen_border_left;
	y = canvas->geometry->first_displayed_line;
	
	PS3_SetViewport(x, y, width, height);

	
	if (!canvas->videoconfig->color_tables.updated){ 
		
		video_color_update_palette(canvas);
	}
}

video_canvas_t *video_canvas_create(video_canvas_t *canvas, unsigned int *width, unsigned int *height, int mapped)
{
	int view_width = 0;
	int view_height = 0;
	int depth = 0;

	PS3_GetViewInfo(&view_width, &view_height, 0, 0, &depth);
	if (view_width > 0 && view_height > 0) {
		*width = (unsigned int)view_width;
		*height = (unsigned int)view_height;
	}
	if (depth == 8 || depth == 16 || depth == 32) {
		canvas->depth = depth;
	} else {
		canvas->depth = 8;
	}
	activeCanvas = canvas;

    return canvas;
}

void video_canvas_destroy(struct video_canvas_s *canvas)
{
	lib_free(canvas->video_draw_buffer_callback);
}

static int video_draw_buffer_alloc(video_canvas_t *canvas,
                                    uint8_t **draw_buffer,
                                    unsigned int fb_width,
                                    unsigned int fb_height,
                                    unsigned int *fb_pitch);

static void video_draw_buffer_free(video_canvas_t *canvas,
                                    uint8_t *draw_buffer);

static void video_draw_buffer_clear(video_canvas_t *canvas,
                                     uint8_t *draw_buffer,
                                     uint8_t value,
                                     unsigned int fb_width,
                                     unsigned int fb_height,
                                     unsigned int fb_pitch);

void video_arch_canvas_init(struct video_canvas_s *canvas)
{
	
	
	
	
	

	canvas->video_draw_buffer_callback = lib_malloc(sizeof(video_draw_buffer_callback_t));
	canvas->video_draw_buffer_callback->draw_buffer_alloc = video_draw_buffer_alloc;
	canvas->video_draw_buffer_callback->draw_buffer_free = video_draw_buffer_free;
	canvas->video_draw_buffer_callback->draw_buffer_clear = video_draw_buffer_clear;
}

static int video_draw_buffer_alloc(video_canvas_t *canvas,
                                    uint8_t **draw_buffer,
                                    unsigned int fb_width,
                                    unsigned int fb_height,
                                    unsigned int *fb_pitch)
{
	PS3_CreateView(fb_width, fb_height, 8);
	PS3_GetViewInfo(0, 0, (unsigned char**)draw_buffer, fb_pitch, 0);
	return 0;
}

static void video_draw_buffer_free(video_canvas_t *canvas, uint8_t *draw_buffer)
{
}

static void video_draw_buffer_clear(video_canvas_t *canvas,
                                     uint8_t *draw_buffer,
                                     uint8_t value,
                                     unsigned int fb_width,
                                     unsigned int fb_height,
                                     unsigned int fb_pitch)
{
	memset(draw_buffer, value, fb_pitch * fb_height);
}

int video_canvas_set_palette(struct video_canvas_s *canvas, struct palette_s *palette)
{
	
	
   
	unsigned int i, col = 0, depth;

    if (palette == NULL){
        return 0;
	}

    canvas->palette = palette;

	unsigned char* rgb_byte_arr = (unsigned char*)lib_malloc(palette->num_entries*3);
	memset(rgb_byte_arr, 0, palette->num_entries*3);
	unsigned char* pstart = rgb_byte_arr;

	for (i = 0; i < palette->num_entries; i++) {
		rgb_byte_arr[0] = palette->entries[i].red;
		rgb_byte_arr[1] = palette->entries[i].green;
		rgb_byte_arr[2] = palette->entries[i].blue;
		rgb_byte_arr += 3;
	}

	PS3_NotifyPalette(pstart, palette->num_entries);
	lib_free(pstart);

    return 0; 
}

void video_canvas_refresh(struct video_canvas_s *canvas,
                          unsigned int xs, unsigned int ys,
                          unsigned int xi, unsigned int yi,
                          unsigned int w, unsigned int h)
{
	static int refresh_logged = 0;
	if (!refresh_logged) {
		refresh_logged = 1;
		ps3_gfx_log_write("emu: first canvas refresh");
	}
	PS3_UpdateView();
}

int video_init()
{
	return 0;
}

void video_shutdown()
{
}

int video_arch_cmdline_options_init(void)
{
	return cmdline_register_options(cmdline_options);
}

int video_arch_resources_init()
{
	return resources_register_int(resources_int);
}

void video_arch_resources_shutdown()
{
}

void ui_display_speed(float percent, float framerate, int warp_flag)
{
	if (last_framerate != framerate || last_percent != percent || last_warp_flag != warp_flag)
		PS3_NotifyFPS((int)framerate, percent, warp_flag);

	last_framerate = framerate;
	last_percent = percent;
	last_warp_flag = warp_flag;
}

void ui_display_drive_led(int drive_number, unsigned int led_pwm1, unsigned int led_pwm2)
{
	int led_status = (led_pwm1 > 500);

	PS3_NotifyDriveStatus(drive_number, led_status);
	
	drive_led_on = led_status;
}

void ui_display_tape_motor_status(int motor)
{
	if (tape_led_on != motor)
		PS3_NotifyTapeMotorStatus(motor);

	tape_led_on = motor;
}

char video_canvas_can_resize(struct video_canvas_s *canvas)
{
	return 1;
}

void video_PS3_ui_init_finalize(void)
{
	video_canvas_resize(active_canvas, 1);
	PS3_ApplySettings();
}

int c64_gui_is_active(void);
static int s_menu_busy = 0;

static void show_menu_trap(uint16_t unused_addr, void *data)
{
	ps3_gfx_log_write("menu: trap enter");
	vsync_suspend_speed_eval();
	ps3_gfx_log_write("menu: eval done");
	PS3_ActivateMenu();
	last_framerate = 0;
	last_percent = 0;
	sound_resume();
	ps3_gfx_log_write("menu: trap exit");
	s_menu_busy = 0;
}

void video_PS3_menu_show()
{
	if (s_menu_busy || c64_gui_is_active()) {
		return;
	}
	s_menu_busy = 1;
	interrupt_maincpu_trigger_trap(show_menu_trap, NULL);
}

void video_PS3_update_palette()
{
	if (!activeCanvas)
		return;

	
	if (!activeCanvas->videoconfig->color_tables.updated){ 
		
		video_color_update_palette(activeCanvas);
	}
	video_canvas_set_palette(activeCanvas, activeCanvas->palette);
}

void video_PS3_get_canvas(struct video_canvas_s** canvas)
{
	*canvas = activeCanvas;
}
