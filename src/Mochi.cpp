
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"
#include "Utilities/Macro.h"
#include "EventSystem.h"
#include "EventClass.h"
HANDLE Mochi::hInstance = 0;


bool __stdcall DllMain(HANDLE hInstance, DWORD dwReason, LPVOID v)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		Mochi::hInstance = hInstance;
		
		break;
	case DLL_PROCESS_DETACH:
		
		break;

	default:
		break;
	} 
	return true;
}



DEFINE_HOOK(0x7CD810, ExeRun, 0x9)
{

	Debug::Log("Mochi Engine Initialized\n");
	return 0;
}

DEFINE_JUMP(LJMP, 0x4068E0, 0x4A4AC0);


DEFINE_HOOK(0x7CD8EF, ExeTerminate, 9) {
	return 0;
}

auto token = EventClass::NetworkingRespondToEvent.Subscribe([](EventType* e) {
	Debug::Log("Recv Event ID: %s\n", EventClass::EventTypeToString(*e));
	});

void PlaceEventRsp() {

}