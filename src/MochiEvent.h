#pragma once
#include <vector>
#include <YRPP.h>
#include <MochiEventType.h>
#include <EventData.h>
class MochiEvent {
public:
	static bool IsValidType(MochiEventType type);
	//******************************事件发送函数******************************//
	static void SendCompleteProduceEvent(HouseClass *pHouse,FactoryClass* pFactory);
	static void SendChangeMoneyEvent(HouseClass* pHouse, int amount);
	static void SendUnlockAllTechEvent(HouseClass* pHouse, bool IsUnlock);
	static void SendSuperWeaponChargeEvent(HouseClass* pHouse, int SuperWeaponIndex);
	static void SendSpecialPlaceEvent(HouseClass* pHouse, SuperWeaponTypeClass* superWeaponType, CellStruct location);
	static void SendPlaceEvent(HouseClass* pHouse ,const char* id,CellStruct location);
	static void SendPlaceEvent(HouseClass* pHouse,TechnoTypeClass* pTechnoTypeClass);
	static void SendActiveSuperWeaponEvent(HouseClass* pHouse,int houseSupersIndex);
	//*******************************事件处理函数******************************//

	static void CoraMoneyChange(EventData* data);
	static void CoraCompleteProduction(EventData* data);
	static void CoraSuperWeaponCharge(EventData* data);
	static void CoraSpecialPlace(EventData* data);
	static void CoraActiveSuperWeapon(EventData* data);
	static void CoraUnlockAllTech(EventData* data);
	static void CoraPlace(EventData* data);
	//*******************************原有事件解析函数******************************//
	static void OriginalPlaceEvent(EventData* data);
	static void OriginalProduceEvent(EventData* data);
	static void OriginalMegaMissionEvent(EventData* data) ;
	static void OriginalMegaMissionFEvent(EventData* data); //带运动学参数的命令
	
};