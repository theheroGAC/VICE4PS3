#ifndef PS3_TYPES_H
#define PS3_TYPES_H

#include <stdint.h>

typedef int32_t SceUID;
typedef uint32_t SceMode;
typedef int64_t SceOff;
typedef uint64_t SceUInt64;

typedef struct SceDateTime {
    unsigned short year;
    unsigned short month;
    unsigned short day;
    unsigned short hour;
    unsigned short minute;
    unsigned short second;
    unsigned int microsecond;
} SceDateTime;

typedef struct SceRtcTick {
    uint64_t tick;
} SceRtcTick;

#endif
