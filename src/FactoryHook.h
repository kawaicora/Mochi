#pragma once
#include <YRPP.h>
#include <EventSystem.h>

class FactoryHook
{
public:
	static Event<FactoryClass*> ProgressUpdateEvent;
	static Event<FactoryClass*> FactoryClassCreateEvent;

	// Non-barracks unit factory ([ID] has CloningFacility=yes + WeaponsFactory=yes):
	// adds [ID]'s ExitCoord to the spawn coordinates. Returns false when the key
	// is absent, or the building is a barracks (engine already adds ExitCoord there).
	static bool ApplyExitCoord(BuildingClass* pBuilding, CoordStruct& coords);

	// "512,256,0" / "0,-256,0" -> CoordStruct. Negative values are supported.
	static bool ParseExitCoord(const char* pRaw, CoordStruct& coords);
};
