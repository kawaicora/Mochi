#include "FactoryHook.h"
#include <Helpers/Macro.h>
Event<FactoryClass*> FactoryHook::ProgressUpdateEvent;
Event<FactoryClass*> FactoryHook::FactoryClassCreateEvent;


DEFINE_HOOK(0x4C9B20, FactoryClass_ProgressUpdate, 5) {
	GET(FactoryClass*, pFactory, ECX);
	FactoryHook::ProgressUpdateEvent.Invoke(pFactory);
	return 0;
}

DEFINE_HOOK(0x4C9DF6, FactoryClass_CTOR, 6) {
	GET(FactoryClass*, pFactory, ESI);
	FactoryHook::FactoryClassCreateEvent.Invoke(pFactory);
	return 0;
}

//--------------------------------------------------------------------------------
// [ID] ExitCoord parser
//
// Accepts 1 to 3 comma separated integers, negative values included
// ("0,-256,0" -> {0, -256, 0}). Spaces and tabs around the numbers are skipped.
// Returns false when the string holds no number at all.
//--------------------------------------------------------------------------------
bool FactoryHook::ParseExitCoord(const char* pRaw, CoordStruct& coords)
{
	coords = CoordStruct { 0, 0, 0 };

	if (!pRaw)
	{
		return false;
	}

	int* const pFields[3] = { &coords.X, &coords.Y, &coords.Z };
	int count = 0;

	const char* p = pRaw;

	while (true)
	{
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		{
			++p;
		}

		if (!*p)
		{
			break;
		}

		// Empty component, e.g. "512,,0": keep the previous value (0).
		if (*p == ',')
		{
			++p;

			if (count < 3)
			{
				++count;
			}

			continue;
		}

		bool negative = false;

		if (*p == '-' || *p == '+')
		{
			negative = (*p == '-');
			++p;
		}

		if (*p < '0' || *p > '9')
		{
			break;
		}

		int value = 0;

		while (*p >= '0' && *p <= '9')
		{
			value = value * 10 + (*p - '0');
			++p;
		}

		if (count >= 3)
		{
			break;
		}

		*pFields[count] = negative ? -value : value;
		++count;

		while (*p == ' ' || *p == '\t')
		{
			++p;
		}

		if (*p != ',')
		{
			break;
		}

		++p;
	}

	return count > 0;
}

//--------------------------------------------------------------------------------
// ExitCoord for unit factories that are not barracks
//
// Vanilla applies ExitCoord only inside the three barracks branches
// (0x444BAA / 0x444C02 / 0x444C54 of BuildingClass::KickOutUnit). Every other
// factory ignores it, so a custom building that is CloningFacility + WeaponsFactory
// always spawns on the FoundationOutside ring cell, no matter what ExitCoord says.
//
// Rules and map rules are both read through CCINIClass::INI_Rules, so a single
// lookup covers both.
//--------------------------------------------------------------------------------
bool FactoryHook::ApplyExitCoord(BuildingClass* pBuilding, CoordStruct& coords)
{
	if (!pBuilding)
	{
		return false;
	}

	BuildingTypeClass* pType = pBuilding->Type;

	if (!pType)
	{
		return false;
	}

	// Barracks already get ExitCoord from the engine's own branches: adding it
	// here would apply it twice.
	if (pType->GDIBarracks || pType->NODBarracks || pType->YuriBarracks)
	{
		return false;
	}

	const char* pID = pType->get_ID();

	if (!pID || !*pID)
	{
		return false;
	}

	INIClass* pINI = CCINIClass::INI_Rules;

	if (!pINI)
	{
		return false;
	}

	if (!pINI->ReadBool(pID, "CloningFacility", false))
	{
		return false;
	}

	if (!pINI->ReadBool(pID, "WeaponsFactory", false))
	{
		return false;
	}

	// Key absent -> leave the engine's spawn position untouched.
	char buffer[0x40] = {};
	pINI->ReadString(pID, "ExitCoord", "", buffer, sizeof(buffer));

	if (!buffer[0])
	{
		return false;
	}

	CoordStruct offset { 0, 0, 0 };

	if (!ParseExitCoord(buffer, offset))
	{
		return false;
	}

	coords.X += offset.X;
	coords.Y += offset.Y;
	coords.Z += offset.Z;

	return true;
}

//--------------------------------------------------------------------------------
// BuildingClass::KickOutUnit, spawn coordinate tail
//
// Common tail for every branch: the spawn CoordStruct is final here.
//   [esp+0x38] = X, [esp+0x3C] = Y, [esp+0x40] = Z
// Same offsets Phobos uses at 0x444B83 (which then skips to this address), so the
// frame is identical for both.
//
// Facing is already computed before this point (it reads GetExitCoords), so the
// orientation of the produced unit stays the vanilla one.
//
// Saved bytes: mov eax,[0xa8e7ac]  (A1 AC E7 A8 00) = 5, one whole instruction,
// no relative jump inside; returning 0 replays it and resumes at 0x444C81.
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x444C7C, BuildingClass_KickOutUnit_ApplyExitCoord, 5)
{
	GET(BuildingClass*, pBuilding, ESI);
	LEA_STACK(CoordStruct*, pCoords, 0x38);

	FactoryHook::ApplyExitCoord(pBuilding, *pCoords);

	return 0;
}

