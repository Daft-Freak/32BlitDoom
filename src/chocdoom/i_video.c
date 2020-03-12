// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "i_scale.h"
#include "w_wad.h"
#include "z_zone.h"

#include "tables.h"
#include "deh_str.h"
#include "doomkeys.h"

#include <stdint.h>
#include <stdbool.h>

#include "engine/api.hpp"
#include "engine/engine.hpp"
#include "engine/input.hpp"

#define GFX_RGB565(r, g, b) ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
#define GFX_RGB565_R(rgb) ((rgb >> 11) << 3)
#define GFX_RGB565_G(rgb) (((rgb >> 5) & 0x3F) << 2)
#define GFX_RGB565_B(rgb) ((rgb & 0x1F) << 3)

// The screen buffer; this is modified to draw things to the screen

byte *I_VideoBuffer = NULL;
screen_mode_t *screen_mode = &mode_stretch_1x;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0;
int mouse_threshold = 10;

// Gamma correction level to use

int usegamma = 0;

int usemouse = 0;

// If true, keyboard mapping is ignored, like in Vanilla Doom.
// The sensible thing to do is to disable this if you have a non-US
// keyboard.

int vanilla_keyboard_mapping = true;


typedef struct
{
	byte r;
	byte g;
	byte b;
} col_t;

col_t *palette;

// Last touch state

// Last button state

static uint32_t last_button_state = 0;

// run state

static bool run;

void I_InitGraphics (void)
{
	I_VideoBuffer = (byte*)Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);

	screenvisible = true;

	if(screen_mode->InitMode)
		screen_mode->InitMode((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE));
}

void I_ShutdownGraphics (void)
{
	Z_Free (I_VideoBuffer);
}

void I_StartFrame (void)
{

}

static void CheckButton(uint32_t changed_buttons, blit::Button button, int key)
{
	event_t event;
	if(changed_buttons & button)
	{
		event.type = (blit::buttons & button) ? ev_keydown : ev_keyup;
		event.data1 = key;
		D_PostEvent(&event);
	}
}

void I_GetEvent (void)
{
	event_t event;

	uint32_t changed_buttons = blit::buttons ^ last_button_state;

	CheckButton(changed_buttons, blit::Button::DPAD_LEFT, KEY_LEFTARROW);
	CheckButton(changed_buttons, blit::Button::DPAD_RIGHT, KEY_RIGHTARROW);
	CheckButton(changed_buttons, blit::Button::DPAD_UP, KEY_UPARROW);
	CheckButton(changed_buttons, blit::Button::DPAD_DOWN, KEY_DOWNARROW);

	CheckButton(changed_buttons, blit::Button::A, KEY_ENTER);
	CheckButton(changed_buttons, blit::Button::A, KEY_FIRE); // bad idea?
	CheckButton(changed_buttons, blit::Button::B, KEY_USE);
	CheckButton(changed_buttons, blit::Button::X, KEY_RSHIFT);
	CheckButton(changed_buttons, blit::Button::Y, KEY_TAB);

	CheckButton(changed_buttons, blit::Button::HOME /*MENU?*/, KEY_ESCAPE);

	last_button_state = blit::buttons;

	event.type = ev_joystick;
	event.data1 = 0; //buttons?
	event.data2 = blit::joystick.x * (INT16_MAX - 1);
	event.data3 = blit::joystick.y * (INT16_MAX - 1);
	D_PostEvent(&event);
}

void I_StartTic (void)
{
	I_GetEvent();
}

void I_UpdateNoBlit (void)
{
}

void I_FinishUpdate (void)
{
	if(!palette)
		return;

	I_InitScale(I_VideoBuffer, blit::screen.ptr(0, 0), SCREENWIDTH);
	screen_mode->DrawScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
}

//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette (byte* pal)
{
	int i;
	col_t* c;

	palette = (col_t*)pal; // since we have the WAD stored in flash, the pointers passed here should stay valid

	blit::Pen cols[256];

	for (i = 0; i < 256; ++i)
	{
		cols[i].r = palette[i].r;
		cols[i].g = palette[i].g;
		cols[i].b = palette[i].b;
		cols[i].a = 0xFF;
	}

	blit::set_screen_palette(cols, 256);
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex (int r, int g, int b)
{
    int best, best_diff, diff;
    int i;
    col_t color;

    best = 0;
    best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
    	color.r = palette[i].r;
    	color.g = palette[i].g;
    	color.b = palette[i].b;

        diff = (r - color.r) * (r - color.r)
             + (g - color.g) * (g - color.g)
             + (b - color.b) * (b - color.b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

void I_SetWindowTitle (char *title)
{
}

void I_GraphicsCheckCommandLine (void)
{
}

void I_SetGrabMouseCallback (grabmouse_callback_t func)
{
}

void I_EnableLoadingDisk (void)
{
}

void I_BindVideoVariables (void)
{
}

void I_DisplayFPSDots (boolean dots_on)
{
}

void I_CheckIsScreensaver (void)
{
}
