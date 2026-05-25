#pragma once
#include "EventSystem.h"
#include <Windows.h>
#include "Debug.h"
#include <iostream>

class GeneralHook
{
public:
	struct CmdLineArgs {
		char** ppArgs;
		int nNumArgs;
	};
	static Event<> YRBootEvent;
	static Event<> ExeTerminateEvent;
	static Event<> ScenarioStartEvent;
	static Event<> CommandClassRegisterEvent;
	static Event<> LogicClassInitEvent;
	static Event<> ScenarioClearClassesEvent;
	static Event<> LogicClassUpdateEvent;
	static Event<> LogicClassUpdateLateEvent;
	static Event<> GScreenClassDrawOnTopEvent;
	static Event<GeneralHook::CmdLineArgs> CmdLineParseEvent;
	

};
Event<> GeneralHook::YRBootEvent;
Event<GeneralHook::CmdLineArgs> GeneralHook::CmdLineParseEvent;
Event<> GeneralHook::ExeTerminateEvent;
Event<> GeneralHook::ScenarioStartEvent;
Event<> GeneralHook::LogicClassInitEvent;
Event<> GeneralHook::CommandClassRegisterEvent;
Event<> GeneralHook::ScenarioClearClassesEvent;
Event<> GeneralHook::LogicClassUpdateEvent;
Event<> GeneralHook::LogicClassUpdateLateEvent;
Event<> GeneralHook::GScreenClassDrawOnTopEvent;


DEFINE_HOOK(0x4068E0, DebugLog, 0x0) {

	return 0x4A4AC0;
}

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