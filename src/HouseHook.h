#pragma once
#include <Windows.h>
#include <string>
#include<HouseClass.h>
#include<EventSystem.h>
#include "Utilities/Macro.h"


struct HouseClassName
{
	HouseClass* pHouseClass;
	NodeNameType* pNameNodeType = nullptr;
};
class HouseHook {
public:
	static Event<HouseClass*> HouseClassCreateEvent;
	static const wchar_t* GetPlayerNameByIndex(int index)
	{
		if (index < NodeNameType::Array.Count)
		{
			NodeNameType* pNameType = NodeNameType::Array[index];

			return pNameType->Name;
		}
		else
		{
			return HouseClass::Array[index]->Type->UIName;
		}
	}
};

Event<HouseClass*> HouseHook::HouseClassCreateEvent;

DEFINE_HOOK(0x4F6532, HouseClass_CTOR, 5) {
	GET(HouseClass*, pHouseClass, EAX);
	HouseHook::HouseClassCreateEvent.Invoke(pHouseClass);
	return 0;
}
