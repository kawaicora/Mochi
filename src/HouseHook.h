#pragma once
#include <string>
#include <EventSystem.h>
#include <Helpers/Macro.h>
#include <MochiHouse.h>
class HouseHook {
public:
	static Event<HouseClass*> HouseClassCreateEvent;
};