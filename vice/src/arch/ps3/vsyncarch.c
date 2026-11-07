/*
 * vsyncarch.c - End-of-frame handling
 *
 * Written by
 *  Amnon-Dan Meir <ammeir71@yahoo.com>
 *
 * Based on code by
 *  Dag Lem <resid@nimrod.no>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"
#include "kbdbuf.h"
#include "ui.h"
#include "vsyncapi.h"
#include "videoarch.h"
#include "controller.h"
#include "debug_ps3.h"

#include <time.h>
#include <stdio.h>
#include <ps3_gfx.h>
#include <ps3_rtc.h> 
#include <ps3_thread.h> 


/* Number of timer units per second. */
unsigned long vsyncarch_frequency(void)
{
    /* Microseconds resolution. */
    return sceRtcGetTickResolution();
}

/* Get time in timer units. */
unsigned long vsyncarch_gettime(void)
{
	SceRtcTick ticks;
	sceRtcGetCurrentTick(&ticks);	

    return ticks.tick;
}

void vsyncarch_init(void)
{
}

/* Display speed (percentage) and frame rate (frames per second). */
void vsyncarch_display_speed(double speed, double frame_rate, int warp_enabled)
{
    ui_display_speed((float)speed, (float)frame_rate, warp_enabled);
}

/* Sleep a number of timer units. */
void vsyncarch_sleep(unsigned long delay)
{
	static int freq_cached = 0;
	static unsigned long freq = 79800000;
	unsigned long long us;

	if (!freq_cached) {
		unsigned long res = sceRtcGetTickResolution();
		if (res > 0) {
			freq = res;
		}
		freq_cached = 1;
	}

	us = ((unsigned long long)delay * 1000000ULL) / (unsigned long long)freq;
	if (us == 0) {
		return;
	}

	sceKernelDelayThread((unsigned int)us);
}

void vsyncarch_presync(void)
{
	PS3_ScanControls();
    kbdbuf_flush();
}

void vsyncarch_postsync(void)
{
}
