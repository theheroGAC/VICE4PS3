/*
 * ps3_paths.c - relative path support for the PS3 port.
 *
 * Based on ps3_paths.c from UAE4All2-HD-PS3.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reent.h>
#include <sys/dirent.h>
#include <sys/syscalls.h>

#include "ps3_paths.h"

#ifndef PS3_TITLE_ID
#define PS3_TITLE_ID "VICEP3001"
#endif

#define PS3_PATH_MAX 1024

static char s_base_dir[PS3_PATH_MAX] = "/dev_hdd0/game/" PS3_TITLE_ID "/USRDIR";

static __typeof__(__syscalls.open_r)     orig_open_r;
static __typeof__(__syscalls.stat_r)     orig_stat_r;
static __typeof__(__syscalls.stat64_r)   orig_stat64_r;
static __typeof__(__syscalls.truncate_r) orig_truncate_r;
static __typeof__(__syscalls.chmod_r)    orig_chmod_r;
static __typeof__(__syscalls.rename_r)   orig_rename_r;
static __typeof__(__syscalls.mkdir_r)    orig_mkdir_r;
static __typeof__(__syscalls.opendir_r)  orig_opendir_r;
static __typeof__(__syscalls.rmdir_r)    orig_rmdir_r;
static __typeof__(__syscalls.link_r)     orig_link_r;
static __typeof__(__syscalls.unlink_r)   orig_unlink_r;
static __typeof__(__syscalls.utime_r)    orig_utime_r;

static int s_hooks_installed = 0;

static const char *resolve(const char *path, char *buf, size_t sz)
{
    size_t blen;

    if (path == NULL || path[0] == '\0' || path[0] == '/') {
        return path;
    }

    while (path[0] == '.' && path[1] == '/') {
        path += 2;
        while (path[0] == '/') {
            path++;
        }
    }
    if (path[0] == '.' && path[1] == '\0') {
        path++;
    }

    blen = strlen(s_base_dir);
    if (path[0] == '\0') {
        snprintf(buf, sz, "%s", s_base_dir);
    } else if (blen > 0 && s_base_dir[blen - 1] == '/') {
        snprintf(buf, sz, "%s%s", s_base_dir, path);
    } else {
        snprintf(buf, sz, "%s/%s", s_base_dir, path);
    }
    return buf;
}

static int hook_open_r(struct _reent *r, const char *file, int flags, int mode)
{
    char buf[PS3_PATH_MAX];
    return orig_open_r(r, resolve(file, buf, sizeof(buf)), flags, mode);
}

static int hook_stat_r(struct _reent *r, const char *path, struct stat *st)
{
    char buf[PS3_PATH_MAX];
    return orig_stat_r(r, resolve(path, buf, sizeof(buf)), st);
}

static int hook_stat64_r(struct _reent *r, const char *path, struct stat *st)
{
    char buf[PS3_PATH_MAX];
    return orig_stat64_r(r, resolve(path, buf, sizeof(buf)), st);
}

static int hook_truncate_r(struct _reent *r, const char *path, off_t len)
{
    char buf[PS3_PATH_MAX];
    return orig_truncate_r(r, resolve(path, buf, sizeof(buf)), len);
}

static int hook_chmod_r(struct _reent *r, const char *path, mode_t mode)
{
    char buf[PS3_PATH_MAX];
    return orig_chmod_r(r, resolve(path, buf, sizeof(buf)), mode);
}

static int hook_rename_r(struct _reent *r, const char *oldp, const char *newp)
{
    char b1[PS3_PATH_MAX];
    char b2[PS3_PATH_MAX];
    return orig_rename_r(r, resolve(oldp, b1, sizeof(b1)), resolve(newp, b2, sizeof(b2)));
}

static int hook_mkdir_r(struct _reent *r, const char *path, mode_t mode)
{
    char buf[PS3_PATH_MAX];
    return orig_mkdir_r(r, resolve(path, buf, sizeof(buf)), mode);
}

static DIR *hook_opendir_r(struct _reent *r, const char *path)
{
    char buf[PS3_PATH_MAX];
    return orig_opendir_r(r, resolve(path, buf, sizeof(buf)));
}

static int hook_rmdir_r(struct _reent *r, const char *path)
{
    char buf[PS3_PATH_MAX];
    return orig_rmdir_r(r, resolve(path, buf, sizeof(buf)));
}

static int hook_link_r(struct _reent *r, const char *oldp, const char *newp)
{
    char b1[PS3_PATH_MAX];
    char b2[PS3_PATH_MAX];
    return orig_link_r(r, resolve(oldp, b1, sizeof(b1)), resolve(newp, b2, sizeof(b2)));
}

static int hook_unlink_r(struct _reent *r, const char *path)
{
    char buf[PS3_PATH_MAX];
    return orig_unlink_r(r, resolve(path, buf, sizeof(buf)));
}

static int hook_utime_r(struct _reent *r, const char *path, const struct utimbuf *times)
{
    char buf[PS3_PATH_MAX];
    return orig_utime_r(r, resolve(path, buf, sizeof(buf)), times);
}

#define PS3_HOOK(name) \
    do { \
        if (__syscalls.name) { \
            orig_##name = __syscalls.name; \
            __syscalls.name = (__typeof__(__syscalls.name))hook_##name; \
        } \
    } while (0)

static void install_hooks(void)
{
    if (s_hooks_installed) {
        return;
    }
    s_hooks_installed = 1;

    PS3_HOOK(open_r);
    PS3_HOOK(stat_r);
    PS3_HOOK(stat64_r);
    PS3_HOOK(truncate_r);
    PS3_HOOK(chmod_r);
    PS3_HOOK(rename_r);
    PS3_HOOK(mkdir_r);
    PS3_HOOK(opendir_r);
    PS3_HOOK(rmdir_r);
    PS3_HOOK(link_r);
    PS3_HOOK(unlink_r);
    PS3_HOOK(utime_r);
}

static void ps3_paths_ctor(void) __attribute__((constructor(110)));
static void ps3_paths_ctor(void)
{
    install_hooks();
}

void ps3_paths_init(int argc, char **argv)
{
    install_hooks();

    if (argc > 0 && argv && argv[0] && argv[0][0] == '/') {
        char tmp[PS3_PATH_MAX];
        char *slash;

        snprintf(tmp, sizeof(tmp), "%s", argv[0]);
        slash = strrchr(tmp, '/');
        if (slash && slash != tmp) {
            *slash = '\0';
            snprintf(s_base_dir, sizeof(s_base_dir), "%s", tmp);
        }
    }
}

const char *ps3_base_dir(void)
{
    return s_base_dir;
}

const char *ps3_resolve_path(const char *path, char *buf, size_t sz)
{
    return resolve(path, buf, sz);
}
