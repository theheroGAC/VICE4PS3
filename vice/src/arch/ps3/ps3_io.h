#ifndef PS3_IO_H
#define PS3_IO_H

#include "ps3_types.h"

#define SCE_O_RDONLY 0x0001
#define SCE_O_WRONLY 0x0002
#define SCE_O_RDWR   0x0003
#define SCE_O_CREAT  0x0040
#define SCE_O_TRUNC  0x0400
#define SCE_O_APPEND 0x0800

#define SCE_S_IFMT   0xF000
#define SCE_S_IFREG  0x1000
#define SCE_S_IFDIR  0x2000

typedef struct SceIoStat {
    SceMode st_mode;
    unsigned int st_attr;
    SceOff st_size;
    SceDateTime st_ctime;
    SceDateTime st_atime;
    SceDateTime st_mtime;
    unsigned int st_private[6];
} SceIoStat;

typedef struct SceIoDirent {
    SceIoStat d_stat;
    char d_name[256];
    void *d_private;
    int dummy;
} SceIoDirent;

#ifdef __cplusplus
extern "C" {
#endif

int sceIoOpen(const char *file, int flags, int mode);
int sceIoClose(int fd);
int sceIoRead(int fd, void *data, unsigned int size);
int sceIoWrite(int fd, const void *data, unsigned int size);
SceOff sceIoLseek(int fd, SceOff offset, int whence);
int sceIoRemove(const char *file);
int sceIoMkdir(const char *path, int mode);
int sceIoRmdir(const char *path);
int sceIoGetstat(const char *file, SceIoStat *stat);
int sceIoDopen(const char *path);
int sceIoDread(int fd, SceIoDirent *dir);
int sceIoDclose(int fd);

#ifdef __cplusplus
}
#endif

#endif
