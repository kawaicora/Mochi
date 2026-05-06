
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
		Debug::InitConsole();
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
	EventClass::NetworkingRespondToEvent.Subscribe([](EventData* data) {
		Debug::Log("Recv Event ID: %s\n", EventClass::EventTypeToString(data->Type));
		switch (data->Type) {
		case EventType::Produce:
			break;
		case EventType::Place:
			
				
			if (data->Place.HeapID != -1) {
				switch (data->Place.RTTIType) {
					case AbstractType::Building:
					case AbstractType::BuildingType:
						BuildingTypeClass* pBuildingTypeClass =  BuildingTypeClass::Array[data->Place.HeapID];
						Debug::Log("Place Bulilding %s  %s", pBuildingTypeClass->Name, pBuildingTypeClass->ID);
						break;
						
				}
			}
			break;
		}
	});
	General::LogicClassUpdateLateEvent.Subscribe([]() {

	});

	isRegistered = true;
}
