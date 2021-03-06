#pragma once

#include <stdint.h>
#include "palentry.h"

struct FScriptPosition;
class FScanner;

int BestColor(const uint32_t* pal, int r, int g, int b, int first = 1, int num = 255);
int PTM_BestColor(const uint32_t* pal_in, int r, int g, int b, bool reverselookup, float powtable, int first = 1, int num = 255);
void DoBlending(const PalEntry* from, PalEntry* to, int count, int r, int g, int b, int a);

// Given an array of colors, fills in remap with values to remap the
// passed array of colors to BaseColors. Used for loading palette downconversions of PNGs.
void MakeRemap(uint32_t* BaseColors, const uint32_t* colors, uint8_t* remap, const uint8_t* useful, int numcolors);
void MakeGoodRemap(uint32_t* BaseColors, uint8_t* Remap);

// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString(const uint32_t* palette, const char* colorstring, FScriptPosition* sc = nullptr);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
FString V_GetColorStringByName(const char* name, FScriptPosition* sc = nullptr);

// Tries to get color by name, then by string
int V_GetColor(const uint32_t* palette, const char* str, FScriptPosition* sc = nullptr);
int V_GetColor(const uint32_t* palette, FScanner& sc);
