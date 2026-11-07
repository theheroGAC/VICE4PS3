#ifndef PS3_THREAD_H
#define PS3_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

void sceKernelDelayThread(unsigned int usec);
void sceKernelDelayThreadCB(unsigned int usec);

#ifdef __cplusplus
}
#endif

#endif
