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
	static void DrawAllFactoryProduction() {

		for (FactoryClass* pFactory : FactoryClass::Array) {
			if (!pFactory) continue;
			if (!pFactory->Owner) continue;
			if (!pFactory->Object) continue;
			
			DrawFactoryProduction(pFactory);
		}
		
	}
	static void DrawFactoryProduction(FactoryClass* pFactory) 
	{
		if (!pFactory) return;
		if (!pFactory->Owner) return;
		if (!pFactory->Object) return;
		for (const auto& building : pFactory->Owner->Buildings) {
			if (!building) {
				continue;
			}
			if (
				pFactory->Object->GetTechnoType()->WhatAmI() == AbstractType::Building ||
				pFactory->Object->GetTechnoType()->WhatAmI() == AbstractType::BuildingType) goto DRAW_FACTORY_PRODUCTION_SKIP_IF_NAVAL;

			if (pFactory->Object->GetTechnoType()->Naval) {
				if (!building->Type->Naval) continue; //如果工厂生产的单位是海军单位，但建筑不是海军建筑，则跳过
			}
			else {
				if (building->Type->Naval) continue; //如果工厂生产的单位不是海军单位，但建筑是海军建筑，则跳过
			}

		DRAW_FACTORY_PRODUCTION_SKIP_IF_NAVAL:

			if (pFactory->Object->GetTechnoType()->WhatAmI() != building->Type->Factory) continue; // Skip if the building's factory type doesn't match the factory's object type
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
	static void DrawRadarView() {
		bool bIsDone = !(*(BYTE*)0xB45B68 > 0);
		if (RadarClass::Instance.unknown_bool_14DA && !RadarClass::Instance.unknown_bool_14BD) {
			auto pRadar = &RadarClass::Instance;
			Point2D location = { *(int*)((uintptr_t)pRadar + 0x11E4), *(int*)((uintptr_t)pRadar + 0x11EC)};
			RectangleStruct bounds{ 0, 0, location.X + 160, location.Y + 110 };
			DSurface::Sidebar->DrawSHP(
				(ConvertClass*)0xB0FBF8,
				(SHPStruct*)0xB04A38,
				32,
				&location,
				&bounds,
				BlitterFlags::None,
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

			RadarClass::Instance.unknown_rect_120C.X = *((int*) (&RadarClass::Instance + 0x11E4));
			RadarClass::Instance.unknown_rect_120C.Y = *((int*) (&RadarClass::Instance + 0x11EC));
			RadarClass::Instance.unknown_rect_120C.Width = 160;
			RadarClass::Instance.unknown_rect_120C.Height = 110;
			
			Debug::Log("Radar: Movie full redrawn\n");
		}
		if (bIsDone) {
			char v14; // [esp+12h] [ebp-1Eh]
			v14 = 0;
			Debug::Log("Radar: Movie Done\n");
			bool v13 = *((BYTE*)&RadarClass::Instance + 5308) == 0;
			*((DWORD*)&RadarClass::Instance + 5292) = 5;
		}
		
	};
	static void DrawRadarTest() {
		auto pRadar = &RadarClass::Instance;
		RectangleStruct rect{ *(int*)((uintptr_t)pRadar + 0x11E4), *(int*)((uintptr_t)pRadar + 0x11EC), 160, 110 };
		Point2D location = { *(int*)((uintptr_t)pRadar + 0x11E4), *(int*)((uintptr_t)pRadar + 0x11EC) };
		DSurface::Sidebar->FillRect(&rect, COLOR_RED);
		//MapClass::Instance.RedrawSidebar(0);
		RectangleStruct bounds{ 0, 0, location.X + 160, location.Y + 110 };
		//RadarClass::Instance->SetVisibleRect(rect);
		DSurface::Sidebar->DrawSHP(
			(ConvertClass*)0xB0FBF8,
			(SHPStruct*)0xB04A38,
			32,
			&location,
			&bounds,
			BlitterFlags::bf_400,
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

	};

	static void PlayMovie(const char* fileName,
		int queueTheme = -1,
		char useHiddenSurface1 = -1,
		char stretchMovie = -1,
		char useHiddenSurface2 = 1,
		char setState1 = -1) {
		
		GameMode gameMode = *(GameMode*)0xA8B238;  //保存当前游戏模式
		*(GameMode*)0xA8B238 = GameMode::Campaign;
		Game::PlayMovie(
			fileName,
			queueTheme,
			useHiddenSurface1,
			stretchMovie,
			useHiddenSurface2,
			setState1
		);
		*(GameMode*)0xA8B238 = gameMode;  //恢复原有游戏模式

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
	//******************************事件发送函数******************************//
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
	static void SendChangeMoneyEvent(HouseClass* pHouse, int amount) {
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
	
	static void SendSuperWeaponChargeEvent(HouseClass* pHouse, int SuperWeaponIndex) {
		EventData* event = EventData::EventClass_CTOR();
		event->HouseIndex = pHouse->ArrayIndex;
		event->Frame = Unsorted::CurrentFrame;
		event->Type = (EventType)MochiEventType::CoraSuperWeaponCharge;
		event->HouseIndex = pHouse->ArrayIndex;
		event->SpecialPlace.ID = SuperWeaponIndex;
		event->AddEvent();
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
				Debug::LogW(L"Generating unit: %S %ls\n", id, pTechnoTypeClass->UIName);
				SendPlaceEvent(pHouse, id, DisplayClass::Instance.Display_ZoneCell);
			}
			for (int i = 0; i < count; i++) {
				auto RandomUnitIDListStr = Mochi::PlayerBaseConfig->GetValueByKey<std::string>("RandomUnitID");
				std::vector<std::string> RandomUnitIDList = MochiINIReader::Split(RandomUnitIDListStr, ',');
				const char* id = RandomUnitIDList[rand() % RandomUnitIDList.size()].c_str();
				TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
				Debug::LogW(L"Generating unit: %S %ls\n", id, pTechnoTypeClass->UIName);
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


	//*******************************事件处理函数******************************//

	static void CoraMoneyChange(EventData* data) {
		Debug::LogW(L"***************************************CoraCompleteChangeMoney*************************************************\n");
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		pHouse->TransactMoney(data->Money.Amount);
		
	};

	static void CoraCompleteProduction(EventData* data) {
		Debug::LogW(L"***************************************CoraCompleteProduction*************************************************\n");
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::GetByTypeAndIndex(data->Produce.RTTIType, data->Produce.HeapID);
		FactoryClass* pFactory = TechnoHook::GetFactoryByTechnoTypeClass(pHouse, pTechnoTypeClass);
		if (!pFactory) {
			Debug::Log("pFactory Is Zero Try TechnoHook::GetFactoryByTechnoTypeForNoOnlineAI HeapID: %d RTTIType: %d\n", data->Produce.HeapID, (int)data->Produce.RTTIType);
			pFactory = TechnoHook::GetFactoryByTechnoTypeForNoOnlineAI(pHouse, pTechnoTypeClass);

		}
		if (!pFactory) {
			Debug::Log("pFactory Is Zero At Try Complete Produce HeapID: %d RTTIType: %d\n", data->Produce.HeapID, (int)data->Produce.RTTIType);
			return;
		}
		FactoryHook::CompleteProdution(pFactory);
		Debug::LogW(L"CompleteProdece: %ls\n", pFactory->Object->GetTechnoType()->UIName);
	};

	static void CoraSuperWeaponCharge(EventData* data) {
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

	static void CoraSpecialPlace(EventData* data) {
		Debug::LogW(L"***************************************CoraSpecialPlace*************************************************\n");
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
		Debug::LogW(L"***************************************CoraActiveSuperWeapon*************************************************\n");
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
		Debug::LogW(L"***************************************CoraUnlockAllTech*************************************************\n");
		HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
		HouseHook::UpdateTechUnlockList(pHouse, data->UnlockAllTech.IsUnlock);
	}
	static void CoraPlace(EventData* data) {
		Debug::LogW(L"***************************************CoraPlace*************************************************\n");
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
	//*******************************原有事件解析函数******************************//
	static void OriginalPlaceEvent(EventData* data) {
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
	}
	static void OriginalProduceEvent(EventData* data) 
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
	}

	static void OriginalMegaMissionEvent(EventData* data) 
	{
		Debug::LogW(L"***************************************MegaMission*************************************************\n");
		TargetClass target = data->MegaMission.Target;
		TargetClass whom = data->MegaMission.Whom;
		TargetClass destination = data->MegaMission.Destination;
		TargetClass follow = data->MegaMission.Follow;
		Debug::LogW(L"MegaMission Event: \nMission: %d \n_gap_: %d \nIsPlanningEvent: %ls\n", data->MegaMission.Mission, data->MegaMission._gap_, data->MegaMission.IsPlanningEvent == true?L"是":L"否");
		Debug::LogW(L"MegaMission Event: \nTarget: %d \nWhom: %d \nDestination: %d \nFollow: %d\n", target, whom, destination, follow);
		Debug::LogW(L"MegaMission Event: \nRTTI: \ntarget: %d \nwhom: %d \ndestination: %d \nfollow: %d\n", data->MegaMission.Target.m_RTTI, data->MegaMission.Whom.m_RTTI, data->MegaMission.Destination.m_RTTI, data->MegaMission.Follow.m_RTTI);
		Debug::LogW(L"MegaMission Event: \nID: \ntarget: %d \nwhom: %d \ndestination: %d \nfollow: %d\n", data->MegaMission.Target.m_ID, data->MegaMission.Whom.m_ID, data->MegaMission.Destination.m_ID, data->MegaMission.Follow.m_ID);

		switch ((Mission)data->MegaMission.Mission)
		{
		case Mission::None:
			Debug::LogW(L"MegaMission Event: 无任务\n");
			break;

		case Mission::Sleep:
			Debug::LogW(L"MegaMission Event: 睡眠\n");
			break;

		case Mission::Attack:
			Debug::LogW(L"MegaMission Event: 攻击\n");
			break;

		case Mission::Move:
			Debug::LogW(L"MegaMission Event: 移动\n");
			break;

		case Mission::QMove:
			Debug::LogW(L"MegaMission Event: 强制移动\n");
			break;

		case Mission::Retreat:
			Debug::LogW(L"MegaMission Event: 撤退\n");
			break;

		case Mission::Guard:
			Debug::LogW(L"MegaMission Event: 警戒\n");
			break;

		case Mission::Sticky:
			Debug::LogW(L"MegaMission Event: 粘附/跟随\n");
			break;

		case Mission::Enter:
			Debug::LogW(L"MegaMission Event: 进入\n");
			break;

		case Mission::Capture:
			Debug::LogW(L"MegaMission Event: 占领\n");
			break;

		case Mission::Eaten:
			Debug::LogW(L"MegaMission Event: 被吞噬\n");
			break;

		case Mission::Harvest:
			Debug::LogW(L"MegaMission Event: 采矿\n");
			break;

		case Mission::Area_Guard:
			Debug::LogW(L"MegaMission Event: 区域警戒\n");
			break;

		case Mission::Return:
			Debug::LogW(L"MegaMission Event: 返回\n");
			break;

		case Mission::Stop:
			Debug::LogW(L"MegaMission Event: 停止\n");
			break;

		case Mission::Ambush:
			Debug::LogW(L"MegaMission Event: 伏击\n");
			break;

		case Mission::Hunt:
			Debug::LogW(L"MegaMission Event: 猎杀\n");
			break;

		case Mission::Unload:
			Debug::LogW(L"MegaMission Event: 卸载\n");
			break;

		case Mission::Sabotage:
			Debug::LogW(L"MegaMission Event: 破坏\n");
			break;

		case Mission::Construction:
			Debug::LogW(L"MegaMission Event: 建造\n");
			break;

		case Mission::Selling:
			Debug::LogW(L"MegaMission Event: 出售\n");
			break;

		case Mission::Repair:
			Debug::LogW(L"MegaMission Event: 修理\n");
			break;

		case Mission::Rescue:
			Debug::LogW(L"MegaMission Event: 救援\n");
			break;

		case Mission::Missile:
			Debug::LogW(L"MegaMission Event: 导弹任务\n");
			break;

		case Mission::Harmless:
			Debug::LogW(L"MegaMission Event: 无害任务\n");
			break;

		case Mission::Open:
			Debug::LogW(L"MegaMission Event: 展开/打开\n");
			break;

		case Mission::Patrol:
			Debug::LogW(L"MegaMission Event: 巡逻\n");
			break;

		case Mission::ParadropApproach:
			Debug::LogW(L"MegaMission Event: 空降接近\n");
			break;

		case Mission::ParadropOverfly:
			Debug::LogW(L"MegaMission Event: 空降飞越\n");
			break;

		case Mission::Wait:
			Debug::LogW(L"MegaMission Event: 等待\n");
			break;

		case Mission::AttackMove:
			Debug::LogW(L"MegaMission Event: 攻击移动\n");
			break;

		case Mission::SpyplaneApproach:
			Debug::LogW(L"MegaMission Event: 间谍飞机接近\n");
			break;

		case Mission::SpyplaneOverfly:
			Debug::LogW(L"MegaMission Event: 间谍飞机飞越\n");
			break;

		default:
			Debug::LogW(
				L"MegaMission Event: 未知任务 %d\n",
				(int)data->MegaMission.Mission);
			break;
		}
	};

	static void OriginalMegaMissionFEvent(EventData* data) //带运动学参数的命令
	{
		Debug::LogW(L"***************************************MegaMissionF*************************************************\n");
		TargetClass target = data->MegaMissionF.Target;
		TargetClass whom = data->MegaMissionF.Whom;
		TargetClass destination = data->MegaMissionF.Destination;
		Debug::LogW(L"MegaMission Event: \nMission: %d \n", data->MegaMissionF.Mission);
		Debug::LogW(L"MegaMissionF Event: \nTarget: %d \nWhom: %d \nDestination: %d \n", target, whom, destination);
		Debug::LogW(L"MegaMissionF Event: \nRTTI: \ntarget: %d \nwhom: %d \ndestination: %d \n", data->MegaMissionF.Target.m_RTTI, data->MegaMissionF.Whom.m_RTTI, data->MegaMissionF.Destination.m_RTTI);
		Debug::LogW(L"MegaMissionF Event: \nID: target: %d \nwhom: %d \ndestination: %d \n", data->MegaMissionF.Target.m_ID, data->MegaMissionF.Whom.m_ID, data->MegaMissionF.Destination.m_ID);
		Debug::LogW(L"MegaMissionF Event: \nSpeed: %d \nMaxSpeed: %d\n", data->MegaMissionF.Speed, data->MegaMissionF.MaxSpeed);

		switch ((Mission)data->MegaMissionF.Mission)
		{
		case Mission::None:
			Debug::LogW(L"MegaMissionF Event: 无任务\n");
			break;

		case Mission::Sleep:
			Debug::LogW(L"MegaMissionF Event: 睡眠\n");
			break;

		case Mission::Attack:
			Debug::LogW(L"MegaMissionF Event: 攻击\n");
			break;

		case Mission::Move:
			Debug::LogW(L"MegaMissionF Event: 移动\n");
			break;

		case Mission::QMove:
			Debug::LogW(L"MegaMissionF Event: 强制移动\n");
			break;

		case Mission::Retreat:
			Debug::LogW(L"MegaMissionF Event: 撤退\n");
			break;

		case Mission::Guard:
			Debug::LogW(L"MegaMissionF Event: 警戒\n");
			break;

		case Mission::Sticky:
			Debug::LogW(L"MegaMissionF Event: 粘附/跟随\n");
			break;

		case Mission::Enter:
			Debug::LogW(L"MegaMissionF Event: 进入\n");
			break;

		case Mission::Capture:
			Debug::LogW(L"MegaMissionF Event: 占领\n");
			break;

		case Mission::Eaten:
			Debug::LogW(L"MegaMissionF Event: 被吞噬\n");
			break;

		case Mission::Harvest:
			Debug::LogW(L"MegaMissionF Event: 采矿\n");
			break;

		case Mission::Area_Guard:
			Debug::LogW(L"MegaMissionF Event: 区域警戒\n");
			break;

		case Mission::Return:
			Debug::LogW(L"MegaMissionF Event: 返回\n");
			break;

		case Mission::Stop:
			Debug::LogW(L"MegaMissionF Event: 停止\n");
			break;

		case Mission::Ambush:
			Debug::LogW(L"MegaMissionF Event: 伏击\n");
			break;

		case Mission::Hunt:
			Debug::LogW(L"MegaMissionF Event: 猎杀\n");
			break;

		case Mission::Unload:
			Debug::LogW(L"MegaMissionF Event: 卸载\n");
			break;

		case Mission::Sabotage:
			Debug::LogW(L"MegaMissionF Event: 破坏\n");
			break;

		case Mission::Construction:
			Debug::LogW(L"MegaMissionF Event: 建造\n");
			break;

		case Mission::Selling:
			Debug::LogW(L"MegaMissionF Event: 出售\n");
			break;

		case Mission::Repair:
			Debug::LogW(L"MegaMissionF Event: 修理\n");
			break;

		case Mission::Rescue:
			Debug::LogW(L"MegaMissionF Event: 救援\n");
			break;

		case Mission::Missile:
			Debug::LogW(L"MegaMissionF Event: 导弹任务\n");
			break;

		case Mission::Harmless:
			Debug::LogW(L"MegaMissionF Event: 无害任务\n");
			break;

		case Mission::Open:
			Debug::LogW(L"MegaMissionF Event: 展开/打开\n");
			break;

		case Mission::Patrol:
			Debug::LogW(L"MegaMissionF Event: 巡逻\n");
			break;

		case Mission::ParadropApproach:
			Debug::LogW(L"MegaMissionF Event: 空降接近\n");
			break;

		case Mission::ParadropOverfly:
			Debug::LogW(L"MegaMissionF Event: 空降飞越\n");
			break;

		case Mission::Wait:
			Debug::LogW(L"MegaMissionF Event: 等待\n");
			break;

		case Mission::AttackMove:
			Debug::LogW(L"MegaMissionF Event: 攻击移动\n");
			break;

		case Mission::SpyplaneApproach:
			Debug::LogW(L"MegaMissionF Event: 间谍飞机接近\n");
			break;

		case Mission::SpyplaneOverfly:
			Debug::LogW(L"MegaMissionF Event: 间谍飞机飞越\n");
			break;

		default:
			Debug::LogW(
				L"MegaMission Event: 未知任务 %d\n",
				(int)data->MegaMission.Mission);
			break;
		}
	};
};
