
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"
#include "Utilities/Macro.h"
#include "EventHook.h"
#include "GeneralHook.h"
#include "FactoryHook.h"
#include "TechnoHook.h"
#include <vector>
#include <YRpp.h>
#include <HouseHook.h>
HANDLE Mochi::hInstance = 0;
bool Mochi::isRegistered = false;


void DrawHouseInfo() {
	

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
			HouseHook::GetPlayerNameByIndex(pHouse->ArrayIndex),
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
}
void ChargeSuperWeapon() {
	for (int i = 0;i < HouseClass::CurrentPlayer->Supers.Count; i++) {
		EventClass* event = EventHook::EventClass_CTOR();
		SuperClass* super = HouseClass::CurrentPlayer->Supers[i];
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
	GeneralHook::CmdLineParseEvent.Subscribe([](GeneralHook::CmdLineArgs args) {
		Debug::Log("Moshi 挂载成功 QvQ\n");
	});
	EventHook::NetworkingRespondToEvent.Subscribe([](EventClass* data) {
		
		switch (data->Type) {
			case (EventType)MochiEventType::CoraCompleteProduction:
			{
				FactoryClass* pFactory;
				HouseClass* pHouse = HouseClass::Array[data->HouseIndex];
				TechnoTypeClass* pTechnoTypeClass =  TechnoTypeClass::GetByTypeAndIndex(data->Produce.RTTIType, data->Produce.HeapID);
				switch (data->Produce.RTTIType) {
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
						BuildCat buildCat = (BuildCat) -1;
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
				}

				if (!pFactory) {
					Debug::Log("pFactory Is Zero At Try Complete Produce HeapID: %d RTTIType: %d\n", data->Produce.HeapID , (int)data->Produce.RTTIType);
					return;
				}
				FactoryHook::CompleteProdution(pFactory);
				Debug::LogW(L"CompleteProdece: %ls\n", pFactory->Object->GetTechnoType()->UIName);

				break;
			}
			

			case (EventType)MochiEventType::CoraSuperWeaponCharge: 
			{
				HouseClass* pHouse = HouseClass::Array[data->HouseIndex];

				SuperClass*  pSuper  = pHouse->Supers[data->SpecialPlace.ID];
				if (pSuper) {
					pSuper->SetCharge(100);
					Debug::LogW(L"SuperWeapon Charged : %ls\n", pSuper->Type->UIName);
				}
				else {
					Debug::Log("pSuper is zero");
				}
				break;
			}
			

			case EventType::Place:
			{
				if (data->Place.HeapID != -1) {
					switch (data->Place.RTTIType) {
						case AbstractType::Unit:
						case AbstractType::UnitType:
						{
							UnitTypeClass* pUnitTypeClass = UnitTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Place Unit %ls  %S \n", pUnitTypeClass->UIName, pUnitTypeClass->ID);
							break;
						}
						
						case AbstractType::Aircraft:
						case AbstractType::AircraftType:
						{
							AircraftTypeClass* pAircraftTypeClass = AircraftTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Place Aircraft %ls  %S \n", pAircraftTypeClass->UIName, pAircraftTypeClass->ID);
							break;
						}
						
						case AbstractType::Infantry:
						case AbstractType::InfantryType:
						{
							InfantryTypeClass* pInfantryTypeClass = InfantryTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Place Infantry %ls  %S \n", pInfantryTypeClass->UIName, pInfantryTypeClass->ID);
							break;
						}
						
						case AbstractType::Building:
						case AbstractType::BuildingType:
						{
							BuildingTypeClass* pBuildingTypeClass = BuildingTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Place Buiding %ls  %S \n", pBuildingTypeClass->UIName, pBuildingTypeClass->ID);
							break;
						}
						
					}
				}
				break;
			}
			
			case EventType::Produce:
			{
				if (data->Produce.HeapID != -1) {
					switch (data->Place.RTTIType) {
						case AbstractType::Unit:
						case AbstractType::UnitType:
						{
							UnitTypeClass* pUnitTypeClass = UnitTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Produce Unit %ls  %S \n", pUnitTypeClass->UIName, pUnitTypeClass->ID);
							break;
						}
					
						case AbstractType::Aircraft:
						case AbstractType::AircraftType:
						{
							AircraftTypeClass* pAircraftTypeClass = AircraftTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Produce Aircraft %ls  %S \n", pAircraftTypeClass->UIName, pAircraftTypeClass->ID);
							break;
						}
					
						case AbstractType::Infantry:
						case AbstractType::InfantryType:
						{
							InfantryTypeClass* pInfantryTypeClass = InfantryTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Produce Infantry %ls  %S \n", pInfantryTypeClass->UIName, pInfantryTypeClass->ID);
							break;
						}
					
						case AbstractType::Building:
						case AbstractType::BuildingType:
						{
							BuildingTypeClass* pBuildingTypeClass = BuildingTypeClass::Array[data->Place.HeapID];
							Debug::LogW(L"Produce Building %ls  %S \n", pBuildingTypeClass->UIName, pBuildingTypeClass->ID);
							break;
						}
					} 
				}
				break;
			}
			
		}//switch (data->Place.RTTIType)
	});

	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() {

		DrawHouseInfo();
	});
	GeneralHook::LogicClassUpdateLateEvent.Subscribe([]() {
		
		ChargeSuperWeapon();
		//Debug::Log("Current cell: %d  %d", DisplayClass::Instance.Display_ZoneCell.X, DisplayClass::Instance.Display_ZoneCell.Y);
		
	});

	HouseHook::HouseClassCreateEvent.Subscribe([](HouseClass* pHouseClass) {
		if (!pHouseClass) {
			Debug::Log("pHouseClass is Zero\n");
			return;
		}
		
		Debug::LogW(L"House Class Created %ls %S \n", pHouseClass->Type->UIName, pHouseClass->Type->ID);
		if (!HouseClass::CurrentPlayer) {
			return;
		}
		if (pHouseClass->ArrayIndex == HouseClass::CurrentPlayer->ArrayIndex) {
			Debug::Log("Current Player  House Created\n  ");
			
			return;
		}
		
	});
	FactoryHook::FactoryClassCreateEvent.Subscribe([](FactoryClass* pFactory) {
		if (!pFactory) {
			return;
		}
		if (!pFactory->Owner) {
			return;
		}
		Debug::LogW(L"Factory Class Created Owner %ls %S\n", pFactory->Owner->Type->UIName, pFactory->Owner->Type->ID);
		wchar_t msg[1024];
		swprintf(msg, L"%ls 开始建造 %ls", HouseHook::GetPlayerNameByIndex(pFactory->Owner->ArrayIndex), pFactory->Object->GetTechnoType()->UIName);
		auto color = ColorScheme::Red;
		if (pFactory->Owner->IsAlliedWith(HouseClass::CurrentPlayer)) {
			color = ColorScheme::Green;
		}

		MessageListClass::Instance.PrintMessage(msg,300, color);
		if (pFactory->Owner != HouseClass::CurrentPlayer) {
			return;
		}
		pFactory->Owner->TransactMoney( pFactory->Balance);
		
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
		if (pFactory->Production.Value == 54 || pFactory->SpecialItem != -1) {
			return;
		}
		if (pFactory->IsSuspended) {
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
		event->Produce.HeapID = TechnoHook::GetIndex(pFactory->Object->WhatAmI(), pFactory->Object->GetTechnoType());
		if (event->Produce.HeapID == -1) {
			return;
		}
		EventHook::AddEvent(event);
	
	});
	isRegistered = true;
}


