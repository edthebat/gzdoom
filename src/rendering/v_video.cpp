/*
** Video basics and init code.
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


#include <stdio.h>

#include "i_system.h"
#include "c_cvars.h"
#include "x86.h"
#include "i_video.h"
#include "r_state.h"
#include "am_map.h"

#include "doomstat.h"

#include "c_console.h"
#include "hu_stuff.h"

#include "m_argv.h"

#include "v_video.h"
#include "v_text.h"
#include "sc_man.h"

#include "filesystem.h"

#include "c_dispatch.h"
#include "cmdlib.h"
#include "sbar.h"
#include "hardware.h"
#include "m_png.h"
#include "r_utility.h"
#include "swrenderer/r_renderer.h"
#include "menu/menu.h"
#include "vm.h"
#include "r_videoscale.h"
#include "i_time.h"
#include "version.h"
#include "g_levellocals.h"
#include "am_map.h"

EXTERN_CVAR(Int, menu_resolution_custom_width)
EXTERN_CVAR(Int, menu_resolution_custom_height)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_w, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_h, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)

CUSTOM_CVAR(Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
}

CUSTOM_CVAR(Int, vid_rendermode, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 4)
	{
		self = 4;
	}
	else if (self == 2 || self == 3)
	{
		self = self - 2; // softpoly to software
	}

	if (usergame)
	{
		// [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
	}
	screen->SetTextureFilterMode();

	// No further checks needed. All this changes now is which scene drawer the render backend calls.
}

CUSTOM_CVAR(Int, vid_preferbackend, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	// [SP] This may seem pointless - but I don't want to implement live switching just
	// yet - I'm pretty sure it's going to require a lot of reinits and destructions to
	// do it right without memory leaks

	switch(self)
	{
	case 2:
		Printf("Selecting SoftPoly backend...\n");
		break;
#ifdef HAVE_VULKAN
	case 1:
		Printf("Selecting Vulkan backend...\n");
		break;
#endif
	default:
		Printf("Selecting OpenGL backend...\n");
	}

	Printf("Changing the video backend requires a restart for " GAMENAME ".\n");
}

CVAR(Int, vid_renderer, 1, 0)	// for some stupid mods which threw caution out of the window...


EXTERN_CVAR(Bool, r_blendmethod)

int active_con_scale();

FRenderer *SWRenderer;

#define DBGBREAK assert(0)

class DDummyFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	DDummyFrameBuffer (int width, int height)
		: DFrameBuffer (0, 0)
	{
		SetVirtualSize(width, height);
	}
	// These methods should never be called.
	void Update() { DBGBREAK; }
	bool IsFullscreen() { DBGBREAK; return 0; }
	int GetClientWidth() { DBGBREAK; return 0; }
	int GetClientHeight() { DBGBREAK; return 0; }
	void InitializeState() override {}

	float Gamma;
};

int DisplayWidth, DisplayHeight;

FFont *SmallFont, *SmallFont2, *BigFont, *BigUpper, *ConFont, *IntermissionFont, *NewConsoleFont, *NewSmallFont, *CurrentConsoleFont, *OriginalSmallFont, *AlternativeSmallFont, *OriginalBigFont;


// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
DFrameBuffer *screen;

CVAR (Int, vid_defwidth, 640, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defheight, 480, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, ticker, false, 0)

CUSTOM_CVAR (Bool, vid_vsync, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetVSync (*self);
	}
}

// [RH] Set true when vid_setmode command has been executed
bool	setmodeneeded = false;

//==========================================================================
//
// DCanvas Constructor
//
//==========================================================================

DCanvas::DCanvas (int _width, int _height, bool _bgra)
{
	// Init member vars
	Width = _width;
	Height = _height;
	Bgra = _bgra;
	Resize(_width, _height);
}

//==========================================================================
//
// DCanvas Destructor
//
//==========================================================================

DCanvas::~DCanvas ()
{
}

//==========================================================================
//
//
//
//==========================================================================

void DCanvas::Resize(int width, int height, bool optimizepitch)
{
	Width = width;
	Height = height;
	
	// Making the pitch a power of 2 is very bad for performance
	// Try to maximize the number of cache lines that can be filled
	// for each column drawing operation by making the pitch slightly
	// longer than the width. The values used here are all based on
	// empirical evidence.
	
	if (width <= 640 || !optimizepitch)
	{
		// For low resolutions, just keep the pitch the same as the width.
		// Some speedup can be seen using the technique below, but the speedup
		// is so marginal that I don't consider it worthwhile.
		Pitch = width;
	}
	else
	{
		// If we couldn't figure out the CPU's L1 cache line size, assume
		// it's 32 bytes wide.
		if (CPU.DataL1LineSize == 0)
		{
			CPU.DataL1LineSize = 32;
		}
		// The Athlon and P3 have very different caches, apparently.
		// I am going to generalize the Athlon's performance to all AMD
		// processors and the P3's to all non-AMD processors. I don't know
		// how smart that is, but I don't have a vast plethora of
		// processors to test with.
		if (CPU.bIsAMD)
		{
			Pitch = width + CPU.DataL1LineSize;
		}
		else
		{
			Pitch = width + MAX(0, CPU.DataL1LineSize - 8);
		}
	}
	int bytes_per_pixel = Bgra ? 4 : 1;
	Pixels.Resize(Pitch * height * bytes_per_pixel);
	memset (Pixels.Data(), 0, Pixels.Size());
}


CCMD(clean)
{
	Printf ("CleanXfac: %d\nCleanYfac: %d\n", CleanXfac, CleanYfac);
}


void V_UpdateModeSize (int width, int height)
{	
	// This calculates the menu scale.
	// The optimal scale will always be to fit a virtual 640 pixel wide display onto the screen.
	// Exceptions are made for a few ranges where the available virtual width is > 480.

	// This reference size is being used so that on 800x450 (small 16:9) a scale of 2 gets used.

	CleanXfac = std::max(std::min(screen->GetWidth() / 400, screen->GetHeight() / 240), 1);
	if (CleanXfac >= 4) CleanXfac--;	// Otherwise we do not have enough space for the episode/skill menus in some languages.
	CleanYfac = CleanXfac;
	CleanWidth = screen->GetWidth() / CleanXfac;
	CleanHeight = screen->GetHeight() / CleanYfac;

	int w = screen->GetWidth();
	int factor;
	if (w < 640) factor = 1;
	else if (w >= 1024 && w < 1280) factor = 2;
	else if (w >= 1600 && w < 1920) factor = 3; 
	else  factor = w / 640;

	if (w < 1360) factor = 1;
	else if (w < 1920) factor = 2;
	else factor = int(factor * 0.7);

	CleanYfac_1 = CleanXfac_1 = factor;// MAX(1, int(factor * 0.7));
	CleanWidth_1 = width / CleanXfac_1;
	CleanHeight_1 = height / CleanYfac_1;

	DisplayWidth = width;
	DisplayHeight = height;

	R_OldBlend = ~0;
}

void V_OutputResized (int width, int height)
{
	V_UpdateModeSize(width, height);
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	C_NewModeAdjust();
	// Reload crosshair if transitioned to a different size
	ST_LoadCrosshair(true);
	if (primaryLevel && primaryLevel->automap)
		primaryLevel->automap->NewResolution();
}

void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *_cx1, int *_cx2)
{
	if (designheight < 240 && realheight >= 480) designheight = 240;
	*cleanx = *cleany = std::min(realwidth / designwidth, realheight / designheight);
}

bool IVideo::SetResolution ()
{
	DFrameBuffer *buff = CreateFrameBuffer();

	if (buff == NULL)	// this cannot really happen
	{
		return false;
	}

	screen = buff;
	screen->InitializeState();
	screen->SetGamma();

	V_UpdateModeSize(screen->GetWidth(), screen->GetHeight());

	return true;
}

//
// V_Init
//

void V_InitScreenSize ()
{ 
	const char *i;
	int width, height, bits;
	
	width = height = bits = 0;
	
	if ( (i = Args->CheckValue ("-width")) )
		width = atoi (i);
	
	if ( (i = Args->CheckValue ("-height")) )
		height = atoi (i);
	
	if (width == 0)
	{
		if (height == 0)
		{
			width = vid_defwidth;
			height = vid_defheight;
		}
		else
		{
			width = (height * 8) / 6;
		}
	}
	else if (height == 0)
	{
		height = (width * 6) / 8;
	}
	// Remember the passed arguments for the next time the game starts up windowed.
	vid_defwidth = width;
	vid_defheight = height;
}

void V_InitScreen()
{
	screen = new DDummyFrameBuffer (vid_defwidth, vid_defheight);
}

void V_Init2()
{
	float gamma = static_cast<DDummyFrameBuffer *>(screen)->Gamma;

	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}

	UCVarValue val;

	val.Bool = !!Args->CheckParm("-devparm");
	ticker.SetGenericRepDefault(val, CVAR_Bool);


	I_InitGraphics();

	Video->SetResolution();	// this only fails via exceptions.
	Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	// init these for the scaling menu
	menu_resolution_custom_width = SCREENWIDTH;
	menu_resolution_custom_height = SCREENHEIGHT;

	screen->SetVSync(vid_vsync);
	screen->SetGamma ();
	FBaseCVar::ResetColors ();
	C_NewModeAdjust();
	setsizeneeded = true;
}

CUSTOM_CVAR (Int, vid_aspect, 0, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
}

// Helper for ActiveRatio and CheckRatio. Returns the forced ratio type, or -1 if none.
int ActiveFakeRatio(int width, int height)
{
	int fakeratio = -1;
	if ((vid_aspect >= 1) && (vid_aspect <= 6))
	{
		// [SP] User wants to force aspect ratio; let them.
		fakeratio = int(vid_aspect);
		if (fakeratio == 3)
		{
			fakeratio = 0;
		}
		else if (fakeratio == 5)
		{
			fakeratio = 3;
		}
	}
	return fakeratio;
}

// Active screen ratio based on cvars and size
float ActiveRatio(int width, int height, float *trueratio)
{
	static float forcedRatioTypes[] =
	{
		4 / 3.0f,
		16 / 9.0f,
		16 / 10.0f,
		17 / 10.0f,
		5 / 4.0f,
		17 / 10.0f,
		21 / 9.0f
	};

	float ratio = width / (float)height;
	int fakeratio = ActiveFakeRatio(width, height);

	if (trueratio)
		*trueratio = ratio;
	return (fakeratio != -1) ? forcedRatioTypes[fakeratio] : (ratio / ViewportPixelAspect());
}

DEFINE_ACTION_FUNCTION(_Screen, GetAspectRatio)
{
	ACTION_RETURN_FLOAT(ActiveRatio(screen->GetWidth(), screen->GetHeight(), nullptr));
}

int AspectBaseWidth(float aspect)
{
	return (int)round(240.0f * aspect * 3.0f);
}

int AspectBaseHeight(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(200.0f * (320.0f / (AspectBaseWidth(aspect) / 3.0f)) * 3.0f);
	else
		return (int)round((200.0f * (4.0f / 3.0f)) / aspect * 3.0f);
}

double AspectPspriteOffset(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return 0.0;
	else
		return ((4.0 / 3.0) / aspect - 1.0) * 97.5;
}

int AspectMultiplier(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(320.0f / (AspectBaseWidth(aspect) / 3.0f) * 48.0f);
	else
		return (int)round(200.0f / (AspectBaseHeight(aspect) / 3.0f) * 48.0f);
}

bool AspectTallerThanWide(float aspect)
{
	return aspect < 1.333f;
}

CCMD(vid_setsize)
{
	if (argv.argc() < 3)
	{
		Printf("Usage: vid_setsize width height\n");
	}
	else
	{
		screen->SetWindowSize((int)strtol(argv[1], nullptr, 0), (int)strtol(argv[2], nullptr, 0));
		V_OutputResized(screen->GetClientWidth(), screen->GetClientHeight());
	}
}


void IVideo::DumpAdapters ()
{
	Printf("Multi-monitor support unavailable.\n");
}

CUSTOM_CVAR_NAMED(Bool, vid_fullscreen, fullscreen, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	setmodeneeded = true;
}

CUSTOM_CVAR(Bool, vid_hdr, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CCMD(vid_listadapters)
{
	if (Video != NULL)
		Video->DumpAdapters();
}

bool vid_hdr_active = false;

DEFINE_GLOBAL(SmallFont)
DEFINE_GLOBAL(SmallFont2)
DEFINE_GLOBAL(BigFont)
DEFINE_GLOBAL(ConFont)
DEFINE_GLOBAL(NewConsoleFont)
DEFINE_GLOBAL(NewSmallFont)
DEFINE_GLOBAL(AlternativeSmallFont)
DEFINE_GLOBAL(OriginalSmallFont)
DEFINE_GLOBAL(OriginalBigFont)
DEFINE_GLOBAL(IntermissionFont)
DEFINE_GLOBAL(CleanXfac)
DEFINE_GLOBAL(CleanYfac)
DEFINE_GLOBAL(CleanWidth)
DEFINE_GLOBAL(CleanHeight)
DEFINE_GLOBAL(CleanXfac_1)
DEFINE_GLOBAL(CleanYfac_1)
DEFINE_GLOBAL(CleanWidth_1)
DEFINE_GLOBAL(CleanHeight_1)
DEFINE_GLOBAL(generic_ui)
