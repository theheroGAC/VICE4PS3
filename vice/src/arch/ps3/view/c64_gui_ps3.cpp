#include "c64_gui_ps3.h"
#include "c64_vkbd_ps3.h"
#include "ps3_gfx.h"
#include "ps3_ctrl.h"
#include "ps3_thread.h"
#include "view.h"
#include "controller.h"
#include "extractor.h"
#include "app_defs.h"

extern "C" {
#include "main.h"
#include "ui.h"
#include "autostart.h"
#include "attach.h"
#include "cartridge.h"
#include "datasette.h"
#include "tape.h"
#include "drive.h"
#include "machine.h"
#include "../../../resources.h"
int resources_get_int(const char *name, int *val);
int resources_set_int(const char *name, int val);
#include "sound.h"
#include "vsync.h"
#include "video.h"
#include "videoarch.h"
#include "joystick.h"
#include "c64cartsystem.h"
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ps3_io.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

extern "C" void PS3_ShutdownAndExit(void);
extern "C" int PS3_LoadGameFile(const char *path);

using std::string;
using std::vector;

enum {
    GUI_BTN_CROSS = 0,
    GUI_BTN_CIRCLE = 1,
    GUI_BTN_SQUARE = 2,
    GUI_BTN_TRIANGLE = 3,
    GUI_BTN_L1 = 9,
    GUI_BTN_R1 = 10,
    GUI_BTN_L2 = 15,
    GUI_BTN_R2 = 16,
};

struct FileItem {
    string name;
    string path;
    bool is_dir;
    size_t size;
    string ext;
};

typedef enum {
    CR_EMPTY = 0,
    CR_TITLE,
    CR_SUBTITLE,
    CR_SECTION,
    CR_TEXT,
    CR_DIM,
    CR_LINK,
    CR_RED
} CreditStyle;

typedef struct {
    const char *text;
    CreditStyle style;
} CreditLine;

static bool s_gui_active = false;
static C64GuiTab s_current_tab = C64_TAB_LIBRARY;
static int s_lib_selected = 0;
static int s_lib_scroll_top = 0;
static string s_current_dir = "";

static vector<FileItem> s_file_list;
static TTF_Font *s_font_11 = NULL;
static TTF_Font *s_font_13 = NULL;
static TTF_Font *s_font_14 = NULL;
static TTF_Font *s_font_16 = NULL;
static TTF_Font *s_font_20 = NULL;
static TTF_Font *s_font_26 = NULL;
static ps3_gfx_texture *s_btn_icons[17] = {0};
static ps3_gfx_texture *s_cover_texture = NULL;
static string s_cover_loaded_path = "";

static int s_drives_selected = 0;
static int s_tapecart_selected = 0;
static int s_display_selected = 0;
static int s_audio_selected = 0;
static int s_controls_selected = 0;
static int s_savestates_selected = 0;
static int s_system_selected = 0;

static unsigned int s_prev_pad_buttons = 0;

static const char *gui_setting_value(int key)
{
    const char *value = PS3_GetGuiSetting(key);
    return (value && *value) ? value : "";
}

static void gui_cycle_setting(int key, const char *const *values, int count)
{
    const char *current;
    int index = -1;

    if (!values || count <= 0) return;

    current = gui_setting_value(key);
    for (int i = 0; i < count; i++) {
        if (strcmp(current, values[i]) == 0) {
            index = i;
            break;
        }
    }

    PS3_ApplyGuiSetting(key, values[(index + 1) % count]);
}

static const char *gui_aspect_label(void)
{
    const char *value = gui_setting_value(ASPECT_RATIO);
    if (strcmp(value, "16:9") == 0) return "16:9 Widescreen";
    if (strcmp(value, "4:3 max") == 0) return "4:3 Max (Fill Height)";
    return "4:3 Original (Authentic CRT)";
}

static const char *gui_border_label(void)
{
    return (strcmp(gui_setting_value(BORDERS), "Hide") == 0) ?
           "Hidden (Full Screen)" : "Show (Authentic)";
}

static const char *gui_filter_label(void)
{
    return (strcmp(gui_setting_value(TEXTURE_FILTER), "Point") == 0) ?
           "Point (Sharp Pixels)" : "Linear (Smooth)";
}

static const char *gui_video_standard_label(void)
{
    const char *value = gui_setting_value(VICII_MODEL);
    if (strcmp(value, "NTSC") == 0 || strcmp(value, "Old NTSC") == 0)
        return "NTSC (60 Hz / USA)";
    if (strcmp(value, "PAL-N") == 0) return "PAL-N (50 Hz)";
    return "PAL (50 Hz / Europe)";
}

static const char *gui_sid_engine_label(void)
{
    const char *engine = gui_setting_value(SID_ENGINE);

    if (strcmp(engine, "ReSID-FP") == 0) {
        return "ReSID-FP (High Quality)";
    }
    return (strcmp(engine, "FastSID") == 0) ?
           "FastSID (Legacy)" : "ReSID (Cycle-Exact / Authentic)";
}

static const char *gui_joystick_port_label(void)
{
    return (strcmp(gui_setting_value(JOYSTICK_PORT), "Port 1") == 0) ?
           "Port 1 (Gamepad)" : "Port 2 (Default C64 Game Port)";
}

static const char *gui_joystick_side_label(void)
{
    return (strcmp(gui_setting_value(JOYSTICK_SIDE), "Right") == 0) ?
           "Right Analog Stick" : "Left Analog Stick";
}

static const char *gui_autofire_label(void)
{
    return PS3_GetAutofire() ? "ON" : "OFF";
}

static const char *gui_color_palette_label(void)
{
    const char *value = gui_setting_value(COLOR_PALETTE);
    return (value && *value) ? value : "Colodore";
}

static const char *gui_autofire_speed_label(void)
{
    const char *value = gui_setting_value(JOYSTICK_AUTOFIRE_SPEED);
    return (value && *value) ? value : "Fast";
}

static const char *gui_sid_filter_label(void)
{
    int enabled = 0;
    resources_get_int("SidFilters", &enabled);
    return enabled ? "Enabled (Analog Filter)" : "Disabled (Clean)";
}

static const char *s_tab_names[C64_TAB_COUNT] = {
    "LIBRARY", "DRIVES", "TAPE / CART", "DISPLAY", "AUDIO", "CONTROLS", "SAVESTATES", "SYSTEM"
};

static const char *s_scan_roots[] = {
    "/dev_usb000/c64",
    "/dev_usb000/roms/c64",
    "/dev_usb000/roms",
    "/dev_usb000/vice",
    "/dev_usb000",
    "/dev_usb001/c64",
    "/dev_usb001/roms/c64",
    "/dev_usb001/roms",
    "/dev_usb001/vice",
    "/dev_usb001",
    "/dev_usb002",
    "/dev_usb003",
    "/dev_usb006",
    "/dev_usb",
    "/dev_hdd0/game/VICEP3001/USRDIR/games",
    "/dev_hdd0/ROMS/C64",
    "/dev_hdd0/GAMES/C64",
    "/dev_hdd0/c64",
    "/dev_hdd0/roms",
    "data/games",
    "games",
    "roms"
};
static const int s_scan_root_count = sizeof(s_scan_roots) / sizeof(s_scan_roots[0]);

static void draw_rect(float x, float y, float w, float h, unsigned int color)
{
    ps3_gfx_draw_rectangle((int)x, (int)y, (int)w, (int)h, color);
}

static void draw_rect_outline(float x, float y, float w, float h, unsigned int color)
{
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    ps3_gfx_draw_line(ix, iy, ix + iw, iy, color);
    ps3_gfx_draw_line(ix, iy + ih, ix + iw, iy + ih, color);
    ps3_gfx_draw_line(ix, iy, ix, iy + ih, color);
    ps3_gfx_draw_line(ix + iw, iy, ix + iw, iy + ih, color);
}

static void draw_round_card(float x, float y, float w, float h, unsigned int bg, unsigned int border)
{
    draw_rect(x, y, w, h, bg);
    draw_rect_outline(x, y, w, h, border);
}

static TTF_Font *get_font_for_size(int size)
{
    if (size <= 11) return s_font_11 ? s_font_11 : (s_font_13 ? s_font_13 : s_font_16);
    if (size <= 13) return s_font_13 ? s_font_13 : s_font_16;
    if (size <= 15) return s_font_14 ? s_font_14 : s_font_16;
    if (size <= 18) return s_font_16;
    if (size <= 23) return s_font_20 ? s_font_20 : s_font_16;
    return s_font_26 ? s_font_26 : s_font_16;
}

struct GuiTextCacheEntry {
    int w;
    int h;
    vector<unsigned char> pixels;
};

static std::map<string, GuiTextCacheEntry> s_gui_text_cache;

static const GuiTextCacheEntry *get_gui_text_cached(int font_size, unsigned int color, const char *text)
{
    if (!text || !*text) return NULL;
    TTF_Font *font = get_font_for_size(font_size);
    if (!font) return NULL;

    char prefix[48];
    snprintf(prefix, sizeof(prefix), "%d|%u|", font_size, color);
    string key = prefix;
    key += text;

    std::map<string, GuiTextCacheEntry>::const_iterator it = s_gui_text_cache.find(key);
    if (it != s_gui_text_cache.end()) {
        return &it->second;
    }

    if (s_gui_text_cache.size() >= 512) {
        s_gui_text_cache.clear();
    }

    SDL_Color c;
    c.r = (color >> 0) & 0xFF;
    c.g = (color >> 8) & 0xFF;
    c.b = (color >> 16) & 0xFF;
    c.a = (color >> 24) & 0xFF;

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, c);
    if (!surf) return NULL;

    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!rgba) return NULL;

    GuiTextCacheEntry entry;
    entry.w = rgba->w;
    entry.h = rgba->h;
    entry.pixels.resize((size_t)rgba->w * (size_t)rgba->h * 4);

    SDL_LockSurface(rgba);
    for (int sy = 0; sy < rgba->h; sy++) {
        memcpy(&entry.pixels[(size_t)sy * rgba->w * 4],
               (const unsigned char *)rgba->pixels + (size_t)sy * rgba->pitch,
               (size_t)rgba->w * 4);
    }
    SDL_UnlockSurface(rgba);
    SDL_FreeSurface(rgba);

    s_gui_text_cache[key] = entry;
    return &s_gui_text_cache[key];
}

static void draw_text(float x, float y, unsigned int color, int font_size, const char *text)
{
    const GuiTextCacheEntry *entry = get_gui_text_cached(font_size, color, text);
    if (!entry || entry->w <= 0 || entry->h <= 0) return;

    unsigned char *fb = (unsigned char *)ps3_gfx_get_current_fb();
    if (!fb) return;

    int dst_x = (int)x;
    int dst_y = (int)y;

    for (int sy = 0; sy < entry->h; sy++) {
        int py = dst_y + sy;
        if (py < 0 || py >= C64_GUI_SCREEN_H) continue;
        const unsigned char *src_row = &entry->pixels[(size_t)sy * entry->w * 4];
        unsigned char *dst_row = fb + (size_t)py * (PS3_GFX_FB_WIDTH * 4);

        for (int sx = 0; sx < entry->w; sx++) {
            int px = dst_x + sx;
            if (px < 0 || px >= C64_GUI_SCREEN_W) continue;

            const unsigned char *src = src_row + sx * 4;
            unsigned int sa = src[3];
            if (sa == 0) continue;

            unsigned char *dst = dst_row + px * 4;
            if (sa == 255) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
            } else {
                unsigned int inv = 255 - sa;
                dst[0] = (unsigned char)((src[0] * sa + dst[0] * inv) / 255);
                dst[1] = (unsigned char)((src[1] * sa + dst[1] * inv) / 255);
                dst[2] = (unsigned char)((src[2] * sa + dst[2] * inv) / 255);
                dst[3] = 255;
            }
        }
    }
}

static void draw_text_clipped(float x, float y, float max_w, unsigned int color, int font_size, const char *str)
{
    if (!str || !*str || max_w <= 10.0f) return;
    const GuiTextCacheEntry *entry = get_gui_text_cached(font_size, color, str);
    if (!entry) return;
    if (entry->w <= (int)max_w) {
        draw_text(x, y, color, font_size, str);
        return;
    }
    TTF_Font *font = get_font_for_size(font_size);
    if (!font) return;

    int w = 0, h = 0;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", str);
    int len = (int)strlen(buf);
    while (len > 3) {
        buf[len - 1] = '\0';
        len--;
        char test[516];
        snprintf(test, sizeof(test), "%s...", buf);
        TTF_SizeUTF8(font, test, &w, &h);
        if (w <= (int)max_w) {
            draw_text(x, y, color, font_size, test);
            return;
        }
    }
}

void c64_gui_draw_progress(const char *title, const char *subtitle, float fraction, const char *item_name)
{
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    draw_rect(0, 0, C64_GUI_SCREEN_W, C64_GUI_SCREEN_H, C64_COLOR_OVERLAY);

    float dw = 580.0f;
    float dh = 220.0f;
    float dx = ((float)C64_GUI_SCREEN_W - dw) * 0.5f;
    float dy = ((float)C64_GUI_SCREEN_H - dh) * 0.5f;

    draw_round_card(dx, dy, dw, dh, C64_COLOR_CARD, C64_COLOR_CELESTINO);

    static const unsigned int rainbow[6] = {
        C64_COLOR_RB_RED,
        C64_COLOR_RB_ORANGE,
        C64_COLOR_RB_YELLOW,
        C64_COLOR_RB_GREEN,
        C64_COLOR_RB_CYAN,
        C64_COLOR_RB_BLUE
    };
    float seg_w = (dw - 32.0f) / 6.0f;
    for (int i = 0; i < 6; i++) {
        draw_rect(dx + 16.0f + i * seg_w, dy + 6.0f, seg_w, 2, rainbow[i]);
    }

    draw_text_clipped(dx + 30.0f, dy + 22.0f, dw - 60.0f, C64_COLOR_CELESTINO, 18, title ? title : "Loading...");
    draw_text_clipped(dx + 30.0f, dy + 56.0f, dw - 60.0f, C64_COLOR_WHITE, 14, subtitle ? subtitle : "Please wait...");

    float bar_x = dx + 30.0f;
    float bar_y = dy + 95.0f;
    float bar_w = dw - 60.0f;
    float bar_h = 24.0f;

    draw_rect(bar_x, bar_y, bar_w, bar_h, C64_COLOR_BG);
    draw_rect_outline(bar_x, bar_y, bar_w, bar_h, C64_COLOR_CARD_BORDER);

    float fill_w = (bar_w - 4.0f) * fraction;
    if (fill_w > 0.0f) {
        draw_rect(bar_x + 2.0f, bar_y + 2.0f, fill_w, bar_h - 4.0f, C64_COLOR_CELESTINO);
    }

    char pct_str[32];
    snprintf(pct_str, sizeof(pct_str), "%d%%", (int)(fraction * 100.0f));
    draw_text(bar_x + bar_w - 45.0f, bar_y - 20.0f, C64_COLOR_GOLD, 13, pct_str);

    if (item_name && item_name[0]) {
        draw_text_clipped(dx + 30.0f, dy + 138.0f, dw - 60.0f, C64_COLOR_MUTED, 13, item_name);
    }

    ps3_gfx_swap_buffers();
}

static void draw_btn_icon(int id, float x, float y, float size)
{
    if (id < 0 || id >= 17) return;
    ps3_gfx_texture *tex = s_btn_icons[id];
    if (!tex) return;

    int tw = (int)ps3_gfx_texture_get_width(tex);
    int th = (int)ps3_gfx_texture_get_height(tex);
    if (tw <= 0 || th <= 0) return;

    ps3_gfx_draw_texture_scale(tex, (int)x, (int)y, size / (float)tw, size / (float)th);
}

static size_t file_size_once(const char *path)
{
    struct stat st;
    size_t size = 0;

    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        size = (size_t)st.st_size;
    }

    if (size == 0 && strncmp(path, "/dev_usb", 8) == 0) {
        SceIoStat info;
        memset(&info, 0, sizeof(info));
        if (sceIoGetstat(path, &info) == 0 && info.st_size > 0) {
            size = (size_t)info.st_size;
        }
    }

    if (size == 0) {
        FILE *fp = fopen(path, "rb");
        if (fp) {
            if (fseek(fp, 0, SEEK_END) == 0) {
                long end = ftell(fp);
                if (end > 0) size = (size_t)end;
            }
            fclose(fp);
        }
    }

    return size;
}

static size_t file_size_bytes(const char *path)
{
    return file_size_once(path);
}

static bool directory_has_visible_entries(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return false;

    bool found = false;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (ent->d_name[0] == '.') continue;
        found = true;
        break;
    }
    closedir(dir);
    return found;
}

static bool is_c64_extension(const char *ext)
{
    if (!ext) return false;
    static const char *exts[] = {
        ".d64", ".d71", ".d81", ".g64", ".prg", ".p00",
        ".t64", ".tap", ".crt", ".zip", ".gz", ".m3u", NULL
    };
    for (int i = 0; exts[i]; i++) {
        if (strcasecmp(ext, exts[i]) == 0) return true;
    }
    return false;
}

static void scan_directory(const string &dirpath)
{
    c64_gui_draw_progress("Scanning Storage", "Reading file list...", 0.3f, dirpath.c_str());

    s_file_list.clear();
    s_lib_selected = 0;
    s_lib_scroll_top = 0;
    s_current_dir = dirpath;

    DIR *dir = opendir(dirpath.c_str());
    if (!dir) return;

    if (dirpath != "/" && dirpath != "" && dirpath != ".") {
        FileItem parent;
        parent.name = "..";
        string p = dirpath;
        size_t last_slash = p.find_last_of("/\\");
        if (last_slash != string::npos && last_slash > 0) {
            parent.path = p.substr(0, last_slash);
        } else {
            parent.path = "/";
        }
        parent.is_dir = true;
        parent.size = 0;
        parent.ext = "";
        s_file_list.push_back(parent);
    }

    struct dirent *ent;
    vector<FileItem> dirs;
    vector<FileItem> files;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (ent->d_name[0] == '.') continue;

        string full_path = dirpath;
        if (!full_path.empty() && full_path[full_path.size() - 1] != '/') full_path += "/";
        full_path += ent->d_name;

        struct stat st;
        bool is_directory = false;
        size_t fsize = 0;
        if (stat(full_path.c_str(), &st) == 0) {
            is_directory = S_ISDIR(st.st_mode);
            if (!is_directory) {
                fsize = file_size_bytes(full_path.c_str());
            }
        } else {
            fsize = file_size_bytes(full_path.c_str());
        }

        FileItem item;
        item.name = ent->d_name;
        item.path = full_path;
        item.is_dir = is_directory;
        item.size = fsize;

        size_t dot_pos = item.name.find_last_of('.');
        if (dot_pos != string::npos) {
            item.ext = item.name.substr(dot_pos);
        } else {
            item.ext = "";
        }

        if (is_directory) {
            dirs.push_back(item);
        } else if (is_c64_extension(item.ext.c_str())) {
            files.push_back(item);
        }
    }
    closedir(dir);

    if (strncmp(dirpath.c_str(), "/dev_usb", 8) == 0) {
        static const int stat_retry_delay[] = { 250, 400, 600, 900 };
        for (int pass = 0; pass < 4; pass++) {
            bool has_zero = false;
            for (const auto &f : files) {
                if (f.size == 0) { has_zero = true; break; }
            }
            if (!has_zero) break;
            SDL_Delay(stat_retry_delay[pass]);
            for (auto &f : files) {
                if (f.size == 0) f.size = file_size_once(f.path.c_str());
            }
        }
    }

    auto comp = [](const FileItem &a, const FileItem &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    };
    std::sort(dirs.begin(), dirs.end(), comp);
    std::sort(files.begin(), files.end(), comp);

    for (const auto &d : dirs) s_file_list.push_back(d);
    for (const auto &f : files) s_file_list.push_back(f);
}

static void select_initial_directory(void)
{
    for (int i = 0; i < s_scan_root_count; i++) {
        if (!directory_has_visible_entries(s_scan_roots[i])) continue;

        scan_directory(s_scan_roots[i]);
        if (s_file_list.size() > 1) return;
    }

    scan_directory("data/games");
}

static void load_cover_for_game(const string &game_path)
{
    if (s_cover_texture) {
        ps3_gfx_free_texture(s_cover_texture);
        s_cover_texture = NULL;
    }
    s_cover_loaded_path = game_path;

    if (game_path.empty()) return;

    size_t last_slash = game_path.find_last_of("/\\");
    string dirname = (last_slash != string::npos) ? game_path.substr(0, last_slash) : ".";
    string filename = (last_slash != string::npos) ? game_path.substr(last_slash + 1) : game_path;
    size_t dot_pos = filename.find_last_of('.');
    string basename = (dot_pos != string::npos) ? filename.substr(0, dot_pos) : filename;

    vector<string> candidates;
    candidates.push_back(dirname + "/covers/" + basename + ".png");
    candidates.push_back(dirname + "/covers/" + basename + ".jpg");
    candidates.push_back(dirname + "/" + basename + ".png");
    candidates.push_back(dirname + "/" + basename + ".jpg");
    candidates.push_back("/dev_hdd0/game/VICEP3001/USRDIR/covers/" + basename + ".png");
    candidates.push_back("/dev_hdd0/game/VICEP3001/USRDIR/covers/" + basename + ".jpg");
    candidates.push_back("data/covers/" + basename + ".png");
    candidates.push_back("data/covers/" + basename + ".jpg");

    for (const auto &cand : candidates) {
        FILE *f = fopen(cand.c_str(), "rb");
        if (f) {
            fclose(f);
            s_cover_texture = ps3_gfx_load_image_file(cand.c_str());
            if (s_cover_texture) break;
        }
    }
}

static void draw_c64_header(void)
{
    draw_rect(0, 0, C64_GUI_SCREEN_W, C64_GUI_HEADER_H, C64_COLOR_HEADER);
    draw_rect(0, C64_GUI_HEADER_H - 2, C64_GUI_SCREEN_W, 2, C64_COLOR_CARD_BORDER);

    static const unsigned int rainbow[6] = {
        C64_COLOR_RB_RED,
        C64_COLOR_RB_ORANGE,
        C64_COLOR_RB_YELLOW,
        C64_COLOR_RB_GREEN,
        C64_COLOR_RB_CYAN,
        C64_COLOR_RB_BLUE
    };
    float seg_w = (float)C64_GUI_SCREEN_W / 6.0f;
    for (int i = 0; i < 6; i++) {
        draw_rect(i * seg_w, 0, seg_w, 3, rainbow[i]);
    }

    draw_text(24, 14, C64_COLOR_CELESTINO, 20, "VICE4PS3");
    draw_text(130, 19, C64_COLOR_MUTED, 13, "Commodore 64 HD Frontend");

    draw_btn_icon(GUI_BTN_L1, 310, 14, 20);
    draw_btn_icon(GUI_BTN_R1, 335, 14, 20);

    const float tab_start_x = 365.0f;
    const float tab_available = (float)C64_GUI_SCREEN_W - tab_start_x - 12.0f;
    float tab_gap = 6.0f;
    int tab_font_size = 13;
    float tab_widths[C64_TAB_COUNT];
    float tab_total = 0.0f;

    for (int i = 0; i < C64_TAB_COUNT; i++) {
        TTF_Font *font = get_font_for_size(tab_font_size);
        int tw = 0, th = 0;
        if (font) TTF_SizeUTF8(font, s_tab_names[i], &tw, &th);
        tab_widths[i] = (float)tw + 12.0f;
        tab_total += tab_widths[i];
    }
    tab_total += tab_gap * (C64_TAB_COUNT - 1);

    if (tab_total > tab_available && s_font_11) {
        tab_font_size = 11;
        tab_total = 0.0f;
        for (int i = 0; i < C64_TAB_COUNT; i++) {
            TTF_Font *font = get_font_for_size(tab_font_size);
            int tw = 0, th = 0;
            if (font) TTF_SizeUTF8(font, s_tab_names[i], &tw, &th);
            tab_widths[i] = (float)tw + 12.0f;
            tab_total += tab_widths[i];
        }
        tab_total += tab_gap * (C64_TAB_COUNT - 1);
    }
    if (tab_total > tab_available) {
        tab_gap = 2.0f;
    }

    float tab_x = tab_start_x;
    for (int i = 0; i < C64_TAB_COUNT; i++) {
        bool active = (i == (int)s_current_tab);
        if (active) {
            draw_round_card(tab_x, 11, tab_widths[i], 26, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
            draw_text(tab_x + 6, tab_font_size == 11 ? 18 : 17, C64_COLOR_WHITE, tab_font_size, s_tab_names[i]);
        } else {
            draw_text(tab_x + 6, tab_font_size == 11 ? 18 : 17, C64_COLOR_MUTED, tab_font_size, s_tab_names[i]);
        }
        tab_x += tab_widths[i] + tab_gap;
    }
}

static void draw_c64_footer(void)
{
    float fy = C64_GUI_SCREEN_H - C64_GUI_FOOTER_H;
    draw_rect(0, fy, C64_GUI_SCREEN_W, C64_GUI_FOOTER_H, C64_COLOR_FOOTER);
    draw_rect(0, fy, C64_GUI_SCREEN_W, 1, C64_COLOR_CARD_BORDER);

    float cur_x = 24.0f;
    auto add_pill = [&](int btn_id, const char *label) {
        draw_btn_icon(btn_id, cur_x, fy + 12, 18);
        cur_x += 22.0f;
        draw_text(cur_x, fy + 15, C64_COLOR_WHITE, 13, label);
        cur_x += (float)(strlen(label) * 7 + 16);
    };

    if (s_current_tab == C64_TAB_LIBRARY) {
        add_pill(GUI_BTN_CROSS, "Run / Open");
        add_pill(GUI_BTN_SQUARE, "Mount Drive 1 (8)");
        add_pill(GUI_BTN_TRIANGLE, "Mount Drive 2 (9)");
        add_pill(GUI_BTN_R2, "Switch USB / HDD");
        add_pill(GUI_BTN_CIRCLE, "Resume Game");
        add_pill(GUI_BTN_L2, "Keyboard");
    } else if (s_current_tab == C64_TAB_SAVESTATES) {
        add_pill(GUI_BTN_CROSS, "Load");
        add_pill(GUI_BTN_SQUARE, "Save");
        add_pill(GUI_BTN_CIRCLE, "Back / Resume");
        add_pill(GUI_BTN_L2, "Keyboard");
    } else {
        add_pill(GUI_BTN_CROSS, "Select / Toggle");
        add_pill(GUI_BTN_CIRCLE, "Back / Resume");
        add_pill(GUI_BTN_L2, "Keyboard");
    }

    draw_text_clipped(cur_x, fy + 15, C64_GUI_SCREEN_W - cur_x - 20.0f, C64_COLOR_CELESTINO, 13, s_current_dir.c_str());
}

static void render_library_tab(void)
{
    float list_x = 24.0f;
    float list_y = C64_GUI_HEADER_H + 12.0f;
    float list_w = 540.0f;
    float list_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;

    draw_round_card(list_x, list_y, list_w, list_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_rect(list_x + 12, list_y + 10, list_w - 24, 26, C64_COLOR_HEADER);
    draw_text_clipped(list_x + 18, list_y + 16, list_w - 36, C64_COLOR_CELESTINO, 13, s_current_dir.c_str());

    float item_y = list_y + 42.0f;
    float item_h = 28.0f;
    int visible_count = (int)((list_h - 48.0f) / item_h);

    if (s_file_list.empty()) {
        draw_text(list_x + 30, item_y + 20, C64_COLOR_MUTED, 14, "No Commodore 64 files found in this folder.");
        draw_text(list_x + 30, item_y + 44, C64_COLOR_MUTED, 13, "Press R2 to switch storage source or connect a USB drive.");
    } else {
        if (s_lib_selected < s_lib_scroll_top) s_lib_scroll_top = s_lib_selected;
        if (s_lib_selected >= s_lib_scroll_top + visible_count) {
            s_lib_scroll_top = s_lib_selected - visible_count + 1;
        }

        for (int i = 0; i < visible_count; i++) {
            int idx = s_lib_scroll_top + i;
            if (idx >= (int)s_file_list.size()) break;

            const FileItem &item = s_file_list[idx];
            bool focused = (idx == s_lib_selected);
            float row_y = item_y + i * item_h;

            if (focused) {
                draw_round_card(list_x + 8, row_y, list_w - 16, item_h - 2, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
            }

            if (item.is_dir) {
                draw_text(list_x + 16, row_y + 6, C64_COLOR_GOLD, 13, "[DIR]");
                draw_text_clipped(list_x + 60, row_y + 6, list_w - 80, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, item.name.c_str());
            } else {
                string badge = item.ext;
                std::transform(badge.begin(), badge.end(), badge.begin(), ::toupper);
                draw_text(list_x + 16, row_y + 6, C64_COLOR_CELESTINO, 13, badge.c_str());
                draw_text_clipped(list_x + 60, row_y + 6, list_w - 140, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, item.name.c_str());

                char sz_str[32];
                if (item.size >= 1024 * 1024) {
                    snprintf(sz_str, sizeof(sz_str), "%.1f MB", (float)item.size / (1024.0f * 1024.0f));
                } else if (item.size >= 1024) {
                    snprintf(sz_str, sizeof(sz_str), "%lu KB", (unsigned long)(item.size / 1024));
                } else {
                    snprintf(sz_str, sizeof(sz_str), "%lu B", (unsigned long)item.size);
                }
                draw_text(list_x + list_w - 75, row_y + 7, C64_COLOR_MUTED, 13, sz_str);
            }
        }
    }

    float info_x = list_x + list_w + 16.0f;
    float info_w = C64_GUI_SCREEN_W - info_x - 24.0f;
    float info_h = list_h;
    draw_round_card(info_x, list_y, info_w, info_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(info_x + 16, list_y + 14, C64_COLOR_CELESTINO, 16, "GAME INFORMATION");
    draw_rect(info_x + 16, list_y + 36, info_w - 32, 1, C64_COLOR_CARD_BORDER);

    if (!s_file_list.empty() && s_lib_selected < (int)s_file_list.size()) {
        const FileItem &item = s_file_list[s_lib_selected];

        if (s_cover_loaded_path != item.path) {
            load_cover_for_game(item.is_dir ? "" : item.path);
            s_cover_loaded_path = item.path;
        }

        float cover_box_x = info_x + 20.0f;
        float cover_box_y = list_y + 48.0f;
        float cover_box_w = info_w - 40.0f;
        float cover_box_h = 190.0f;
        draw_rect(cover_box_x, cover_box_y, cover_box_w, cover_box_h, C64_COLOR_BG);
        draw_rect_outline(cover_box_x, cover_box_y, cover_box_w, cover_box_h, C64_COLOR_CARD_BORDER);

        if (s_cover_texture) {
            int cw = (int)ps3_gfx_texture_get_width(s_cover_texture);
            int ch = (int)ps3_gfx_texture_get_height(s_cover_texture);
            float scale = (float)cover_box_h / (float)ch;
            if ((float)cw * scale > cover_box_w) scale = (float)cover_box_w / (float)cw;
            int rw = (int)(cw * scale);
            int rh = (int)(ch * scale);
            int rx = (int)(cover_box_x + (cover_box_w - rw) * 0.5f);
            int ry = (int)(cover_box_y + (cover_box_h - rh) * 0.5f);
            ps3_gfx_draw_texture(s_cover_texture, rx, ry);
        } else {
            draw_text(cover_box_x + 60, cover_box_y + 85, C64_COLOR_MUTED, 14, "NO COVER ART");
        }

        float detail_y = cover_box_y + cover_box_h + 16.0f;
        draw_text(info_x + 16, detail_y, C64_COLOR_MUTED, 13, "TITLE:");
        draw_text_clipped(info_x + 16, detail_y + 16, info_w - 32, C64_COLOR_WHITE, 14, item.name.c_str());

        detail_y += 38.0f;
        draw_text(info_x + 16, detail_y, C64_COLOR_MUTED, 13, "TYPE / EXTENSION:");
        draw_text(info_x + 16, detail_y + 16, C64_COLOR_CELESTINO, 14, item.is_dir ? "DIRECTORY" : item.ext.c_str());

        detail_y += 38.0f;
        draw_text(info_x + 16, detail_y, C64_COLOR_MUTED, 13, "PATH:");
        draw_text_clipped(info_x + 16, detail_y + 16, info_w - 32, C64_COLOR_MUTED, 13, item.path.c_str());
    }
}

static void render_drives_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "FLOPPY DRIVE CONFIGURATION");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 36, card_y + 62, C64_COLOR_MUTED, 13,
              "Commodore IEC bus uses device numbers 8-11; Drive 1 is C64 unit 8.");

    float drive_y = card_y + 92.0f;
    for (int i = 0; i < 4; i++) {
        char label[64];
        char drive_key[32];
        int drive_type = 0;
        snprintf(label, sizeof(label), "Drive %d  (C64 unit %d)", i + 1, 8 + i);
        snprintf(drive_key, sizeof(drive_key), "Drive%dType", 8 + i);
        resources_get_int(drive_key, &drive_type);

        const char *disk_name = file_system_get_disk_name(8 + i);
        const char *status;
        if (!drive_type) {
            status = "Disabled (PS3 default)";
        } else if (disk_name && *disk_name) {
            status = disk_name;
        } else {
            status = "Active - No disk";
        }

        draw_text(card_x + 36, drive_y + i * 30.0f, C64_COLOR_CELESTE_LIGHT, 14, label);
        draw_text_clipped(card_x + card_w - 360, drive_y + i * 30.0f, 340,
                          C64_COLOR_CELESTINO, 14, status);
    }

    draw_rect(card_x + 24, card_y + 220, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    static const char *options[] = {
        "True Drive Emulation (TDE)",
        "Drive Sound Emulation"
    };
    int count = 2;
    float item_y = card_y + 238.0f;
    float item_h = 40.0f;

    int tde = 0;
    resources_get_int("DriveTrueEmulation", &tde);
    int sound_emu = 0;
    resources_get_int("DriveSoundEmulation", &sound_emu);

    for (int i = 0; i < count; i++) {
        bool focused = (i == s_drives_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        draw_text(card_x + 36, row_y + 8, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, options[i]);

        const char *status = (i == 0)
            ? (tde ? "Enabled (Cycle-Exact)" : "Disabled (Fast)")
            : (sound_emu ? "Enabled (Drive Mechanics Audio)" : "Disabled");
        draw_text_clipped(card_x + card_w - 320, row_y + 8, 300, C64_COLOR_CELESTINO, 14, status);
    }
}

static void render_tapecart_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "DATASETTE TAPE & CARTRIDGE MANAGEMENT");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    const char *tape_name = tape_get_file_name();
    if (!tape_name || !*tape_name) tape_name = "Empty";

    const char *cart_name = cartridge_get_file_name(cart_getid_slotmain());
    if (!cart_name || !*cart_name) cart_name = "Empty";

    draw_text(card_x + 36, card_y + 64, C64_COLOR_MUTED, 13, "Tape image:");
    draw_text_clipped(card_x + 140, card_y + 64, card_w - 176, C64_COLOR_CELESTE_LIGHT, 13, tape_name);
    draw_text(card_x + 36, card_y + 86, C64_COLOR_MUTED, 13, "Cartridge image:");
    draw_text_clipped(card_x + 170, card_y + 86, card_w - 206, C64_COLOR_CELESTE_LIGHT, 13, cart_name);

    static const char *options[] = {
        "Reset Tape Counter",
        "Datasette Reset with CPU",
        "Detach Cartridge",
        "Cartridge Reset Mode"
    };
    int count = 4;
    float item_y = card_y + 120.0f;
    float item_h = 40.0f;

    int datasette_reset = 1;
    resources_get_int("DatasetteResetWithCPU", &datasette_reset);
    int cart_reset = 1;
    resources_get_int("CartridgeReset", &cart_reset);

    for (int i = 0; i < count; i++) {
        bool focused = (i == s_tapecart_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        draw_text(card_x + 36, row_y + 8, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, options[i]);

        const char *val = "";
        if (i == 0) val = "Press Cross to Zero Counter";
        else if (i == 1) val = datasette_reset ? "Enabled" : "Disabled";
        else if (i == 2) val = "Press Cross to Eject";
        else if (i == 3) val = cart_reset ? "Enabled (Reset on Detach)" : "Disabled (No Reset)";

        draw_text_clipped(card_x + card_w - 320, row_y + 8, 300, C64_COLOR_CELESTINO, 14, val);
    }
}

static void render_display_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "DISPLAY & RESOLUTION SETTINGS");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    static const char *options[] = {
        "Aspect Ratio",
        "Video Standard",
        "VIC-II Border Mode",
        "Scaling Filter Quality",
        "Color Palette"
    };
    int count = 5;
    float item_y = card_y + 60.0f;
    float item_h = 40.0f;

    for (int i = 0; i < count; i++) {
        bool focused = (i == s_display_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        draw_text(card_x + 36, row_y + 8, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, options[i]);

        const char *val = "";
        if (i == 0) val = gui_aspect_label();
        else if (i == 1) val = gui_video_standard_label();
        else if (i == 2) val = gui_border_label();
        else if (i == 3) val = gui_filter_label();
        else if (i == 4) val = gui_color_palette_label();

        draw_text_clipped(card_x + card_w - 360, row_y + 8, 340, C64_COLOR_CELESTINO, 14, val);
    }
}

static void render_audio_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "SID SOUND & AUDIO SETTINGS");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    static const char *options[] = {
        "SID Chip Model",
        "SID Emulation Engine",
        "SID Analog Filters"
    };
    int count = 3;
    float item_y = card_y + 60.0f;
    float item_h = 40.0f;

    const char *sid_model = gui_setting_value(SID_MODEL);

    for (int i = 0; i < count; i++) {
        bool focused = (i == s_audio_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        draw_text(card_x + 36, row_y + 8, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, options[i]);

        const char *val = "";
        if (i == 0) val = strcmp(sid_model, "8580") == 0 ? "8580 (New C64C / Crisp)" : "6581 (Old C64 / Warm & Filtered)";
        else if (i == 1) val = gui_sid_engine_label();
        else if (i == 2) val = gui_sid_filter_label();

        draw_text_clipped(card_x + card_w - 340, row_y + 8, 320, C64_COLOR_CELESTINO, 14, val);
    }

    draw_text(card_x + 36, item_y + 3 * item_h + 10, C64_COLOR_MUTED, 13,
              "Audio backend: 48000 Hz, 16-bit stereo, 200 ms buffer");
}

static void render_controls_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "CONTROLLER & JOYSTICK CONFIGURATION");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    static const char *options[] = {
        "Joystick Port",
        "Analog Stick Side",
        "Swap Joystick Ports",
        "Auto-Fire Mode",
        "Auto-Fire Speed"
    };
    int count = 5;
    float item_y = card_y + 60.0f;
    float item_h = 40.0f;

    for (int i = 0; i < count; i++) {
        bool focused = (i == s_controls_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        draw_text(card_x + 36, row_y + 8, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, options[i]);

        const char *val = "";
        if (i == 0) val = gui_joystick_port_label();
        else if (i == 1) val = gui_joystick_side_label();
        else if (i == 2) val = "Press Cross to Swap Ports";
        else if (i == 3) val = gui_autofire_label();
        else if (i == 4) val = gui_autofire_speed_label();

        draw_text_clipped(card_x + card_w - 340, row_y + 8, 320, C64_COLOR_CELESTINO, 14, val);
    }
}

static void render_savestates_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "SAVESTATES & SNAPSHOTS");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    float item_y = card_y + 60.0f;
    float item_h = 32.0f;

    for (int i = 0; i < 10; i++) {
        bool focused = (i == s_savestates_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        char slot_label[64];
        snprintf(slot_label, sizeof(slot_label), "Savestate Slot #%d", i);
        draw_text(card_x + 36, row_y + 6, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, slot_label);

        char slot_path[256];
        snprintf(slot_path, sizeof(slot_path), "data/saves/slot%d.vsf", i);
        struct stat st;
        bool exists = (stat(slot_path, &st) == 0);
        const char *status = exists ? "Occupied (Press Cross to Load, Square to Overwrite)" : "Empty";
        draw_text_clipped(card_x + card_w - 420, row_y + 6, 400, exists ? C64_COLOR_GOLD : C64_COLOR_MUTED, 13, status);
    }
}

static void render_system_tab(void)
{
    float card_x = 24.0f;
    float card_y = C64_GUI_HEADER_H + 12.0f;
    float card_w = C64_GUI_SCREEN_W - 48.0f;
    float card_h = C64_GUI_SCREEN_H - C64_GUI_HEADER_H - C64_GUI_FOOTER_H - 24.0f;
    draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

    draw_text(card_x + 24, card_y + 18, C64_COLOR_CELESTINO, 18, "SYSTEM, RESET & CREDITS");
    draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

    static const char *options[] = {
        "Soft Reset (CPU Reset)",
        "Hard Reset (Cold Power Cycle)",
        "Autostart Attached Image",
        "Warp Mode (Fast Forward)",
        "About & Scrolling Credits",
        "Exit VICE4PS3 to XMB"
    };
    int count = 6;
    float item_y = card_y + 60.0f;
    float item_h = 36.0f;

    int warp = 0;
    resources_get_int("WarpMode", &warp);

    for (int i = 0; i < count; i++) {
        bool focused = (i == s_system_selected);
        float row_y = item_y + i * item_h;
        if (focused) {
            draw_round_card(card_x + 20, row_y, card_w - 40, item_h - 4, C64_COLOR_CARD_FOCUSED, C64_COLOR_CELESTINO);
        }
        draw_text(card_x + 36, row_y + 8, focused ? C64_COLOR_WHITE : C64_COLOR_CELESTE_LIGHT, 14, options[i]);

        const char *val = "";
        if (i == 0) val = "Trigger 6510 Soft Reset";
        else if (i == 1) val = "Full C64 Cold Reboot";
        else if (i == 2) val = "Autostart Boot Disk / Tape / Cart";
        else if (i == 3) val = warp ? "ON (Unlimited Speed)" : "OFF (100% 50/60 Hz)";
        else if (i == 4) val = "View UAE4all2HD-Style Credits";
        else if (i == 5) val = "Return to PlayStation 3 XMB";

        draw_text_clipped(card_x + card_w - 340, row_y + 8, 320, C64_COLOR_CELESTINO, 14, val);
    }
}

void c64_show_credits(void)
{
    static const CreditLine credits[] = {
        { "VICE4PS3", CR_TITLE },
        { "Commodore 64 High Definition Emulator for PlayStation 3", CR_SUBTITLE },
        { "Version 1.00 - Full HD Edition", CR_DIM },
        { "", CR_EMPTY },
        { "Presented with the UAE4all2HD UI Architecture", CR_TEXT },
        { "Crafted in authentic Commodore 64 Classic Blue & Celestino", CR_TEXT },
        { "Equipped with On-Screen C64 Keyboard & Game Library", CR_TEXT },
        { "", CR_EMPTY },

        { "=====  Commodore 64 Pioneers  =====", CR_SECTION },
        { "In memory of the visionaries who created the greatest 8-bit computer:", CR_DIM },
        { "Jack Tramiel - Founder of Commodore Business Machines", CR_RED },
        { "Chuck Peddle - Designer of the legendary MOS 6502 CPU", CR_RED },
        { "Bob Yannes - Designer of the iconic MOS 6581 SID Chip", CR_RED },
        { "Al Charpentier - Designer of the MOS 6567/6569 VIC-II Chip", CR_RED },
        { "And to every Commodore 64 enthusiast across the world.", CR_DIM },
        { "", CR_EMPTY },

        { "=====  UAE4ALL2 HD PS3 Tribute  =====", CR_SECTION },
        { "Frontend design, layout, font styling, and scrolling credits", CR_TEXT },
        { "faithfully adapted from UAE4ALL2 HD PS3 by theheroGAC.", CR_TEXT },
        { "Pemu button glyphs courtesy of PFBNeo / pemu project.", CR_DIM },
        { "Fira Sans typography under SIL Open Font License.", CR_DIM },
        { "", CR_EMPTY },

        { "=====  The VICE Development Team  =====", CR_SECTION },
        { "Dag Lem, Andreas Boose, Tibor Biczo, Ettore Perazzoli,", CR_TEXT },
        { "Teemu Rantanen, Jouko Valjus, Jarkko Sonninen, Andre Fachat,", CR_TEXT },
        { "David Hansel, Daniel Sladic, Markus Brenner, Spiro Trikaliotis,", CR_TEXT },
        { "Christian Vogelgsang, Fabrizio Gennari, M. Kiesel, Marco van den Heuvel,", CR_TEXT },
        { "Bas Wassink, Michael C. Martin, David Hogan, and the entire team.", CR_TEXT },
        { "", CR_EMPTY },

        { "=====  PS3 PSL1GHT SDK & Homebrew  =====", CR_SECTION },
        { "Ported using modern GCC 13.2 cross-toolchain and PSL1GHT v2.", CR_TEXT },
        { "Thanks to the devkitPro team, Hermes, PSL1GHT authors,", CR_TEXT },
        { "and the passionate PlayStation 3 scene preservationists.", CR_DIM },
        { "", CR_EMPTY },

        { "READY.", CR_RED },
        { "LOAD\"*\",8,1", CR_TEXT },
        { "RUN", CR_SUBTITLE },
        { "", CR_EMPTY },
        { "Thank you for playing!", CR_RED }
    };

    const int total_lines = (int)(sizeof(credits) / sizeof(credits[0]));
    const float line_h = 28.0f;
    const float content_h = (float)total_lines * line_h;

    float scroll = 0.0f;
    float speed = 1.0f;
    bool paused = false;
    int frame_count = 0;
    unsigned int prev_btn = 0;

    while (1) {
        SceCtrlData ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        sceCtrlPeekBufferPositive(0, &ctrl, 1);
        unsigned int pressed = ctrl.buttons & (~prev_btn);
        prev_btn = ctrl.buttons;
        frame_count++;

        if (frame_count > 10 && (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_START))) {
            break;
        }

        if (pressed & SCE_CTRL_SQUARE) {
            paused = !paused;
        }

        if (pressed & SCE_CTRL_UP) speed -= 0.5f;
        if (pressed & SCE_CTRL_DOWN) speed += 0.5f;

        if (ctrl.buttons & SCE_CTRL_UP) speed -= 0.1f;
        if (ctrl.buttons & SCE_CTRL_DOWN) speed += 0.1f;

        if (speed < -4.0f) speed = -4.0f;
        if (speed > 6.0f) speed = 6.0f;

        if (!paused && speed != 0.0f) {
            scroll += speed;
            if (scroll >= content_h) scroll -= content_h;
            if (scroll < 0.0f) scroll += content_h;
        }

        draw_rect(0, 0, C64_GUI_SCREEN_W, C64_GUI_SCREEN_H, C64_COLOR_BG);

        float card_x = 24.0f;
        float card_y = 16.0f;
        float card_w = C64_GUI_SCREEN_W - 48.0f;
        float card_h = C64_GUI_SCREEN_H - 32.0f;
        draw_round_card(card_x, card_y, card_w, card_h, C64_COLOR_CARD, C64_COLOR_CARD_BORDER);

        draw_text(card_x + 24, card_y + 16, C64_COLOR_CELESTINO, 20, "VICE4PS3");
        draw_text(card_x + 130, card_y + 21, C64_COLOR_MUTED, 13, "About & Scrolling Credits");

        draw_rect(card_x + 24, card_y + 44, card_w - 48, 1, C64_COLOR_CARD_BORDER);

        static const unsigned int rainbow[6] = {
            C64_COLOR_RB_RED,
            C64_COLOR_RB_ORANGE,
            C64_COLOR_RB_YELLOW,
            C64_COLOR_RB_GREEN,
            C64_COLOR_RB_CYAN,
            C64_COLOR_RB_BLUE
        };
        float seg_w = (card_w - 48.0f) / 6.0f;
        for (int i = 0; i < 6; i++) {
            draw_rect(card_x + 24 + i * seg_w, card_y + 46, seg_w, 2, rainbow[i]);
        }

        float area_top = card_y + 60.0f;
        float area_bottom = card_y + card_h - 40.0f;

        for (int i = 0; i < total_lines; i++) {
            float y = area_top + (float)i * line_h - scroll;
            if (y < area_top - line_h || y > area_bottom) continue;

            const CreditLine *line = &credits[i];
            if (line->style == CR_EMPTY) continue;

            int fsize = 14;
            unsigned int col = C64_COLOR_WHITE;

            switch (line->style) {
                case CR_TITLE:    fsize = 20; col = C64_COLOR_CELESTINO; break;
                case CR_SUBTITLE: fsize = 16; col = C64_COLOR_CELESTE_LIGHT; break;
                case CR_SECTION:  fsize = 16; col = C64_COLOR_GOLD; break;
                case CR_LINK:     fsize = 13; col = C64_COLOR_CELESTINO; break;
                case CR_DIM:      fsize = 13; col = C64_COLOR_MUTED; break;
                case CR_RED:      fsize = 14; col = C64_COLOR_RB_RED; break;
                default:          fsize = 14; col = C64_COLOR_WHITE; break;
            }

            int w = 0, h = 0;
            TTF_Font *font = get_font_for_size(fsize);
            if (font) TTF_SizeUTF8(font, line->text, &w, &h);
            float cx = (float)C64_GUI_SCREEN_W * 0.5f;
            float x = cx - (float)w * 0.5f;
            if (x < card_x + 20) x = card_x + 20;
            draw_text_clipped(x, y, card_w - 40, col, fsize, line->text);
        }

        float foot_y = card_y + card_h - 30.0f;
        draw_rect(card_x + 24, foot_y - 8, card_w - 48, 1, C64_COLOR_CARD_BORDER);
        draw_btn_icon(9, card_x + 30, foot_y - 2, 16);
        draw_text(card_x + 50, foot_y, C64_COLOR_WHITE, 13, "Exit");
        draw_btn_icon(8, card_x + 95, foot_y - 2, 16);
        draw_text(card_x + 115, foot_y, C64_COLOR_WHITE, 13, paused ? "Resume" : "Pause");
        draw_btn_icon(3, card_x + 180, foot_y - 2, 16);
        draw_btn_icon(1, card_x + 200, foot_y - 2, 16);
        draw_text(card_x + 220, foot_y, C64_COLOR_WHITE, 13, "Adjust Speed");

        ps3_gfx_swap_buffers();
        SDL_PumpEvents();
    }
}

static void poll_gui_input(void)
{
    SceCtrlData ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    sceCtrlPeekBufferPositive(0, &ctrl, 1);

    unsigned int pressed = ctrl.buttons & (~s_prev_pad_buttons);
    unsigned int released = s_prev_pad_buttons & (~ctrl.buttons);
    s_prev_pad_buttons = ctrl.buttons;

    if (c64_vkbd_is_visible()) {
        c64_vkbd_process_input(pressed, ctrl.buttons, released);
        return;
    }

    if (pressed & SCE_CTRL_L1) {
        s_current_tab = (C64GuiTab)((s_current_tab + C64_TAB_COUNT - 1) % C64_TAB_COUNT);
    }
    if (pressed & SCE_CTRL_R1) {
        s_current_tab = (C64GuiTab)((s_current_tab + 1) % C64_TAB_COUNT);
    }
    if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_SELECT)) {
        c64_gui_request_close();
        return;
    }
    if (pressed & (SCE_CTRL_LTRIGGER | SCE_CTRL_START)) {
        c64_vkbd_toggle();
        return;
    }

    if (s_current_tab == C64_TAB_LIBRARY) {
        if (!s_file_list.empty()) {
            static int s_lib_nav_delay = 7;
            static int s_lib_nav_holds = 0;
            unsigned int lib_dirs = ctrl.buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN);
            unsigned int lib_fresh = pressed & (SCE_CTRL_UP | SCE_CTRL_DOWN);

            if (!lib_dirs) {
                s_lib_nav_delay = 7;
                s_lib_nav_holds = 0;
            } else if (lib_fresh) {
                if ((lib_fresh & SCE_CTRL_UP) && s_lib_selected > 0) {
                    s_lib_selected--;
                }
                if ((lib_fresh & SCE_CTRL_DOWN) && s_lib_selected + 1 < (int)s_file_list.size()) {
                    s_lib_selected++;
                }
                s_lib_nav_delay = 7;
                s_lib_nav_holds = 0;
            } else {
                s_lib_nav_holds++;
                if (--s_lib_nav_delay <= 0) {
                    if ((lib_dirs & SCE_CTRL_UP) && s_lib_selected > 0) {
                        s_lib_selected--;
                    }
                    if ((lib_dirs & SCE_CTRL_DOWN) && s_lib_selected + 1 < (int)s_file_list.size()) {
                        s_lib_selected++;
                    }
                    if (s_lib_nav_holds > 30) {
                        s_lib_nav_delay = 1;
                    } else if (s_lib_nav_holds > 10) {
                        s_lib_nav_delay = 2;
                    } else {
                        s_lib_nav_delay = 4;
                    }
                }
            }
            if (pressed & SCE_CTRL_CROSS) {
                const FileItem &item = s_file_list[s_lib_selected];
                if (item.is_dir) {
                    scan_directory(item.path);
                } else {
                    c64_gui_draw_progress("Starting Commodore 64 Game", "Preparing image and resetting CPU...", 0.65f, item.name.c_str());
                    resources_set_int("AutostartPrgMode", 1);
                    resources_set_int("AutostartHandleTrueDriveEmulation", 1);
                    c64_gui_draw_progress("Starting Commodore 64 Game", "Mounting image and resetting CPU...", 0.85f, item.name.c_str());
                    int ret = PS3_LoadGameFile(item.path.c_str());
                    if (ret < 0) {
                        ps3_gfx_log_write("gui: autostart failed");
                        c64_gui_draw_progress("Could Not Start Image", "The selected C64 image could not be mounted.", 0.0f, item.name.c_str());
                        SDL_Delay(1200);
                        return;
                    }
                    c64_gui_request_close();
                }
            }
            if (pressed & SCE_CTRL_SQUARE) {
                const FileItem &item = s_file_list[s_lib_selected];
                if (!item.is_dir) {
                    c64_gui_draw_progress("Mounting Floppy Disk", "Preparing image for Drive 1 (unit 8)...", 0.8f, item.name.c_str());
                    const char *image_path = Extractor::getInst()->extract(item.path.c_str(), 8);
                    if (image_path) {
                        c64_gui_draw_progress("Mounting Floppy Disk", "Attaching to Drive 1 (unit 8)...", 1.0f, item.name.c_str());
                        file_system_attach_disk(8, image_path);
                    }
                }
            }
            if (pressed & SCE_CTRL_TRIANGLE) {
                const FileItem &item = s_file_list[s_lib_selected];
                if (!item.is_dir) {
                    c64_gui_draw_progress("Mounting Floppy Disk", "Preparing image for Drive 2 (unit 9)...", 0.8f, item.name.c_str());
                    const char *image_path = Extractor::getInst()->extract(item.path.c_str(), 9);
                    if (image_path) {
                        c64_gui_draw_progress("Mounting Floppy Disk", "Enabling Drive 2 (unit 9)...", 0.9f, item.name.c_str());
                        resources_set_int("Drive9Type", 1542);
                        c64_gui_draw_progress("Mounting Floppy Disk", "Attaching to Drive 2 (unit 9)...", 1.0f, item.name.c_str());
                        if (file_system_attach_disk(9, image_path) < 0) {
                            c64_gui_draw_progress("Could Not Mount Drive 2", "The image could not be attached to C64 unit 9.", 0.0f, item.name.c_str());
                            SDL_Delay(1200);
                        }
                    }
                }
            }
        }
        if (pressed & SCE_CTRL_RTRIGGER) {
            static int root_idx = 0;
            root_idx = (root_idx + 1) % s_scan_root_count;
            scan_directory(s_scan_roots[root_idx]);
        }
    } else if (s_current_tab == C64_TAB_DRIVES) {
        if (pressed & SCE_CTRL_UP) {
            if (s_drives_selected > 0) s_drives_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_drives_selected < 1) s_drives_selected++;
        }
        if (pressed & SCE_CTRL_CROSS) {
            if (s_drives_selected == 0) {
                int tde = 0;
                resources_get_int("DriveTrueEmulation", &tde);
                resources_set_int("DriveTrueEmulation", tde ? 0 : 1);
            } else if (s_drives_selected == 1) {
                int sound_emu = 0;
                resources_get_int("DriveSoundEmulation", &sound_emu);
                resources_set_int("DriveSoundEmulation", sound_emu ? 0 : 1);
            }
        }
    } else if (s_current_tab == C64_TAB_TAPECART) {
        if (pressed & SCE_CTRL_UP) {
            if (s_tapecart_selected > 0) s_tapecart_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_tapecart_selected < 3) s_tapecart_selected++;
        }
        if (pressed & SCE_CTRL_CROSS) {
            if (s_tapecart_selected == 0) {
                datasette_reset_counter();
            } else if (s_tapecart_selected == 1) {
                int datasette_reset = 1;
                resources_get_int("DatasetteResetWithCPU", &datasette_reset);
                resources_set_int("DatasetteResetWithCPU", datasette_reset ? 0 : 1);
            } else if (s_tapecart_selected == 2) {
                cartridge_detach_image(-1);
            } else if (s_tapecart_selected == 3) {
                int cart_reset = 1;
                resources_get_int("CartridgeReset", &cart_reset);
                resources_set_int("CartridgeReset", cart_reset ? 0 : 1);
            }
        }
    } else if (s_current_tab == C64_TAB_DISPLAY) {
        if (pressed & SCE_CTRL_UP) {
            if (s_display_selected > 0) s_display_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_display_selected < 4) s_display_selected++;
        }
        if (pressed & SCE_CTRL_CROSS) {
            static const char *aspect_values[] = {"4:3", "16:9", "4:3 max"};
            static const char *standard_values[] = {"PAL", "NTSC", "Old NTSC", "PAL-N"};
            static const char *border_values[] = {"Show", "Hide"};
            static const char *filter_values[] = {"Point", "Linear"};
            static const char *palette_values[] = {"Pepto (PAL)", "Colodore", "Vice", "Ptoing", "RGB", "None"};

            switch (s_display_selected) {
            case 0:
                gui_cycle_setting(ASPECT_RATIO, aspect_values, 3);
                break;
            case 1:
                gui_cycle_setting(VICII_MODEL, standard_values, 4);
                break;
            case 2:
                gui_cycle_setting(BORDERS, border_values, 2);
                break;
            case 3:
                gui_cycle_setting(TEXTURE_FILTER, filter_values, 2);
                break;
            case 4:
                gui_cycle_setting(COLOR_PALETTE, palette_values, 6);
                break;
            default:
                break;
            }
        }
    } else if (s_current_tab == C64_TAB_AUDIO) {
        if (pressed & SCE_CTRL_UP) {
            if (s_audio_selected > 0) s_audio_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_audio_selected < 2) s_audio_selected++;
        }
        if (pressed & SCE_CTRL_CROSS) {
            static const char *sid_models[] = {"6581", "8580"};
            static const char *sid_engines[] = {"ReSID", "ReSID-FP", "FastSID"};
            static const char *filter_values[] = {"Enabled", "Disabled"};

            if (s_audio_selected == 0) {
                gui_cycle_setting(SID_MODEL, sid_models, 2);
            } else if (s_audio_selected == 1) {
                gui_cycle_setting(SID_ENGINE, sid_engines, 3);
            } else if (s_audio_selected == 2) {
                gui_cycle_setting(SID_FILTERS, filter_values, 2);
            }
        }
    } else if (s_current_tab == C64_TAB_CONTROLS) {
        if (pressed & SCE_CTRL_UP) {
            if (s_controls_selected > 0) s_controls_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_controls_selected < 4) s_controls_selected++;
        }
        if (pressed & SCE_CTRL_CROSS) {
            static const char *port_values[] = {"Port 1", "Port 2"};
            static const char *side_values[] = {"Left", "Right"};
            static const char *speed_values[] = {"Slow", "Medium", "Fast"};

            switch (s_controls_selected) {
            case 0:
                gui_cycle_setting(JOYSTICK_PORT, port_values, 2);
                break;
            case 1:
                gui_cycle_setting(JOYSTICK_SIDE, side_values, 2);
                break;
            case 2: {
                const char *port = gui_setting_value(JOYSTICK_PORT);
                PS3_ApplyGuiSetting(JOYSTICK_PORT, strcmp(port, "Port 1") == 0 ? "Port 2" : "Port 1");
                break;
            }
            case 3:
                PS3_SetAutofire(!PS3_GetAutofire());
                break;
            case 4:
                gui_cycle_setting(JOYSTICK_AUTOFIRE_SPEED, speed_values, 3);
                break;
            default:
                break;
            }
        }
    } else if (s_current_tab == C64_TAB_SAVESTATES) {
        if (pressed & SCE_CTRL_UP) {
            if (s_savestates_selected > 0) s_savestates_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_savestates_selected < 9) s_savestates_selected++;
        }
        if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_SQUARE)) {
            char slot_path[256];
            snprintf(slot_path, sizeof(slot_path), "data/saves/slot%d.vsf", s_savestates_selected);

            if (pressed & SCE_CTRL_CROSS) {
                struct stat st;
                if (stat(slot_path, &st) == 0) {
                    c64_gui_draw_progress("Loading Savestate", "Restoring machine state...", 0.8f, slot_path);
                    if (PS3_LoadStateFile(slot_path) >= 0) {
                        c64_gui_request_close();
                        return;
                    }
                    c64_gui_draw_progress("Could Not Load Savestate", "The snapshot could not be restored.", 0.0f, slot_path);
                    SDL_Delay(1000);
                }
            }

            if (pressed & SCE_CTRL_SQUARE) {
                mkdir("data/saves", 0777);
                c64_gui_draw_progress("Saving Savestate", "Writing machine snapshot...", 0.8f, slot_path);
                if (PS3_SaveStateFile(slot_path) >= 0) {
                    c64_gui_draw_progress("Savestate Saved", "Snapshot written to the selected slot.", 1.0f, slot_path);
                    SDL_Delay(500);
                } else {
                    c64_gui_draw_progress("Could Not Save Savestate", "Check free space in data/saves.", 0.0f, slot_path);
                    SDL_Delay(1000);
                }
            }
        }
    } else if (s_current_tab == C64_TAB_SYSTEM) {
        if (pressed & SCE_CTRL_UP) {
            if (s_system_selected > 0) s_system_selected--;
        }
        if (pressed & SCE_CTRL_DOWN) {
            if (s_system_selected < 5) s_system_selected++;
        }
        if (pressed & SCE_CTRL_CROSS) {
            if (s_system_selected == 0) {
                c64_gui_draw_progress("Resetting Commodore 64", "Sending soft reset signal to 6510 CPU...", 0.85f, "Reinitializing CPU registers");
                if (ui_emulation_is_paused()) {
                    ui_pause_emulation(0);
                }
                machine_trigger_reset(MACHINE_RESET_MODE_SOFT);
                c64_gui_request_close();
            } else if (s_system_selected == 1) {
                c64_gui_draw_progress("Resetting Commodore 64", "Full cold reboot and memory clear...", 0.95f, "Cold start C64 kernel");
                if (ui_emulation_is_paused()) {
                    ui_pause_emulation(0);
                }
                machine_trigger_reset(MACHINE_RESET_MODE_HARD);
                c64_gui_request_close();
            } else if (s_system_selected == 2) {
                const char *cur_disk = file_system_get_disk_name(8);
                if (cur_disk && *cur_disk) {
                    c64_gui_draw_progress("Autostarting Disk", "Mounting boot image...", 0.8f, cur_disk);
                    autostart_autodetect(cur_disk, NULL, 0, AUTOSTART_MODE_RUN);
                    c64_gui_request_close();
                }
            } else if (s_system_selected == 3) {
                int w = 0;
                resources_get_int("WarpMode", &w);
                resources_set_int("WarpMode", w ? 0 : 1);
            } else if (s_system_selected == 4) {
                c64_show_credits();
            } else if (s_system_selected == 5) {
                c64_gui_draw_progress("Exiting VICE4PS3", "Returning to PlayStation 3 XMB...", 1.0f, "");
                PS3_ShutdownAndExit();
            }
        }
    }
}

void c64_gui_init(void)
{
    static const char *font_paths[] = {
        "data/font.ttf",
        "resources/data/font.ttf",
        "/dev_hdd0/game/VICEP3001/USRDIR/data/font.ttf",
        "/dev_hdd0/game/VICEP3001/USRDIR/resources/data/font.ttf",
        NULL
    };
    for (int i = 0; font_paths[i]; i++) {
        FILE *f = fopen(font_paths[i], "rb");
        if (f) {
            fclose(f);
            s_font_11 = TTF_OpenFont(font_paths[i], 11);
            s_font_13 = TTF_OpenFont(font_paths[i], 13);
            s_font_14 = TTF_OpenFont(font_paths[i], 14);
            s_font_16 = TTF_OpenFont(font_paths[i], 16);
            s_font_20 = TTF_OpenFont(font_paths[i], 20);
            s_font_26 = TTF_OpenFont(font_paths[i], 26);
            break;
        }
    }

    for (int i = 0; i < 17; i++) {
        char path[256];
        snprintf(path, sizeof(path), "data/buttons/%d.png", i);
        FILE *f = fopen(path, "rb");
        if (!f) {
            snprintf(path, sizeof(path), "resources/data/buttons/%d.png", i);
            f = fopen(path, "rb");
        }
        if (!f) {
            snprintf(path, sizeof(path), "/dev_hdd0/game/VICEP3001/USRDIR/data/buttons/%d.png", i);
            f = fopen(path, "rb");
        }
        if (f) {
            fclose(f);
            s_btn_icons[i] = ps3_gfx_load_image_file(path);
        }
    }

    select_initial_directory();
}

void c64_gui_shutdown(void)
{
    if (s_cover_texture) {
        ps3_gfx_free_texture(s_cover_texture);
        s_cover_texture = NULL;
    }
    for (int i = 0; i < 17; i++) {
        if (s_btn_icons[i]) {
            ps3_gfx_free_texture(s_btn_icons[i]);
            s_btn_icons[i] = NULL;
        }
    }
    if (s_font_11) { TTF_CloseFont(s_font_11); s_font_11 = NULL; }
    if (s_font_13) { TTF_CloseFont(s_font_13); s_font_13 = NULL; }
    if (s_font_14) { TTF_CloseFont(s_font_14); s_font_14 = NULL; }
    if (s_font_16) { TTF_CloseFont(s_font_16); s_font_16 = NULL; }
    if (s_font_20) { TTF_CloseFont(s_font_20); s_font_20 = NULL; }
    if (s_font_26) { TTF_CloseFont(s_font_26); s_font_26 = NULL; }
}

void c64_gui_show(void)
{
    static bool first_frame = true;
    static int gui_iters_logged = 0;
    char itbuf[32];

    s_gui_active = true;
    ps3_gfx_log_write("gui: show");

    while (s_gui_active) {
        if (gui_iters_logged < 3) {
            gui_iters_logged++;
            snprintf(itbuf, sizeof(itbuf), "gui: iter %d", gui_iters_logged);
            ps3_gfx_log_write(itbuf);
        }
        poll_gui_input();
        if (!s_gui_active) break;

        draw_rect(0, 0, C64_GUI_SCREEN_W, C64_GUI_SCREEN_H, C64_COLOR_BG);

        draw_c64_header();

        switch (s_current_tab) {
            case C64_TAB_LIBRARY:    render_library_tab(); break;
            case C64_TAB_DRIVES:     render_drives_tab(); break;
            case C64_TAB_TAPECART:   render_tapecart_tab(); break;
            case C64_TAB_DISPLAY:    render_display_tab(); break;
            case C64_TAB_AUDIO:      render_audio_tab(); break;
            case C64_TAB_CONTROLS:   render_controls_tab(); break;
            case C64_TAB_SAVESTATES: render_savestates_tab(); break;
            case C64_TAB_SYSTEM:     render_system_tab(); break;
            default: break;
        }

        draw_c64_footer();

        if (c64_vkbd_is_visible()) {
            c64_vkbd_render();
        }

        ps3_gfx_swap_buffers();
        if (first_frame) {
            ps3_gfx_log_write("gui: first frame");
            first_frame = false;
        }
        SDL_PumpEvents();
        sceKernelDelayThread(16000);
    }

    for (int wait_rel = 0; wait_rel < 30; wait_rel++) {
        SceCtrlData ctrl_rel;
        memset(&ctrl_rel, 0, sizeof(ctrl_rel));
        sceCtrlPeekBufferPositive(0, &ctrl_rel, 1);
        if (!(ctrl_rel.buttons & (SCE_CTRL_SELECT | SCE_CTRL_CIRCLE))) {
            break;
        }
        sceKernelDelayThread(10000);
    }
}

bool c64_gui_is_active(void)
{
    return s_gui_active;
}

void c64_gui_request_close(void)
{
    s_gui_active = false;
}
