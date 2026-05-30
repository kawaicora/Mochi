#pragma once
#include <YRPP.h>
#include <vector>
class MochiHouse
{
public:
	static bool IsPlayerInstantConstruction;
	static bool IsAutoChargePlayerAllSuperweapon;
	static bool IsAutoGetCrate;
	struct TechUnlockStruct
	{
		HouseClass* pHouse;
		bool IsUnlock;
	};
	static std::vector<MochiHouse::TechUnlockStruct> TechUnlockList;
	static const wchar_t* GetPlayerNameByHouseIndex(int index);
	static int FindSuperWeaponIndex(HouseClass* pHouse, SuperWeaponType const type);
	static SuperClass* FindSuperWeapon(HouseClass* pHouse, SuperWeaponType const type);
	static void UpdateTechUnlockList(HouseClass* pHouseClass, bool isUnlock);
	static bool IsTechUnlocked(HouseClass* pHouse);

};
