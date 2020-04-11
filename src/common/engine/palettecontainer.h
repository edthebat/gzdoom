#pragma once

#include <stdint.h>
#include "memarena.h"
#include "palentry.h"

struct FRemapTable
{
	FRemapTable(int count = 256) { NumEntries = count; }
	FRemapTable(const FRemapTable& o) = default;

	bool operator==(const FRemapTable& o);
	void MakeIdentity();
	bool IsIdentity() const;
	bool AddIndexRange(int start, int end, int pal1, int pal2);
	bool AddColorRange(int start, int end, int r1, int g1, int b1, int r2, int g2, int b2);
	bool AddDesaturation(int start, int end, double r1, double g1, double b1, double r2, double g2, double b2);
	bool AddColourisation(int start, int end, int r, int g, int b);
	bool AddTint(int start, int end, int r, int g, int b, int amount);
	bool AddToTranslation(const char* range);
	bool AddColors(int start, int count, const uint8_t*);

	uint8_t Remap[256];				// For the software renderer
	PalEntry Palette[256];			// The ideal palette this maps to
	int crc32;
	int Index;
	int NumEntries;				// # of elements in this table (usually 256)
	bool Inactive = false;				// This table is inactive and should be treated as if it was passed as NULL

private:
};

// A class that initializes unusued pointers to NULL. This is used so that when
// the TAutoGrowArray below is expanded, the new elements will be NULLed.
class FRemapTablePtr
{
public:
	FRemapTablePtr() throw() : Ptr(0) {}
	FRemapTablePtr(FRemapTable* p) throw() : Ptr(p) {}
	FRemapTablePtr(const FRemapTablePtr& p) throw() : Ptr(p.Ptr) {}
	operator FRemapTable* () const throw() { return Ptr; }
	FRemapTablePtr& operator= (FRemapTable* p) throw() { Ptr = p; return *this; }
	FRemapTablePtr& operator= (FRemapTablePtr& p) throw() { Ptr = p.Ptr; return *this; }
	FRemapTable& operator*() const throw() { return *Ptr; }
	FRemapTable* operator->() const throw() { return Ptr; }
private:
	FRemapTable* Ptr = nullptr;
};


#define TRANSLATION_SHIFT 16
#define TRANSLATION_MASK ((1<<TRANSLATION_SHIFT)-1)
#define TRANSLATIONTYPE_MASK (255<<TRANSLATION_SHIFT)

inline uint32_t TRANSLATION(uint8_t a, uint32_t b)
{
	return (a << TRANSLATION_SHIFT) | b;
}
inline int GetTranslationType(uint32_t trans)
{
	return (trans & TRANSLATIONTYPE_MASK) >> TRANSLATION_SHIFT;
}
inline int GetTranslationIndex(uint32_t trans)
{
	return (trans & TRANSLATION_MASK);
}

class PaletteContainer
{
	FMemArena remapArena;
	TArray<FRemapTable*> uniqueRemaps;
	TArray<TAutoGrowArray<FRemapTablePtr, FRemapTable*>> TranslationTables;
public:
	void Init(int numslots);	// This cannot be a constructor!!!
	void Clear();
	FRemapTable* AddRemap(FRemapTable* remap);
	void UpdateTranslation(int trans, FRemapTable* remap);
	int AddTranslation(int slot, FRemapTable* remap, int count = 1);
	void CopyTranslation(int dest, int src);
	int StoreTranslation(int slot, FRemapTable* remap);
	FRemapTable* TranslationToTable(int translation);

	void PushIdentityTable(int slot)
	{
		AddTranslation(slot, nullptr);
	}

	FRemapTable* GetTranslation(int slot, int index)
	{
		return TranslationTables[slot].GetVal(index);
	}

	void ClearTranslationSlot(int slot)
	{
		TranslationTables[slot].Clear();
	}

	unsigned NumTranslations(int slot) const
	{
		return TranslationTables[slot].Size();
	}

};

extern PaletteContainer palMgr;

