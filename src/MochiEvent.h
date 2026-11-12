#pragma once
#include <vector>
#include <YRPP.h>
#include <MochiEventType.h>
#include <EventData.h>
class MochiEvent {
public:
	static void SetHouseAIControlFlag(HouseClass* pHouse, bool isPlayerBefore);
	static bool GetHouseAIControlFlag(HouseClass* pHouse);
	static bool IsValidType(MochiEventType type);
	
	//******************************事件发送函数******************************//
	static void SendMessagePacketEx(wchar_t * msg,  int size, bool justShowAllied, int totalPackCount, int currPackCount,int Idx);
	static void SendMoveEvent(AbstractClass* pAbstract, CellStruct location);
	static void SendAttackEvent(AbstractClass* pAbstract, CellStruct location);
    static void SendAIControlEvent(HouseClass *pHouse, bool isAIControl);
    static void SendCompleteProduceEvent(HouseClass *pHouse, FactoryClass *pFactory);
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
    static void CoraAIControl(EventData *data);
    static void CoraUnlockAllTech(EventData *data);
    static void CoraPlace(EventData* data);
	static void CoraMessage(EventData* data);

	//*******************************原有事件解析函数******************************//
	static void OriginalFrameSyncEvent(EventData* data);
	static void OriginalFrameInfoEvent(EventData* data);
	static void OriginalPlaceEvent(EventData* data);
	static void OriginalProduceEvent(EventData* data);
	static void OriginalMegaMissionEvent(EventData* data) ;
	static void OriginalMegaMissionFEvent(EventData* data); //带运动学参数的命令

	//*******************************回调事件函数******************************//
	static void OnHouseCreate(HouseClass * pHouse);
	static void OnEventReceived(EventData* data);
private:
	static struct HouseAIControlFlag
	{
		HouseClass* pHouse;
		bool IsPlayerBefore;   // 翻成 AI 之前，这家是不是玩家
	} HouseAIControlFlags[0x40];  // 记录每个 House 的 AI 控制状态，最多支持 64 个 House
};