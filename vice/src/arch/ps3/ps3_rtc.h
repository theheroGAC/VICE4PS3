#ifndef PS3_RTC_H
#define PS3_RTC_H

#include <time.h>
#include "ps3_types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t sceRtcGetTickResolution(void);
int sceRtcGetCurrentTick(SceRtcTick *tick);
int sceRtcGetCurrentClock(SceDateTime *time, int tz);
int sceRtcGetTime_t(const SceDateTime *time, time_t *seconds);

#ifdef __cplusplus
}
#endif

#endif
