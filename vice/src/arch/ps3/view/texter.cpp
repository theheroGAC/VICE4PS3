
/* texter.cpp: Little wrapper to simplify text drawing.

   Copyright (C) 2019-2020 Amnon-Dan Meir.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Author contact information: 
     Email: ammeir71@yahoo.com
*/

#include <cstddef>
#include <ps3_gfx.h>

static ps3_gfx_font*	gs_font;
static ps3_gfx_pgf*	gs_default_font;
static bool			gs_initiated = false;

static void txtr_init()
{
	// Call this function after ps3_gfx_init().
	gs_font = NULL;
	gs_default_font = ps3_gfx_load_default_pgf();
	gs_initiated = true;
}

void txtr_free()
{
	if (gs_font)
		ps3_gfx_free_font(gs_font);
	if (gs_default_font)
		ps3_gfx_free_pgf(gs_default_font);
}

void txtr_load_font_file(const char* path)
{
	if (gs_font)
		ps3_gfx_free_font(gs_font);

	gs_font = ps3_gfx_load_font_file(path);
}

void txtr_load_font_mem(const unsigned char* buf, int size)
{
	if (gs_font)
		ps3_gfx_free_font(gs_font);

	gs_font = ps3_gfx_load_font_mem(buf, size);
}

void txtr_draw_text(int x, int y, int color, const char* text, float size)
{
	if (!gs_initiated)
		txtr_init();

	if (gs_font){
		if (size == 1) size = 20;
		ps3_gfx_font_draw_text(gs_font, x, y, color, size, text); 
	}
	else if (gs_default_font)
		ps3_gfx_pgf_draw_text(gs_default_font, x, y, color, size, text);
}

int txtr_get_text_width(const char* text, float font_size)
{
	if (!gs_initiated)
		txtr_init();

	if (gs_font)
		return ps3_gfx_font_text_width(gs_font, font_size, text);
	else if (gs_default_font)
		return ps3_gfx_pgf_text_width(gs_default_font, 1.0f, text);
	else
		return 0;
}

int txtr_get_text_height(const char* text, float font_size)
{
	if (!gs_initiated)
		txtr_init();

	if (gs_font)
		return ps3_gfx_font_text_height(gs_font, font_size, text);
	else if (gs_default_font)
		return ps3_gfx_pgf_text_height(gs_default_font, 1.0f, text);
	else
		return 0;
}
