#pragma once
#include "EventSystem.h"

#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"

class General
{
public:
	static Event<> YRBootEvent;
	static Event<> ExeTerminateEvent;
	static Event<> ScenarioStartEvent;
	static Event<> LogicClassInitEvent;
	static Event<> ScenarioClearClassesEvent;
	static Event<> LogicClassUpdateEvent;
	static Event<> LogicClassUpdateLateEvent;
	static Event<> GScreenClassDrawOnTopEvent;
};
Event<> General::YRBootEvent;
Event<> General::ExeTerminateEvent;
Event<> General::ScenarioStartEvent;
Event<> General::LogicClassInitEvent;
Event<> General::ScenarioClearClassesEvent;
Event<> General::LogicClassUpdateEvent;
Event<> General::LogicClassUpdateLateEvent;
Event<> General::GScreenClassDrawOnTopEvent;
DEFINE_JUMP(LJMP, 0x4068E0, 0x4A4AC0); //日志重定向

DEFINE_HOOK(0x7CD810, YRBoot, 0x9)
{
	General::YRBootEvent.Invoke();
	return 0;
}


DEFINE_HOOK(0x7CD8EF, ExeTerminate, 9) {
	General::ExeTerminateEvent.Invoke();
	return 0;
}

DEFINE_HOOK(0x6875F3, Scenario_Start, 6) {
	General::ScenarioStartEvent.Invoke();
	Debug::Log("Scenario_Start\n");
	return  0;
}

DEFINE_HOOK(0x685582, LogicClass_Init, 6) {
	General::LogicClassInitEvent.Invoke();
	Debug::Log("LogicClass_Ini\nt");
	return  0;
}


DEFINE_HOOK(0x685659, Scenario_ClearClasses, 0xA) {
	Debug::Log("Scenario_ClearClasses\n");
	return 0;
}
DEFINE_HOOK(0x55AFB3, LogicClass_Update, 0x6) {
	General::LogicClassUpdateEvent.Invoke();
	return 0;
}

DEFINE_HOOK(0x55B719, LogicClass_Update_Late, 0x5) {
	General::LogicClassUpdateLateEvent.Invoke();
	return 0;
}

DEFINE_HOOK(0x4F4583, GScreenClass_DrawOnTop_TheDarkSideOfTheMoon, 6)
{
	General::GScreenClassDrawOnTopEvent.Invoke();
	//for (auto const& pHouse : HouseClass::Array) {
	//	if (!pHouse) {
	//		continue;
	//	}

	//	if (!pHouse->IsAlliedWith(HouseClass::CurrentPlayer) && pHouse != HouseClass::FindNeutral() && HouseClass::FindSpecial()) {

	//	}
	//}
	//const auto pSurface = DSurface::Composite;
	//RectangleStruct rect = pSurface->GetRect();
	//rect.Height -= 128; // account for bottom bar
	//for (auto const& pNameType : NodeNameType::Array) {
	//	auto const& pHouse = HouseClass::Array[pNameType->HouseIndex];
	//	if (!pHouse) {
	//		continue;
	//	}

	//	//Debug::Log("Player name: %ls\n", pNameType->Name);

	//	Point2D pLoc;
	//	pLoc.X = 0;
	//	pLoc.Y = rect.Height;
	//	ColorStruct fColorStruct;
	//	fColorStruct.R = 255;
	//	fColorStruct.G = 0;
	//	fColorStruct.B = 100;
	//	ColorStruct bgColorStruct;
	//	bgColorStruct.R = 0;
	//	bgColorStruct.G = 0;
	//	bgColorStruct.B = 0;
	//	const COLORREF fColor = Drawing::RGB_To_Int(fColorStruct);
	//	const COLORREF bgColor = Drawing::RGB_To_Int(bgColorStruct);
	//	RectangleStruct pBounds;
	//	pSurface->DrawText(pNameType->Name, &pBounds, &pLoc, fColor, bgColor, (TextPrintType)-1);
	//	rect.Height -= 20;
	//}

	return 0;
}