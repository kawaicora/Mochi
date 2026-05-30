#pragma once
#include <EventSystem.h>
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