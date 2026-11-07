#ifndef PS3_TOUCH_H
#define PS3_TOUCH_H

#include <stdint.h>

#define SCE_TOUCH_PORT_FRONT 1
#define SCE_TOUCH_PORT_BACK  2
#define SCE_TOUCH_MAX_REPORT 16

#define SCE_TOUCH_SAMPLING_STATE_STOP  0
#define SCE_TOUCH_SAMPLING_STATE_START 1

typedef struct SceTouchReport {
    uint8_t id;
    uint8_t force;
    int16_t x;
    int16_t y;
    uint8_t reserved[8];
} SceTouchReport;

typedef struct SceTouchData {
    uint64_t timeStamp;
    unsigned int reportNum;
    SceTouchReport report[SCE_TOUCH_MAX_REPORT];
} SceTouchData;

#ifdef __cplusplus
extern "C" {
#endif

int sceTouchSetSamplingState(int port, int state);
int sceTouchEnableTouchForce(int port);
int sceTouchPeek(int port, SceTouchData *pData, unsigned int nBufs);

#ifdef __cplusplus
}
#endif

#endif
