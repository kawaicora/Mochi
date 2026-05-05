
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"
#include "Utilities/Macro.h"
#include "EventSystem.h"
#include "EventClass.h"
#include "General.h"
HANDLE Mochi::hInstance = 0;
bool Mochi::isRegistered = false;

bool __stdcall DllMain(HANDLE hInstance, DWORD dwReason, LPVOID v)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		Mochi::hInstance = hInstance;
		Mochi::RegisterEvent();
		break;
	case DLL_PROCESS_DETACH:
		
		break;

	default:
		break;
	} 
	return true;
}



void Mochi::RegisterEvent() {
	if (isRegistered) {
		return;
	}
	EventClass::NetworkingRespondToEvent.Subscribe([](EventType* e) {
		Debug::Log("Recv Event ID: %s\n", EventClass::EventTypeToString(*e));
	});
	General::LogicClassUpdateLateEvent.Subscribe([]() {

	});

	isRegistered = true;
}

