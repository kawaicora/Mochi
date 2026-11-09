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
#include <MovieHook.h>
#include <iostream>

#ifdef ENABLE_EXT_NETWORK
// Forward declaration - Network.h includes winsock2.h which conflicts with windows.h
class Network {
public:
	static bool Init(const char* host, unsigned short port);
	static void Shutdown();
};
// Same reason: UdpHook.h pulls in winsock2.h, so declare the entry points here.
void UdpHookDrain();
void UdpHookInit();
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

		//
		UdpHookInit();
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
		Debug::Log("INILoadConfig  mochi.ini \n");
		MochiINI::INIReader.LoadINI("mochi.ini");
		MochiINI::PlayerBaseConfig = (MochiINI::Section*)MochiINI::INIReader.GetSection("PlayerBaseConfig");
		if (!MochiINI::PlayerBaseConfig) {
			// 配置不存在时，创建 Section
			MochiINI::PlayerBaseConfig =
				&MochiINI::INIReader["PlayerBaseConfig"];
			MochiINI::INIReader["PlayerBaseConfig"]["LaunchSuperWeaponList"].Raw = "NukeSpecial, PsychicDominatorSpecial, LightningStormSpecial, IronCurtainSpecial";
			MochiINI::INIReader["PlayerBaseConfig"]["IsInstantConstruction"].Raw = "false";
			MochiINI::INIReader["PlayerBaseConfig"]["IsAutoChargePlayerAllSuperweapon"].Raw = "false";
			MochiINI::INIReader["PlayerBaseConfig"]["IsUnlockAllTech"].Raw = "false";
			MochiINI::INIReader["PlayerBaseConfig"]["IsAIControl"].Raw = "false";
			MochiINI::INIReader["PlayerBaseConfig"]["IsDrawGameObjectInfo"].Raw = "false";
			MochiINI::INIReader["PlayerBaseConfig"]["IsDrawHouseInfo"].Raw = "false";
			
			
		}

		MochiINI::INIReader.SaveINI("mochi.ini");

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
	
	GeneralHook::ScenarioStartReadSceneINIEvent.Subscribe([]() {
		Debug::Log("Scenario Started\n");

		// rules INI 已完整读取并解析：打印 [General] MochiLoadLog 里存的字符串。
		// 例：MochiLoadLog=MOCHI加载游戏RULES_INI完成
		// 只打印一次，避免每局都刷一行。
		static bool s_RulesLoadLogPrinted = false;
		if (!s_RulesLoadLogPrinted) {
			if (CCINIClass* pRules = CCINIClass::INI_Rules) {
				char buffer[0x200] = {};
				if (pRules->ReadString("General", "MochiLoadLog", "", buffer, sizeof(buffer)) > 0
					&& buffer[0]) {
					Debug::Log("%s\n", buffer);
					s_RulesLoadLogPrinted = true;
				}
			}
		}

		//p = SurfaceDebug::CreateSurfaceWindow(DSurface::Sidebar, L"Sidebar",false);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Hidden, L"Hidden", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Primary, L"Primary", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Tile, L"Title", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Alternate, L"Alternate", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Temp, L"Temp", true);
		//SurfaceDebug::CreateSurfaceWindow(DSurface::Composite, L"Composite", true);
		//MochiGame::PlayMovie("V_001");

	});

	// 游戏真正开始（0x687868）：0x689E90 已经把 CurrentPlayer 的
	// IsHumanPlayer / IsInPlayerControl 置 1，这里再按 mochi.ini 覆盖才不会被冲掉。
	GeneralHook::ScenarioStartEvent.Subscribe([]() {
		Debug::Log("Scenario Started (game really starting)\n");

		bool isAIControl =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsAIControl", false);
		MochiGame::SetCurrentPlayerAIControl(isAIControl);
		bool isUnlockAllTech =
			MochiINI::PlayerBaseConfig->GetValueByKey<bool>(
				"IsUnlockAllTech", false);
		MochiEvent::SendUnlockAllTechEvent(HouseClass::CurrentPlayer, isUnlockAllTech);

	});
	// 电影播放（全屏过场与雷达视频共用一套 hook）
	// 字幕需要实时播放时间时，订阅 MovieTimeEvent 读 info.CurrentTimeMs 即可。
	MovieHook::MovieStartEvent.Subscribe([](const MovieHook::MovieInfo& info) {
		Debug::Log("[Movie] start: %dx%d, %d frames, %d fps, duration=%dms\n",
			info.Width, info.Height, info.TotalFrames, info.Fps, info.DurationMs);
	});
	MovieHook::MovieTimeEvent.Subscribe([](const MovieHook::MovieInfo& info) {
		// 示例只按整秒打点，避免每帧刷日志；做字幕时直接用 CurrentTimeMs
		static void* s_lastAnim = nullptr;
		static int s_lastSecond = -1;
		int second = info.CurrentTimeMs / 1000;
		if (s_lastAnim != info.pAnim || s_lastSecond != second) {
			s_lastAnim = info.pAnim;
			s_lastSecond = second;
			Debug::Log("[Movie] time: frame %d/%d = %dms\n",
				info.CurrentFrame, info.TotalFrames, info.CurrentTimeMs);
		}
	});
	MovieHook::MovieEndEvent.Subscribe([](const MovieHook::MovieInfo& info) {
		Debug::Log("[Movie] end: frame %d/%d = %dms\n",
			info.CurrentFrame, info.TotalFrames, info.CurrentTimeMs);
	});
	MovieHook::MoviePauseEvent.Subscribe([](const MovieHook::MovieInfo& info) {
		Debug::Log("[Movie] paused at %dms (frame %d)\n",
			info.CurrentTimeMs, info.CurrentFrame);
	});
	MovieHook::MovieResumeEvent.Subscribe([](const MovieHook::MovieInfo& info) {
		Debug::Log("[Movie] resumed at %dms (frame %d)\n",
			info.CurrentTimeMs, info.CurrentFrame);
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


