#include "MochiEvent.h"
#include "Debug.h"
#include <MochiHouse.h>
#include <MochiTechno.h>
#include <MochiFactory.h>
#include <MochiMegaMission.h>
//******************************事件发送函数******************************//
void MochiEvent::SendMoveEvent(AbstractClass* pAbstract,CellStruct location) {
	if (CellClass* pCell = MapClass::Instance.TryGetCellAt(location)) {

		HouseClass* pHouse = pAbstract->GetOwningHouse();
		EventData* event = EventData::EventClass_CTOR();
		event->Type = EventType::MegaMission;
		event->Frame = Unsorted::CurrentFrame;
		event->HouseIndex = (unsigned char)pHouse->ArrayIndex;
		event->MegaMission.Whom = TargetClass(pAbstract);
		event->MegaMission.Destination = TargetClass(pCell);
		event->MegaMission.Mission = (unsigned char)Mission::Move;
		event->AddEvent();
	}
	else {
		Debug::LogW(L"无效的坐标 %d %d", location.X, location.Y);
	}
	
}
void MochiEvent::SendAttackEvent(AbstractClass* pAbstract, CellStruct location) {
	if (CellClass* pCell = MapClass::Instance.TryGetCellAt(location)) {

		HouseClass* pHouse = pAbstract->GetOwningHouse();
		EventData* event = EventData::EventClass_CTOR();
		event->Type = EventType::MegaMission;
		event->Frame = Unsorted::CurrentFrame;
		event->HouseIndex = (unsigned char)pHouse->ArrayIndex;
		event->MegaMission.Whom = TargetClass(pAbstract);
		event->MegaMission.Target = TargetClass(pCell);
		event->MegaMission.Mission = (unsigned char)Mission::Attack;
		event->AddEvent();
	}
	else {
		Debug::LogW(L"无效的坐标 %d %d", location.X, location.Y);
	}

}

void MochiEvent::SendCompleteProduceEvent(HouseClass* pHouse, FactoryClass* pFactory) {
	if (!pFactory) {
		return;
	}
	if (!pFactory->Owner) {
		return;
	}
	if (pFactory->Owner != pHouse) {
		return;
	}
	if (!pFactory->Object) {
		return;
	}
	if (pFactory->Production.Value == 54 || pFactory->SpecialItem != -1) {
		return;
	}
	if (pFactory->IsSuspended) {
		return;
	}
	if (pFactory->Owner->Available_Money() < pFactory->Balance) {
		return;
	}
	EventData* event = EventData::EventClass_CTOR();
	event->Type = (EventType)MochiEventType::CoraCompleteProduction;
	event->Frame = Unsorted::CurrentFrame;
	event->HouseIndex = (unsigned char)pHouse->ArrayIndex;

	event->Produce.RTTIType = pFactory->Object->WhatAmI();
	event->Produce.IsNaval = pFactory->Object->GetTechnoType()->Naval;
	event->Produce.HeapID = MochiTechno::GetIndex(pFactory->Object->GetTechnoType());
	if (event->Produce.HeapID == -1) {
		return;
	}
	event->AddEvent();
};
void MochiEvent::SendChangeMoneyEvent(HouseClass* pHouse, int amount) {
	if (!pHouse) {
		return;
	}
	EventData* event = EventData::EventClass_CTOR();
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraMoneyChange;
	event->HouseIndex = pHouse->ArrayIndex;
	event->Money.Amount = amount;
	event->AddEvent();
};
void MochiEvent::SendUnlockAllTechEvent(HouseClass* pHouse, bool IsUnlock) {
	if (!pHouse) {
		return;
	}
	EventData* event = EventData::EventClass_CTOR();
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraUnlockAllTech;
	event->HouseIndex = pHouse->ArrayIndex;
	event->UnlockAllTech.IsUnlock = IsUnlock;
	event->AddEvent();
};

void MochiEvent::SendSuperWeaponChargeEvent(HouseClass* pHouse, int SuperWeaponIndex) {
	EventData* event = EventData::EventClass_CTOR();
	event->HouseIndex = pHouse->ArrayIndex;
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraSuperWeaponCharge;
	event->HouseIndex = pHouse->ArrayIndex;
	event->SpecialPlace.ID = SuperWeaponIndex;
	event->AddEvent();
};


void MochiEvent::SendSpecialPlaceEvent(HouseClass* pHouse, SuperWeaponTypeClass* superWeaponType, CellStruct location) {
	EventData* event = EventData::EventClass_CTOR();
	//发送事件
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraSpecialPlace;
	event->HouseIndex = pHouse->ArrayIndex;
	event->SpecialPlace.ID = superWeaponType->ArrayIndex;
	event->SpecialPlace.Location = location;
	event->AddEvent();

};

void MochiEvent::SendPlaceEvent(HouseClass* pHouse, const char* id, CellStruct location) {
	EventData* event = EventData::EventClass_CTOR();
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraPlace;
	event->HouseIndex = pHouse->ArrayIndex;

	TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
	if (!pTechnoTypeClass) {
		Debug::Log("Can't Find TechnoTypeClass By ID: %s\n", id);
		return;
	}
	int heapId = MochiTechno::GetIndex(pTechnoTypeClass);
	if (heapId == -1)
	{
		Debug::Log("Invalid HeapID: %d\n", heapId);
	}
	event->Place.RTTIType = pTechnoTypeClass->WhatAmI();
	event->Place.HeapID = heapId;
	event->Place.IsNaval = pTechnoTypeClass->Naval;
	event->Place.Location = location;
	event->AddEvent();

};
void MochiEvent::SendPlaceEvent(HouseClass* pHouse, TechnoTypeClass* pTechnoTypeClass) {
	EventData* event = EventData::EventClass_CTOR();
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraPlace;
	event->HouseIndex = pHouse->ArrayIndex;
	event->Place.RTTIType = pTechnoTypeClass->WhatAmI();
	event->Place.HeapID = MochiTechno::GetIndex(pTechnoTypeClass);
	event->Place.IsNaval = pTechnoTypeClass->Naval;
	event->Place.Location = DisplayClass::Instance.Display_ZoneCell;
	event->AddEvent();

};

void MochiEvent::SendActiveSuperWeaponEvent(HouseClass* pHouse, int houseSupersIndex) {
	EventData* event = EventData::EventClass_CTOR();
	SuperClass* super = pHouse->Supers[houseSupersIndex];
	if (!super) {
		Debug::Log("super is zero\n");
		return;
	}
	if (super->IsReady) {
		Debug::LogW(L"super Is Ready: %ls\n", super->Type->UIName);
		return;
	}
	if (super->CanFire()) {
		Debug::LogW(L"super Can Fire: %ls\n", super->Type->UIName);
		return;
	}

	//发送事件
	event->Frame = Unsorted::CurrentFrame;
	event->Type = (EventType)MochiEventType::CoraActiveSuperWeapon;
	event->HouseIndex = pHouse->ArrayIndex;
	event->SpecialPlace.ID = houseSupersIndex;
	event->AddEvent();
};
//*******************************事件工具方法******************************//
bool MochiEvent::IsValidType(MochiEventType type)  {
	return ((unsigned char)type >= 0x70 && (unsigned char)type <= 0x8F);
}
//*******************************事件处理函数******************************//

void MochiEvent::CoraMoneyChange(EventData* data) {
	Debug::LogW(L"***************************************CoraCompleteChangeMoney*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	pHouse->TransactMoney(data->Money.Amount);

};

void MochiEvent::CoraCompleteProduction(EventData* data) {
	Debug::LogW(L"***************************************CoraCompleteProduction*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::GetByTypeAndIndex(data->Produce.RTTIType, data->Produce.HeapID);
	FactoryClass* pFactory = MochiTechno::GetFactoryByTechnoTypeClass(pHouse, pTechnoTypeClass);
	if (!pFactory) {
		Debug::Log("pFactory Is Zero Try TechnoHook::GetFactoryByTechnoTypeForNoOnlineAI HeapID: %d RTTIType: %d\n", data->Produce.HeapID, (int)data->Produce.RTTIType);
		pFactory = MochiTechno::GetFactoryByTechnoTypeForNoOnlineAI(pHouse, pTechnoTypeClass);

	}
	if (!pFactory) {
		Debug::Log("pFactory Is Zero At Try Complete Produce HeapID: %d RTTIType: %d\n", data->Produce.HeapID, (int)data->Produce.RTTIType);
		return;
	}
	MochiFactory::CompleteProdution(pFactory);
	Debug::LogW(L"CompleteProdece: %ls\n", pFactory->Object->GetTechnoType()->UIName);
};

void MochiEvent::CoraSuperWeaponCharge(EventData* data) {
	Debug::LogW(L"***************************************CoraSuperWeaponCharge*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	SuperClass* pSuper = pHouse->Supers[data->SpecialPlace.ID];
	if (pSuper) {
		pSuper->SetCharge(100);
		Debug::LogW(L"SuperWeapon Charged : %ls\n", pSuper->Type->UIName);
	}
	else {
		Debug::Log("pSuper is zero");
	}
};

void MochiEvent::CoraSpecialPlace(EventData* data) {
	Debug::LogW(L"***************************************CoraSpecialPlace*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	SuperWeaponTypeClass* superWeaponType = SuperWeaponTypeClass::Array[data->SpecialPlace.ID];
	if (!superWeaponType) {
		Debug::Log("Can't Find SuperWeaponType By Index: %d\n", data->SpecialPlace.ID);
		return;
	}
	SuperWeaponType type = superWeaponType->Type;
	SuperClass* super = MochiHouse::FindSuperWeapon(pHouse, type);
	if (!super) {
		Debug::Log("Can't Find SuperWeapon By Type: %d\n", (int)type);
		return;
	}
	CDTimerClass timer;
	super->RechargeTimer = timer;
	super->SetCharge(100);
	super->ChargeDrainState = ChargeDrainState::Ready;
	super->IsReady = true;
	if (!super->CanFire()) {
		Debug::LogW(L"!%ls 不可发射 类型: %d\n", super->Type->UIName, (int)type);
		return;
	}
	super->Launch(data->SpecialPlace.Location, pHouse == HouseClass::CurrentPlayer);
	Debug::LogW(L"Launch SuperWeapon : %ls\n", super->Type->UIName);
};

void MochiEvent::CoraActiveSuperWeapon(EventData* data) {
	Debug::LogW(L"***************************************CoraActiveSuperWeapon*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	SuperWeaponTypeClass* superWeaponType = SuperWeaponTypeClass::Array[data->SpecialPlace.ID];
	if (!superWeaponType) {
		Debug::Log("Can't Find SuperWeaponType By Index: %d\n", data->SpecialPlace.ID);
		return;
	}
	SuperWeaponType type = superWeaponType->Type;
	SuperClass* super = MochiHouse::FindSuperWeapon(pHouse, type);
	if (!super) {
		Debug::Log("Can't Find SuperWeapon By Type: %d\n", (int)type);
		return;
	}
	if (!super->CanFire()) {
		Debug::LogW(L"!%ls 不可发射 类型: %d\n", super->Type->UIName, (int)type);
		return;
	}
	super->Type->ShowTimer = true;
	super->Type->ManualControl = true;
	CDTimerClass timer;
	super->RechargeTimer = timer;
	super->ShouldDrawProgress();
	int left = super->RechargeTimer.GetTimeLeft();
	int duration = Game::F2I(
		(super->GetRechargeTime() - left)
		* RulesClass::Instance->ChargeToDrainRatio);
	super->RechargeTimer.Start(duration);


};

void MochiEvent::CoraUnlockAllTech(EventData* data) {
	Debug::LogW(L"***************************************CoraUnlockAllTech*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	MochiHouse::UpdateTechUnlockList(pHouse, data->UnlockAllTech.IsUnlock);
};
void MochiEvent::CoraPlace(EventData* data) {
	Debug::LogW(L"***************************************CoraPlace*************************************************\n");
	HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
	if (data->Place.HeapID != -1) {
		TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::GetByTypeAndIndex(data->Place.RTTIType, data->Place.HeapID);
		if (!pTechnoTypeClass) {
			Debug::Log("Can't Find TechnoType By HeapID: %d RTTIType: %d\n", data->Place.HeapID, (int)data->Place.RTTIType);
			return;
		}

		if (MochiTechno::PlaceTechnoAtMap(pTechnoTypeClass, data->Place.Location, pHouse)) {
			Debug::LogW(L"Place Object Success: %ls At (%d,%d)\n", pTechnoTypeClass->UIName, data->Place.Location.X, data->Place.Location.Y);

		}
		else {
			Debug::LogW(L"Place Object Failed: %ls At (%d,%d)\n", pTechnoTypeClass->UIName, data->Place.Location.X, data->Place.Location.Y);
			return;
		}
	}
	else {
		Debug::Log("Invalid HeapID: %d\n", data->Place.HeapID);
		return;
	}
};

//*******************************原有事件解析函数******************************//
void MochiEvent::OriginalFrameSyncEvent(EventData* data)
{
	Debug::LogW(L"***************************************FrameSync*************************************************\n");
	Debug::LogW(L"House: %ls Frame: %d\n", MochiHouse::GetPlayerNameByHouseIndex(data->HouseIndex), data->Frame);
}
void MochiEvent::OriginalFrameInfoEvent(EventData* data)
{
	Debug::LogW(L"***************************************FrameInfo*************************************************\n");
	Debug::LogW(L"House: %ls Frame: %d CRC: 0x%08X CommandCount: %d Delay: %d\n", MochiHouse::GetPlayerNameByHouseIndex(data->HouseIndex), data->Frame,data->FrameInfo.CRC, data->FrameInfo.CommandCount, data->FrameInfo.Delay);
}

void MochiEvent::OriginalPlaceEvent(EventData* data) {
	Debug::LogW(L"***************************************Place*************************************************\n");
	if (data->Place.HeapID != -1) {
		switch (data->Place.RTTIType) {
		case AbstractType::Unit:
		case AbstractType::UnitType:
		{
			UnitTypeClass* pUnitTypeClass = UnitTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"放置单位 %ls  %S \n", pUnitTypeClass->UIName, pUnitTypeClass->ID);
			break;
		}

		case AbstractType::Aircraft:
		case AbstractType::AircraftType:
		{
			AircraftTypeClass* pAircraftTypeClass = AircraftTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"放置航空器 %ls  %S \n", pAircraftTypeClass->UIName, pAircraftTypeClass->ID);
			break;
		}

		case AbstractType::Infantry:
		case AbstractType::InfantryType:
		{
			InfantryTypeClass* pInfantryTypeClass = InfantryTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"放置步兵 %ls  %S \n", pInfantryTypeClass->UIName, pInfantryTypeClass->ID);
			break;
		}

		case AbstractType::Building:
		case AbstractType::BuildingType:
		{
			BuildingTypeClass* pBuildingTypeClass = BuildingTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"放置建筑 %ls  %S \n", pBuildingTypeClass->UIName, pBuildingTypeClass->ID);
			break;
		}

		}
	}
};
void MochiEvent::OriginalProduceEvent(EventData* data)
{
	Debug::LogW(L"***************************************Produce*************************************************\n");
	if (data->Produce.HeapID != -1) {
		switch (data->Place.RTTIType) {
		case AbstractType::Unit:
		case AbstractType::UnitType:
		{
			UnitTypeClass* pUnitTypeClass = UnitTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"开始建造单位 %ls  %S \n", pUnitTypeClass->UIName, pUnitTypeClass->ID);
			break;
		}

		case AbstractType::Aircraft:
		case AbstractType::AircraftType:
		{
			AircraftTypeClass* pAircraftTypeClass = AircraftTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"开始建造航空器 %ls  %S \n", pAircraftTypeClass->UIName, pAircraftTypeClass->ID);
			break;
		}

		case AbstractType::Infantry:
		case AbstractType::InfantryType:
		{
			InfantryTypeClass* pInfantryTypeClass = InfantryTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"开始建造步兵 %ls  %S \n", pInfantryTypeClass->UIName, pInfantryTypeClass->ID);
			break;
		}

		case AbstractType::Building:
		case AbstractType::BuildingType:
		{
			BuildingTypeClass* pBuildingTypeClass = BuildingTypeClass::Array[data->Place.HeapID];
			Debug::LogW(L"开始建造建筑 %ls  %S \n", pBuildingTypeClass->UIName, pBuildingTypeClass->ID);
			break;
		}
		}
	}
};

void MochiEvent::OriginalMegaMissionEvent(EventData* data)
{
	Debug::LogW(L"***************************************MegaMission*************************************************\n");
	TargetClass target = data->MegaMission.Target;
	TargetClass whom = data->MegaMission.Whom;
	TargetClass destination = data->MegaMission.Destination;
	TargetClass follow = data->MegaMission.Follow;
	Debug::LogW(L"MegaMission Event: \nMission: %d \n_gap_: %d \nIsPlanningEvent: %ls\n", data->MegaMission.Mission, data->MegaMission._gap_, data->MegaMission.IsPlanningEvent == true ? L"是" : L"否");
	Debug::LogW(
		L"MegaMission Event: \n**Target: \n****RTTI:%S \n****ID:%d\n**Whom:\n****RTTI:%S \n****ID:%d\n**destination: \n****RTTI:%S \n****ID:%d \nfollow:\n****RTTI:%S \n****ID:%d \n", 
		AbstractClass::GetRTTIName((AbstractType)data->MegaMission.Target.m_RTTI), 
		data->MegaMission.Target.m_ID,
		AbstractClass::GetRTTIName((AbstractType)data->MegaMission.Whom.m_RTTI), 
		data->MegaMission.Whom.m_ID,
		AbstractClass::GetRTTIName((AbstractType)data->MegaMission.Destination.m_RTTI),
		data->MegaMission.Destination.m_ID,
		AbstractClass::GetRTTIName((AbstractType)data->MegaMission.Follow.m_RTTI),
		data->MegaMission.Follow.m_ID
	);
		
	MochiMegaMission::ShowMegaMissionInfo(data);
	
};

void MochiEvent::OriginalMegaMissionFEvent(EventData* data) //带运动学参数的命令
{
	Debug::LogW(L"***************************************MegaMissionF*************************************************\n");
	TargetClass target = data->MegaMissionF.Target;
	TargetClass whom = data->MegaMissionF.Whom;
	TargetClass destination = data->MegaMissionF.Destination;
	Debug::LogW(L"MegaMission Event: \nMission: %d \n", data->MegaMissionF.Mission);
	Debug::LogW(
		L"MegaMissionF Event: \n**Target: \n****RTTI:%S \n****ID:%d\n**Whom:\n****RTTI:%S \n****ID:%d\n**destination: \n****RTTI:%S \n****ID:%d \n",
		AbstractClass::GetRTTIName((AbstractType)data->MegaMissionF.Target.m_RTTI),
		data->MegaMissionF.Target.m_ID,
		AbstractClass::GetRTTIName((AbstractType)data->MegaMissionF.Whom.m_RTTI),
		data->MegaMissionF.Whom.m_ID,
		AbstractClass::GetRTTIName((AbstractType)data->MegaMissionF.Destination.m_RTTI),
		data->MegaMissionF.Destination.m_ID
	);
	Debug::LogW(L"MegaMissionF Event: \nSpeed: %d \nMaxSpeed: %d\n", data->MegaMissionF.Speed, data->MegaMissionF.MaxSpeed);
	MochiMegaMission::ShowMegaMissionFInfo(data);
	
};