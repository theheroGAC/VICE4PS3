






















#include "controller.h"
#include "control_pad.h"
#include "file_explorer.h"
#include "peripherals.h"
#include "extractor.h"
#include "guitools.h"
#include "ctrl_defs.h"
#include "app_defs.h"
#include "debug_ps3.h"
#include "ps3_gfx.h"
#include "resid/sid.h"


extern "C" {
#include "main.h"
#include "ui.h"
#include "autostart.h"
#include "cartridge.h"
#include "resources.h"
#include "mem.h"
#include "video.h"
#include "machine.h"
#include "sound.h"
#include "vsync.h"
#include "sid.h"
#include "joy.h"
#include "attach.h"
#include "tape.h"
#include "datasette.h"
#include "drive.h"
#include "c64cartsystem.h"
#include "joystick.h"
#include "keyboard.h"
#include "log.h"
#include "videoarch.h"
#include "tapecontents.h"
#include "diskcontents.h"
#include "attach.h"
#include "lib.h"
#include "mousedrv.h"
#include "raster.h"
#include "snapshot.h"
#include "kbdbuf.h"
#include "maincpu.h"
#include "t64.h"
}

#include <cstring>
#include <stdio.h>
#include <pthread.h>
#include <ps3_thread.h> 


extern "C" bool c64_gui_is_active(void);
static int s_menu_cooldown = 0;
static View* gs_view;

extern "C" int PS3_CreateView(int width, int height, int depth)
{
	return gs_view->createView(width, height, depth);
}

extern "C" void PS3_UpdateView()
{
	gs_view->updateView();
	
	gs_frameDrawn = true;
}

extern "C" void PS3_SetViewport(int x, int y, int width, int height)
{
	
	if (gs_view->isBorderlessView()){
		video_canvas_s* canvas;
		video_PS3_get_canvas(&canvas);
		width = canvas->geometry->gfx_size.width;
		height = canvas->geometry->gfx_size.height;
		x = canvas->geometry->extra_offscreen_border_left + canvas->geometry->gfx_position.x;
		y = canvas->geometry->gfx_position.y;
	}

	gs_view->updateViewport(x, y, width, height);
}

extern "C" void	PS3_GetViewInfo(int* width, int* height, unsigned char** ppixels, int* pitch, int* bpp)
{
	gs_view->getViewInfo(width, height, ppixels, pitch, bpp);
}

extern "C" void PS3_ScanControls()
{
	static ControlPadMap* maps[64];
	int size = 0;

	
	if (gs_bootTime){
		ps3_gfx_log_write("menu: boot request");
		video_PS3_menu_show();
		gs_bootTime = false;
		return;
	}

	gs_view->scanControls(maps, &size, gs_scanMouse);

	for (int i=0; i<size; ++i){
		ControlPadMap* map = maps[i];
		
		if (!map) continue;
		if (map->isjoystick){
			if (map->ispress)
				joystick_value[g_joystickPort] |= map->joypin;
			else
				joystick_value[g_joystickPort] &= ~map->joypin;
			continue;
		}
		if (map->iskey){
			
			int row = (map->mid & 0x08)? -(map->mid >> 4): (map->mid >> 4);
			int column = map->mid & 0x07;
			keyboard_set_keyarr_any(row, column, map->ispress);
			continue;
		}

		
		if (!map->ispress && map->mid != 136) 
			continue;

		
		switch (map->mid){
		case 126: 
			if (s_menu_cooldown > 0 || c64_gui_is_active() || gs_showMenuTimer > 0) {
				break;
			}
			ps3_gfx_log_write("act: mid 126 menu pressed");
			if (ui_emulation_is_paused()){
				gs_view->activateMenu();
				setSoundVolume(100); 
				s_menu_cooldown = 20;
				break;
			}

			setPendingAction(CTRL_ACTION_SHOW_MENU);
			s_menu_cooldown = 20;
			break;
		case 127: 
			gs_view->toggleKeyboardOnView();
			keyboard_clear_keymatrix(); 
			gs_view->updateView(); 
			break;
		case 128: 
			ps3_gfx_log_write("act: mid 128 pause pressed");
			if (!ui_emulation_is_paused())
				setPendingAction(CTRL_ACTION_PAUSE);
			else{
				ui_pause_emulation(0);
				gs_view->displayPaused(0);
				gs_view->updateView();
				setSoundVolume(100); 
			}
			break;
		case 129: 
			toggleJoystickPorts();
			break;
		case 130: 
			toggleWarpMode();
			break;
		case 136: 
			if (map->ispress)
				gs_autofireOn = true;
			else{
				gs_autofireOn = false;
				joystick_value[g_joystickPort] &= ~0x10; 
			}
			break;
		case 137: 
			if (!ui_emulation_is_paused()){ 
				machine_trigger_reset(gs_machineResetMode);
				keyboard_clear_keymatrix(); 
			}
			break;
		case 138: 
			gs_view->toggleStatusbarOnView();
			break;
		default:
			break;
		}
	}

	if (gs_autofireOn){
		
		if (++gs_frameCounter % gs_moduloDivider == 0){
			uint8_t joy_pins = joystick_value[g_joystickPort];
			joystick_value[g_joystickPort] = (joy_pins & 0x10)? joy_pins & ~0x10: joy_pins | 0x10;
			gs_frameCounter = 0;
		}
	}

	checkPendingActions();

	
	
	
	
	
	if (!gs_frameDrawn && gs_view->pendingRedraw())
		gs_view->updateView();

	gs_frameDrawn = false;
}

extern "C" void PS3_ApplySettings()
{
	resources_set_int(VICE_RES_VICII_FILTER, 0); 
	resources_set_int(VICE_RES_VIRTUAL_DEVICES, 1);
	resources_set_int("Drive8Type", 1542);
	resources_set_int("Drive9Type", DRIVE_TYPE_NONE);
	resources_set_int("Drive10Type", DRIVE_TYPE_NONE);
	resources_set_int("Drive11Type", DRIVE_TYPE_NONE);
	resources_set_int("AutostartPrgMode", 1);
	resources_set_int("AutostartHandleTrueDriveEmulation", 1);
	resources_set_int("DriveSoundEmulationVolume", 2000);
	gs_view->applyAllSettings();
	resources_set_int(VICE_RES_SID_ENGINE, SID_ENGINE_RESID);
	resources_set_int(VICE_RES_SID_RESID_SAMPLING, 1);
	resources_set_int("SidFilters", 1);
	resources_set_int("AutostartPrgMode", 1);
	resources_set_int("AutostartHandleTrueDriveEmulation", 1);
	resources_set_int("SoundSampleRate", 48000);
	resources_set_int("SoundBufferSize", 100);
	resources_set_int("SoundFragmentSize", 2);
	resources_set_int("SoundSpeedAdjustment", SOUND_ADJUST_EXACT);
	resources_set_int("SoundOutput", SOUND_OUTPUT_SYSTEM);
	sound_close();
}

extern "C" void PS3_ActivateMenu()
{
	ps3_gfx_log_write("menu: activate enter");
	
	sound_suspend();
	ps3_gfx_log_write("menu: activate suspend done");
	gs_view->activateMenu();
	ps3_gfx_log_write("menu: activate show done");
	sound_resume();
	if (gs_savedSoundVolume >= 0) {
		resources_set_int(VICE_RES_SOUND_VOLUME, gs_savedSoundVolume);
		gs_savedSoundVolume = -1;
	}
	s_menu_cooldown = 20;
	ps3_gfx_log_write("menu: activate resume done");
}

extern "C" const char* PS3_GetGuiSetting(int key)
{
	static const char empty[] = "";
	const char* value = NULL;

	if (gs_view != NULL) {
		gs_view->getSettingValues(key, &value, NULL, NULL, NULL);
	}

	return (value != NULL) ? value : empty;
}

extern "C" void PS3_ApplyGuiSetting(int key, const char* value)
{
	if (gs_view != NULL) {
		gs_view->applyGuiSetting(key, value);
	}
}

extern "C" int PS3_GetAutofire(void)
{
	return gs_autofireOn ? 1 : 0;
}

extern "C" void PS3_SetAutofire(int enabled)
{
	gs_autofireOn = enabled != 0;
	if (!gs_autofireOn) {
		joystick_value[g_joystickPort] &= ~0x10;
	}
}

extern "C" int PS3_LoadStateFile(const char* path)
{
	return (gs_view != NULL) ? gs_view->loadState(path) : -1;
}

extern "C" int PS3_LoadGameFile(const char* path)
{
	Controller ctrl;
	return ctrl.loadFile(CTRL_AUTO_DETECT_LOAD, path, 0);
}

extern "C" int PS3_SaveStateFile(const char* path)
{
	return (gs_view != NULL) ? gs_view->saveState(path) : -1;
}

extern "C" int PS3_RGBToPixel(uint8_t r, uint8_t g, uint8_t b)
{
	return gs_view->convertRGBToPixel(r,g,b);
}

extern "C" void PS3_NotifyPalette(unsigned char* palette, int size)
{
	gs_view->setPalette(palette, size);
}

extern "C" void PS3_NotifyFPS(int fps, float percent, int warp_flag)
{
	gs_view->setFPSCount(fps, (int)percent, warp_flag);
}

extern "C" void	PS3_NotifyTapeCounter(int counter)
{
	gs_view->setTapeCounter(counter);
}

extern "C" void	PS3_NotifyTapeControl(int control)
{	
	
	if (isTapOnTape()){
		gs_view->setTapeControl(control);
	}
}

extern "C" void	PS3_NotifyDriveStatus(int drive, int led)
{
	gs_view->setDriveLed(drive, led);
}

extern "C" void	PS3_NotifyDriveContent(int drive, const char* image)
{
	
}

extern "C" void PS3_NotifyTapeMotorStatus(int motor)
{
	gs_view->setTapeMotorStatus(motor);
}

extern "C" void	PS3_NotifyDriveTrack(unsigned int drive, unsigned int track)
{
	gs_view->setDriveTrack(drive, track);
}

extern "C" int	PS3_ShowMessage(const char* msg, int msg_type)
{
	return gs_view->showMessage(msg, msg_type);
}

extern "C" void	PS3_NotifyReset()
{
	
	if (gs_bootTime) 
		return;

	
	
	if (!gs_autoStartInProgress){
		const char* cart_file_name = cartridge_get_file_name(cart_getid_slotmain());
		g_game_file = (!cart_file_name)? "BASIC": g_devDataSrc[DEV_CARTRIDGE].src_file;
	}

	gs_view->notifyReset();
	gs_scanScreenPressPlayTimer = 0;
	gs_scanScreenLoadingTimer = 0;
	gs_scanScreenReadyTimer = 0;
	gs_autoStartInProgress = false;
}

Controller::Controller()
{
}

Controller::~Controller()
{
}

void Controller::init(View* view)
{
	gs_view = view;
}

int Controller::loadFile(int load_type, const char* file, int index)
{
	int ret = 0;

	switch (load_type){
	case CTRL_AUTO_DETECT_LOAD:
		{

		if (!file)
			return -1;

		const char* image_file = Extractor::getInst()->extract(file);

		if (!image_file){
			return -1;
		}
		
		int image_type = getImageType(image_file);

		
		detachImage(CARTRIDGE);
		
		
		if (isTapOnTape()){
			detachImage(DATASETTE);
		}

		
		datasette_control(DATASETTE_CONTROL_RESET);

		
		resources_set_int(VICE_RES_WARP_MODE, 0); 

		
		pauseEmulation(false);

		
		int cartridge_reset = 1;
		if (image_type == IMAGE_CARTRIDGE){
			resources_get_int(VICE_RES_CARTRIDGE_RESET, &cartridge_reset);
			if (!cartridge_reset)
				resources_set_int(VICE_RES_CARTRIDGE_RESET, 1);
		}
	
		resources_set_int("AutostartPrgMode", 1);
		resources_set_int("AutostartHandleTrueDriveEmulation", 1);
		ret = autostart_autodetect(image_file, NULL, index, AUTOSTART_MODE_RUN);

		
		if (!cartridge_reset)
			resources_set_int(VICE_RES_CARTRIDGE_RESET, 0);

		if (ret < 0){
			return -1;
		}

		if (image_type == IMAGE_DISK || image_type == IMAGE_PROGRAM){
			
			gs_view->setDriveDiskPresence(0, 1);
			g_devDataSrc[DEV_DRIVE8].src_file = file;
			g_devDataSrc[DEV_DRIVE8].image_file = image_file;
			
			syncSetting(DRIVE); 
		}else if (image_type == IMAGE_TAPE){
			g_devDataSrc[DEV_DATASETTE].src_file = file;
			g_devDataSrc[DEV_DATASETTE].image_file = image_file;
			syncSetting(DATASETTE); 
		}else if (image_type == IMAGE_CARTRIDGE){
			g_devDataSrc[DEV_CARTRIDGE].src_file = file;
			g_devDataSrc[DEV_CARTRIDGE].image_file = image_file;
		}

		gs_autoStartInProgress = true;
		g_game_file = file; 
		break;
		}
	case CTRL_CART_LOAD:
		
		pauseEmulation(false);
		machine_trigger_reset(MACHINE_RESET_MODE_HARD);
		break;
	case CTRL_DISK_LOAD:
		{

		if (isCpuInRam()) 
			return -1; 

		
		resources_set_int(VICE_RES_WARP_MODE, 0); 

		char* prg_name = NULL;
		string str_prg_name;

		int drive_id = getCurrentDriveId();
		const char* image = file_system_get_disk_name(drive_id);

		image_contents_t *contents = diskcontents_filesystem_read(image);
		if (contents) {
			prg_name = image_contents_filename_by_number(contents, index);
			image_contents_destroy(contents);
		}

		
		if (prg_name){
			str_prg_name = prg_name;
			size_t pos = str_prg_name.find_first_of(0xa0);
			if (pos != string::npos){
				str_prg_name = str_prg_name.substr(0, pos);
			}
			lib_free(prg_name);
		}

		g_game_file = g_devDataSrc[drive_id-8].src_file;
		gs_loadProgramName = !str_prg_name.empty()? str_prg_name.c_str(): "*";

		setPendingAction(CTRL_ACTION_KBDCMD_LOAD_DISK);
		pauseEmulation(false);
		break;
		}
	case CTRL_TAPE_LOAD:
		{
		if (!tape_image_dev1)
			return -1;
		
		if (isCpuInRam()) 
			return -1; 

		string str_prg_name;

		if (tape_image_dev1->type == TAPE_TYPE_TAP){
			if (index > 0) index--; 
			tape_seek_to_file(tape_image_dev1, index); 
		}
		else if (tape_image_dev1->type == TAPE_TYPE_T64){ 
			char* prg_name = NULL;
			const char* image_name = (const char*)tape_image_dev1->name;
			if (!image_name) image_name = file; 
			
			
			image_contents_s* contents = tapecontents_read(image_name);
			if (contents) {
				prg_name = image_contents_filename_by_number(contents, index); 
				image_contents_destroy(contents);
			}

			
			if (prg_name){
				str_prg_name = prg_name;
				size_t pos = str_prg_name.find_first_of(0xa0);
				if (pos != string::npos){
					str_prg_name = str_prg_name.substr(0, pos);
				}
				lib_free(prg_name);
			}

			if (index > 1){
				tape_seek_to_file(tape_image_dev1, index-2); 
			}
			else{
				
				
				
				
				
				
				int size = ((t64_t*)(tape_image_dev1->data))->header.num_entries;
				tape_seek_to_file(tape_image_dev1, size-1);
			}
		}
		else{
			return -1;
		}

		g_game_file = g_devDataSrc[DEV_DATASETTE].src_file;
		gs_loadProgramName = str_prg_name.c_str();

		setPendingAction(CTRL_ACTION_KBDCMD_LOAD_TAPE);
		pauseEmulation(false);
		break;
		}
	}

	return ret;
}

int Controller::loadState(const char* file)
{
	
	resources_set_int(VICE_RES_WARP_MODE, 0);
	
	string cartridge;
	const char* file_name = cartridge_get_file_name(cart_getid_slotmain());
	
	if (file_name)
		cartridge = file_name;

	int ret = machine_read_snapshot((char*)file, 0);
	
	if (!cartridge.empty()){
		
		
		
		
		int cartridge_reset;
		resources_get_int(VICE_RES_CARTRIDGE_RESET, &cartridge_reset);
		resources_set_int(VICE_RES_CARTRIDGE_RESET, 0);
		cartridge_attach_image(CARTRIDGE_CRT, cartridge.c_str());
		resources_set_int(VICE_RES_CARTRIDGE_RESET, cartridge_reset);
	}

	return ret;
}

int Controller::saveState(const char* file_name)
{
	
	
	
	
	
	
	string current_game;
	string datasette_game;
	string attached_image_file;

	if (!isTapOnTape())
		goto case_normal;

	if (!tape_image_dev1->name)
		goto case_normal;

	attached_image_file = tape_image_dev1->name;

	
	current_game = getFileNameNoExt(g_game_file.c_str());
	datasette_game = getFileNameNoExt(g_devDataSrc[DEV_DATASETTE].src_file.c_str());

	if (current_game != datasette_game)
		goto case_exception; 

case_normal:
	return machine_write_snapshot(file_name, 0, 0, 0);

case_exception:
	tape_image_detach(1);
	int ret = machine_write_snapshot(file_name, 0, 0, 0);
	tape_image_attach(1, attached_image_file.c_str());
	return ret;
}

int Controller::patchSaveState(patch_data_s* patch)
{
	if (!patch || !patch->snapshot_file || !patch->data)
		return -1;
	
	FILE *fp;

	if (!(fp = fopen(patch->snapshot_file, "a+"))){
		return -1;
	}

	
	
	
	
	
	
	
	
	
	
	
	
	

	int module_size = 16 + 1 + 1 + 4 + 4 + patch->data_size;
	char* buf = new char[module_size];

	char name[16] = {0};

	if (patch->module_name)
		strncpy(name, patch->module_name, 16);

	
	memcpy(buf, name, 16);
	buf[16] = patch->major;
	buf[17] = patch->minor;
	buf[18] = module_size;
	buf[19] = module_size >> 8;
	buf[20] = module_size >> 16;
	buf[21] = module_size >> 24;
	
	buf[22] = patch->data_size;
	buf[23] = patch->data_size >> 8;
	buf[24] = patch->data_size >> 16;
	buf[25] = patch->data_size >> 24;
	memcpy(&buf[26], patch->data, patch->data_size);
	
	if (fwrite(buf, 1, module_size, fp) < 1) {
		fclose(fp);
        return -1;
    }

	fclose(fp);
	return 0;
}

int	Controller::getSaveStatePatch(patch_data_s* pinfo)
{
	uint8_t major_version;
	uint8_t minor_version;
	snapshot_t* snapshot;
	snapshot_module_t* patch_module;

	
	

	
	snapshot = snapshot_open(pinfo->snapshot_file, &major_version, &minor_version, machine_get_name());
	
	if (!snapshot){
		return -1;
	}

	
	patch_module = snapshot_module_open(snapshot,
                                        pinfo->module_name,
                                        &pinfo->major,
                                        &pinfo->minor);

	if (!patch_module){
		snapshot_close(snapshot);
		return -1;
	}

	
	if (snapshot_module_read_dword(patch_module, &pinfo->data_size) < 0){
		snapshot_module_close(patch_module);
		snapshot_close(snapshot);
		return -1;
	}

	
	snapshot_module_read_byte_array(patch_module,
                                    (uint8_t*)pinfo->data, 
									pinfo->data_size);
	

	snapshot_module_close(patch_module);
	snapshot_close(snapshot);

	return 0;
}

int	Controller::getSaveStatePatchInfo(patch_data_s* pinfo)
{
	uint8_t major_version;
	uint8_t minor_version;
	snapshot_t* snapshot;
	snapshot_module_t* patch_module;

	snapshot = snapshot_open(pinfo->snapshot_file, &major_version, &minor_version, machine_get_name());
	
	if (!snapshot){
		return -1;
	}
	
	patch_module = snapshot_module_open(snapshot,pinfo->module_name,&pinfo->major,&pinfo->minor);
	
	if (!patch_module){
		snapshot_close(snapshot);
		return -1;
	}
		
	
	if (snapshot_module_read_dword(patch_module, &pinfo->data_size) < 0){
		snapshot_module_close(patch_module);
		snapshot_close(snapshot);
		return -1;
	}
	
	snapshot_module_close(patch_module);
	snapshot_close(snapshot);

	return 0;
}

void Controller::resetComputer()
{
	machine_trigger_reset(gs_machineResetMode);
}

void Controller::setModelProperty(int key, const char* value)
{
	if (!key || !value)
		return;

	switch (key){
	case JOYSTICK_PORT:
		changeJoystickPort(value);break;
	case COLOR_PALETTE:
		setColorPalette(value);break;
	case CPU_SPEED:
		setCpuSpeed(value);break;
	case SOUND:
		setAudioPlayback(value);break;
	case SID_ENGINE:
		setSidEngine(value);break;
	case SID_MODEL:
		setSidModel(value);break;
	case SID_FILTERS:
		setSidFilters(value);break;
	case VICII_MODEL:
		setViciiModel(value);break;
	case DRIVE_STATUS:
		setDriveStatus(value);break;
	case DRIVE_TRUE_EMULATION:
		setDriveEmulation(value);break;
	case DRIVE_SOUND_EMULATION:
		setDriveSoundEmulation(value);break;
	case DATASETTE_RESET_WITH_CPU:
		setDatasetteReset(value);break;
	case CARTRIDGE_RESET:
		setCartridgeReset(value);break;
	case MACHINE_RESET:
		setMachineResetMode(value); break;
	}
}

void Controller::getImageFileContents(int peripheral, const char* image, const char*** values, int* values_size)
{
	*values = NULL;
	*values_size = 0;
	image_contents_t* content = NULL;

	if (peripheral == DRIVE || peripheral == DATASETTE){
		

		if (peripheral == DRIVE){
			int drive_id = getCurrentDriveId();
			content = diskcontents_read(image, drive_id);
		}
		else if (peripheral == DATASETTE)
			content = tapecontents_read(image);

		if (!content)
			return;

		image_contents_file_list_t* entry = content->file_list;

		
		int list_size = 0;
		while (entry){
			list_size++;
			entry = entry->next;
		}

		list_size++; 
		*values = new const char*[list_size];
		*values_size = list_size;
		const char** p = *values;

		*p++ = image_contents_to_string(content, IMAGE_CONTENTS_STRING_ASCII); 

		entry = content->file_list;

		for (int i=0; i<list_size-1; ++i){
			*p++ = image_contents_file_to_string(entry, 1); 
			entry = entry->next;
		}
	}

	if (content){
		image_contents_destroy(content);
	}
}

void Controller::changeJoystickPort(const char* port)
{
	if (!strcmp(port, "Port 1")){
		g_joystickPort = 1;
		resources_set_int(VICE_RES_JOY_PORT1_DEV, 1);  
		resources_set_int(VICE_RES_JOY_PORT2_DEV, 0);  
	}
	else if (!strcmp(port, "Port 2")){
		g_joystickPort = 2;
		resources_set_int(VICE_RES_JOY_PORT2_DEV, 1);
		resources_set_int(VICE_RES_JOY_PORT1_DEV, 0);
	}
}

void Controller::setCpuSpeed(const char* val)
{
	int value;

	if(!strcmp(val, "100%"))
		value = 100;
	else if (!strcmp(val, "125%"))
		value = 125;
	else if (!strcmp(val, "150%"))
		value = 150;
	else if (!strcmp(val, "175%"))
		value = 175;
	else if (!strcmp(val, "200%"))
		value = 200;
	else 
		value = 100;

	resources_set_int(VICE_RES_CPU_SPEED, value);
}

void Controller::setAudioPlayback(const char* val)
{
	int value = !strcmp(val, "Enabled")? 1: 0; 
	resources_set_int(VICE_RES_SOUND, value);
}

void Controller::setSidEngine(const char* val)
{
	int value;
	if (!strcmp(val, "FastSID"))
		value = SID_ENGINE_FASTSID;
	else if (!strcmp(val, "ReSID"))
		value = SID_ENGINE_RESID;
	else if (!strcmp(val, "ReSID-FP"))
		value = SID_ENGINE_RESID_FP;
	else
		return;

	resources_set_int(VICE_RES_SID_ENGINE, value);
	
}

void Controller::setSidModel(const char* val)
{
	int value;
	if (!strcmp(val, "6581"))
		value = SID_MODEL_6581;
	else if (!strcmp(val, "8580"))
		value = SID_MODEL_8580;
	else
		return;

	resources_set_int(VICE_RES_SID_MODEL, value);
}

void Controller::setSidFilters(const char* val)
{
	if (!strcmp(val, "Enabled"))
		resources_set_int("SidFilters", 1);
	else if (!strcmp(val, "Disabled"))
		resources_set_int("SidFilters", 0);
}

void Controller::setBorderVisibility(const char* val)
{
	
	

	int x,y,width,height;
	struct video_canvas_s* canvas;

	video_PS3_get_canvas(&canvas);

	if (!canvas)
		return;

	if (!strcmp(val,"Hide")){
		width = canvas->geometry->gfx_size.width;
		height = canvas->geometry->gfx_size.height;
		x = canvas->geometry->extra_offscreen_border_left + canvas->geometry->gfx_position.x;
		y = canvas->geometry->gfx_position.y;
	}
	else{
		width = canvas->draw_buffer->canvas_width;
		height = canvas->draw_buffer->canvas_height;
		x = canvas->geometry->extra_offscreen_border_left;
		y = canvas->geometry->first_displayed_line;
	}

	gs_view->updateViewport(x,y,width,height);
}













































void Controller::setJoystickAutofireSpeed(const char* val)
{
	
	

	if (!strcmp(val, "Fast"))
		gs_moduloDivider = 3; 
	else if (!strcmp(val, "Medium"))
		gs_moduloDivider = 6; 
	else if (!strcmp(val, "Slow"))
		gs_moduloDivider = 12; 

	gs_frameCounter = 0;
}

void Controller::setViciiModel(const char* val)
{
	int value = MACHINE_SYNC_PAL;

	if (!strcmp(val, "NTSC"))
		value = MACHINE_SYNC_NTSC;
	else if (!strcmp(val, "Old NTSC"))
		value = MACHINE_SYNC_NTSCOLD;
	else if (!strcmp(val, "PAL-N"))
		value = MACHINE_SYNC_PALN;

	resources_set_int(VICE_RES_MACHINE_VIDEO_STANDARD, value); 
}

void Controller::setCrtEmulation()
{
	resources_set_int(VICE_RES_VICII_DOUBLE_SCAN, 1);
	resources_set_int(VICE_RES_VICII_DOUBLE_SIZE, 1);
	
	resources_set_int(VICE_RES_VICII_FILTER, 1); 
}

void Controller::setColorPalette(const char* val)
{
	const char* curr_palette;
	const char* new_palette = NULL;

    resources_get_string_sprintf("%sPaletteFile", &curr_palette, "VICII");
	if (!curr_palette) curr_palette = "";

	if (!strcmp(val, "Pepto (PAL)") && strcmp("pepto-pal", curr_palette))
		new_palette = "pepto-pal";
	else if (!strcmp(val, "Colodore") && strcmp("colodore", curr_palette))
		new_palette = "colodore";
	else if (!strcmp(val, "Vice") && strcmp("vice", curr_palette))
		new_palette = "vice";
	else if (!strcmp(val, "Ptoing") && strcmp("ptoing", curr_palette))
		new_palette = "ptoing";
	else if (!strcmp(val, "RGB") && strcmp("rgb", curr_palette))
		new_palette = "rgb";
	else if (!strcmp(val, "None"))
		new_palette = "None";
	
	if (new_palette){
		if (strcmp(val, "None")){
			resources_set_int(VICE_RES_VICII_EXTERNAL_PALETTE, 1);
			resources_set_string_sprintf("%sPaletteFile", new_palette, "VICII");
		}
		else{
			resources_set_int(VICE_RES_VICII_EXTERNAL_PALETTE, 0);
		}

		updatePalette();
	}
}

void Controller::setDriveStatus(const char* val)
{
	int drive_id = getCurrentDriveId();

	if (!strcmp(val, "Active")){
		resources_set_int_sprintf("Drive%dType", 1542, drive_id);
		gs_view->setDriveStatus(drive_id-8, 1);
	}
	else if (!strcmp(val, "Not active")){
		resources_set_int_sprintf("Drive%dType", DRIVE_TYPE_NONE, drive_id);
		gs_view->setDriveStatus(drive_id-8, 0);
	}
}

void Controller::setDriveEmulation(const char* val)
{
	
	if (!strcmp(val, "Fast"))
		resources_set_int(VICE_RES_DRIVE_TRUE_EMULATION, 0);
	else if (!strcmp(val, "True"))
		resources_set_int(VICE_RES_DRIVE_TRUE_EMULATION, 1);
}

void Controller::setDriveSoundEmulation(const char* val)
{
	
	if (!strcmp(val, "Disabled"))
		resources_set_int(VICE_RES_DRIVE_SOUND_EMULATION, 0);
	else if (!strcmp(val, "Enabled"))
		resources_set_int(VICE_RES_DRIVE_SOUND_EMULATION, 1);
}

void Controller::setJoystickPort(const char* val)
{
	if (!strcmp(val, "Port 1")){
		resources_set_int(VICE_RES_JOY_DEVICE_1, JOYDEV_JOYSTICK);
		resources_set_int(VICE_RES_JOY_DEVICE_2, JOYDEV_NONE);
	}
	else if (!strcmp(val, "Port 2")){
		resources_set_int(VICE_RES_JOY_DEVICE_1, JOYDEV_NONE);
		resources_set_int(VICE_RES_JOY_DEVICE_2, JOYDEV_JOYSTICK);
	}
}

void Controller::setDatasetteReset(const char* val)
{
	
	if (!strcmp(val, "Enabled"))
		resources_set_int(VICE_RES_DATASETTE_RESET_WITH_CPU, 1);
	else if (!strcmp(val, "Disabled"))
		resources_set_int(VICE_RES_DATASETTE_RESET_WITH_CPU, 0);
}

void Controller::setCartridgeReset(const char* val)
{
	
	if (!strcmp(val, "Enabled"))
		resources_set_int(VICE_RES_CARTRIDGE_RESET, 1);
	else if (!strcmp(val, "Disabled"))
		resources_set_int(VICE_RES_CARTRIDGE_RESET, 0);
}

void Controller::setMachineResetMode(const char* val)
{
	if (!strcmp(val, "Hard"))
		gs_machineResetMode = MACHINE_RESET_MODE_HARD;
	else if (!strcmp(val, "Soft"))
		gs_machineResetMode = MACHINE_RESET_MODE_SOFT;
}

int Controller::attachDriveImage(int drive, const char* image)
{
	if(!strcmp(image, "Empty"))
		return -1;

	if (file_system_attach_disk(drive, image) < 0)
		return -1;

	
	gs_view->setDriveDiskPresence(drive-8, 1);

	return 0;
}

int Controller::attachTapeImage(const char* val)
{
	if(!strcmp(val, "Empty"))
		return -1;

	if (tape_image_attach(1, val) < 0)
		return -1;

	return 0;
}

int Controller::attachCartridgeImage(const char* val)
{
	if(!strcmp(val, "Empty"))
		return -1; 

	if (cartridge_attach_image(CARTRIDGE_CRT, val) < 0)
		return -1;

	return 0;
}

void Controller::detachDriveImage(int drive)
{
	file_system_detach_disk(drive);

	
	gs_view->setDriveDiskPresence(drive-8, 0);
}

int Controller::detachTapeImage()
{
	return tape_image_detach(1);
}

void Controller::detachCartridgeImage()
{
	cartridge_detach_image(-1);
}

void Controller::syncSetting(int key)
{
	

	const char*		dev_image_file = NULL;  
	const char*		dev_data_src = NULL;	
	const char*		stn_value;
	const char**	stn_value_list;
	const char*		stn_data_src;			
	int				stn_list_size = 0;
	
	switch (key){
	case DRIVE:
	case DATASETTE:
	{
		int drive_id;

		
		if (key == DRIVE){
			drive_id = getCurrentDriveId();
			dev_image_file = file_system_get_disk_name(drive_id);
			dev_data_src = g_devDataSrc[drive_id-8].src_file.c_str();
		}else{
			dev_image_file = tape_get_file_name();
			dev_data_src = g_devDataSrc[DEV_DATASETTE].src_file.c_str();
		}

		
		gs_view->getSettingValues(key, &stn_value, &stn_data_src, &stn_value_list, &stn_list_size);

		if (!dev_image_file){
			
			
			if (stn_value_list && stn_list_size){
				const char** p = stn_value_list;
				for (int i=0; i<stn_list_size; ++i){
					lib_free(*p++);
				}
				delete[] stn_value_list;
			}

			
			if (key == DRIVE){
				g_devDataSrc[drive_id-8].src_file.clear();
				g_devDataSrc[drive_id-8].image_file.clear();
			}else{
				g_devDataSrc[DEV_DATASETTE].src_file.clear();
				g_devDataSrc[DEV_DATASETTE].image_file.clear();
			}

			gs_view->onSettingChanged(key,"Empty","",0,0,15);
			break;
		}
		
		if (!strcmp(stn_value, "Empty")){
			
			
			getImageFileContents(key, dev_image_file, &stn_value_list, &stn_list_size);
			if (stn_list_size){
				gs_view->onSettingChanged(key, stn_value_list[0], dev_data_src, stn_value_list, stn_list_size, 15);
			}else
				gs_view->onSettingChanged(key, "", dev_data_src, stn_value_list, stn_list_size, 15);
		}
		else{
			
			
			if (!strcmp(stn_data_src, dev_data_src)){
				return;
			}
			
			if (stn_value_list && stn_list_size){
				const char** p = stn_value_list;
				for (int i=0; i<stn_list_size; ++i){
					lib_free(*p++);
				}
				delete[] stn_value_list;
			}

			getImageFileContents(key, dev_image_file, &stn_value_list, &stn_list_size);

			if (stn_list_size){
				gs_view->onSettingChanged(key, stn_value_list[0], dev_data_src, stn_value_list, stn_list_size, 15);
			}else
				gs_view->onSettingChanged(key, "", dev_data_src, stn_value_list, stn_list_size, 15);
		}
		break;
	}
	case DRIVE_STATUS:
		{
		
		int drive_type;
		int drive_id = getCurrentDriveId();
		if (resources_get_int_sprintf("Drive%dType", &drive_type, drive_id) < 0){
			return;
		}
		string str_val = (drive_type == DRIVE_TYPE_NONE)? "Not active": "Active";
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);

		
		for (int i=8; i<12; ++i){
			if (resources_get_int_sprintf("Drive%dType", &drive_type, i) < 0)
				continue;
			gs_view->setDriveStatus(i-8, drive_type);
		}
		break;
		}
	case DRIVE_TRUE_EMULATION:
		{
		int val; 
		string str_val;
		
		if (resources_get_int(VICE_RES_DRIVE_TRUE_EMULATION, &val) < 0)
			return;
		
		str_val = val? "True": "Fast";
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case DRIVE_SOUND_EMULATION:
		{
		int val; 
		string str_val;
		if (resources_get_int(VICE_RES_DRIVE_SOUND_EMULATION, &val) < 0)
			return;
		str_val = val? "Enabled": "Disabled";
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case DATASETTE_RESET_WITH_CPU:
		{
		int val; 
		string str_val;
		if (resources_get_int(VICE_RES_DATASETTE_RESET_WITH_CPU, &val) < 0)
			return;
		str_val = val? "Enabled": "Disabled";
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case CARTRIDGE:
		dev_image_file = cartridge_get_file_name(cart_getid_slotmain());
		dev_data_src = g_devDataSrc[DEV_CARTRIDGE].src_file.c_str();
		if (dev_image_file){
			gs_view->onSettingChanged(key, getFileNameFromPath(dev_image_file).c_str(),dev_data_src,0,0,3);
		}else{
			g_devDataSrc[DEV_CARTRIDGE].src_file.clear();
			g_devDataSrc[DEV_CARTRIDGE].image_file.clear();
			gs_view->onSettingChanged(key, "Empty","",0,0,3);
		}
		break;
	case CARTRIDGE_RESET:
		{
		int val; 
		string str_val;
		
		if (resources_get_int(VICE_RES_CARTRIDGE_RESET, &val) < 0)
			return;
		
		str_val = val? "Enabled": "Disabled";
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case VICII_MODEL:
		{
		int val; 
		string str_val;

		if (resources_get_int(VICE_RES_MACHINE_VIDEO_STANDARD, &val) < 0)
			return;
		
		switch (val){
		case MACHINE_SYNC_PAL:
			str_val = "PAL";
			break;
		case MACHINE_SYNC_NTSC:
			str_val = "NTSC";
			break;
		case MACHINE_SYNC_NTSCOLD:
			str_val = "Old NTSC";
			break;
		case MACHINE_SYNC_PALN:
			str_val = "PAL-N";
			break;
		}

		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case SID_ENGINE:
		{
		int val; 
		string str_val;

		if (resources_get_int(VICE_RES_SID_ENGINE, &val) < 0)
			return;
		
		switch (val){
		case SID_ENGINE_FASTSID: str_val = "FastSID"; break;
		case SID_ENGINE_RESID: str_val = "ReSID"; break;
		case SID_ENGINE_RESID_FP: str_val = "ReSID-FP"; break;
		}
		
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case SID_MODEL:
		{
		int val; 
		string str_val;

		if (resources_get_int(VICE_RES_SID_MODEL, &val) < 0)
			return;
		
		switch (val){
		case SID_MODEL_6581: str_val = "6581"; break;
		case SID_MODEL_8580: str_val = "8580"; break;
		}
		
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	case JOYSTICK_PORT:
		{
		int val_port1;
		int val_port2;
		string str_val;

		
		
		if (resources_get_int(VICE_RES_JOY_PORT1_DEV, &val_port1) < 0)
			return;

		if (resources_get_int(VICE_RES_JOY_PORT2_DEV, &val_port2) < 0)
			return;

		if (val_port1 == 1 && val_port2 != 1){
			g_joystickPort = 1;
			str_val = "Port 1";
		}
		else if (val_port2 == 1 && val_port1 != 1){
			g_joystickPort = 2;
			str_val = "Port 2";
		}

		if (!str_val.empty())
			gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);

		break;
		}
	case CPU_SPEED:
		{
		int val; 
		string str_val;

		if (resources_get_int(VICE_RES_CPU_SPEED, &val) < 0)
			return;
		
		switch (val){
		case 100: str_val = "100%"; break;
		case 125: str_val = "125%"; break;
		case 150: str_val = "150%"; break;
		case 175: str_val = "175%"; break;
		case 200: str_val = "200%"; break;
		}

		if (!str_val.empty())
			gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);

		break;
		}
	case SOUND:
		{
		int val; 
		string str_val;
		if (resources_get_int(VICE_RES_SOUND, &val) < 0)
			return;

		str_val = val==1? "Enabled": "Disabled";
		gs_view->onSettingChanged(key, str_val.c_str(),0,0,0,1);
		break;
		}
	default:
		break;
	}
}

void Controller::syncPeripherals()
{
	syncSetting(DRIVE);
	syncSetting(DRIVE_STATUS);
	syncSetting(DRIVE_TRUE_EMULATION);
	syncSetting(DRIVE_SOUND_EMULATION);
	syncSetting(DATASETTE);
	syncSetting(CARTRIDGE);
	syncSetting(CARTRIDGE_RESET);
}

void Controller::syncModelSettings()
{
	syncSetting(VICII_MODEL);
	syncSetting(SID_ENGINE);
	syncSetting(SID_MODEL);
	syncSetting(COLOR_PALETTE);
	syncSetting(JOYSTICK_PORT);
	syncSetting(CPU_SPEED);
	syncSetting(SOUND);
	syncSetting(DRIVE_STATUS);
	syncSetting(DRIVE_TRUE_EMULATION);
	syncSetting(DRIVE_SOUND_EMULATION);
	syncSetting(CARTRIDGE_RESET);
}

void Controller::updatePalette()
{
	video_PS3_update_palette();
}

void Controller::resumeSound()
{
	sound_resume();
}

string Controller::getFileNameFromPath(const char* fpath)
{
	string fname = fpath;

	size_t slash_pos = fname.find_last_of("/");
	if (slash_pos != string::npos){
		return fname.substr(slash_pos+1, string::npos);
	}

	size_t colon_pos = fname.find_last_of(":");
	if (colon_pos != string::npos){
		return fname.substr(colon_pos+1, string::npos);
	}

	return fname;
}

string Controller::getFileNameNoExt(const char* fpath)
{
	string fname = fpath;
	
	
	size_t slash_pos = fname.find_last_of("/");
	if (slash_pos != string::npos){
		fname = fname.substr(slash_pos+1, string::npos);
	}else{
		size_t colon_pos = fname.find_last_of(":");
		if (colon_pos != string::npos){
			fname = fname.substr(colon_pos+1, string::npos);
		}
	}

	
	size_t comma_pos = fname.find_last_of(".");
	if (comma_pos != string::npos){
		fname = fname.substr(0, comma_pos);
	}

	return fname;
}

int Controller::getImageType(const char* image)
{
	int ret = -1;
	string extension;
	string file = image;

	static const char* disk_ext[] = {"D64","D71","D80","D81","D82","G64","G41","X64",0};
	static const char* tape_ext[] = {"T64","TAP",0};
	static const char* cart_ext[] = {"CRT",0};
	static const char* prog_ext[] = {"PRG","P00",0};

	size_t dot_pos = file.find_last_of(".");
	if (dot_pos != string::npos)
		extension = file.substr(dot_pos+1, string::npos);

	if (extension.empty())
		return ret;
	
	strToUpperCase(extension);
	
	const char** p = disk_ext;

	while (*p){
		if (!strcmp(extension.c_str(), *p))	
			return IMAGE_DISK;
		p++;
	}
	
	p = tape_ext;

	while (*p){
		if (!strcmp(extension.c_str(), *p))	
			return IMAGE_TAPE;
		p++;
	}
	
	p = cart_ext;

	while (*p){
		if (!strcmp(extension.c_str(), *p))	
			return IMAGE_CARTRIDGE;
		p++;
	}
	
	p = prog_ext;

	while (*p){
		if (!strcmp(extension.c_str(), *p))	
			return IMAGE_PROGRAM;
		p++;
	}

	return ret;
}

void Controller::setTapeControl(int action)
{
	switch (action){
	case DATASETTE_CONTROL_STOP:
	case DATASETTE_CONTROL_START:
	case DATASETTE_CONTROL_FORWARD:
	case DATASETTE_CONTROL_REWIND:
	case DATASETTE_CONTROL_RECORD:
	case DATASETTE_CONTROL_RESET:
	case DATASETTE_CONTROL_RESET_COUNTER:
		datasette_control(action);
		break;
	}
}

void Controller::setCartControl(int action)
{
	switch (action){
	case CART_CONTROL_FREEZE:
		keyboard_clear_keymatrix();
        cartridge_trigger_freeze(); 
		break;
	case CART_CONTROL_SET_DEFAULT:
		cartridge_set_default(); 
		break;
	case CART_CONTROL_FLUSH_IMAGE:
		break;
	case CART_CONTROL_SAVE_IMAGE:
		break;
	}
}

int Controller::getViewport(ViewPort* vp, bool borders)
{
	struct video_canvas_s* canvas;

	video_PS3_get_canvas(&canvas);

	if (!canvas)
		return -1;

	if (borders){
		vp->width = canvas->draw_buffer->canvas_width;
		vp->height = canvas->draw_buffer->canvas_height;
		vp->x = canvas->geometry->extra_offscreen_border_left;
		vp->y = canvas->geometry->first_displayed_line;
	}
	else{
		vp->width = canvas->geometry->gfx_size.width;
		vp->height = canvas->geometry->gfx_size.height;
		vp->x = canvas->geometry->extra_offscreen_border_left + canvas->geometry->gfx_position.x;
		vp->y = canvas->geometry->gfx_position.y;
	}

	return 0;
}

int Controller::attachImage(int device, const char* file)
{
	int drive_id = getCurrentDriveId();

	const char* image_file = Extractor::getInst()->extract(file, drive_id);

	if (!image_file)
		return -1;

	
	int type = getImageType(image_file);
	switch (type){
	case IMAGE_DISK:
	case IMAGE_PROGRAM:
		if (device != DRIVE) return -1;
		break;
	case IMAGE_TAPE:
		if (device != DATASETTE) return -1;
		break;
	case IMAGE_CARTRIDGE:
		if (device != CARTRIDGE) return -1;
		break;
	}
	
	
	detachImage(device);
	
	switch (device){
	case DRIVE:
		if (attachDriveImage(drive_id, image_file) < 0) 
			return -1;
		g_devDataSrc[drive_id-8].src_file = file;
		g_devDataSrc[drive_id-8].image_file = image_file;
		break;
	case DATASETTE:
		if (attachTapeImage(image_file) < 0) 
			return -1;
		g_devDataSrc[DEV_DATASETTE].src_file = file;
		g_devDataSrc[DEV_DATASETTE].image_file = image_file;
		break;
	case CARTRIDGE:
		if (attachCartridgeImage(image_file) < 0)
			return -1;
		g_devDataSrc[DEV_CARTRIDGE].src_file = file;
		g_devDataSrc[DEV_CARTRIDGE].image_file = image_file;
		break;
	}

	return 0;
}

void Controller::detachImage(int device)
{
	switch (device){
	case DRIVE:
		{
		int drive_id = getCurrentDriveId();
		detachDriveImage(drive_id);
		g_devDataSrc[drive_id-8].src_file.clear();
		g_devDataSrc[drive_id-8].image_file.clear();
		break;
		}
	case DATASETTE:
		detachTapeImage();
		g_devDataSrc[DEV_DATASETTE].src_file.clear();
		g_devDataSrc[DEV_DATASETTE].image_file.clear();
		break;
	case CARTRIDGE:
		detachCartridgeImage();
		g_devDataSrc[DEV_CARTRIDGE].src_file.clear();
		g_devDataSrc[DEV_CARTRIDGE].image_file.clear();
	}
}

static void toggleJoystickPorts()
{
	if (g_joystickPort == 1){
		g_joystickPort = 2;
		resources_set_int(VICE_RES_JOY_PORT1_DEV, 0); 
		resources_set_int(VICE_RES_JOY_PORT2_DEV, 1); 
		if (gs_view) gs_view->onSettingChanged(JOYSTICK_PORT,"Port 2","",0,0,1);
	}
	else{
		g_joystickPort = 1;
		resources_set_int(VICE_RES_JOY_PORT1_DEV, 1); 
		resources_set_int(VICE_RES_JOY_PORT2_DEV, 0); 
		if (gs_view) gs_view->onSettingChanged(JOYSTICK_PORT,"Port 1","",0,0,1);
	}
}

extern "C" void PS3_ToggleJoystickPorts(void)
{
	toggleJoystickPorts();
}

static void toggleWarpMode()
{
	int value;
    if (resources_get_int(VICE_RES_WARP_MODE, &value) < 0)
        return;

	value = value? 0:1;
	resources_set_int(VICE_RES_WARP_MODE, value);
}

static void	checkPendingActions()
{	
	if (s_menu_cooldown > 0){
		s_menu_cooldown--;
	}
	if (gs_showMenuTimer > 0){
		if (--gs_showMenuTimer == 0)
			video_PS3_menu_show();
	}
	if (gs_pauseTimer > 0){
		if (--gs_pauseTimer != 0) return;
		ps3_gfx_log_write("act: pause trap queued");
		ui_pause_emulation(1);
		gs_view->displayPaused(1);
		gs_view->setFPSCount(0,0,0);
		gs_view->updateView();
	}
	if (gs_loadDiskTimer > 0){
		if (--gs_loadDiskTimer != 0) return;
		if (isCpuInRam()) return; 

		int drive_id = getCurrentDriveId();
		char* cmd = lib_msprintf("LOAD\"%s\",%d,1:\r", gs_loadProgramName.c_str(), drive_id);
		kbdbuf_feed(cmd);
		lib_free(cmd);
		
		
		setPendingAction(CTRL_ACTION_SCANSCR_LOADING_READY);
	}
	if (gs_loadTapeTimer > 0){
		if (--gs_loadTapeTimer != 0) return;
		if (isCpuInRam()) return; 
		
		if (gs_loadProgramName.empty())
			kbdbuf_feed("LOAD:\r");
		else{
			char* cmd = lib_msprintf("LOAD\"%s\":\r", gs_loadProgramName.c_str());
			kbdbuf_feed(cmd);
			lib_free(cmd);
		}

		setPendingAction(CTRL_ACTION_SCANSCR_PRESSPLAYONTAPE);
	}
	if (gs_kbdCmdRunTimer > 0){
		if (--gs_kbdCmdRunTimer != 0) return;
		kbdbuf_feed("RUN\r");
	}
	if (gs_scanScreenPressPlayTimer > 0){
		if (--gs_scanScreenPressPlayTimer != 0) return;
		int ret = scanScreen("PRESS PLAY ON TAPE", CURSOR_NOWAIT_BLINK);
		
		switch (ret){
		case 0: 
			datasette_control(DATASETTE_CONTROL_START);
			setPendingAction(CTRL_ACTION_SCANSCR_LOADING_READY);
			break;
		case 1: 
			
			setPendingAction(CTRL_ACTION_SCANSCR_LOADING_READY);
			break;
		case 2: 
			if (isCpuInRam()) return;
			gs_scanScreenPressPlayTimer = 50;
			break;
		}
	}
	if (gs_scanScreenLoadingTimer > 0){
		if (--gs_scanScreenLoadingTimer != 0) return;
		int ret = scanScreen("LOADING", CURSOR_NOWAIT_BLINK);

		switch (ret){
		case 0:
			setPendingAction(CTRL_ACTION_SCANSCR_LOADING_READY);
			break;
		case 1:
			break;
		case 2:
			if (isCpuInRam()) return;
			gs_scanScreenLoadingTimer = 50;
		}
	}
	if (gs_scanScreenReadyTimer > 0){
		if (--gs_scanScreenReadyTimer != 0) return;
		int ret = scanScreen("READY.", CURSOR_WAIT_BLINK);

		switch (ret){
		case 0:
			
			setPendingAction(CTRL_ACTION_KBDCMD_RUN);
			break;
		case 1:
			break;
		case 2:
			if (isCpuInRam()){ 
				return;
			}
			gs_scanScreenReadyTimer = 50;
		}
	}
	if (gs_activateDriveTimer > 0){
		if (--gs_activateDriveTimer != 0) return;

		int drive_id = getCurrentDriveId();
		resources_set_int_sprintf("Drive%dType", 1542, drive_id);
	}
	if (gs_deactivateDriveTimer > 0){
		if (--gs_deactivateDriveTimer != 0) return;

		int drive_id = getCurrentDriveId();
		resources_set_int_sprintf("Drive%dType", DRIVE_TYPE_NONE, drive_id);
	}
	if (gs_activateDriveAndLoadDiskTimer > 0){
		if (--gs_activateDriveAndLoadDiskTimer != 0) return;

		int drive_id = getCurrentDriveId();
		resources_set_int_sprintf("Drive%dType", 1542, drive_id);

		
		gs_loadDiskTimer = 50;
	}	
}

static void setPendingAction(ctrl_pending_action_e action)
{
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	switch (action){

	case CTRL_ACTION_SHOW_MENU:
		if (!gs_showMenuTimer){
			gs_showMenuTimer = 10;
			ps3_gfx_log_write("act: menu queued");
			resources_get_int(VICE_RES_SOUND_VOLUME, &gs_savedSoundVolume);
			setSoundVolume(0);
		}
		break;
	case CTRL_ACTION_PAUSE:
		if (!gs_pauseTimer){
			gs_pauseTimer = 10;
			ps3_gfx_log_write("act: pause queued");
			resources_get_int(VICE_RES_SOUND_VOLUME, &gs_savedSoundVolume);
			setSoundVolume(0);
		}
		break;
	case CTRL_ACTION_ACTIVATE_DRIVE:
		gs_activateDriveTimer = 25;
		break;
	case CTRL_ACTION_DEACTIVATE_DRIVE:
		gs_deactivateDriveTimer = 25;
		break;
	case CTRL_ACTION_KBDCMD_LOAD_DISK:
		if (!gs_loadDiskTimer){
			
			
			
			

			int tde;
			resources_get_int(VICE_RES_DRIVE_TRUE_EMULATION, &tde);
			if (tde == 1)
				gs_activateDriveAndLoadDiskTimer = 20;
			else
				gs_loadDiskTimer = 20;
		}
		break;
	case CTRL_ACTION_KBDCMD_LOAD_TAPE:
		if (!gs_loadTapeTimer){
			gs_loadTapeTimer = 50;
		}
		break;
	case CTRL_ACTION_KBDCMD_RUN:
		if (!gs_kbdCmdRunTimer){
			gs_kbdCmdRunTimer = 5;
		}
		break;
	case CTRL_ACTION_SCANSCR_PRESSPLAYONTAPE:
		if (!gs_scanScreenPressPlayTimer){
			gs_scanScreenPressPlayTimer = 50;
		}
		break;
	case CTRL_ACTION_SCANSCR_LOADING:
		if (!gs_scanScreenLoadingTimer){
			gs_scanScreenLoadingTimer = 50;
		}
		break;
	case CTRL_ACTION_SCANSCR_LOADING_READY:
		if (!gs_scanScreenReadyTimer){
			gs_scanScreenReadyTimer = 50;
		}
		break;
	}
}

static void setSoundVolume(int vol)
{
	resources_set_int(VICE_RES_SOUND_VOLUME, vol); 
}

static void pauseEmulation(bool pause)
{
	if (pause){
		if (ui_emulation_is_paused())
			return;

		ui_pause_emulation(1);
		gs_view->displayPaused(1);		

	}
	else{
		if (!ui_emulation_is_paused())
			return;

		ui_pause_emulation(0);
		gs_view->displayPaused(0);	
	}
}

static int scanScreen(const char *s, unsigned int blink_mode)
{
	
	
	
	
	
	
	
	
	int blnsw = 0xcc;
	int pnt   = 0xd1;
	int pntr  = 0xd3;
	int lnmx  = 0xd5;
	
    int screen_addr, line_length, cursor_column, addr, i;

    screen_addr = (int)(mem_read((uint16_t)(pnt)) | (mem_read((uint16_t)(pnt + 1)) << 8));
    cursor_column = (int)mem_read((uint16_t)(pntr));

    line_length = (int)(lnmx < 0 ? -lnmx : mem_read((uint16_t)(lnmx)) + 1);

    if (!kbdbuf_is_empty()) {
        return 2;
    }

    if (blink_mode == CURSOR_WAIT_BLINK && cursor_column != 0) {
        return 2;
    }

    if (blink_mode == CURSOR_WAIT_BLINK && blnsw != 0 && mem_read(blnsw) != 0) {
        return 2;
    }

    if (blink_mode == CURSOR_WAIT_BLINK) {
        addr = screen_addr - line_length;
    } else {
        addr = screen_addr;
    }

    for (i = 0; s[i] != '\0'; i++) {
        if (mem_read((uint16_t)(addr + i)) != s[i] % 64) {
            if (mem_read((uint16_t)(addr + i)) != (uint8_t)32) {
                return 1; 
            }
            return 2; 
        }
    }
    return 0; 
}

static bool isCpuInRam()
{
	
	if (machine_addr_in_ram(reg_pc)) {
		return true;
	}
  
	return false;
}

static bool isFileOfType(const char* fname, const char* type)
{
	string extension;
	string file = fname;

	size_t dot_pos = file.find_last_of(".");
	if (dot_pos != string::npos)
		extension = file.substr(dot_pos+1, string::npos);

	if (extension.empty())
		return false;

	if (extension == type)
		return true;

	strToUpperCase(extension);

	if (extension == type)
		return true;

	return false;
}

static void strToUpperCase(string& str)
{
   for (std::string::iterator p = str.begin(); p != str.end(); ++p)
       *p = toupper(*p);
}

static int getCurrentDriveId()
{
	int ret = 8;
	const char* key_value;
	
	
	gs_view->getSettingValues(DRIVE_NUMBER, &key_value,0,0,0);

	if (!strcmp(key_value, "8"))
		ret = 8;
	else if (!strcmp(key_value, "9"))
		ret = 9;
	else if (!strcmp(key_value, "10"))
		ret = 10;
	else if (!strcmp(key_value, "11"))
		ret = 11;

	return ret;
}

static bool isTapOnTape()
{
	

	if (!tape_image_dev1)
		return false;

	if (tape_image_dev1->type == TAPE_TYPE_TAP)
		return true;

	return false;
}