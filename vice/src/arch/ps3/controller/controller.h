#ifndef CONTROLLER_H
#define CONTROLLER_H
#ifndef __cplusplus
void		PS3_CreateView(int width, int height, int depth);
void		PS3_UpdateView();
void		PS3_SetViewport(int x, int y, int width, int height);
void		PS3_GetViewInfo(int* width, int* height, unsigned char** ppixels, int* pitch, int* bpp);
void		PS3_ScanControls();
void		PS3_ApplySettings();
void		PS3_ActivateMenu();
int			PS3_RGBToPixel(uint8_t r, uint8_t g, uint8_t b);
void		PS3_NotifyPalette(unsigned char* palette, int size);
void		PS3_NotifyFPS(int fps, float percent, int warp_flag);
void		PS3_NotifyTapeCounter(int count);
void		PS3_NotifyTapeControl(int control);
void		PS3_NotifyDriveStatus(int drive, int led);
void		PS3_NotifyDriveContent(int drive, const char* image);
void		PS3_NotifyTapeMotorStatus(int motor);
void		PS3_NotifyDriveTrack(unsigned int drive, unsigned int track);
void		PS3_NotifyReset();
int			PS3_ShowMessage(const char* msg, int msg_type);
#else
#include "view.h"
#include <string>
#include <vector>
extern "C" {
#include "imagecontents.h"
}
extern int g_joystickPort;
extern "C" void PS3_ToggleJoystickPorts(void);
extern "C" const char* PS3_GetGuiSetting(int key);
extern "C" void PS3_ApplyGuiSetting(int key, const char* value);
extern "C" int PS3_GetAutofire(void);
extern "C" void PS3_SetAutofire(int enabled);
extern "C" int PS3_LoadStateFile(const char* path);
extern "C" int PS3_SaveStateFile(const char* path);
typedef struct{
	string fname;
	string fpath;
}file_info_s;
typedef struct{
	int row;
	int column;
	int ispress;
}key_action_s;
typedef struct{
	const char* snapshot_file;
	const char* module_name;
	uint8_t		major;
	uint8_t		minor;
	const char*	data;
	uint32_t	data_size;
}patch_data_s;
enum ctrl_load_e{
	CTRL_AUTO_DETECT_LOAD,
	CTRL_DISK_LOAD,
	CTRL_TAPE_LOAD,
	CTRL_CART_LOAD
};
using std::string;
using std::vector;
class View;
class Peripherals;
struct dev_data_s;
class Controller
{
private:
	bool			m_inGame;
	string			getFileNameFromPath(const char* fpath);
	string			getFileNameNoExt(const char* fpath);
	void			changeJoystickPort(const char* port);
	void			setCpuSpeed(const char* val);
	void			setAudioPlayback(const char* val);
	void			setSidEngine(const char* val);
	void			setSidModel(const char* val);
	void			setSidFilters(const char* val);
	void			setViciiModel(const char* val);
	void			setCrtEmulation();
	void			setColorPalette(const char* val);
	void			setJoystickPort(const char* val);
	void			setDriveStatus(const char* val);
	void			setDriveEmulation(const char* val);
	void			setDriveSoundEmulation(const char* val);
	void			setDatasetteReset(const char* val);
	void			setCartridgeReset(const char* val);
	void			setMachineResetMode(const char* val);
	void			setMouseSampling(const char* val);
	int				getImageType(const char* image);
	void			getImageFileContents(int peripheral, const char* image, const char*** values, int* size);
	void			updatePalette();
	void			resumeSound();
	int				attachDriveImage(int drive, const char* image);
	int				attachTapeImage(const char* image);
	int				attachCartridgeImage(const char* image);
	void			detachDriveImage(int drive);
	int				detachTapeImage();
	void			detachCartridgeImage();
public:
					Controller();
					~Controller();
	void			init(View* view);
	void			resetComputer();
	int				loadFile(int load_type, const char* file_path, int index = 0);
	int				loadState(const char* file);
	int				saveState(const char* file_name);
	int				patchSaveState(patch_data_s* patch);
	int				getSaveStatePatch(patch_data_s* patch_info);
	int				getSaveStatePatchInfo(patch_data_s* patch_info);
	int				getViewport(ViewPort* vp, bool borders);
	void			syncSetting(int key);
	void			syncPeripherals();
	void			syncModelSettings();
	void			setModelProperty(int key, const char* value);
	int				attachImage(int device, const char* image);
	void			detachImage(int device);
	void			setTapeControl(int action);
	void			setCartControl(int action);
	void			setBorderVisibility(const char* val);
	void			setJoystickAutofireSpeed(const char* val);
};
#endif
#endif
