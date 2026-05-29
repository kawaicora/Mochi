#pragma once
#include <string>
#include<EventSystem.h>
#include <Helpers/Macro.h>


struct HouseClassName
{
	HouseClass* pHouseClass;
	NodeNameType* pNameNodeType = nullptr;
};
class HouseHook {
public:
	static bool IsPlayerInstantConstruction;
	static bool IsAutoChargePlayerAllSuperweapon;
	static bool IsAutoGetCrate;
	struct TechUnlockStruct
	{
		HouseClass* pHouse;
		bool IsUnlock;
	};
	static Event<HouseClass*> HouseClassCreateEvent;
	static std::vector<HouseHook::TechUnlockStruct> TechUnlockList;
	static const wchar_t* GetPlayerNameByHouseIndex(int index)
	{

		for (int i = 0; i < NodeNameType::Array.Count; i++)
		{
			NodeNameType * node = NodeNameType::Array[i];
			if (node->HouseIndex == index) {
				return  node->Name;
			}
		}

		return HouseClass::Array[index]->Type->UIName;
		
	};

	static int FindSuperWeaponIndex(HouseClass* pHouse,SuperWeaponType const type) {
		for (int i = 0; i < pHouse->Supers.Count; ++i) {
			if (pHouse->Supers.Items[i]->Type->Type == type) {
				return i;
			}
		}
		return -1;
	}

	static SuperClass* FindSuperWeapon(HouseClass* pHouse, SuperWeaponType const type) {
		for (int i = 0; i < pHouse->Supers.Count; ++i) {
			if (pHouse->Supers.Items[i]->Type->Type == type) {
				return pHouse->Supers[i];
			}
		}
		return nullptr;
	}
	static void UpdateTechUnlockList(HouseClass* pHouseClass, bool isUnlock)
	{
		for (auto& item : TechUnlockList)
		{
			if (item.pHouse == pHouseClass)
			{
				item.IsUnlock = isUnlock;
				return;
			}
		}

		TechUnlockList.push_back({ pHouseClass, isUnlock });
	}
	static bool IsTechUnlocked(HouseClass* pHouse)
	{
		for (auto& item : TechUnlockList)
		{
			if (item.pHouse == pHouse)
			{
				return item.IsUnlock;
			}
		}
		return false;
	}
};
std::vector<HouseHook::TechUnlockStruct> HouseHook::TechUnlockList = {};
bool HouseHook::IsPlayerInstantConstruction = false;
bool HouseHook::IsAutoChargePlayerAllSuperweapon = false;
bool HouseHook::IsAutoGetCrate = false;
Event<HouseClass*> HouseHook::HouseClassCreateEvent;

DEFINE_HOOK(0x4F6532, HouseClass_CTOR, 5) {
	GET(HouseClass*, pHouseClass, EAX);
	HouseHook::HouseClassCreateEvent.Invoke(pHouseClass);
	return 0;
}


DEFINE_HOOK(0x4F8361, HouseClass_CanBuild, 0x8) {

	/* return
		 1 - Tech shown
		 0 - Tech not shown
		-1 - Tech greyed out
	 */
	GET(HouseClass*, pHouse, ECX);
	GET_STACK(TechnoTypeClass const* const, pItem, 0x4);
	GET_STACK(bool const, buildLimitOnly, 0x8);
	GET_STACK(bool const, includeInProduction, 0xC);
	if (HouseHook::IsTechUnlocked(pHouse)) {
		R->EAX(1);
	}
	
	return 0x4F7BEE;
}