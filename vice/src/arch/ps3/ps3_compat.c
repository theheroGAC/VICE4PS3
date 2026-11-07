#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <sys/systime.h>

#include <SDL2/SDL.h>

#include "ps3_types.h"
#include "ps3_ctrl.h"
#include "ps3_touch.h"
#include "ps3_io.h"
#include "ps3_thread.h"
#include "ps3_power.h"
#include "ps3_rtc.h"
#include "ps3_gfx.h"

#define PS3_PAD_LEFT    0
#define PS3_PAD_DOWN    1
#define PS3_PAD_RIGHT   2
#define PS3_PAD_UP      3
#define PS3_PAD_START   4
#define PS3_PAD_R3      5
#define PS3_PAD_L3      6
#define PS3_PAD_SELECT  7
#define PS3_PAD_SQUARE  8
#define PS3_PAD_CROSS   9
#define PS3_PAD_CIRCLE  10
#define PS3_PAD_TRIANGLE 11
#define PS3_PAD_R1      12
#define PS3_PAD_L1      13
#define PS3_PAD_R2      14
#define PS3_PAD_L2      15

static SDL_Joystick *ps3_joystick = NULL;
static int ps3_joystick_started = 0;

static void ps3_open_joystick(void)
{
    if (!ps3_joystick_started) {
        ps3_joystick_started = 1;
        if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0) {
            SDL_InitSubSystem(SDL_INIT_JOYSTICK);
        }
        SDL_JoystickEventState(SDL_ENABLE);
    }
    if (ps3_joystick == NULL && SDL_NumJoysticks() > 0) {
        ps3_joystick = SDL_JoystickOpen(0);
    }
}

static unsigned char ps3_axis_to_byte(Sint16 value)
{
    int out = ((int)value + 32768) >> 8;
    if (out < 0) {
        out = 0;
    }
    if (out > 255) {
        out = 255;
    }
    return (unsigned char)out;
}

static int ps3_button(int button)
{
    if (ps3_joystick == NULL) {
        return 0;
    }
    return SDL_JoystickGetButton(ps3_joystick, button) ? 1 : 0;
}

static Sint16 ps3_axis(int axis)
{
    if (ps3_joystick == NULL) {
        return 0;
    }
    if (axis >= SDL_JoystickNumAxes(ps3_joystick)) {
        return 0;
    }
    return SDL_JoystickGetAxis(ps3_joystick, axis);
}

int sceCtrlSetSamplingMode(int mode)
{
    (void)mode;
    ps3_open_joystick();
    return 0;
}

int sceCtrlPeekBufferPositive(int port, SceCtrlData *pad_data, int count)
{
    (void)port;
    (void)count;

    if (pad_data == NULL) {
        return -1;
    }

    ps3_open_joystick();
    SDL_JoystickUpdate();

    memset(pad_data, 0, sizeof(*pad_data));
    pad_data->timestamp = (uint64_t)SDL_GetTicks();

    if (ps3_button(PS3_PAD_LEFT)) {
        pad_data->buttons |= SCE_CTRL_LEFT;
    }
    if (ps3_button(PS3_PAD_DOWN)) {
        pad_data->buttons |= SCE_CTRL_DOWN;
    }
    if (ps3_button(PS3_PAD_RIGHT)) {
        pad_data->buttons |= SCE_CTRL_RIGHT;
    }
    if (ps3_button(PS3_PAD_UP)) {
        pad_data->buttons |= SCE_CTRL_UP;
    }
    if (ps3_button(PS3_PAD_START)) {
        pad_data->buttons |= SCE_CTRL_START;
    }
    if (ps3_button(PS3_PAD_SELECT)) {
        pad_data->buttons |= SCE_CTRL_SELECT;
    }
    if (ps3_button(PS3_PAD_L3)) {
        pad_data->buttons |= SCE_CTRL_L3;
    }
    if (ps3_button(PS3_PAD_R3)) {
        pad_data->buttons |= SCE_CTRL_R3;
    }
    if (ps3_button(PS3_PAD_SQUARE)) {
        pad_data->buttons |= SCE_CTRL_SQUARE;
    }
    if (ps3_button(PS3_PAD_CROSS)) {
        pad_data->buttons |= SCE_CTRL_CROSS;
    }
    if (ps3_button(PS3_PAD_CIRCLE)) {
        pad_data->buttons |= SCE_CTRL_CIRCLE;
    }
    if (ps3_button(PS3_PAD_TRIANGLE)) {
        pad_data->buttons |= SCE_CTRL_TRIANGLE;
    }
    if (ps3_button(PS3_PAD_L1)) {
        pad_data->buttons |= SCE_CTRL_L1;
    }
    if (ps3_button(PS3_PAD_R1)) {
        pad_data->buttons |= SCE_CTRL_R1;
    }
    if (ps3_button(PS3_PAD_L2)) {
        pad_data->buttons |= SCE_CTRL_LTRIGGER;
    }
    if (ps3_button(PS3_PAD_R2)) {
        pad_data->buttons |= SCE_CTRL_RTRIGGER;
    }

    pad_data->lx = ps3_axis_to_byte(ps3_axis(0));
    pad_data->ly = ps3_axis_to_byte(ps3_axis(1));
    if (ps3_joystick != NULL && SDL_JoystickNumAxes(ps3_joystick) >= 4) {
        pad_data->rx = ps3_axis_to_byte(ps3_axis(2));
        pad_data->ry = ps3_axis_to_byte(ps3_axis(3));
    } else {
        pad_data->rx = pad_data->lx;
        pad_data->ry = pad_data->ly;
    }

    return 0;
}

int sceCtrlReadBufferPositive(int port, SceCtrlData *pad_data, int count)
{
    int ret = sceCtrlPeekBufferPositive(port, pad_data, count);
    SDL_Delay(16);
    return ret;
}

static int ps3_touch_enabled = 0;
static int ps3_touch_x = 480;
static int ps3_touch_y = 272;

int sceTouchSetSamplingState(int port, int state)
{
    (void)port;
    ps3_touch_enabled = (state == SCE_TOUCH_SAMPLING_STATE_START);
    return 0;
}

int sceTouchEnableTouchForce(int port)
{
    (void)port;
    return 0;
}

int sceTouchPeek(int port, SceTouchData *pData, unsigned int nBufs)
{
    SceCtrlData pad;

    (void)port;
    (void)nBufs;

    if (pData == NULL) {
        return -1;
    }

    memset(pData, 0, sizeof(*pData));
    pData->timeStamp = (uint64_t)SDL_GetTicks();

    if (!ps3_touch_enabled) {
        return 0;
    }

    sceCtrlPeekBufferPositive(0, &pad, 1);

    if (pad.rx <= 40) {
        ps3_touch_x -= 8;
    } else if (pad.rx >= 216) {
        ps3_touch_x += 8;
    }
    if (pad.ry <= 40) {
        ps3_touch_y -= 8;
    } else if (pad.ry >= 216) {
        ps3_touch_y += 8;
    }
    if (ps3_touch_x < 0) {
        ps3_touch_x = 0;
    }
    if (ps3_touch_x > 959) {
        ps3_touch_x = 959;
    }
    if (ps3_touch_y < 0) {
        ps3_touch_y = 0;
    }
    if (ps3_touch_y > 543) {
        ps3_touch_y = 543;
    }

    if (pad.buttons & SCE_CTRL_R3) {
        pData->reportNum = 1;
        pData->report[0].id = 0;
        pData->report[0].force = 128;
        pData->report[0].x = (int16_t)(ps3_touch_x * 2);
        pData->report[0].y = (int16_t)(ps3_touch_y * 2);
    }

    return 0;
}

int sceIoOpen(const char *file, int flags, int mode)
{
    int posix_flags = 0;

    switch (flags & 3) {
        case SCE_O_WRONLY:
            posix_flags = O_WRONLY;
            break;
        case SCE_O_RDWR:
            posix_flags = O_RDWR;
            break;
        default:
            posix_flags = O_RDONLY;
            break;
    }
    if (flags & SCE_O_CREAT) {
        posix_flags |= O_CREAT;
    }
    if (flags & SCE_O_TRUNC) {
        posix_flags |= O_TRUNC;
    }
    if (flags & SCE_O_APPEND) {
        posix_flags |= O_APPEND;
    }

    return open(file, posix_flags, mode);
}

int sceIoClose(int fd)
{
    return close(fd);
}

int sceIoRead(int fd, void *data, unsigned int size)
{
    return (int)read(fd, data, (size_t)size);
}

int sceIoWrite(int fd, const void *data, unsigned int size)
{
    return (int)write(fd, data, (size_t)size);
}

SceOff sceIoLseek(int fd, SceOff offset, int whence)
{
    return (SceOff)lseek(fd, (off_t)offset, whence);
}

int sceIoRemove(const char *file)
{
    return remove(file);
}

int sceIoMkdir(const char *path, int mode)
{
    int ret = mkdir(path, (mode_t)mode);
    if (ret != 0 && errno == EEXIST) {
        return 0;
    }
    return ret;
}

int sceIoRmdir(const char *path)
{
    return rmdir(path);
}

static SceMode ps3_map_mode(mode_t mode)
{
    SceMode out = (SceMode)(mode & 0777);

    if (S_ISDIR(mode)) {
        out |= SCE_S_IFDIR;
    } else if (S_ISREG(mode)) {
        out |= SCE_S_IFREG;
    }

    return out;
}

static void ps3_fill_stat(const struct stat *st, SceIoStat *out)
{
    memset(out, 0, sizeof(*out));
    out->st_mode = ps3_map_mode(st->st_mode);
    out->st_size = (SceOff)st->st_size;
}

int sceIoGetstat(const char *file, SceIoStat *st_out)
{
    struct stat st;

    if (st_out == NULL) {
        return -1;
    }
    if (stat(file, &st) < 0) {
        return -1;
    }

    ps3_fill_stat(&st, st_out);
    return 0;
}

#define PS3_MAX_DIR_HANDLES 32
#define PS3_DIR_FD_BASE 100

typedef struct ps3_dir_handle_s {
    DIR *dir;
    char path[1024];
} ps3_dir_handle_t;

static ps3_dir_handle_t ps3_dir_handles[PS3_MAX_DIR_HANDLES];

int sceIoDopen(const char *path)
{
    int i;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        return -1;
    }

    for (i = 0; i < PS3_MAX_DIR_HANDLES; i++) {
        if (ps3_dir_handles[i].dir == NULL) {
            ps3_dir_handles[i].dir = dir;
            snprintf(ps3_dir_handles[i].path, sizeof(ps3_dir_handles[i].path), "%s", path);
            return PS3_DIR_FD_BASE + i;
        }
    }

    closedir(dir);
    return -1;
}

int sceIoDread(int fd, SceIoDirent *dir)
{
    int index = fd - PS3_DIR_FD_BASE;
    struct dirent *entry;
    struct stat st;
    char full_path[1280];

    if (index < 0 || index >= PS3_MAX_DIR_HANDLES || ps3_dir_handles[index].dir == NULL) {
        return -1;
    }
    if (dir == NULL) {
        return -1;
    }

    for (;;) {
        entry = readdir(ps3_dir_handles[index].dir);
        if (entry == NULL) {
            return 0;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }
        break;
    }

    memset(dir, 0, sizeof(*dir));
    snprintf(dir->d_name, sizeof(dir->d_name), "%s", entry->d_name);
    snprintf(full_path, sizeof(full_path), "%s/%s", ps3_dir_handles[index].path, entry->d_name);
    if (stat(full_path, &st) == 0) {
        ps3_fill_stat(&st, &dir->d_stat);
    }

    return 1;
}

int sceIoDclose(int fd)
{
    int index = fd - PS3_DIR_FD_BASE;

    if (index < 0 || index >= PS3_MAX_DIR_HANDLES || ps3_dir_handles[index].dir == NULL) {
        return -1;
    }

    closedir(ps3_dir_handles[index].dir);
    ps3_dir_handles[index].dir = NULL;
    ps3_dir_handles[index].path[0] = '\0';
    return 0;
}

void sceKernelDelayThread(unsigned int usec)
{
    if (usec == 0) {
        return;
    }
    sysUsleep(usec);
}

void sceKernelDelayThreadCB(unsigned int usec)
{
    sceKernelDelayThread(usec);
}

int scePowerSetArmClockFrequency(int freq)
{
    (void)freq;
    return 0;
}

int scePowerSetBusClockFrequency(int freq)
{
    (void)freq;
    return 0;
}

int scePowerSetGpuClockFrequency(int freq)
{
    (void)freq;
    return 0;
}

int scePowerSetGpuXbarClockFrequency(int freq)
{
    (void)freq;
    return 0;
}

uint64_t sceRtcGetTickResolution(void)
{
    return 1000000ULL;
}

int sceRtcGetCurrentTick(SceRtcTick *tick)
{
    struct timeval tv;

    if (tick == NULL) {
        return -1;
    }
    gettimeofday(&tv, NULL);
    tick->tick = (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
    return 0;
}

int sceRtcGetCurrentClock(SceDateTime *dt, int tz)
{
    time_t now = time(NULL);
    struct tm *tm = (tz == 0) ? gmtime(&now) : localtime(&now);

    if (dt == NULL || tm == NULL) {
        return -1;
    }

    dt->year = (unsigned short)(tm->tm_year + 1900);
    dt->month = (unsigned short)(tm->tm_mon + 1);
    dt->day = (unsigned short)tm->tm_mday;
    dt->hour = (unsigned short)tm->tm_hour;
    dt->minute = (unsigned short)tm->tm_min;
    dt->second = (unsigned short)tm->tm_sec;
    dt->microsecond = 0;
    return 0;
}

static int64_t ps3_days_from_civil(int year, int month, int day)
{
    int era;
    int yoe;
    int doy;
    int doe;

    year -= month <= 2;
    era = (year >= 0 ? year : year - 399) / 400;
    yoe = year - era * 400;
    doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

int sceRtcGetTime_t(const SceDateTime *time, time_t *seconds)
{
    int64_t days;

    if (time == NULL || seconds == NULL) {
        return -1;
    }

    days = ps3_days_from_civil(time->year, time->month, time->day);
    *seconds = (time_t)(days * 86400 + (int64_t)time->hour * 3600 + (int64_t)time->minute * 60 + time->second);
    return 0;
}

static int ps3_sdl_render_trace_count = 0;

void ps3_sdl_render_cp(const char *what)
{
    if (ps3_sdl_render_trace_count < 64) {
        ps3_gfx_log_write(what);
        ps3_sdl_render_trace_count++;
    }
}

void ps3_sdl_flip_timeout(unsigned int ms)
{
    char text[64];

    snprintf(text, sizeof(text), "sdl: flip timeout %u", ms);
    ps3_gfx_log_write(text);
}

void ps3_sdl_fifo_blocked(unsigned int ms)
{
    char text[64];

    if (ps3_sdl_render_trace_count >= 64) {
        return;
    }
    snprintf(text, sizeof(text), "sdl: fifo blocked %u", ms);
    ps3_gfx_log_write(text);
    ps3_sdl_render_trace_count++;
}

float SDL_roundf(float x)
{
    return roundf(x);
}
