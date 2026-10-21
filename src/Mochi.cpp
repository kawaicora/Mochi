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
#include <SurfaceDebug.h>
#include <DrawCapture.h>
#include <PreviewWindow.h>
#include <iostream>
HANDLE Mochi::hInstance = 0;
bool Mochi::isRegistered = false;
std::vector<HWND> hwnds;
HWND p;
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


	GeneralHook::YRBootEvent.Subscribe([]() {


#ifdef DEBUG
		DebugTools::DetachFromDebugger();

#ifdef DEBUG_LOG_CONSOLE
		Debug::CreateConsoleAndSetUTF8();
		GeneralHook::DebugLogEvent.Subscribe([](const char* pFormat, va_list pArg) {
			char buf[2048] = { 0 };
			__try
			{
				vsprintf_s(buf, pFormat, pArg); // MSVC三参数模板重载，Ares同款
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				strcpy(buf, "[Mochi ERROR: Bad format string]");
			}
			std::cout << buf;
			});
#endif

#endif

#if defined(DEBUG) && defined(WAIT_DEBUGGER_ATTACH)
		Debug::Log("开始等待调试器附加");
		DebugTools::WaitDebuggerAttachAndBreak();
#endif


		Debug::Log("Moshi Booted Successfully QvQ\n");

		});
	RadarHook::RadarDrawEvent.Subscribe([]() {
		if (!RadarHook::SkipRadarDraw)
		{
			//RadarHook::SkipRadarDraw = true;
			//Debug::LogW("禁用小地图");

		}
		else {
			//RadarHook::DrawRadar();
		}
		//SurfaceDebug::RenderSurfaceWindow(p);
	});

	GeneralHook::CmdLineParseEvent.Subscribe([](GeneralHook::CmdLineArgs args) {
		Debug::LogW(L"Mochi 挂载成功 QvQ\n");
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
		MakeCommand<MoveCommandClass>();
		MakeCommand<AttackCommandClass>();
		
		//Testing...
		
		MakeCommand<TestCommandClass>();

	});
	
	GeneralHook::ScenarioStartEvent.Subscribe([]() {
		Debug::Log("Scenario Started\n");
		DrawCapture::Install();
		PreviewWindow::Create();
		//p = SurfaceDebug::CreateSurfaceWindow(DSurface::Sidebar, L"Sidebar",false);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Hidden, L"Hidden", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Primary, L"Primary", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Tile, L"Title", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Alternate, L"Alternate", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Temp, L"Temp", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Composite, L"Composite", true);
		//MochiGame::PlayMovie("V_001");
		
	});
	GeneralHook::LogicClassInitEvent.Subscribe([]() {
		Debug::Log("Logic Class Initialized\n");
	});


	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() {

		MochiGame::DrawHouseInfo();
		MochiGame::DrawAllGameObjectInfo(false, true);
		MochiGame::DrawAllFactoryProduction();
	});

	GeneralHook::LogicClassUpdateEvent.Subscribe([]() {
		DrawCapture::Publish();
		PreviewWindow::RenderFrame();
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

			case (EventType)MochiEventType::CoraMessage:
			{
				MochiEvent::CoraMessage(data);

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

	isRegistered = true;
}


