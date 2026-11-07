#include <SDL2/SDL.h>
#include <sys/process.h>
#include <stdlib.h>

#include "view.h"
#include "controller.h"
#include "c64_gui_ps3.h"
#include "ps3_gfx.h"

extern "C" {
#include "main.h"
#include "machine.h"
#include "sound.h"
#include "ps3_paths.h"
#include <sys/stat.h>
#include <lv2/process.h>
}

SYS_PROCESS_PARAM(1001, 0x200000);

int main(int argc, char *argv[])
{
	ps3_paths_init(argc, argv);

	mkdir("/dev_hdd0/vice", 0777);
	mkdir("/dev_hdd0/vice/games", 0777);
	mkdir("/dev_hdd0/vice/roms", 0777);
	mkdir("/dev_hdd0/vice/saves", 0777);
	mkdir("/dev_hdd0/vice/covers", 0777);
	mkdir("/dev_hdd0/vice/conf", 0777);

	mkdir("data", 0777);
	mkdir("data/games", 0777);
	mkdir("data/saves", 0777);
	mkdir("data/vice", 0777);
	mkdir("data/tmp", 0777);
	mkdir("data/covers", 0777);

	FILE *log = fopen("/dev_hdd0/vice/vice_log.txt", "w");
	if (log) {
		fprintf(log, "VICE4PS3 startup: argc=%d base=%s\n", argc, ps3_base_dir());
		fclose(log);
	}

	FILE *sdl_log = fopen("/dev_hdd0/vice/sdl_log.txt", "w");
	if (sdl_log) {
		fclose(sdl_log);
	}

	ps3_gfx_log_write("boot: main");
	ps3_gfx_boot_test();

	View		view;
	Controller	controller; 

	controller.init(&view);
	ps3_gfx_log_write("boot: view init");
	view.init(&controller);
	ps3_gfx_log_write("boot: view ready");

	c64_gui_draw_progress("Starting VICE", "Initializing Commodore 64 emulation...", 0.9f, NULL);
	ps3_gfx_log_write("boot: main program");
	{
		int rc = main_program(argc, argv);
		char message[64];

		snprintf(message, sizeof(message), "boot: main program returned rc=%d", rc);
		ps3_gfx_log_write(message);
	}

	c64_gui_draw_progress("Startup Error", "VICE stopped during initialization.", 0.0f, NULL);
	SDL_Delay(5000);

	sysProcessExit(0);
	return 0;
}

static bool s_shutdown_done = false;

static void shutdown_system(void)
{
    if (s_shutdown_done) {
        return;
    }
    s_shutdown_done = true;

    // Stop the SDL callback before tearing down VICE and the renderer.
    sound_close();
    machine_shutdown();
    c64_gui_shutdown();
    SDL_Quit();
}

void main_exit()
{
    shutdown_system();
}

extern "C" void PS3_ShutdownAndExit(void)
{
    shutdown_system();
    sysProcessExit(0);
    exit(0);
}

extern "C" void PS3_ExitProcessNow(void)
{
    /* XMB quit: the firmware is already terminating the process. Do not run
     * SDL_Quit/machine teardown from the render loop; sysProcessExit is the
     * supported way to end it and cannot leave a half-freed SDL state. */
    sysProcessExit(0);
    exit(0);
}
