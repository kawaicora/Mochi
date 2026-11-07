
#include "GeneralHook.h"
#include <Helpers/Macro.h>
#include <YRPP.h>
Event<> GeneralHook::YRBootEvent;
Event<GeneralHook::CmdLineArgs> GeneralHook::CmdLineParseEvent;
Event<const char*, va_list> GeneralHook::DebugLogEvent;
Event<> GeneralHook::ExeTerminateEvent;
Event<> GeneralHook::LogicClassInitEvent;
Event<> GeneralHook::ScenarioStartEvent;
Event<> GeneralHook::CommandClassRegisterEvent;
Event<> GeneralHook::ScenarioClearClassesEvent;
Event<> GeneralHook::LogicClassUpdateEvent;
Event<> GeneralHook::MessagePumpEvent;
Event<> GeneralHook::LogicClassUpdateLateEvent;
Event<> GeneralHook::GScreenClassDrawOnTopEvent;


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

DEFINE_HOOK(0x6875F3, Scenario_Start, 6) {
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