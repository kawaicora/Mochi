#pragma once

#pragma once
#include "EventSystem.h"
#include <Windows.h>
#include "Debug.h"
#include <iostream>
#include <FactoryClass.h>
#include <Helpers/Macro.h>
#include <HouseClass.h>
#include <Unsorted.h>

class FactoryHook
{
public:
	static Event<FactoryClass*> ProgressUpdateEvent;
	static Event<FactoryClass*> FactoryClassCreateEvent;
	static bool CompleteProdution(FactoryClass* pFactory);
};
Event<FactoryClass*> FactoryHook::ProgressUpdateEvent;
Event<FactoryClass*> FactoryHook::FactoryClassCreateEvent;


DEFINE_HOOK(0x4C9B20, FactoryClass_ProgressUpdate, 5) {
	GET(FactoryClass*, pFactory, ECX);
	FactoryHook::ProgressUpdateEvent.Invoke(pFactory);
	return 0;
}

DEFINE_HOOK(0x4C9DF6, FactoryClass_CTOR, 6) {
	GET(FactoryClass*, pFactory, ESI);
	FactoryHook::FactoryClassCreateEvent.Invoke(pFactory);
	return 0;
}


bool FactoryHook::CompleteProdution(FactoryClass* pFactory) {
	if (!pFactory) {
		return false;
	}
	if (!pFactory->Owner) {
		return false;
	}
	HouseClass* owner = pFactory->Owner;

	if (owner->Available_Money() >= pFactory->Balance) {
		owner->TakeMoney(pFactory->Balance);
		pFactory->Production.Value = 54;
		pFactory->Balance = 0;
		pFactory->Production.HasChanged = 1;
		pFactory->IsDifferent = 1;
		pFactory->IsSuspended = 1;
		pFactory->Production.Step = Unsorted::CurrentFrame;
		CDTimerClass timer{};
		pFactory->Production.Timer = timer;
	}

	

	return true;
}