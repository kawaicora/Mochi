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
#include <MochiNetwork.h>
#include <iostream>

#ifdef ENABLE_EXT_NETWORK
// Forward declaration - Network.h includes winsock2.h which conflicts with windows.h
class Network {
public:
	static bool Init(const char* host, unsigned short port);
	static void Shutdown();
};
// Same reason: UdpHook.h pulls in winsock2.h, so declare the entry point here.
void UdpHookDrain();
#include <MochiINI.h>
#endif
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

		// 随机序列号：生成128位数字追加到 Woldata.key
		{
			srand((unsigned)GetTickCount());
			char keyStr[129];
			for (int i = 0; i < 128; i++) {
				keyStr[i] = '0' + (rand() % 10);
			}
			keyStr[128] = '\0';

			char exePath[MAX_PATH];
			GetModuleFileNameA(NULL, exePath, MAX_PATH);
			std::string dir(exePath);
			auto slash = dir.find_last_of('\\');
			if (slash != std::string::npos) dir = dir.substr(0, slash);

			std::string keyPath = dir + "\\Woldata.key";
			FILE* fp = nullptr;
			fopen_s(&fp, keyPath.c_str(), "a");
			if (fp) {
				fprintf(fp, "%s\\SETUP.INI", keyStr);
				fclose(fp);
			}
		}

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

		// Mirror the game's own log stream (and ours, which goes through it) to
		// debug\debug.log. va_copy keeps the va_list reusable for other
		// subscribers, e.g. the console one below.
		GeneralHook::DebugLogEvent.Subscribe([](const char* pFormat, va_list pArg) {
			char buf[2048] = { 0 };
			va_list copy;
			va_copy(copy, pArg);
			__try
			{
				vsprintf_s(buf, pFormat, copy);
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				buf[0] = 0;
			}
			va_end(copy);
			if (buf[0]) Debug::LogToFile(buf);
			});

#ifdef DEBUG_LOG_CONSOLE
		Debug::CreateConsoleAndSetUTF8();
		GeneralHook::DebugLogEvent.Subscribe([](const char* pFormat, va_list pArg) {
			char buf[2048] = { 0 };
			va_list copy;
			va_copy(copy, pArg);
			__try
			{
				vsprintf_s(buf, pFormat, copy); // MSVC three-arg overload, same as Ares
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				strcpy(buf, "[Mochi ERROR: Bad format string]");
			}
			va_end(copy);
			std::cout << buf;
			});
#endif

#endif

#if defined(DEBUG) && defined(WAIT_DEBUGGER_ATTACH)
		Debug::Log("开始等待调试器附加");
		DebugTools::WaitDebuggerAttachAndBreak();
#endif


		Debug::Log("Moshi Booted Successfully QvQ\n");

		// Initialize MochiNetwork
		MochiNetwork::Initialize();

#ifdef ENABLE_EXT_NETWORK
		// Initialize external network relay (TCP -> UDP injection)
		MochiINI::INIReader.LoadINI("mochi.ini");
		auto serverConfig = (MochiINI::Section*)MochiINI::INIReader.GetSection("ServerConfig");
		if (!serverConfig) {
			serverConfig = &MochiINI::INIReader["ServerConfig"];
			MochiINI::INIReader["ServerConfig"]["Address"].Raw = "app.kawaimoe.org:36211";
			MochiINI::INIReader.SaveINI("mochi.ini");
		}
		std::string serverAddress = serverConfig->GetValueByKey<std::string>("Address", "app.kawaimoe.org:36211");
		auto colon = serverAddress.find_last_of(':');
		std::string host = serverAddress.substr(0, colon);
		unsigned short port = (unsigned short)atoi(serverAddress.substr(colon + 1).c_str());
		Debug::Log("ExtNetwork: %s:%d\n", host.c_str(), port);
		Network::Init(host.c_str(), port);
#endif

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
		MakeCommand<GiveMoneyCommandClass>();
		MakeCommand<GenUnitCommandClass>();
		MakeCommand<DrawGameObjectInfoCommandClass>();
		MakeCommand<DrawHouseInfoCommandClass>();

		
		
		MakeCommand<LaunchSuperWeaponACommandClass>();
		MakeCommand<LaunchSuperWeaponBCommandClass>();
		MakeCommand<LaunchSuperWeaponCCommandClass>();
		MakeCommand<LaunchSuperWeaponDCommandClass>();
		
		MakeCommand<MoveCommandClass>();
		MakeCommand<AttackCommandClass>();

		
		//Testing...
		
		MakeCommand<TestCommandClass>();

	});
	
	GeneralHook::ScenarioStartEvent.Subscribe([]() {
		Debug::Log("Scenario Started\n");
		//p = SurfaceDebug::CreateSurfaceWindow(DSurface::Sidebar, L"Sidebar",false);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Hidden, L"Hidden", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Primary, L"Primary", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Tile, L"Title", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Alternate, L"Alternate", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Temp, L"Temp", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Composite, L"Composite", true);
		//MochiGame::PlayMovie("V_001");

		bool isAIControl =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsAIControl", true);
		MochiGame::SetCurrentPlayerAIControl(isAIControl);
		bool isUnlockAllTech =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsUnlockAllTech", true);
		MochiEvent::SendUnlockAllTechEvent(HouseClass::CurrentPlayer, isUnlockAllTech);
		
	});
	GeneralHook::LogicClassInitEvent.Subscribe([]() {
		Debug::Log("Logic Class Initialized\n");
	});


	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() {
#ifdef ENABLE_EXT_NETWORK
		// Injection point that actually runs: the logic-update hooks are not
		// reached in this build, and this draw hook fires every frame both in
		// the lobby and in game. Rendering never touches the network stack, so
		// handing packets over here is safe.
		UdpHookDrain();
#endif
		bool isDrawHouseInfo =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsDrawHouseInfo", true);
		if (isDrawHouseInfo) MochiGame::DrawHouseInfo();
		

		bool isDrawGameObjectInfo =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsDrawGameObjectInfo", true);
		if (isDrawGameObjectInfo) MochiGame::DrawAllGameObjectInfo(false, true);
		
		bool isDrawAllFactoryProduction =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsDrawAllFactoryProduction", true);
		if (isDrawAllFactoryProduction) MochiGame::DrawAllFactoryProduction();
		
	});

	GeneralHook::MessagePumpEvent.Subscribe([]() {
#ifdef ENABLE_EXT_NETWORK
		// Runs on the game thread inside the window message pump, which is the
		// only loop active in the lobby / game-room screens as well as in a
		// match, so relayed packets keep flowing in every phase.
		UdpHookDrain();
#endif
	});

	GeneralHook::LogicClassUpdateEvent.Subscribe([]() {
#ifdef ENABLE_EXT_NETWORK
		// Inject relay packets on the game thread, once per logic frame, with a
		// per-frame budget. Draining from the sendto hook instead caused the
		// whole queue to be swallowed in one tick whenever the game paused
		// sending, and that burst crashed the client.
		UdpHookDrain();
#endif
	});

#ifdef ENABLE_EXT_NETWORK
	GeneralHook::ExeTerminateEvent.Subscribe([]() {
		Network::Shutdown();
	});
#endif

	GeneralHook::LogicClassUpdateLateEvent.Subscribe([]() {
#ifdef ENABLE_EXT_NETWORK
		// Second injection point: late in the frame, after the logic has run.
		UdpHookDrain();
#endif
		bool isAutoChargePlayerAllSuperweapon =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsAutoChargePlayerAllSuperweapon", true);
		if (isAutoChargePlayerAllSuperweapon) {
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
		bool isInstantConstruction =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsInstantConstruction", true);
		if (isInstantConstruction) {
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


