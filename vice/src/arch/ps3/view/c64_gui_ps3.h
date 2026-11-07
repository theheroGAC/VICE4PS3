#ifndef C64_GUI_PS3_H
#define C64_GUI_PS3_H

#include <stdint.h>
#include <stdbool.h>
#include "ps3_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C64_GUI_SCREEN_W 960
#define C64_GUI_SCREEN_H 544
#define C64_GUI_HEADER_H 54.0f
#define C64_GUI_FOOTER_H 44.0f

#define C64_COLOR_BG            RGBA8(32, 24, 75, 255)
#define C64_COLOR_HEADER        RGBA8(42, 32, 94, 255)
#define C64_COLOR_FOOTER        RGBA8(27, 20, 62, 255)
#define C64_COLOR_CARD          RGBA8(45, 35, 102, 235)
#define C64_COLOR_CARD_FOCUSED  RGBA8(62, 50, 138, 255)
#define C64_COLOR_CARD_BORDER   RGBA8(85, 69, 176, 255)
#define C64_COLOR_CELESTINO     RGBA8(112, 164, 215, 255)
#define C64_COLOR_CELESTE_LIGHT RGBA8(163, 151, 236, 255)
#define C64_COLOR_WHITE         RGBA8(255, 255, 255, 255)
#define C64_COLOR_MUTED         RGBA8(138, 135, 176, 255)
#define C64_COLOR_GOLD          RGBA8(245, 180, 25, 255)
#define C64_COLOR_SUCCESS       RGBA8(46, 204, 113, 255)
#define C64_COLOR_DANGER        RGBA8(231, 76, 60, 255)
#define C64_COLOR_OVERLAY       RGBA8(15, 11, 38, 220)

#define C64_COLOR_RB_RED        RGBA8(220, 60, 50, 255)
#define C64_COLOR_RB_ORANGE     RGBA8(235, 130, 40, 255)
#define C64_COLOR_RB_YELLOW     RGBA8(240, 210, 50, 255)
#define C64_COLOR_RB_GREEN      RGBA8(60, 190, 80, 255)
#define C64_COLOR_RB_CYAN       RGBA8(60, 180, 230, 255)
#define C64_COLOR_RB_BLUE       RGBA8(80, 100, 220, 255)

typedef enum {
    C64_TAB_LIBRARY = 0,
    C64_TAB_DRIVES,
    C64_TAB_TAPECART,
    C64_TAB_DISPLAY,
    C64_TAB_AUDIO,
    C64_TAB_CONTROLS,
    C64_TAB_SAVESTATES,
    C64_TAB_SYSTEM,
    C64_TAB_COUNT
} C64GuiTab;

typedef struct {
    unsigned int buttons;
    unsigned int pressed;
    unsigned int released;
    int lx, ly;
    int rx, ry;
} C64InputState;

void c64_gui_init(void);
void c64_gui_shutdown(void);
void c64_gui_show(void);
bool c64_gui_is_active(void);
void c64_gui_request_close(void);
void c64_show_credits(void);
void c64_gui_draw_progress(const char *title, const char *subtitle, float fraction, const char *item_name);

#ifdef __cplusplus
}
#endif

#endif
