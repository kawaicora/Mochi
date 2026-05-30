#pragma once
#include <vector>
#include <YRPP.h>
#include <MochiEvent.h>
#include <Debug.h>
#include <MochiINI.h>
#include <HouseHook.h>
class MochiGame {
public:
	static void ReadINI(const char* file);
	static std::vector<HouseClass*> GetSelectedObjectOwnerHouse();
	static void DrawAllFactoryProduction();
	static void DrawFactoryProduction(FactoryClass* pFactory);
	static std::vector<ObjectClass*> GetSelectedObjectList();
	static void DrawAllGameObjectInfo(bool bFilterNotSelected, bool bFilterNotHouse = true);
	static void DrawObjectInfo(ObjectClass* obj);
	static void DrawHouseInfo();
	static void DrawRadarTest();
	static void PlayMovie(const char* fileName,
		int queueTheme = -1,
		char useHiddenSurface1 = -1,
		char stretchMovie = -1,
		char useHiddenSurface2 = 1,
		char setState1 = -1);
	//***********************************************Command Functions************************************************
	static void SetCurrentPlayerAIControl(bool bCurrentPlayIsAIControl);
	static void ToggleCurrentPlayerAIControl();
	static void ActiveAllSuperWeapon(HouseClass* pHouse);
	static void RandomGenUnits(HouseClass* pHouse);
	static void ChargeAllSuperWeapon(HouseClass* pHouse);
	static void LaunchSuperWeaponAtMouseLocation(HouseClass* pHouse, int index);
};