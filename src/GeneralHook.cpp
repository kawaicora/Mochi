
#include "GeneralHook.h"
#include <Helpers/Macro.h>
#include <YRPP.h>
#include "MochiGame.h"
Event<> GeneralHook::YRBootEvent;
Event<GeneralHook::CmdLineArgs> GeneralHook::CmdLineParseEvent;
Event<const char*, va_list> GeneralHook::DebugLogEvent;
Event<> GeneralHook::ExeTerminateEvent;
Event<> GeneralHook::LogicClassInitEvent;
Event<> GeneralHook::ScenarioStartReadSceneINIEvent;
Event<> GeneralHook::ScenarioStartEvent;
Event<> GeneralHook::CommandClassRegisterEvent;
Event<> GeneralHook::ScenarioClearClassesEvent;
Event<> GeneralHook::LogicClassUpdateEvent;
Event<> GeneralHook::MessagePumpEvent;
Event<> GeneralHook::LogicClassUpdateLateEvent;
Event<> GeneralHook::GScreenClassDrawOnTopEvent;

//**************************启动必须***************************************
DEFINE_HOOK(0x4A80D0, CD_AlwaysFindYR, 6)
{
	if (MochiGame::bNoCD) {
		R->EAX(2);
		return 0x4A8265;
	}
	return 0;
}

DEFINE_HOOK(0x4790E0, CD_AlwaysAvailable, 7)
{
	if (MochiGame::bNoCD) {
		R->AL(1);
		return 0x479109;
	}
	return 0;
}

DEFINE_HOOK(0x479110, CD_NeverAsk, 5)
{
	if (MochiGame::bNoCD) {
		R->AL(1);
		return 0x4791EA;
	}
	return 0;
}

DEFINE_HOOK(0x47AE36, CDFileClass_SetFileName, 8)
{
	GET(void*, CDControl, EAX);

	if (!CDControl || MochiGame::bNoCD) {
		return 0x47AEF0;
	}
	return 0x47AE3E;
}

DEFINE_HOOK(0x47B026, FileFindOpen, 8)
{
	GET(void*, CDControl, EBX);

	if (!CDControl || MochiGame::bNoCD) {
		return 0x47B0AE;
	}
	return 0x47B02E;
}

DEFINE_HOOK(0x49F5C0, CopyProtection_IsLauncherRunning, 8)
{
	R->AL(1);
	return 0x49F61A;
}

DEFINE_HOOK(0x49F620, CopyProtection_NotifyLauncher, 5)
{
	R->AL(1);
	return 0x49F733;
}

DEFINE_HOOK(0x49F7A0, CopyProtection_CheckProtectedData, 8)
{
	R->AL(1);
	return 0x49F8A7;
}

// this douchebag blows your base up when it thinks you're cheating
DEFINE_HOOK(0x55CFDF, BlowMeUp, 0)
{
	return 0x55D059;
}

//*******************************启动必须结束******************************************

DEFINE_HOOK(0x4068E0, DebugLog, 0x0) {
	return 0x4A4AC0;
}

#ifdef DEBUG_LOG_CONSOLE
DEFINE_HOOK(0x4A4AF9, DebugLogRet, 1)
{
	try {
		LEA_STACK(va_list const, args, 0x8);
		GET_STACK(const char* const, pFormat, 0x4);

		GeneralHook::DebugLogEvent.Invoke(pFormat, args);

	}
	catch(...){
	
	}
	return 0x4A4E96;
}
#endif

DEFINE_HOOK(0x7CD810, YRBoot, 0x9)
{
	GeneralHook::YRBootEvent.Invoke();
	return 0;
}
DEFINE_HOOK(0x52F639, YR_CmdLineParse, 0x5)
{
	GET(char**, ppArgs, ESI);
	GET(int, nNumArgs, EDI);
	GeneralHook::CmdLineArgs args;
	args.ppArgs = ppArgs;
	args.nNumArgs = nNumArgs;

	GeneralHook::CmdLineParseEvent.Invoke(args);
	return 0;
}


DEFINE_HOOK(0x532E37, CommandClassCallback_Register, 0)
{
	GeneralHook::CommandClassRegisterEvent.Invoke();
	DWORD* D = GameCreate<DWORD>();
	R->EAX(D);	//Allocate TypeSelectCommandClass
	return 0x532E41;
}

DEFINE_HOOK(0x7CD8EF, ExeTerminate, 9) {
	GeneralHook::ExeTerminateEvent.Invoke();
	return 0;
}

DEFINE_HOOK(0x6875F3, Scenario_Start_Read_Scene_INI, 6) {
	GeneralHook::ScenarioStartReadSceneINIEvent.Invoke();
	return  0;
}

// 游戏真正开始的时刻：0x689E90 尾部把 CurrentPlayer->IsHumanPlayer / IsInPlayerControl
// 强行置 1 之后，这里（cmp [0xA8B238], esi / 6 字节）才是房子与控制权定稿的点。
DEFINE_HOOK(0x687868, Scenario_Start, 6) {
	GeneralHook::ScenarioStartEvent.Invoke();
	return  0;
}

DEFINE_HOOK(0x685582, LogicClass_Init, 6) {
	GeneralHook::LogicClassInitEvent.Invoke();
	return  0;
}


DEFINE_HOOK(0x685659, Scenario_ClearClasses, 0xA) {
	return 0;
}
DEFINE_HOOK(0x55AFB3, LogicClass_Update, 0x6) {
	GeneralHook::LogicClassUpdateEvent.Invoke();
	return 0;
}

DEFINE_HOOK(0x55B719, LogicClass_Update_Late, 0x5) {
	GeneralHook::LogicClassUpdateLateEvent.Invoke();
	return 0;
}

DEFINE_HOOK(0x4F4583, GScreenClass_DrawOnTop_TheDarkSideOfTheMoon, 6)
{
	GeneralHook::GScreenClassDrawOnTopEvent.Invoke();
	return 0;
}

// The game's own PeekMessage wrapper (0x53E770). Every window message pump in
// the game goes through it, lobby / game-room / in-match alike, so this is the
// one place that ticks in all of them. The first two instructions are
// "sub esp,0x34" (3 bytes) and "mov eax,[0xAA0444]" (5 bytes); 8 bytes covers
// both without splitting an instruction. ECX/EDX carry live arguments into the
// body, so they are preserved around the call.
DEFINE_HOOK(0x53E770, MessagePump, 8)
{
	__asm push eax
	__asm push ecx
	__asm push edx
	GeneralHook::MessagePumpEvent.Invoke();
	__asm pop edx
	__asm pop ecx
	__asm pop eax
	return 0;
}