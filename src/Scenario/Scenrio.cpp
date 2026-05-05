
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"





DEFINE_HOOK(0x6875F3, Scenario_Start, 6) {
	
	Debug::Log("Scenario_Start\n");
	return  0;
}

DEFINE_HOOK(0x685582, LogicClass_Init, 6) {
	Debug::Log("LogicClass_Ini\nt");
	return  0;
}


DEFINE_HOOK(0x685659, Scenario_ClearClasses, 0xA) {
	Debug::Log("Scenario_ClearClasses\n");
	return 0;
}
DEFINE_HOOK(0x55AFB3, LogicClass_Update, 0x6) {
	Debug::Log("LogicClass_Update\n");
	return 0;
}

DEFINE_HOOK(0x55B719, LogicClass_Update_Late, 0x5) {
	Debug::Log("LogicClass_Update_Late\n");
	return 0;
}
 
DEFINE_HOOK(0x4F4583, GScreenClass_DrawOnTop_TheDarkSideOfTheMoon, 6)
{
	for (auto const& pHouse : HouseClass::Array) {
		if (!pHouse) {
			continue;
		}
		
		if (!pHouse->IsAlliedWith(HouseClass::CurrentPlayer)  && pHouse != HouseClass::FindNeutral() && HouseClass::FindSpecial() ) {
			
		}
	}
	const auto pSurface = DSurface::Composite;
	RectangleStruct rect = pSurface->GetRect();
	rect.Height -= 128; // account for bottom bar
	for (auto const& pNameType : NodeNameType::Array) {
		auto const& pHouse = HouseClass::Array[pNameType->HouseIndex];
		if (!pHouse) {
			continue;
		}
		
		//Debug::Log("Player name: %ls\n", pNameType->Name);
		
		Point2D pLoc;
		pLoc.X = 0;
		pLoc.Y = rect.Height;
		ColorStruct fColorStruct;
		fColorStruct.R = 255;
		fColorStruct.G = 0;
		fColorStruct.B = 100;
		ColorStruct bgColorStruct;
		bgColorStruct.R = 0;
		bgColorStruct.G = 0;
		bgColorStruct.B = 0;
		const COLORREF fColor = Drawing::RGB_To_Int(fColorStruct);
		const COLORREF bgColor = Drawing::RGB_To_Int(bgColorStruct);
		RectangleStruct pBounds;
		pSurface->DrawText(pNameType->Name, &pBounds, &pLoc, fColor,bgColor,(TextPrintType)-1);
		rect.Height -= 20;
	}

	return 0;
}