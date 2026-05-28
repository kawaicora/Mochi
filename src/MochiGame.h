#pragma once
#include <vector>
#include <YRpp.h>
#include <HouseHook.h>
#include "EventHook.h"
#include <TechnoHook.h>

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
	static std::vector<HouseClass*> GetSelectedObjectOwnerHouse() {
		std::vector<HouseClass*> HouseList;
		for (auto const& abs : ObjectClass::Array) {
			if (!abs) continue;
			auto obj = dynamic_cast<ObjectClass*>(abs);
			if (!obj) continue;
			if (obj->IsSelected == false) continue;
			if (HouseClass* pHouse = obj->GetOwningHouse()) {
				if (std::find(HouseList.begin(), HouseList.end(), pHouse) == HouseList.end()) {
					HouseList.push_back(pHouse);
				}
			}
		}
		return HouseList;
	};
	static void DrawPlayerFactoryProduction() {
		if (!HouseClass::CurrentPlayer) return;
		if (HouseClass::CurrentPlayer->Primary_ForAircraft) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_ForAircraft);
		}
		if(HouseClass::CurrentPlayer->Primary_ForBuildings){
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_ForBuildings);
		}
		if (HouseClass::CurrentPlayer->Primary_ForDefenses) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_ForDefenses);
		}
		if(HouseClass::CurrentPlayer->Primary_ForVehicles){
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_ForVehicles);
		}
		if (HouseClass::CurrentPlayer->Primary_ForInfantry) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_ForInfantry);
		}
		if (HouseClass::CurrentPlayer->Primary_ForShips) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_ForShips);
		}
		if (HouseClass::CurrentPlayer->Primary_Unused1) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_Unused1);
		}
		if (HouseClass::CurrentPlayer->Primary_Unused2) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_Unused2);
		}
		if (HouseClass::CurrentPlayer->Primary_Unused3) {
			DrawPlayerFactoryProduction(HouseClass::CurrentPlayer->Primary_Unused3);
		}
	}
	static void DrawPlayerFactoryProduction(FactoryClass* pFactory) 
	{
		if (!pFactory) return;
		if (!pFactory->Owner) return;
		if (!pFactory->Object) return;
		if (pFactory->Owner != HouseClass::CurrentPlayer) return;
		for (const auto& building : pFactory->Owner->Buildings) {
			if (!building) {
				continue;
			}
			AbstractType FactoryType = building->Type->Factory;
		
			if (pFactory->Object->GetTechnoType()->WhatAmI() != FactoryType) continue; // Skip if the building's factory type doesn't match the factory's object type
			//if (!building->IsPrimaryFactory) continue; // Skip if it's not a primary factory
			const int cameoWidth = 60;
			const int cameoHeight = 48;
			SHPStruct* cameo = pFactory->Object->GetType()->GetCameo();
			const wchar_t* uiname = pFactory->Object->GetType()->UIName;
			std::pair<Point2D, bool> result = TacticalClass::Instance->CoordsToClient(building->GetCoords());
			if (!result.second) continue;
			Point2D location = result.first;
			location.Y -= 50; // Adjust the Y coordinate to be above the building

			RectangleStruct darkenBounds{ 0, 0, location.X + cameoWidth, location.Y + cameoHeight };
			DSurface::Composite->DrawSHP(
				FileSystem::CAMEO_PAL,
				cameo,
				0,
				&location,
				&darkenBounds,
				BlitterFlags(0xE00),
				0,
				0,
				ZGradient::Ground,
				1000,
				0,
				nullptr,
				0,
				0,
				0
			);
		}
		
	};
	static void DrawBuildingsFactoryProduction() {
		for (auto const& building : BuildingClass::Array) {
			if (!building) continue;
			if (!building->Factory) continue;
			if (!building->Factory->Owner) continue;
			if (!building->Factory->Object) continue;
			//if (!building->IsPrimaryFactory) continue; // Skip if it's not a primary factory
			//if (bFilterSelf && building->Factory->Owner == HouseClass::CurrentPlayer) continue;  //没有用因为玩家用鼠标点击建造没有设置Building里面的Factory属性，除非玩家建造物的Factory属性被手动设置了才会生效
			const int cameoWidth = 60;
			const int cameoHeight = 48;
			SHPStruct* cameo = building->Factory->Object->GetType()->GetCameo();
			const wchar_t* uiname = building->Factory->Object->GetType()->UIName;
			std::pair<Point2D, bool> result = TacticalClass::Instance->CoordsToClient(building->GetCoords());
			if (!result.second) continue;

			Point2D location = result.first;
			location.Y -= 50; // Adjust the Y coordinate to be above the building
			RectangleStruct darkenBounds{ 0, 0, location.X + cameoWidth, location.Y + cameoHeight };
			DSurface::Composite->DrawSHP(
				FileSystem::CAMEO_PAL,
				cameo,
				0,
				&location,
				&darkenBounds,
				BlitterFlags(0xE00),
				0,
				0,
				ZGradient::Ground,
				1000,
				0,
				nullptr,
				0,
				0,
				0
			);
		}

	};

	static std::vector<ObjectClass*> GetSelectedObjectList() {
		std::vector<ObjectClass*> ObjectList;
		for (auto const& abs : ObjectClass::Array) {
			if (!abs) continue;
			auto obj = dynamic_cast<ObjectClass*>(abs);
			if (!obj) continue;
			if (obj->IsSelected == false) continue;
		
			ObjectList.push_back(obj);
		}
		return ObjectList;
	}
	static void DrawAllGameObjectInfo (bool bFilterNotSelected , bool bFilterNotHouse = true)
	{
		for (auto const& abs : ObjectClass::Array) {
			if (!abs) continue;
			
			auto obj = dynamic_cast<ObjectClass*>(abs);
			if (!obj) continue;
			if (bFilterNotHouse && !obj->GetOwningHouse()) continue;
			if (bFilterNotSelected && obj->IsSelected == false) continue;
			DrawObjectInfo(obj);
		}
	};
	static void DrawObjectInfo(ObjectClass* obj) {
		wchar_t buf[512];
		std::pair<Point2D, bool> result = TacticalClass::Instance->CoordsToClient(obj->GetCoords());
		if (!result.second) {
			return;
		}
		Point2D loc = result.first;
		const char* id = obj->GetType()->ID;
		if (!id) return;
		int strength = obj->GetType()->Strength;
		int hp = obj->Health;
		const wchar_t* uiname = obj->GetType()->UIName;
		if (!uiname) uiname = L"";
		if (uiname == L"") {
			swprintf(buf, 512, L"%S %d/%d", id, hp, strength);
		}
		else {
			swprintf(buf, 512, L"%ls %d/%d", uiname, hp, strength);
		}
		RectangleStruct wanted = Drawing::GetTextDimensions(buf, loc, (DWORD)-1, 0, 0);
		RectangleStruct rect = { 0, 0, wanted.Width, wanted.Height };
		loc.X -= wanted.Width/2;
		loc.Y += 20;
		if (HouseClass* pHouse = obj->GetOwningHouse()) {
			DSurface::Composite->DrawTextA(buf, loc.X, loc.Y, Drawing::RGB_To_Int(pHouse->Color));
		}
		else {
			DSurface::Composite->DrawTextA(buf, loc.X, loc.Y, COLOR_GRAYTEXT);
		}
		
	};
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
		
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::GetByTypeAndIndex(data->Produce.RTTIType, data->Produce.HeapID);

		FactoryClass* pFactory = TechnoHook::GetFactoryByTechnoTypeClass(pHouse, pTechnoTypeClass);

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