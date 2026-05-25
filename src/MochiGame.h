#pragma once
#include <vector>
#include <YRpp.h>
#include <HouseHook.h>
#include "HouseClass.h"
#include "EventHook.h"

class MochiGame {
public:
	
	static void ReadINI(const char* file) {
		CCINIClass* pINI = GameCreate<CCINIClass>();
		if (pINI) {
			CCFileClass* cfg = GameCreate<CCFileClass>(file);

			if (cfg) {
				if (cfg->Exists()) {
					pINI->ReadCCFile(cfg);
				}
				GameDelete(cfg);
			}
		}
	};
	static void SetCurrentPlayerAIControl(bool bCurrentPlayIsAIControl) {
		HouseClass::CurrentPlayer->IsHumanPlayer = !bCurrentPlayIsAIControl; //设置成相反的值
		ToggleCurrentPlayerAIControl(); //调用切换函数以应用更改
		
	}
	static void ToggleCurrentPlayerAIControl() {
		if (HouseClass::CurrentPlayer->IsHumanPlayer) {
			HouseClass::CurrentPlayer->IsHumanPlayer = false;
			HouseClass::CurrentPlayer->Production = HouseClass::CurrentPlayer->AutocreateAllowed = true;
			HouseClass::CurrentPlayer->IQLevel = 5;
			HouseClass::CurrentPlayer->IQLevel2 = 5;
			HouseClass::CurrentPlayer->AIDifficulty = AIDifficulty::Hard;
			MessageListClass::Instance.PrintMessage(L"AI接管控制!", 300, ColorScheme::Green);
		}
		else {


			HouseClass::CurrentPlayer->IsHumanPlayer = true;
			HouseClass::CurrentPlayer->Production = HouseClass::CurrentPlayer->AutocreateAllowed = false;
			HouseClass::CurrentPlayer->IQLevel = 0;
			HouseClass::CurrentPlayer->IQLevel2 = 0;
			HouseClass::CurrentPlayer->AIDifficulty = AIDifficulty::Normal;
			MessageListClass::Instance.PrintMessage(L"玩家接管控制!", 300, ColorScheme::Green);
		}
	}
	static void DrawHouseInfo() {
		wchar_t buf[512];
		std::vector<int> withNameHouseIndex;
		Point2D loc;
		loc.X = 0;
		const int AdvCommBarHeight = 32;
		int offset = AdvCommBarHeight;

		for (auto const& pHouse : HouseClass::Array) {
			if (!pHouse) {
				continue;
			}
			if (pHouse == HouseClass::FindNeutral() || pHouse == HouseClass::FindSpecial()) {
				continue;
			}
			if (pHouse->Defeated) {
				continue;
			}
			swprintf(
				buf,
				512,
				L"%ls %ls %d  ",
				HouseHook::GetPlayerNameByHouseIndex(pHouse->ArrayIndex),
				L"\u8d44\u91d1:",
				pHouse->Available_Money()
			);
			auto wanted = Drawing::GetTextDimensions(buf, loc, (DWORD)-1, 0, 0);

			auto h = DSurface::Composite->GetHeight();
			RectangleStruct rect = { 0, h - wanted.Height - offset, wanted.Width, wanted.Height };
			DSurface::Composite->FillRect(&rect, COLOR_BLACK);

			DSurface::Composite->DrawTextA(buf, 0, rect.Y, Drawing::RGB_To_Int(pHouse->Color));
			offset += wanted.Height;

		}

		withNameHouseIndex.clear();
	};
	static void SendCompleteProduceEvent(HouseClass *pHouse,FactoryClass* pFactory) {
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
		event->Produce.HeapID = TechnoHook::GetIndex(pFactory->Object->GetTechnoType());
		if (event->Produce.HeapID == -1) {
			return;
		}
		event->AddEvent();
	};

	static void SendUnlockAllTechEvent(HouseClass* pHouse,bool IsUnlock) {
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
	static void ChargeAllSuperWeapon(HouseClass* pHouse) {

		for (int i = 0;i < pHouse->Supers.Count; i++) {
			EventData* event = EventData::EventClass_CTOR();
			SuperClass* super = pHouse->Supers[i];
			if (!super) {

				Debug::Log("super is zero\n");
				continue;
			}
			int total = super->Type->RechargeTime;
			int remain = super->RechargeTimer.GetTimeLeft();

			int percent = (1.0f - (float)remain / total) * 100.0f;
			//Debug::Log("SuperWeapon: %ls IsReady: %ls CanFire: %ls  ChargePercent: %d \n",  super->Type->UIName,  super->IsReady ? L"true":L"false", super->CanFire()? L"true" : L"false",percent);

			if (super->IsReady) {
				continue;
			}
			if (super->CanFire()) {
				continue;
			}

			if (percent == 100) {
				continue;
			}
			SendSuperWeaponChargeEvent(pHouse, i);
		}
	}
	static void SendSuperWeaponChargeEvent(HouseClass* pHouse, int SuperWeaponIndex) {
		EventData* event = EventData::EventClass_CTOR();
		event->HouseIndex = pHouse->ArrayIndex;
		event->Frame = Unsorted::CurrentFrame;
		event->Type = (EventType)MochiEventType::CoraSuperWeaponCharge;
		event->HouseIndex = pHouse->ArrayIndex;
		event->SpecialPlace.ID = SuperWeaponIndex;
		event->AddEvent();
	};

	static void LaunchSuperWeaponAtMouseLocation(HouseClass* pHouse, int index) {
		try {

			auto LaunchSuperWeaponListStr = Mochi::PlayerBaseConfig->GetValueByKey<std::string>("LaunchSuperWeaponList");
			std::vector<std::string> LaunchSuperWeaponList = MochiINIReader::Split(LaunchSuperWeaponListStr, ',');
			SuperWeaponTypeClass* superWeaponType = SuperWeaponTypeClass::Find(LaunchSuperWeaponList[index].c_str());
			MochiGame::SendSpecialPlaceEvent(pHouse, superWeaponType, DisplayClass::Instance.Display_ZoneCell);
		}
		catch (...) {
			Debug::Log("Failed to launch super weapon\n");
		}
	};
	static void SendSpecialPlaceEvent(HouseClass* pHouse, SuperWeaponTypeClass* superWeaponType,CellStruct location ) {
		EventData* event = EventData::EventClass_CTOR();
		//发送事件
		event->Frame = Unsorted::CurrentFrame;
		event->Type = (EventType)MochiEventType::CoraSpecialPlace;
		event->HouseIndex = pHouse->ArrayIndex;
		event->SpecialPlace.ID = superWeaponType->ArrayIndex;
		event->SpecialPlace.Location = location;
		event->AddEvent();
			
	}
	static void RandomGenUnits(HouseClass* pHouse) {
		try {
			int count = Mochi::PlayerBaseConfig->GetValueByKey<int>("GenCount", 10);
			for (int i = 0; i < count; i++) {
				auto RandomInfantryIDListStr = Mochi::PlayerBaseConfig->GetValueByKey<std::string>("RandomInfantryID");
				std::vector<std::string> RandomInfantryIDList = MochiINIReader::Split(RandomInfantryIDListStr, ',');
				const char* id = RandomInfantryIDList[rand() % RandomInfantryIDList.size()].c_str();
				TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
				Debug::LogW(L"Generating unit: %S\n %ls", id, pTechnoTypeClass->UIName);
				SendPlaceEvent(pHouse, id, DisplayClass::Instance.Display_ZoneCell);
			}
			for (int i = 0; i < count; i++) {
				auto RandomUnitIDListStr = Mochi::PlayerBaseConfig->GetValueByKey<std::string>("RandomUnitID");
				std::vector<std::string> RandomUnitIDList = MochiINIReader::Split(RandomUnitIDListStr, ',');
				const char* id = RandomUnitIDList[rand() % RandomUnitIDList.size()].c_str();
				TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
				Debug::LogW(L"Generating unit: %S\n %ls", id, pTechnoTypeClass->UIName);
				SendPlaceEvent(pHouse, id, DisplayClass::Instance.Display_ZoneCell);
			}
		}catch(...){
			Debug::Log("Failed to generate units\n");
			return;
		}
	}
	static void SendPlaceEvent(HouseClass* pHouse ,const char* id,CellStruct location) {
		EventData* event = EventData::EventClass_CTOR();
		event->Frame = Unsorted::CurrentFrame;
		event->Type = (EventType)MochiEventType::CoraPlace;
		event->HouseIndex = pHouse->ArrayIndex;

		TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
		if (!pTechnoTypeClass) {
			Debug::Log("Can't Find TechnoTypeClass By ID: %s\n", id);
			return;
		}
		int heapId = TechnoHook::GetIndex(pTechnoTypeClass);
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
	static void SendPlaceEvent(HouseClass* pHouse,TechnoTypeClass* pTechnoTypeClass) {
		EventData* event = EventData::EventClass_CTOR();
		event->Frame = Unsorted::CurrentFrame;
		event->Type = (EventType)MochiEventType::CoraPlace;
		event->HouseIndex = pHouse->ArrayIndex;
		event->Place.RTTIType = pTechnoTypeClass->WhatAmI();
		event->Place.HeapID = TechnoHook::GetIndex( pTechnoTypeClass);
		event->Place.IsNaval = pTechnoTypeClass->Naval;
		event->Place.Location = DisplayClass::Instance.Display_ZoneCell;
		event->AddEvent();

	};
	static void SendActiveAllSuperWeaponEvent(HouseClass* pHouse) {
		for (int i = 0; i < pHouse->Supers.Count; i++) {
			EventData* event = EventData::EventClass_CTOR();
			SuperClass* super = pHouse->Supers[i];
			if (!super) {
				Debug::Log("super is zero\n");
				continue;
			}
			if (super->IsReady) {
				Debug::LogW(L"super Is Ready: %ls\n", super->Type->UIName);
				continue;
			}
			if (super->CanFire()) {
				Debug::LogW(L"super Can Fire: %ls\n", super->Type->UIName);
				continue;
			}
			//发送事件
			event->Frame = Unsorted::CurrentFrame;
			event->Type = (EventType)MochiEventType::CoraActiveSuperWeapon;
			event->HouseIndex = pHouse->ArrayIndex;
			event->SpecialPlace.ID = i;
			event->AddEvent();
		}
	};
	//        事件处理函数
	static void CoraCompleteProduction(EventData* data) {
		FactoryClass* pFactory;
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::GetByTypeAndIndex(data->Produce.RTTIType, data->Produce.HeapID);

		switch (data->Produce.RTTIType)
		{
		case AbstractType::Unit:
		case AbstractType::UnitType:
		{
			if (data->Produce.IsNaval) {
				pFactory = pHouse->Primary_ForShips;
			}
			else {
				pFactory = pHouse->Primary_ForVehicles;
			}
			break;
		}
		case AbstractType::Aircraft:
		case AbstractType::AircraftType:
		{
			pFactory = pHouse->Primary_ForAircraft;
			break;
		}

		case AbstractType::Infantry:
		case AbstractType::InfantryType:
		{
			pFactory = pHouse->Primary_ForInfantry;
			break;
		}

		case AbstractType::Building:
		case AbstractType::BuildingType:
		{
			BuildCat buildCat = (BuildCat)-1;
			if (const auto pBuildingType = abstract_cast<BuildingTypeClass*>(pTechnoTypeClass)) {
				buildCat = pBuildingType->BuildCat;
			}
			if (buildCat == BuildCat::Combat) {
				pFactory = pHouse->Primary_ForDefenses;
			}
			else {
				pFactory = pHouse->Primary_ForBuildings;
			}
			break;
		}
		default:
			Debug::Log("*** Warning ***\n");
			Debug::Log("Can't Process This  RTTIType %d \n", (int)data->Produce.RTTIType);
			return;

		}

		if (!pFactory) {
			Debug::Log("pFactory Is Zero At Try Complete Produce HeapID: %d RTTIType: %d\n", data->Produce.HeapID, (int)data->Produce.RTTIType);
			return;
		}
		FactoryHook::CompleteProdution(pFactory);
		Debug::LogW(L"CompleteProdece: %ls\n", pFactory->Object->GetTechnoType()->UIName);
	};

	static void CoraSuperWeaponCharge(EventData* data) {
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

	static void CoraSpecialPlace(EventData* data) {
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		SuperWeaponTypeClass* superWeaponType = SuperWeaponTypeClass::Array[data->SpecialPlace.ID];
		if (!superWeaponType) {
			Debug::Log("Can't Find SuperWeaponType By Index: %d\n", data->SpecialPlace.ID);
			return;
		}
		SuperWeaponType type = superWeaponType->Type;
		SuperClass* super = HouseHook::FindSuperWeapon(pHouse,type);
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
			Debug::LogW(L"!%ls 不可发射 类型: %d\n", super->Type->UIName,(int)type);
			return;
		}
		super->Launch( data->SpecialPlace.Location, pHouse == HouseClass::CurrentPlayer);
		Debug::LogW(L"Launch SuperWeapon : %ls\n", super->Type->UIName);
	};

	static void CoraActiveSuperWeapon(EventData* data) {
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		SuperWeaponTypeClass* superWeaponType = SuperWeaponTypeClass::Array[data->SpecialPlace.ID];
		if (!superWeaponType) {
			Debug::Log("Can't Find SuperWeaponType By Index: %d\n", data->SpecialPlace.ID);
			return;
		}
		SuperWeaponType type = superWeaponType->Type;
		SuperClass* super = HouseHook::FindSuperWeapon(pHouse, type);
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


	}

	static void CoraUnlockAllTech(EventData* data) {
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		HouseHook::UpdateTechUnlockList(pHouse, data->UnlockAllTech.IsUnlock);
	}
	static void CoraPlace(EventData* data) {
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		if (data->Place.HeapID != -1) {
			TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::GetByTypeAndIndex(data->Place.RTTIType, data->Place.HeapID);
			if(!pTechnoTypeClass) {
				Debug::Log("Can't Find TechnoType By HeapID: %d RTTIType: %d\n", data->Place.HeapID, (int)data->Place.RTTIType);
				return;
			}
		
			if (TechnoHook::PlaceTechnoAtMap(pTechnoTypeClass, data->Place.Location, pHouse)) {
				Debug::LogW(L"Place Object Success: %ls At (%d,%d)\n", pTechnoTypeClass->UIName, data->Place.Location.X, data->Place.Location.Y);

			}
			else {
				Debug::LogW(L"Place Object Failed: %ls At (%d,%d)\n", pTechnoTypeClass->UIName, data->Place.Location.X, data->Place.Location.Y);
				return;
			}
		}else{
			Debug::Log("Invalid HeapID: %d\n", data->Place.HeapID);
			return;
		}
	};
private:
	static bool PlayerUnlockAllCameo;
};


bool MochiGame::PlayerUnlockAllCameo = false;