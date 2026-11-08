#pragma once
#include <string>
#include <EventSystem.h>
#include <Helpers/Macro.h>
#include <MochiHouse.h>

class HouseHook {
public:
	static Event<HouseClass*> HouseClassCreateEvent;

	// AI 接管时统计"还活着、且不是我的盟友"的 house 数量
	static int CountAliveNonAlliedHouses();
};
