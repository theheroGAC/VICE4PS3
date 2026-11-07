#ifndef PS3_POWER_H
#define PS3_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

int scePowerSetArmClockFrequency(int freq);
int scePowerSetBusClockFrequency(int freq);
int scePowerSetGpuClockFrequency(int freq);
int scePowerSetGpuXbarClockFrequency(int freq);

#ifdef __cplusplus
}
#endif

#endif
