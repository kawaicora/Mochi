#pragma once
#include <YRPP.h>
class MochiTechno {
public:
	static int GetIndex(TechnoTypeClass* pTechnoTypeClass);
	static FactoryClass* GetFactoryByTechnoTypeForNoOnlineAI(HouseClass* pHouse, TechnoTypeClass* pTechnoTypeClass);
	static FactoryClass* GetFactoryByTechnoTypeClass(HouseClass* pHouse, TechnoTypeClass* pTechnoTypeClass);
	static bool PlaceTechnoAtMap(TechnoClass* pTechno, CellStruct location);
	static bool PlaceTechnoAtMap(TechnoTypeClass* pTechnoType, CellStruct location, HouseClass* pHouse);
	static bool PlaceTechnoAtMap(const char* id, CellStruct location, HouseClass* pHouse);
};
