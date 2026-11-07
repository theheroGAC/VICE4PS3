#ifndef PS3_CTRL_H
#define PS3_CTRL_H

#include <stdint.h>
#include "ps3_types.h"

#define SCE_CTRL_SELECT        0x000001
#define SCE_CTRL_L3            0x000002
#define SCE_CTRL_R3            0x000004
#define SCE_CTRL_START         0x000008
#define SCE_CTRL_UP            0x000010
#define SCE_CTRL_RIGHT         0x000020
#define SCE_CTRL_DOWN          0x000040
#define SCE_CTRL_LEFT          0x000080
#define SCE_CTRL_LTRIGGER      0x000100
#define SCE_CTRL_RTRIGGER      0x000200
#define SCE_CTRL_L1            0x000400
#define SCE_CTRL_R1            0x000800
#define SCE_CTRL_TRIANGLE      0x001000
#define SCE_CTRL_CIRCLE        0x002000
#define SCE_CTRL_CROSS         0x004000
#define SCE_CTRL_SQUARE        0x008000
#define SCE_CTRL_INTERCEPTED   0x00010000
#define SCE_CTRL_VOLUP         0x00100000
#define SCE_CTRL_VOLDOWN       0x00400000

#define SCE_CTRL_MODE_DIGITAL  0
#define SCE_CTRL_MODE_ANALOG   1

typedef struct SceCtrlData {
    uint64_t timestamp;
    unsigned int buttons;
    unsigned char lx;
    unsigned char ly;
    unsigned char rx;
    unsigned char ry;
    unsigned char lt;
    unsigned char rt;
    unsigned char reserved[4];
} SceCtrlData;

#ifdef __cplusplus
extern "C" {
#endif

int sceCtrlSetSamplingMode(int mode);
int sceCtrlPeekBufferPositive(int port, SceCtrlData *pad_data, int count);
int sceCtrlReadBufferPositive(int port, SceCtrlData *pad_data, int count);

#ifdef __cplusplus
}
#endif

#endif
