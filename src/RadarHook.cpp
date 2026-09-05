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

	SHPStruct* radarSHP = FileSystem::LoadSHPFile("radar.shp");

	if (radarSHP == nullptr) {
		Debug::Log("DrawBoard: radar.shp failed to load, skip\n");
		return;
	}
	const int borderFrame = 32;   // 最后一帧 = 边框

	RectangleStruct lastBounds{ 0,0 , 128,720 };
	Point2D framePos{ 0,48 };

	//Debug::Log("DrawBoard: frameCount=%d drawing frame=%d\n", frameCount, borderFrame);

	
	CC_Draw_Shape(
		(Surface*)DSurface::Sidebar,
		FileSystem::SIDEBAR_PAL,
		radarSHP,
		borderFrame,
		&framePos,
		&lastBounds,
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