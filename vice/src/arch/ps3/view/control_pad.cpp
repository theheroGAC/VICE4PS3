





















#include "control_pad.h"
#include "c64_vkbd_ps3.h"
#include "controls.h"
#include "vkeyboard.h"
#include "guitools.h"
#include "debug_ps3.h"
#include <cstring>
#include <ps3_ctrl.h>

extern "C" void PS3_ToggleJoystickPorts(void);
#include <ps3_touch.h>
#include <ps3_thread.h>


static int	gs_analogDirectionLookUp[9] = {0, ANALOG_UP, ANALOG_DOWN, 0, ANALOG_LEFT, 0, 0, 0, ANALOG_RIGHT};

ControlPad::ControlPad()
{
}

ControlPad::~ControlPad()
{
}

void ControlPad::init(View* view, Controls* controls, VirtualKeyboard* keyboard)
{
	m_view = view;
	m_controls = controls;
	m_keyboard = keyboard;
	m_joystickMask = 0x00;
	m_realBtnMask = 0;
	m_joystickScanSide = 0;
	
	
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
	
	
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	
}

void ControlPad::scan(ControlPadMap** maps, int* psize, bool scan_keyboard, bool scan_mouse)
{
	static SceCtrlData ctrl;
	static SceTouchData touch;
	
	char curr_joystick_bits = 0;
	static char prev_joystick_bits = 0;
	static int prevButtonsScan = 0;
	static TouchCoordinates touchBuf[16];
	static ControlPadMap mouseBuf[8];
	static int scan_count = 0;
	
	sceCtrlPeekBufferPositive(0, &ctrl, 1);

	if (c64_vkbd_is_visible()) {
		unsigned int pressed = ctrl.buttons & (~prevButtonsScan);
		unsigned int released = prevButtonsScan & (~ctrl.buttons);
		c64_vkbd_process_input(pressed, ctrl.buttons, released);
		prevButtonsScan = ctrl.buttons;
		return;
	}

	if ((ctrl.buttons & (~prevButtonsScan)) & (SCE_CTRL_LTRIGGER | SCE_CTRL_L1)) {
		c64_vkbd_toggle();
		prevButtonsScan = ctrl.buttons;
		return;
	}

	if ((ctrl.buttons & (~prevButtonsScan)) & SCE_CTRL_R3) {
		PS3_ToggleJoystickPorts();
	}


	
	curr_joystick_bits = 0x00;
	int jx = (m_joystickScanSide)? ctrl.rx: ctrl.lx;
	int jy = (m_joystickScanSide)? ctrl.ry: ctrl.ly;

	
	if (jx <= 40)
		curr_joystick_bits |= 0x04; 
	else if (jx >= 216)
		curr_joystick_bits |= 0x08; 
	
	if (jy <= 40){
		curr_joystick_bits |= 0x01; 
	}
	else if (jy >= 216)
		curr_joystick_bits |= 0x02; 


	getMaps(ctrl.buttons, prevButtonsScan, curr_joystick_bits, prev_joystick_bits, maps, psize);

	prevButtonsScan = ctrl.buttons;
	prev_joystick_bits = curr_joystick_bits;


	if (scan_keyboard && !(++scan_count % 2)) {

		scan_count = 0;

		
		sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

		
		int touch_count = touch.reportNum;

		for (int i=0; i<touch_count; ++i){
			touchBuf[i].x = touch.report[i].x;
			touchBuf[i].y = touch.report[i].y;	
		}
		
		
		m_keyboard->input(touchBuf, touch_count);
		m_keyboard->getKeyMaps(maps, psize);
	}
}

void ControlPad::getMaps(int curr_bmask, int prev_bmask, 
						  char curr_jmask, char prev_jmask,
						  ControlPadMap** maps, int* size)
{
	
	
	

	
	int new_actions = prev_bmask ^ curr_bmask;
	if (new_actions){
		ControlPadMap* map;
		int bit_slider = 1;
		while (bit_slider != 0x00010000){ 
			if (new_actions & bit_slider){
				m_realBtnMask ^= bit_slider;
				if(map = m_controls->getMappedKeyDigital(bit_slider, m_realBtnMask)){
					map->ispress = curr_bmask & bit_slider; 
					*maps++ = map; (*size)++;

					
					
					
					
					
					
					if ((bit_slider & SCE_CTRL_LTRIGGER) && !(curr_bmask & bit_slider)){ 
						if (map->ind != SCE_CTRL_LTRIGGER){ 
							map = m_controls->getMappedKeyDigital(SCE_CTRL_LTRIGGER, 0);
							map->ispress = false;
							*maps++ = map; (*size)++;
						}
					}
				}
			}
			bit_slider <<= 1;
		}
	}

	
	new_actions = prev_jmask ^ curr_jmask;
	if (new_actions){
		ControlPadMap* map;
		int bit_slider = 1;
		while (bit_slider != 0x10){
			if (new_actions & bit_slider){
				if (map = m_controls->getMappedKeyAnalog(gs_analogDirectionLookUp[bit_slider])){
					map->ispress = curr_jmask & bit_slider; 
					*maps++ = map; (*size)++;
				}
			}
			bit_slider <<= 1;
		}
	}
}

void ControlPad::changeJoystickScanSide(const char* side)
{
	if (!strcmp(side, "Right"))
		m_joystickScanSide = 1; 
	else if (!strcmp(side, "Left"))
		m_joystickScanSide = 0;
}

void ControlPad::waitTillButtonsReleased()
{
	SceCtrlData ctrl;
	sceCtrlPeekBufferPositive(0, &ctrl, 1); 

	if (ctrl.buttons){
		for (int i = 0; i < 200; i++)
		{
			sceCtrlReadBufferPositive(0, &ctrl, 1);
			if (!ctrl.buttons){
				break;
			}
			sceKernelDelayThread(2000);
		}
	}
}