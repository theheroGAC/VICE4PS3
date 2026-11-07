#include "c64_vkbd_ps3.h"
#include "c64_gui_ps3.h"
#include "ps3_gfx.h"
#include "ps3_ctrl.h"

extern "C" {
#include "keyboard.h"
#include "machine.h"
void c64keyboard_restore_key(int v);
}

#include <stdio.h>
#include <string.h>

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int up;
    int down;
    int left;
    int right;
    int mid;
    const char *label;
    const char *sublabel;
} C64KeyDef;

static bool s_vkbd_visible = false;
static int s_selected_key = 62;
static bool s_shift_locked = false;
static bool s_ctrl_locked = false;
static bool s_cbm_locked = false;
static int s_active_press_mid = -1;
static int s_held_key_mid = -1;
static int s_vkbd_y_offset = 270;
static int s_vkbd_alpha = 240;
static ps3_gfx_font *s_vkbd_font = NULL;

#define KEY_CNT 67

static C64KeyDef s_keys[KEY_CNT];
static int s_key_sub_x[KEY_CNT];
static int s_key_sub_y[KEY_CNT];
static int s_key_main_x[KEY_CNT];
static int s_key_main_y[KEY_CNT];

static void compute_text_offsets(void)
{
    for (int i = 0; i < KEY_CNT; i++) {
        const C64KeyDef *k = &s_keys[i];
        bool has_sub = (k->sublabel && k->sublabel[0] != '\0');
        bool is_crsr = (k->mid >= 201 && k->mid <= 204);
        if (has_sub) {
            int sub_w = s_vkbd_font ? ps3_gfx_font_text_width(s_vkbd_font, 9.0f, k->sublabel) : ((int)strlen(k->sublabel) * 8);
            s_key_sub_x[i] = k->x + (k->w - sub_w) / 2;
            s_key_sub_y[i] = k->y + 14;

            float main_scale = is_crsr ? 9.5f : 10.0f;
            int main_w = s_vkbd_font ? ps3_gfx_font_text_width(s_vkbd_font, main_scale, k->label) : ((int)strlen(k->label) * 9);
            s_key_main_x[i] = k->x + (k->w - main_w) / 2;
            s_key_main_y[i] = k->y + 30;
        } else {
            float label_scale = 10.0f;
            int label_w = s_vkbd_font ? ps3_gfx_font_text_width(s_vkbd_font, label_scale, k->label) : ((int)strlen(k->label) * 9);
            s_key_main_x[i] = k->x + (k->w - label_w) / 2;
            s_key_main_y[i] = k->y + 23;
            s_key_sub_x[i] = 0;
            s_key_sub_y[i] = 0;
        }
    }
}

static void init_key_defs(void)
{
    int idx = 0;
    int ky = 28;
    int kw = 48;
    int kh = 38;
    int gap = 4;

    s_keys[idx] = { 10 + 0*(kw+gap), ky, kw, kh, 62, 17, 16, 1, 113, "<-", "" }; idx++;
    s_keys[idx] = { 10 + 1*(kw+gap), ky, kw, kh, 62, 18, 0, 2, 112, "1", "!" }; idx++;
    s_keys[idx] = { 10 + 2*(kw+gap), ky, kw, kh, 62, 19, 1, 3, 115, "2", "\"" }; idx++;
    s_keys[idx] = { 10 + 3*(kw+gap), ky, kw, kh, 62, 20, 2, 4, 16, "3", "#" }; idx++;
    s_keys[idx] = { 10 + 4*(kw+gap), ky, kw, kh, 62, 21, 3, 5, 19, "4", "$" }; idx++;
    s_keys[idx] = { 10 + 5*(kw+gap), ky, kw, kh, 62, 22, 4, 6, 32, "5", "%" }; idx++;
    s_keys[idx] = { 10 + 6*(kw+gap), ky, kw, kh, 62, 23, 5, 7, 35, "6", "&" }; idx++;
    s_keys[idx] = { 10 + 7*(kw+gap), ky, kw, kh, 62, 24, 6, 8, 48, "7", "'" }; idx++;
    s_keys[idx] = { 10 + 8*(kw+gap), ky, kw, kh, 62, 25, 7, 9, 51, "8", "(" }; idx++;
    s_keys[idx] = { 10 + 9*(kw+gap), ky, kw, kh, 63, 26, 8, 10, 64, "9", ")" }; idx++;
    s_keys[idx] = { 10 + 10*(kw+gap), ky, kw, kh, 63, 27, 9, 11, 67, "0", "" }; idx++;
    s_keys[idx] = { 10 + 11*(kw+gap), ky, kw, kh, 64, 28, 10, 12, 80, "+", "" }; idx++;
    s_keys[idx] = { 10 + 12*(kw+gap), ky, kw, kh, 65, 29, 11, 13, 83, "-", "" }; idx++;
    s_keys[idx] = { 10 + 13*(kw+gap), ky, kw, kh, 65, 30, 12, 14, 96, "\xC2\xA3", "" }; idx++;
    s_keys[idx] = { 10 + 14*(kw+gap), ky, kw, kh, 66, 30, 13, 15, 99, "HOME", "CLR" }; idx++;
    s_keys[idx] = { 10 + 15*(kw+gap), ky, kw+8, kh, 66, 30, 14, 16, 0, "DEL", "INST" }; idx++;
    s_keys[idx] = { 856, ky, kw, kh, 66, 31, 15, 0, 4, "F1", "F2" }; idx++;

    ky += kh + gap;
    s_keys[idx] = { 10, ky, kw+8, kh, 0, 32, 31, 18, 114, "CTRL", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 0*(kw+gap), ky, kw, kh, 1, 33, 17, 19, 118, "Q", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 1*(kw+gap), ky, kw, kh, 2, 34, 18, 20, 17, "W", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 2*(kw+gap), ky, kw, kh, 3, 35, 19, 21, 22, "E", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 3*(kw+gap), ky, kw, kh, 4, 36, 20, 22, 33, "R", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 4*(kw+gap), ky, kw, kh, 5, 37, 21, 23, 38, "T", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 5*(kw+gap), ky, kw, kh, 6, 38, 22, 24, 49, "Y", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 6*(kw+gap), ky, kw, kh, 7, 39, 23, 25, 54, "U", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 7*(kw+gap), ky, kw, kh, 8, 40, 24, 26, 65, "I", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 8*(kw+gap), ky, kw, kh, 9, 41, 25, 27, 70, "O", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 9*(kw+gap), ky, kw, kh, 10, 42, 26, 28, 71, "P", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 10*(kw+gap), ky, kw, kh, 11, 43, 27, 29, 86, "@", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 11*(kw+gap), ky, kw, kh, 12, 44, 28, 30, 97, "*", "" }; idx++;
    s_keys[idx] = { 10 + (kw+8+gap) + 12*(kw+gap), ky, kw+8, kh, 14, 45, 29, 31, 56, "REST", "" }; idx++;
    s_keys[idx] = { 856, ky, kw, kh, 16, 46, 30, 17, 5, "F3", "F4" }; idx++;

    ky += kh + gap;
    s_keys[idx] = { 10, ky, kw+6, kh, 17, 47, 46, 33, 119, "R/S", "RUN" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 0*(kw+gap), ky, kw, kh, 18, 48, 32, 34, 18, "A", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 1*(kw+gap), ky, kw, kh, 19, 49, 33, 35, 21, "S", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 2*(kw+gap), ky, kw, kh, 20, 50, 34, 36, 34, "D", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 3*(kw+gap), ky, kw, kh, 21, 51, 35, 37, 37, "F", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 4*(kw+gap), ky, kw, kh, 22, 52, 36, 38, 50, "G", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 5*(kw+gap), ky, kw, kh, 23, 53, 37, 39, 53, "H", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 6*(kw+gap), ky, kw, kh, 24, 54, 38, 40, 66, "J", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 7*(kw+gap), ky, kw, kh, 25, 55, 39, 41, 69, "K", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 8*(kw+gap), ky, kw, kh, 26, 56, 40, 42, 82, "L", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 9*(kw+gap), ky, kw, kh, 27, 57, 41, 43, 85, ":", "[" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 10*(kw+gap), ky, kw, kh, 28, 58, 42, 44, 98, ";", "]" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 11*(kw+gap), ky, kw, kh, 29, 59, 43, 45, 101, "=", "" }; idx++;
    s_keys[idx] = { 10 + (kw+6+gap) + 12*(kw+gap), ky, kw+14, kh, 30, 60, 44, 46, 1, "RET", "" }; idx++;
    s_keys[idx] = { 856, ky, kw, kh, 31, 61, 45, 32, 6, "F5", "F6" }; idx++;

    ky += kh + gap;
    s_keys[idx] = { 10, ky, kw, kh, 32, 62, 61, 48, 117, "C=", "" }; idx++;
    s_keys[idx] = { 10 + 1*(kw+gap), ky, kw+4, kh, 33, 62, 47, 49, 23, "SHFT", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 1*(kw+gap), ky, kw, kh, 34, 62, 48, 50, 20, "Z", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 2*(kw+gap), ky, kw, kh, 35, 62, 49, 51, 39, "X", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 3*(kw+gap), ky, kw, kh, 36, 62, 50, 52, 36, "C", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 4*(kw+gap), ky, kw, kh, 37, 62, 51, 53, 55, "V", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 5*(kw+gap), ky, kw, kh, 38, 62, 52, 54, 52, "B", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 6*(kw+gap), ky, kw, kh, 39, 62, 53, 55, 71, "N", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 7*(kw+gap), ky, kw, kh, 40, 62, 54, 56, 68, "M", "" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 8*(kw+gap), ky, kw, kh, 41, 62, 55, 57, 87, ",", "<" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 9*(kw+gap), ky, kw, kh, 42, 62, 56, 58, 84, ".", ">" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 10*(kw+gap), ky, kw, kh, 43, 63, 57, 59, 103, "/", "?" }; idx++;
    s_keys[idx] = { 10 + (kw+4+gap) + 11*(kw+gap), ky, kw+4, kh, 44, 63, 58, 60, 100, "SHFT", "" }; idx++;
    s_keys[idx] = { 750, ky, 52, kh, 45, 65, 59, 61, 201, "UP", "^" }; idx++;
    s_keys[idx] = { 856, ky, kw, kh, 46, 66, 60, 47, 3, "F7", "F8" }; idx++;

    ky += kh + gap;
    s_keys[idx] = { 120, ky, 440, kh, 53, 0, 47, 63, 116, "SPACE", "" }; idx++;
    s_keys[idx] = { 580, ky, 58, kh, 58, 10, 62, 64, 119, "STOP", "" }; idx++;
    s_keys[idx] = { 688, ky, 52, kh, 59, 12, 63, 65, 203, "LEFT", "<" }; idx++;
    s_keys[idx] = { 750, ky, 52, kh, 60, 13, 64, 66, 202, "DOWN", "v" }; idx++;
    s_keys[idx] = { 812, ky, 52, kh, 61, 14, 65, 62, 204, "RIGHT", ">" }; idx++;
}

void c64_vkbd_init(void)
{
    static const char *font_paths[] = {
        "data/font.ttf",
        "resources/data/font.ttf",
        "/dev_hdd0/game/VICEP3001/USRDIR/data/font.ttf",
        "/dev_hdd0/game/VICEP3001/USRDIR/resources/data/font.ttf",
        NULL
    };

    init_key_defs();
    if (!s_vkbd_font) {
        for (int i = 0; font_paths[i]; i++) {
            s_vkbd_font = ps3_gfx_load_font_file(font_paths[i]);
            if (s_vkbd_font) break;
        }
    }
    compute_text_offsets();
    s_vkbd_visible = false;
    s_selected_key = 62;
}

void c64_vkbd_shutdown(void)
{
    s_vkbd_visible = false;
    keyboard_clear_keymatrix();
    if (s_vkbd_font) {
        ps3_gfx_free_font(s_vkbd_font);
        s_vkbd_font = NULL;
    }
}

void c64_vkbd_toggle(void)
{
    s_vkbd_visible = !s_vkbd_visible;
    if (!s_vkbd_visible) {
        keyboard_clear_keymatrix();
    }
}

void c64_vkbd_show(void)
{
    s_vkbd_visible = true;
}

void c64_vkbd_hide(void)
{
    s_vkbd_visible = false;
    keyboard_clear_keymatrix();
}

bool c64_vkbd_is_visible(void)
{
    return s_vkbd_visible;
}

static void trigger_c64_key(int mid, int press)
{
    if (mid < 0) return;

    if (mid == 56) {
        if (press) c64keyboard_restore_key(1);
        else c64keyboard_restore_key(0);
        return;
    }

    if (mid == 201) {
        keyboard_set_keyarr_any(1, 7, press ? 1 : (s_shift_locked ? 1 : 0));
        keyboard_set_keyarr_any(0, 7, press);
        return;
    }
    if (mid == 202) {
        keyboard_set_keyarr_any(0, 7, press);
        return;
    }
    if (mid == 203) {
        keyboard_set_keyarr_any(1, 7, press ? 1 : (s_shift_locked ? 1 : 0));
        keyboard_set_keyarr_any(0, 2, press);
        return;
    }
    if (mid == 204) {
        keyboard_set_keyarr_any(0, 2, press);
        return;
    }

    int row = (mid & 0x08) ? -(mid >> 4) : (mid >> 4);
    int column = mid & 0x07;
    keyboard_set_keyarr_any(row, column, press);
}

static void vkbd_nav_move(unsigned int bit)
{
    if (bit == SCE_CTRL_UP) {
        s_selected_key = s_keys[s_selected_key].up;
    } else if (bit == SCE_CTRL_DOWN) {
        s_selected_key = s_keys[s_selected_key].down;
    } else if (bit == SCE_CTRL_LEFT) {
        s_selected_key = s_keys[s_selected_key].left;
    } else if (bit == SCE_CTRL_RIGHT) {
        s_selected_key = s_keys[s_selected_key].right;
    }
}

bool c64_vkbd_process_input(unsigned int pressed, unsigned int held, unsigned int released)
{
    if (!s_vkbd_visible) {
        return false;
    }

    if (s_active_press_mid >= 0) {
        trigger_c64_key(s_active_press_mid, 0);
        s_active_press_mid = -1;
    }

    SceCtrlData ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    sceCtrlPeekBufferPositive(0, &ctrl, 1);

    if (ctrl.ry < 64) {
        s_vkbd_y_offset -= 4;
        if (s_vkbd_y_offset < 10) s_vkbd_y_offset = 10;
    } else if (ctrl.ry > 192) {
        s_vkbd_y_offset += 4;
        if (s_vkbd_y_offset > 290) s_vkbd_y_offset = 290;
    }

    if (ctrl.rx < 64) {
        s_vkbd_alpha -= 3;
        if (s_vkbd_alpha < 120) s_vkbd_alpha = 120;
    } else if (ctrl.rx > 192) {
        s_vkbd_alpha += 3;
        if (s_vkbd_alpha > 255) s_vkbd_alpha = 255;
    }

    static const unsigned int nav_bits[4] = {
        SCE_CTRL_UP, SCE_CTRL_DOWN, SCE_CTRL_LEFT, SCE_CTRL_RIGHT
    };
    static int nav_delay = 8;
    static int nav_holds = 0;

    int nav_idx = -1;
    int i;
    for (i = 0; i < 4; i++) {
        if ((pressed | held) & nav_bits[i]) {
            nav_idx = i;
            break;
        }
    }
    if (nav_idx < 0) {
        nav_delay = 8;
        nav_holds = 0;
    } else if (pressed & nav_bits[nav_idx]) {
        vkbd_nav_move(nav_bits[nav_idx]);
        nav_delay = 8;
        nav_holds = 0;
    } else {
        nav_holds++;
        if (--nav_delay <= 0) {
            vkbd_nav_move(nav_bits[nav_idx]);
            if (nav_holds > 30) {
                nav_delay = 1;
            } else if (nav_holds > 10) {
                nav_delay = 2;
            } else {
                nav_delay = 3;
            }
        }
    }

    if (pressed & SCE_CTRL_SQUARE) {
        trigger_c64_key(116, 1);
        s_active_press_mid = 116;
        return true;
    }

    if (pressed & SCE_CTRL_TRIANGLE) {
        trigger_c64_key(0, 1);
        s_active_press_mid = 0;
        return true;
    }

    if (pressed & SCE_CTRL_CIRCLE) {
        if (s_shift_locked || s_ctrl_locked || s_cbm_locked) {
            if (s_shift_locked) { trigger_c64_key(23, 0); s_shift_locked = false; }
            if (s_ctrl_locked) { trigger_c64_key(114, 0); s_ctrl_locked = false; }
            if (s_cbm_locked) { trigger_c64_key(117, 0); s_cbm_locked = false; }
        } else {
            c64_vkbd_hide();
        }
        return true;
    }

    if (pressed & (SCE_CTRL_LTRIGGER | SCE_CTRL_START)) {
        c64_vkbd_hide();
        return true;
    }

    if (pressed & SCE_CTRL_L1) {
        s_vkbd_y_offset -= 30;
        if (s_vkbd_y_offset < 10) s_vkbd_y_offset = 10;
    }
    if (pressed & SCE_CTRL_R1) {
        s_vkbd_y_offset += 30;
        if (s_vkbd_y_offset > 290) s_vkbd_y_offset = 290;
    }

    if (s_held_key_mid >= 0 && !(held & SCE_CTRL_CROSS)) {
        trigger_c64_key(s_held_key_mid, 0);
        s_held_key_mid = -1;
    }

    if (released & SCE_CTRL_CROSS && s_held_key_mid >= 0) {
        trigger_c64_key(s_held_key_mid, 0);
        s_held_key_mid = -1;
    }

    if (pressed & SCE_CTRL_CROSS) {
        int mid = s_keys[s_selected_key].mid;

        if (mid == 23 || mid == 100) {
            s_shift_locked = !s_shift_locked;
            trigger_c64_key(23, s_shift_locked ? 1 : 0);
        } else if (mid == 114) {
            s_ctrl_locked = !s_ctrl_locked;
            trigger_c64_key(114, s_ctrl_locked ? 1 : 0);
        } else if (mid == 117) {
            s_cbm_locked = !s_cbm_locked;
            trigger_c64_key(117, s_cbm_locked ? 1 : 0);
        } else {
            trigger_c64_key(mid, 1);
            s_held_key_mid = mid;

            if (s_shift_locked && mid != 201 && mid != 203) {
                trigger_c64_key(23, 0);
                s_shift_locked = false;
            }
        }
    }

    return true;
}

void c64_vkbd_render(void)
{
    if (!s_vkbd_visible) {
        return;
    }

    int bg_x = 4;
    int bg_y = s_vkbd_y_offset;
    int bg_w = C64_GUI_SCREEN_W - 8;
    int bg_h = 246;
    int ri;
    static const unsigned int rainbow[5] = {
        RGBA8(227, 36, 36, 255),
        RGBA8(247, 148, 30, 255),
        RGBA8(255, 216, 40, 255),
        RGBA8(64, 160, 64, 255),
        RGBA8(48, 92, 220, 255)
    };

    ps3_gfx_draw_rectangle(bg_x, bg_y, bg_w, bg_h, RGBA8(18, 22, 34, s_vkbd_alpha));
    ps3_gfx_draw_line(bg_x, bg_y, bg_x + bg_w, bg_y, RGBA8(58, 70, 102, 255));
    ps3_gfx_draw_line(bg_x, bg_y + 24, bg_x + bg_w, bg_y + 24, RGBA8(36, 44, 66, 255));
    ps3_gfx_draw_line(bg_x, bg_y + bg_h - 1, bg_x + bg_w, bg_y + bg_h - 1, RGBA8(12, 16, 26, 255));
    ps3_gfx_draw_line(bg_x, bg_y, bg_x, bg_y + bg_h, RGBA8(46, 56, 82, 255));
    ps3_gfx_draw_line(bg_x + bg_w - 1, bg_y, bg_x + bg_w - 1, bg_y + bg_h, RGBA8(46, 56, 82, 255));

    for (ri = 0; ri < 5; ri++) {
        ps3_gfx_draw_rectangle(bg_x + bg_w - 38, bg_y + 5 + ri * 3, 28, 3, rainbow[ri]);
    }

    if (s_shift_locked) {
        ps3_gfx_draw_rectangle(bg_x + 10, bg_y + 4, 52, 16, C64_COLOR_GOLD);
        if (s_vkbd_font) ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 16, bg_y + 16, RGBA8(28, 20, 6, 255), 9.0f, "SHIFT");
    } else {
        ps3_gfx_draw_rectangle(bg_x + 10, bg_y + 4, 52, 16, RGBA8(26, 32, 48, 255));
        if (s_vkbd_font) ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 16, bg_y + 16, RGBA8(100, 115, 145, 255), 9.0f, "SHIFT");
    }

    if (s_ctrl_locked) {
        ps3_gfx_draw_rectangle(bg_x + 68, bg_y + 4, 48, 16, C64_COLOR_CELESTINO);
        if (s_vkbd_font) ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 76, bg_y + 16, RGBA8(12, 24, 48, 255), 9.0f, "CTRL");
    } else {
        ps3_gfx_draw_rectangle(bg_x + 68, bg_y + 4, 48, 16, RGBA8(26, 32, 48, 255));
        if (s_vkbd_font) ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 76, bg_y + 16, RGBA8(100, 115, 145, 255), 9.0f, "CTRL");
    }

    if (s_cbm_locked) {
        ps3_gfx_draw_rectangle(bg_x + 122, bg_y + 4, 48, 16, RGBA8(180, 120, 235, 255));
        if (s_vkbd_font) ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 130, bg_y + 16, RGBA8(32, 10, 48, 255), 9.0f, "CBM");
    } else {
        ps3_gfx_draw_rectangle(bg_x + 122, bg_y + 4, 48, 16, RGBA8(26, 32, 48, 255));
        if (s_vkbd_font) ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 130, bg_y + 16, RGBA8(100, 115, 145, 255), 9.0f, "CBM");
    }

    if (s_vkbd_font) {
        ps3_gfx_font_draw_text(s_vkbd_font, bg_x + 185, bg_y + 16, RGBA8(132, 148, 180, 255), 9.5f,
            "[X] Type   [Sq] Space   [Tri] Del   [O] Close   [R-Stick] Pos/Alpha");
    }

    for (int i = 0; i < KEY_CNT; i++) {
        const C64KeyDef *k = &s_keys[i];
        int kx = bg_x + k->x;
        int ky = bg_y + k->y;
        bool is_selected = (i == s_selected_key);
        bool is_fn = (k->mid >= 3 && k->mid <= 6);
        bool is_crsr = (k->mid >= 201 && k->mid <= 204);
        bool is_mod = (k->mid == 114 || k->mid == 117 || k->mid == 23 || k->mid == 100 || k->mid == 1 || k->mid == 56 || k->mid == 119);

        unsigned int key_color;
        unsigned int border_color;

        if (is_selected) {
            key_color = RGBA8(32, 108, 198, 255);
            border_color = C64_COLOR_CELESTINO;
        } else if (is_crsr) {
            key_color = RGBA8(24, 46, 74, 255);
            border_color = RGBA8(44, 88, 140, 255);
        } else if (is_fn) {
            key_color = RGBA8(54, 40, 24, 255);
            border_color = RGBA8(92, 70, 42, 255);
        } else if (is_mod) {
            bool locked = (k->mid == 23 || k->mid == 100) ? s_shift_locked : ((k->mid == 114) ? s_ctrl_locked : ((k->mid == 117) ? s_cbm_locked : false));
            key_color = locked ? C64_COLOR_GOLD : RGBA8(26, 32, 48, 255);
            border_color = locked ? RGBA8(255, 230, 100, 255) : RGBA8(48, 58, 84, 255);
        } else {
            key_color = RGBA8(32, 38, 56, 255);
            border_color = RGBA8(54, 66, 94, 255);
        }

        ps3_gfx_draw_rectangle(kx, ky, k->w, k->h, key_color);

        ps3_gfx_draw_line(kx + 1, ky + 1, kx + k->w - 1, ky + 1, is_selected ? C64_COLOR_WHITE : border_color);
        ps3_gfx_draw_line(kx, ky, kx + k->w, ky, border_color);
        ps3_gfx_draw_line(kx, ky + k->h - 1, kx + k->w, ky + k->h - 1, RGBA8(14, 18, 28, 255));
        ps3_gfx_draw_line(kx, ky, kx, ky + k->h, border_color);
        ps3_gfx_draw_line(kx + k->w - 1, ky, kx + k->w - 1, ky + k->h, border_color);

        if (is_selected) {
            ps3_gfx_draw_line(kx + 1, ky + 1, kx + k->w - 1, ky + 1, C64_COLOR_WHITE);
            ps3_gfx_draw_line(kx + 1, ky + k->h - 2, kx + k->w - 1, ky + k->h - 2, C64_COLOR_CELESTINO);
        }
    }

    if (s_vkbd_font) {
        for (int i = 0; i < KEY_CNT; i++) {
            const C64KeyDef *k = &s_keys[i];
            if (k->sublabel && k->sublabel[0] != '\0') {
                bool is_selected = (i == s_selected_key);
                bool is_fn = (k->mid >= 3 && k->mid <= 6);
                bool is_crsr = (k->mid >= 201 && k->mid <= 204);
                unsigned int sub_color = is_selected ? C64_COLOR_WHITE : (is_crsr ? C64_COLOR_CELESTINO : (is_fn ? RGBA8(210, 180, 140, 255) : RGBA8(130, 160, 205, 255)));
                ps3_gfx_font_draw_text(s_vkbd_font, bg_x + s_key_sub_x[i], bg_y + s_key_sub_y[i], sub_color, 9.0f, k->sublabel);
            }
        }

        for (int i = 0; i < KEY_CNT; i++) {
            const C64KeyDef *k = &s_keys[i];
            bool is_selected = (i == s_selected_key);
            bool is_crsr = (k->mid >= 201 && k->mid <= 204);
            bool is_fn = (k->mid >= 3 && k->mid <= 6);
            bool is_mod = (k->mid == 114 || k->mid == 117 || k->mid == 23 || k->mid == 100 || k->mid == 1 || k->mid == 56 || k->mid == 119);
            unsigned int text_color;

            if (is_selected) {
                text_color = C64_COLOR_WHITE;
            } else if (is_crsr) {
                text_color = C64_COLOR_CELESTINO;
            } else if (is_fn) {
                text_color = RGBA8(248, 234, 210, 255);
            } else if (is_mod) {
                bool locked = (k->mid == 23 || k->mid == 100) ? s_shift_locked : ((k->mid == 114) ? s_ctrl_locked : ((k->mid == 117) ? s_cbm_locked : false));
                text_color = locked ? RGBA8(30, 20, 6, 255) : RGBA8(220, 228, 244, 255);
            } else {
                text_color = RGBA8(234, 240, 252, 255);
            }

            float main_scale = is_crsr ? 9.5f : 10.0f;
            ps3_gfx_font_draw_text(s_vkbd_font, bg_x + s_key_main_x[i], bg_y + s_key_main_y[i], text_color, main_scale, k->label);
        }
    }
}
