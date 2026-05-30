#pragma once
#include <YRPP.h>
class MochiMap
{
public:
	static bool IsCellOccupationOccupied(CellClass* pCell);
	static bool IsFoundationValid(CellStruct base, TechnoTypeClass* pType);
	static CellClass* FindNearestValidLandCell(CellStruct base, TechnoTypeClass* pType, int maxRadius = 64);
};
