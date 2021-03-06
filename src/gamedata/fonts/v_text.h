/*
** v_text.h
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

#ifndef __V_TEXT_H__
#define __V_TEXT_H__

#include "v_font.h"
#include "printf.h"

struct FBrokenLines
{
	unsigned	Width;
	FString		Text;
};

#define TEXTCOLOR_ESCAPE		'\034'
#define TEXTCOLOR_ESCAPESTR		"\034"

#define TEXTCOLOR_NORMAL		"\034-"
#define TEXTCOLOR_BOLD			"\034+"

#define TEXTCOLOR_CHAT			"\034*"
#define TEXTCOLOR_TEAMCHAT		"\034!"

TArray<FBrokenLines> V_BreakLines (FFont *font, int maxwidth, const uint8_t *str, bool preservecolor = false);
inline TArray<FBrokenLines> V_BreakLines (FFont *font, int maxwidth, const char *str, bool preservecolor = false)
 { return V_BreakLines (font, maxwidth, (const uint8_t *)str, preservecolor); }
inline TArray<FBrokenLines> V_BreakLines (FFont *font, int maxwidth, const FString &str, bool preservecolor = false)
 { return V_BreakLines (font, maxwidth, (const uint8_t *)str.GetChars(), preservecolor); }

#endif //__V_TEXT_H__
