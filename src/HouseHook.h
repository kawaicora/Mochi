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

	static const wchar_t* GetPlayerNameByHouseIndex(int index)
	{
		NodeNameType  NodeNameTypeList[8]{ -1,-1,-1,-1,-1,-1,-1,-1 };

		for (int i = 0; i < NodeNameType::Array.Count; i++)
		{
			NodeNameType * node = NodeNameType::Array[i];
			if (node->HouseIndex == index) {
				return  node->Name;
			}
		}

		return HouseClass::Array[index]->Type->UIName;
		
	};
private:
	
	
};

Event<HouseClass*> HouseHook::HouseClassCreateEvent;

DEFINE_HOOK(0x4F6532, HouseClass_CTOR, 5) {
	GET(HouseClass*, pHouseClass, EAX);
	HouseHook::HouseClassCreateEvent.Invoke(pHouseClass);
	return 0;
}
