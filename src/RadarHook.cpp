#include "RadarHook.h"
#include <GeneralHook.h>
#include <Debug.h>
#include <YRPP.h>

bool RadarHook::SkipRadarDraw = false;
Event<> RadarHook::RadarDrawEvent;
DEFINE_HOOK(0x653100, Radar_Draw, 0x5)
{

	RadarHook::RadarDrawEvent.Invoke();
	if (RadarHook::SkipRadarDraw) {
		return 0x653127;
	}
	else {
		return 0;
	}

}



void RadarHook::DrawRadar()
{
	RadarHook::DrawBoard();
	//RadarHook::DrawView();
	
}


void RadarHook::DrawBoard()
{
	auto pRadar = (uintptr_t)&RadarClass::Instance;
	Point2D drawPos{ 0,0};
	RectangleStruct bounds  = DSurface::Sidebar->GetRect();
	int frameIdx = 0;

	//Draw 0,0

	drawPos = { 0,0 };
	//SHP w = 168 h = 16  
	//DrawFrameIndex = 0



	//Draw
	drawPos = { 11,20 };
	//SHP w 72, h 18


	drawPos = { 83,20 };

	//Draw
	drawPos = { 0,48 };
	//SHP w 168, h 110

	SHPStruct* board = (SHPStruct*)0x1919B850;


	if (board == nullptr) {
		Debug::Log("DrawBoard: radar.shp failed to load, skip\n");
		return;
	}

	

	CC_Draw_Shape(
		(Surface*)DSurface::Sidebar,
		FileSystem::SIDEBAR_PAL,
		board,
		32,
		&drawPos,
		&bounds,
		(BlitterFlags)0,
		0,
		0,
		(ZGradient)0,
		0x3E8,
		0,
		0,
		0,
		0,
		0
	);

}

void RadarHook::DrawView() {
	auto pRadar = (uintptr_t)&RadarClass::Instance;
	RectangleStruct rect{
		*(int*)(pRadar + 0x11E4),
		*(int*)(pRadar + 0x11EC),   // 内容 Y(0x11EC)
		168,
		110
	};
	DSurface::Sidebar->FillRect(&rect, COLOR_RED);
}