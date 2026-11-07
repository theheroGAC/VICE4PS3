#ifndef C64_VKBD_PS3_H
#define C64_VKBD_PS3_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void c64_vkbd_init(void);
void c64_vkbd_shutdown(void);
void c64_vkbd_toggle(void);
void c64_vkbd_show(void);
void c64_vkbd_hide(void);
bool c64_vkbd_is_visible(void);
void c64_vkbd_render(void);
bool c64_vkbd_process_input(unsigned int pressed, unsigned int held, unsigned int released);

#ifdef __cplusplus
}
#endif

#endif
