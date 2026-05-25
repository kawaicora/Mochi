
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"
#include <Helpers/Macro.h>
#include "EventHook.h"
#include "GeneralHook.h"
#include "FactoryHook.h"
#include "TechnoHook.h"
#include <vector>
#include <YRpp.h>
#include <HouseHook.h>

#include "DebugTools.h"
#include "MochiGame.h"
#include <SomeCommandClass.h>

HANDLE Mochi::hInstance = 0;
bool Mochi::isRegistered = false;
MochiINIReader Mochi::INIReader;
MochiINIReader::Section* Mochi::PlayerBaseConfig;
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

//#define WAIT_DEBUGGER_ATTACH
//#define DEBUGGER_AUTO_BREAK
/// <summary>
/// 注册事件
/// </summary>
void Mochi::RegisterEvent() {
	if (isRegistered) {
		return;
	}
	GeneralHook::YRBootEvent.Subscribe([]() {
		Debug::Log("Moshi Booted Successfully QvQ\n");
#ifdef DEBUG 
		//MessageBoxW(NULL, L"按下确认后开始等待调试器附加\n附加后自动触发断点", L"确认", MB_OK);
		DebugTools::DetachFromDebugger();
#elif DEBUG and WAIT_DEBUGGER_ATTACH
		Debug::Log("开始等待调试器附加");
		DebugTools::WaitDebuggerAttachAndBreak();
#elif DEBUG and DEBUGGER_AUTO_BREAK
		Debug::Log("自动触发断点");
		DebugTools::WaitDebuggerAttachAndBreak();

#endif



		});
	GeneralHook::CmdLineParseEvent.Subscribe([](GeneralHook::CmdLineArgs args) {
		Debug::Log("Moshi 挂载成功 QvQ\n");
		Debug::Log("###############################INILoadConfig######################################\n");
		INIReader.LoadINI("__Mochi.ini");
		PlayerBaseConfig = (MochiINIReader::Section*) INIReader.GetSection("PlayerBaseConfig");

		if (PlayerBaseConfig)
		{
			for (auto& v : PlayerBaseConfig->Keys)
			{
				Debug::Log(
					"%s %s\n",
					v.first.c_str(), v.second.c_str());
			}
		}
		Debug::Log("###############################INILoadConfigEnd######################################\n");
	});

	GeneralHook::CommandClassRegisterEvent.Subscribe([](){
		MakeCommand<AIControlCommandClass>();
		MakeCommand<InstantConstructionCommandClass>();
		MakeCommand<AutoChargePlayerAllSuperweaponCommandClass>();
		MakeCommand<UnlockAllUnitCommandClass>();
		MakeCommand<LaunchSuperWeaponACommandClass>();
		MakeCommand<LaunchSuperWeaponBCommandClass>();
		MakeCommand<LaunchSuperWeaponCCommandClass>();
		MakeCommand<LaunchSuperWeaponDCommandClass>();

		//Testing...
		MakeCommand<GenUnitCommandClass>();
		MakeCommand<ActiveAllSuperWeaponCommandClass>();
		
	});



	EventHook::NetworkingRespondToEvent.Subscribe([](EventData* data) {
		Debug::Log("Receive Event Type: %d HouseIndex: %d Frame: %d\n", (int)data->Type, data->HouseIndex, data->Frame);	
		switch (data->Type) {
			case (EventType)MochiEventType::CoraCompleteProduction:
			{
				
				MochiGame::CoraCompleteProduction(data);
				break;
			}
			

			case (EventType)MochiEventType::CoraSuperWeaponCharge: 
			{
				MochiGame::CoraSuperWeaponCharge(data);
				break;
			}
			case (EventType)MochiEventType::CoraSpecialPlace:
			{
				MochiGame::CoraSpecialPlace(data);
				break;
			}
			case (EventType)MochiEventType::CoraActiveSuperWeapon:
			{
				MochiGame::CoraActiveSuperWeapon(data);
				break;
			}
			case (EventType)MochiEventType::CoraPlace:
			{
				MochiGame::CoraPlace(data);
				break;
			}

			case (EventType)MochiEventType::CoraUnlockAllTech:
			{
				MochiGame::CoraUnlockAllTech(data);
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
				break;
			}
			
		}//switch (data->Place.RTTIType)
	});

	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() {

		MochiGame::DrawHouseInfo();
	});
	GeneralHook::LogicClassUpdateLateEvent.Subscribe([]() {
		if (HouseHook::IsAutoChargePlayerAllSuperweapon) {
			MochiGame::ChargeAllSuperWeapon(HouseClass::CurrentPlayer);
		}
		
		//Debug::Log("Current cell: %d  %d", DisplayClass::Instance.Display_ZoneCell.X, DisplayClass::Instance.Display_ZoneCell.Y);
		
	});

	HouseHook::HouseClassCreateEvent.Subscribe([](HouseClass* pHouseClass) {
		if (!pHouseClass) {
			Debug::Log("pHouseClass is Zero\n");
			return;
		}
		
		Debug::LogW(L"House Class Created %ls %S \n", HouseHook::GetPlayerNameByHouseIndex(pHouseClass->ArrayIndex), pHouseClass->Type->ID);
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
		swprintf(msg, L"%ls 开始建造 %ls", HouseHook::GetPlayerNameByHouseIndex(pFactory->Owner->ArrayIndex), pFactory->Object->GetTechnoType()->UIName);
		auto color = ColorScheme::Red;
		if (pFactory->Owner->IsAlliedWith(HouseClass::CurrentPlayer)) {
			color = ColorScheme::Green;
		}

		MessageListClass::Instance.PrintMessage(msg,300, color);
		if (pFactory->Owner != HouseClass::CurrentPlayer) {
			return;
		}
		//pFactory->Owner->TransactMoney( pFactory->Balance);
		
	});
	FactoryHook::ProgressUpdateEvent.Subscribe([](FactoryClass* pFactory) {
		if (HouseHook::IsPlayerInstantConstruction) {
			MochiGame::SendCompleteProduceEvent(HouseClass::CurrentPlayer,pFactory);
		}
	});
	isRegistered = true;
}


