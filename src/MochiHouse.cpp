#include "MochiHouse.h"

std::vector<MochiHouse::TechUnlockStruct> MochiHouse::TechUnlockList = {};
bool MochiHouse::IsPlayerInstantConstruction = false;
bool MochiHouse::IsAutoChargePlayerAllSuperweapon = false;
bool MochiHouse::IsAutoGetCrate = false;

const wchar_t* MochiHouse::GetPlayerNameByHouseIndex(int index)
{

	for (int i = 0; i < NodeNameType::Array.Count; i++)
	{
		NodeNameType* node = NodeNameType::Array[i];
		if (node->HouseIndex == index) {
			return  node->Name;
		}
	}

	return HouseClass::Array[index]->Type->UIName;

};

int MochiHouse::FindSuperWeaponIndex(HouseClass* pHouse, SuperWeaponType const type) {
	for (int i = 0; i < pHouse->Supers.Count; ++i) {
		if (pHouse->Supers.Items[i]->Type->Type == type) {
			return i;
		}
	}
	return -1;
}

SuperClass* MochiHouse::FindSuperWeapon(HouseClass* pHouse, SuperWeaponType const type) {
	for (int i = 0; i < pHouse->Supers.Count; ++i) {
		if (pHouse->Supers.Items[i]->Type->Type == type) {
			return pHouse->Supers[i];
		}
	}
	return nullptr;
}
void MochiHouse::UpdateTechUnlockList(HouseClass* pHouseClass, bool isUnlock)
{
	for (auto& item : MochiHouse::TechUnlockList)
	{
		if (item.pHouse == pHouseClass)
		{
			item.IsUnlock = isUnlock;
			return;
		}
	}

	MochiHouse::TechUnlockList.push_back({ pHouseClass, isUnlock });
}
bool MochiHouse::IsTechUnlocked(HouseClass* pHouse)
{
	for (auto& item : MochiHouse::TechUnlockList)
	{
		if (item.pHouse == pHouse)
		{
			return item.IsUnlock;
		}
	}
	return false;
}