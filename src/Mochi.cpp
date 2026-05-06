
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"
#include "Utilities/Macro.h"
#include "EventHook.h"
#include "GeneralHook.h"
#include "FactoryHook.h"
#include <vector>
#include "../INILoader.h"
HANDLE Mochi::hInstance = 0;
bool Mochi::isRegistered = false;



struct _WIN32_FIND_DATAA FindFileData;

void DrawHouseInfo() {
	

	wchar_t buf[512];
	std::vector<int> withNameHouseIndex;
	Point2D loc;
	loc.X = 0;
	const int AdvCommBarHeight = 32;
	int offset = AdvCommBarHeight;

	for (auto const& pNameType : NodeNameType::Array) {
		auto const& pHouse = HouseClass::Array[pNameType->HouseIndex];
		if (!pHouse) {
			continue;
		}
		withNameHouseIndex.push_back(pNameType->HouseIndex);
		//Debug::LogGame("Player name: %ls\n", pNameType->Name);
		swprintf(buf, 512, L"%ls Money: %d  ", pNameType->Name, pHouse->Available_Money());
		auto wanted = Drawing::GetTextDimensions(buf, loc, (DWORD)-1, 0, 0);
		
		auto h = DSurface::Composite->GetHeight();
		RectangleStruct rect = { 0, h - wanted.Height - offset, wanted.Width, wanted.Height };
		DSurface::Composite->FillRect(&rect, COLOR_BLACK);
		DSurface::Composite->DrawTextA(buf, 0, rect.Y, COLOR_GREEN);
		offset += wanted.Height;

	}
	for (auto const& pHouse : HouseClass::Array) {
		if (!pHouse) {
			continue;
		}
		if (pHouse == HouseClass::FindNeutral() || pHouse == HouseClass::FindSpecial()) {
			continue;
		}
		if (std::find(withNameHouseIndex.begin(),
			withNameHouseIndex.end(),
			pHouse->ArrayIndex) != withNameHouseIndex.end()) {
			continue;
		}
		
		swprintf(buf, 512, L"%ls Money: %d  ", pHouse->Type->UIName, pHouse->Available_Money());
		auto wanted = Drawing::GetTextDimensions(buf, loc, (DWORD)-1, 0, 0);
		
		auto h = DSurface::Composite->GetHeight();
		RectangleStruct rect = { 0, h - wanted.Height - offset, wanted.Width, wanted.Height };
		DSurface::Composite->FillRect(&rect, COLOR_BLACK);
		DSurface::Composite->DrawTextA(buf, 0, rect.Y, COLOR_GREEN);
		offset += wanted.Height;

	}

	withNameHouseIndex.clear();
}
void ChargeSuperWeapon() {
	for (int i = 0;i < HouseClass::CurrentPlayer->Supers.Count; i++) {
		EventClass* event = EventHook::EventClass_CTOR();
		SuperClass* super = HouseClass::CurrentPlayer->Supers[i];
		if (!super) {

			Debug::Log("super is zero\n");
			continue;
		}
		if (super->CustomChargeTime == 100) {
			Debug::Log("super is ready %d \n", super->CustomChargeTime);
			continue;
		}
		if (super->IsReady) {
			Debug::Log("super is ready\n");
			continue;
		}
		if (super->CanFire()) {
			Debug::Log("super is ready\n");
			continue;
		}

		//发送事件


		event->Type = (EventType)MochiEventType::CoraSuperWeaponCharge;
		event->HouseIndex = HouseClass::CurrentPlayer->ArrayIndex;
		event->SpecialPlace.ID = i;
		EventHook::AddEvent(event);
	}
}
//********************************

/// <summary>
/// 
/// </summary>
/// <param name="hInstance"></param>
/// <param name="dwReason"></param>
/// <param name="v"></param>
/// <returns></returns>

bool __stdcall DllMain(HANDLE hInstance, DWORD dwReason, LPVOID v)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		Mochi::hInstance = hInstance;
		SetConsoleOutputCP(CP_UTF8);
		Mochi::RegisterEvent();
		
	}
		break;
	case DLL_PROCESS_DETACH:
		
		break;

	default:
		break;
	} 
	return true;
}


/// <summary>
/// 注册事件
/// </summary>
void Mochi::RegisterEvent() {
	if (isRegistered) {
		return;
	}
	EventHook::NetworkingRespondToEvent.Subscribe([](EventClass* data) {
		
		switch (data->Type) {
		case (EventType)MochiEventType::CoraCompleteProduction:
		{
			FactoryClass* pFactory;
			HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
			switch (data->Produce.RTTIType) {
			case AbstractType::Unit:
			case AbstractType::UnitType:

				if (data->Produce.IsNaval) {
					pFactory = pHouse->Primary_ForShips;
				}
				else {
					pFactory = pHouse->Primary_ForVehicles;
				}
				break;
			case AbstractType::Aircraft:
			case AbstractType::AircraftType:
				pFactory = pHouse->Primary_ForAircraft;
				break;
			case AbstractType::Infantry:
			case AbstractType::InfantryType:
				pFactory = pHouse->Primary_ForInfantry;
				break;
			case AbstractType::Building:
			case AbstractType::BuildingType:
				if (BuildingTypeClass::Array[data->Produce.HeapID]->BuildCat == BuildCat::Combat) {
					pFactory = pHouse->Primary_ForDefenses;
				}
				else {
					pFactory = pHouse->Primary_ForBuildings;
				}
				break;
			}

			if (!pFactory) {
				return;
			}
			FactoryHook::CompleteProdution(pFactory);
			Debug::LogW(L"CompleteProdece: %ls\n", pFactory->Object->GetTechnoType()->UIName);
		}
			break;

		case (EventType)MochiEventType::CoraSuperWeaponCharge: 
		{
			HouseClass* pHouse = HouseClass::Array[data->HouseIndex];

			SuperClass*  pSuper  = pHouse->Supers[data->SpecialPlace.ID];
			if (pSuper) {
				pSuper->SetCharge(100);
				Debug::LogW(L"SuperWeapon Charged : %ls\n", pSuper->Type->UIName);
			}
			else {
				Debug::LogW(L"pSuper is zero");
			}

		}
			break;

		case EventType::Place:
		{
			if (data->Place.HeapID != -1) {
				switch (data->Place.RTTIType) {
				case AbstractType::Building:
				case AbstractType::BuildingType:
					BuildingTypeClass* pBuildingTypeClass = BuildingTypeClass::Array[data->Place.HeapID];
					Debug::LogW(L"Place Bulilding %ls  %s \n", pBuildingTypeClass->UIName, pBuildingTypeClass->ID);
					break;

				}
			}
		}
			break;
		}
	});

	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() {

		DrawHouseInfo();
	});
	GeneralHook::LogicClassUpdateLateEvent.Subscribe([]() {
		
		ChargeSuperWeapon();
		
		
	});
	GeneralHook::LogicClassInitEvent.Subscribe([]() {
	
		auto FirstFileA =  FindFirstFileA("__Mochi.ini",&FindFileData);
		if (FirstFileA == (HANDLE) - 1) {
			return;
		}
	});
	GeneralHook::ScenarioStartEvent.Subscribe([]() {

		


	});
	FactoryHook::ProgressUpdateEvent.Subscribe([](FactoryClass* pFactory) {
		if (!pFactory) {
			return;
		}
		if (!pFactory->Owner) {
			return;
		}
		if (pFactory->Owner != HouseClass::CurrentPlayer) {
			return;
		}
		if (!pFactory->Object) {
			return;
		}
		if (pFactory->Production.Value == 54) {
			return;
		}
		if (pFactory->Owner->Available_Money() < pFactory->Balance) {
			return;
		}
		auto event = (EventClass*)malloc(sizeof(EventClass));
		memset(event, 0, sizeof(EventClass));


		event->Type = (EventType)MochiEventType::CoraCompleteProduction;
		event->HouseIndex = HouseClass::CurrentPlayer->ArrayIndex;
		event->Produce.RTTIType = pFactory->Object->WhatAmI();
		event->Produce.IsNaval = pFactory->Object->GetTechnoType()->Naval;
		event->Produce.HeapID = pFactory->Object->GetArrayIndex();
		EventHook::AddEvent(event);
		free(event);
	});
	isRegistered = true;
}


