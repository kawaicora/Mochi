#include "Mochi.h"
#include <Windows.h>
#include <Helpers/Macro.h>
#include <vector>
#include <string>
#include <GeneralHook.h>
#include <DebugTools.h>
#include <Debug.h>
#include <MochiINI.h>
#include <MochiGame.h>
#include <MochiCommandClass.h>
#include <FactoryHook.h>
#include <MochiEventType.h>
#include <EventData.h>
#include <EventHook.h>

HANDLE Mochi::hInstance = 0;
bool Mochi::isRegistered = false;

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
		MochiINI::INIReader.LoadINI("mochi.ini");
		MochiINI::PlayerBaseConfig = (MochiINI::Section*)MochiINI::INIReader.GetSection("PlayerBaseConfig");

		if (MochiINI::PlayerBaseConfig)
		{
			for (auto& v : MochiINI::PlayerBaseConfig->Keys)
			{
				Debug::Log(
					"%s %s\n",
					v.first.c_str(), v.second.c_str());
			}
		}
		Debug::Log("###############################INILoadConfigEnd######################################\n");
	});

	GeneralHook::CommandClassRegisterEvent.Subscribe([]() {
		MakeCommand<AIControlCommandClass>();
		MakeCommand<InstantConstructionCommandClass>();
		MakeCommand<AutoChargePlayerAllSuperweaponCommandClass>();
		MakeCommand<UnlockAllUnitCommandClass>();
		MakeCommand<LaunchSuperWeaponACommandClass>();
		MakeCommand<LaunchSuperWeaponBCommandClass>();
		MakeCommand<LaunchSuperWeaponCCommandClass>();
		MakeCommand<LaunchSuperWeaponDCommandClass>();
		MakeCommand<GiveMoneyCommandClass>();
		MakeCommand<GenUnitCommandClass>();
		MakeCommand<PlayMovieCommandClass>();
		MakeCommand<MoveCommandClass>();
		MakeCommand<AttackCommandClass>();
		
		//Testing...
		
		MakeCommand<ActiveAllSuperWeaponCommandClass>();

	});

	GeneralHook::ScenarioStartEvent.Subscribe([]() {
		Debug::Log("Scenario Started\n");
		MochiGame::PlayMovie("V_001");
		//Game::EnableMPDebug = true;
		//Game::EnableMPSyncDebug = true;
		//Game::DrawMPDebugStats = true;
	});
	GeneralHook::LogicClassInitEvent.Subscribe([]() {
		Debug::Log("Logic Class Initialized\n");
	});



	EventHook::NetworkingRespondToEvent.Subscribe([](EventData* data) {
		Debug::Log("Receive Event Type: 0x%02X HouseIndex: %d Frame: %d\n", (int)data->Type, data->HouseIndex, data->Frame);
		switch (data->Type) {
			//****Mochi事件处理****//
			case (EventType)MochiEventType::CoraMoneyChange:
			{
				MochiEvent::CoraMoneyChange(data);
				break;
			}
			case (EventType)MochiEventType::CoraCompleteProduction:
			{

				MochiEvent::CoraCompleteProduction(data);
				break;
			}
			case (EventType)MochiEventType::CoraSuperWeaponCharge:
			{
				MochiEvent::CoraSuperWeaponCharge(data);
				break;
			}
			case (EventType)MochiEventType::CoraSpecialPlace:
			{
				MochiEvent::CoraSpecialPlace(data);
				break;
			}
			case (EventType)MochiEventType::CoraActiveSuperWeapon:
			{
				MochiEvent::CoraActiveSuperWeapon(data);
				break;
			}
			case (EventType)MochiEventType::CoraPlace:
			{
				MochiEvent::CoraPlace(data);
				break;
			}

			case (EventType)MochiEventType::CoraUnlockAllTech:
			{
				MochiEvent::CoraUnlockAllTech(data);
				break;
			}
			//****原有事件处理****//
			case EventType::FrameSync:
			{
				MochiEvent::OriginalFrameSyncEvent(data);
				break;
			}
			case EventType::FrameInfo:
			{
				MochiEvent::OriginalFrameInfoEvent(data);
				break;
			}
			case EventType::Place:
			{
				MochiEvent::OriginalPlaceEvent(data);
				break;
			}

			case EventType::Produce:
			{
				MochiEvent::OriginalProduceEvent(data);
				break;
			}


			case EventType::MegaMission:
			{
				MochiEvent::OriginalMegaMissionEvent(data);
				break;
			}
			case EventType::MegaMissionF:
			{
				MochiEvent::OriginalMegaMissionFEvent(data);
				break;
			}
		}//switch (data->Place.RTTIType)
	});

	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() {

		MochiGame::DrawHouseInfo();
		MochiGame::DrawAllGameObjectInfo(false, true);
		MochiGame::DrawAllFactoryProduction();
		
		//MochiEvent::DrawRadarTest();
		//MochiUtilities::UpdateScript();
		//MochiUtilities::Render();
	});

	GeneralHook::LogicClassUpdateEvent.Subscribe([]() {
		
	});

	GeneralHook::LogicClassUpdateLateEvent.Subscribe([]() {
		if (MochiHouse::IsAutoChargePlayerAllSuperweapon) {
			MochiGame::ChargeAllSuperWeapon(HouseClass::CurrentPlayer);
		}
		
		//Debug::Log("Current cell: %d  %d", DisplayClass::Instance.Display_ZoneCell.X, DisplayClass::Instance.Display_ZoneCell.Y);

	});

	HouseHook::HouseClassCreateEvent.Subscribe([](HouseClass* pHouseClass) {
		if (!pHouseClass) {
			Debug::Log("pHouseClass is Zero\n");
			return;
		}

		Debug::LogW(L"House Class Created %ls %S \n", MochiHouse::GetPlayerNameByHouseIndex(pHouseClass->ArrayIndex), pHouseClass->Type->ID);
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
		if (!pFactory->Object) {
			return;
		}
		Debug::LogW(L"Factory Class Created Owner %ls %S\n", pFactory->Owner->Type->UIName, pFactory->Owner->Type->ID);

	});

	FactoryHook::ProgressUpdateEvent.Subscribe([](FactoryClass* pFactory) {
		if (MochiHouse::IsPlayerInstantConstruction) {
			MochiEvent::SendCompleteProduceEvent(HouseClass::CurrentPlayer, pFactory);
		}
	});
	isRegistered = true;
}


