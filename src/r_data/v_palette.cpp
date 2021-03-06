/*
** v_palette.cpp
** Automatic colormap generation for "colored lights", etc.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "g_level.h"

#ifdef _WIN32
#include <io.h>
#else
#define O_BINARY 0
#endif

#include "templates.h"
#include "v_video.h"
#include "filesystem.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "x86.h"
#include "g_levellocals.h"
#include "m_png.h"

uint32_t Col2RGB8[65][256];
uint32_t *Col2RGB8_LessPrecision[65];
uint32_t Col2RGB8_Inverse[65][256];
uint32_t Col2RGB8_2[63][256]; // this array's second dimension is called up by pointer as Col2RGB8_LessPrecision[] elsewhere.
ColorTable32k RGB32k;
ColorTable256k RGB256k;

FPalette GPalette;
FColorMatcher ColorMatcher;

/* Current color blending values */
int		BlendR, BlendG, BlendB, BlendA;

/**************************/
/* Gamma correction stuff */
/**************************/

uint8_t newgamma[256];
CUSTOM_CVAR (Float, Gamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self == 0.f)
	{ // Gamma values of 0 are illegal.
		self = 1.f;
		return;
	}

	if (screen != NULL)
	{
		screen->SetGamma ();
	}
}

CCMD (bumpgamma)
{
	// [RH] Gamma correction tables are now generated on the fly for *any* gamma level
	// Q: What are reasonable limits to use here?

	float newgamma = Gamma + 0.1f;

	if (newgamma > 3.0)
		newgamma = 1.0;

	Gamma = newgamma;
	Printf ("Gamma correction level %g\n", *Gamma);
}






FPalette::FPalette ()
{
}

FPalette::FPalette (const uint8_t *colors)
{
	SetPalette (colors);
}

void FPalette::SetPalette (const uint8_t *colors)
{
	for (int i = 0; i < 256; i++, colors += 3)
	{
		BaseColors[i] = PalEntry (colors[0], colors[1], colors[2]);
		Remap[i] = i;
	}

	// Find white and black from the original palette so that they can be
	// used to make an educated guess of the translucency % for a BOOM
	// translucency map.
	WhiteIndex = BestColor ((uint32_t *)BaseColors, 255, 255, 255, 0, 255);
	BlackIndex = BestColor ((uint32_t *)BaseColors, 0, 0, 0, 0, 255);
}

int ReadPalette(int lumpnum, uint8_t *buffer)
{
	if (lumpnum < 0)
	{
		return 0;
	}
	FileData lump = fileSystem.ReadFile(lumpnum);
	uint8_t *lumpmem = (uint8_t*)lump.GetMem();
	memset(buffer, 0, 768);

	FileReader fr;
	fr.OpenMemory(lumpmem, lump.GetSize());
	auto png = M_VerifyPNG(fr);
	if (png)
	{
		uint32_t id, len;
		fr.Seek(33, FileReader::SeekSet);
		fr.Read(&len, 4);
		fr.Read(&id, 4);
		bool succeeded = false;
		while (id != MAKE_ID('I', 'D', 'A', 'T') && id != MAKE_ID('I', 'E', 'N', 'D'))
		{
			len = BigLong((unsigned int)len);
			if (id != MAKE_ID('P', 'L', 'T', 'E'))
				fr.Seek(len, FileReader::SeekCur);
			else
			{
				int PaletteSize = MIN<int>(len, 768);
				fr.Read(buffer, PaletteSize);
				return PaletteSize / 3;
			}
			fr.Seek(4, FileReader::SeekCur);	// Skip CRC
			fr.Read(&len, 4);
			id = MAKE_ID('I', 'E', 'N', 'D');
			fr.Read(&id, 4);
		}
		I_Error("%s contains no palette", fileSystem.GetFileFullName(lumpnum));
	}
	if (memcmp(lumpmem, "JASC-PAL", 8) == 0)
	{
		FScanner sc;
		
		sc.OpenMem(fileSystem.GetFileFullName(lumpnum), (char*)lumpmem, int(lump.GetSize()));
		sc.MustGetString();
		sc.MustGetNumber();	// version - ignore
		sc.MustGetNumber();	
		int colors = MIN(256, sc.Number) * 3;
		for (int i = 0; i < colors; i++)
		{
			sc.MustGetNumber();
			if (sc.Number < 0 || sc.Number > 255)
			{
				sc.ScriptError("Color %d value out of range.", sc.Number);
			}
			buffer[i] = sc.Number;
		}
		return colors / 3;
	}
	else
	{
		memcpy(buffer, lumpmem, MIN<size_t>(768, lump.GetSize()));
		return 256;
	}
}

//==========================================================================
//
// BuildTransTable
//
// Build the tables necessary for blending
//
//==========================================================================

static void BuildTransTable (const PalEntry *palette)
{
	int r, g, b;
	
	// create the RGB555 lookup table
	for (r = 0; r < 32; r++)
		for (g = 0; g < 32; g++)
			for (b = 0; b < 32; b++)
				RGB32k.RGB[r][g][b] = ColorMatcher.Pick ((r<<3)|(r>>2), (g<<3)|(g>>2), (b<<3)|(b>>2));
	// create the RGB666 lookup table
	for (r = 0; r < 64; r++)
		for (g = 0; g < 64; g++)
			for (b = 0; b < 64; b++)
				RGB256k.RGB[r][g][b] = ColorMatcher.Pick ((r<<2)|(r>>4), (g<<2)|(g>>4), (b<<2)|(b>>4));
	
	int x, y;
	
	// create the swizzled palette
	for (x = 0; x < 65; x++)
		for (y = 0; y < 256; y++)
			Col2RGB8[x][y] = (((palette[y].r*x)>>4)<<20) |
			((palette[y].g*x)>>4) |
			(((palette[y].b*x)>>4)<<10);
	
	// create the swizzled palette with the lsb of red and blue forced to 0
	// (for green, a 1 is okay since it never gets added into)
	for (x = 1; x < 64; x++)
	{
		Col2RGB8_LessPrecision[x] = Col2RGB8_2[x-1];
		for (y = 0; y < 256; y++)
		{
			Col2RGB8_2[x-1][y] = Col2RGB8[x][y] & 0x3feffbff;
		}
	}
	Col2RGB8_LessPrecision[0] = Col2RGB8[0];
	Col2RGB8_LessPrecision[64] = Col2RGB8[64];
	
	// create the inverse swizzled palette
	for (x = 0; x < 65; x++)
		for (y = 0; y < 256; y++)
		{
			Col2RGB8_Inverse[x][y] = (((((255-palette[y].r)*x)>>4)<<20) |
									  (((255-palette[y].g)*x)>>4) |
									  ((((255-palette[y].b)*x)>>4)<<10)) & 0x3feffbff;
		}
}


void InitPalette ()
{
	uint8_t pal[768];
	
	ReadPalette(fileSystem.GetNumForName("PLAYPAL"), pal);

	GPalette.SetPalette (pal);
	MakeGoodRemap ((uint32_t*)GPalette.BaseColors, GPalette.Remap);
	ColorMatcher.SetPalette ((uint32_t *)GPalette.BaseColors);

	if (GPalette.Remap[0] == 0)
	{ // No duplicates, so settle for something close to color 0
		GPalette.Remap[0] = BestColor ((uint32_t *)GPalette.BaseColors,
			GPalette.BaseColors[0].r, GPalette.BaseColors[0].g, GPalette.BaseColors[0].b, 1, 255);
	}

	// Colormaps have to be initialized before actors are loaded,
	// otherwise Powerup.Colormap will not work.
	R_InitColormaps ();
	BuildTransTable (GPalette.BaseColors);

}

CCMD (testblend)
{
	FString colorstring;
	int color;
	float amt;

	if (argv.argc() < 3)
	{
		Printf ("testblend <color> <amount>\n");
	}
	else
	{
		if ( !(colorstring = V_GetColorStringByName (argv[1])).IsEmpty() )
		{
			color = V_GetColorFromString (NULL, colorstring);
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
		}
		amt = (float)atof (argv[2]);
		if (amt > 1.0f)
			amt = 1.0f;
		else if (amt < 0.0f)
			amt = 0.0f;
		BaseBlendR = RPART(color);
		BaseBlendG = GPART(color);
		BaseBlendB = BPART(color);
		BaseBlendA = amt;
	}
}

