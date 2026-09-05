#pragma once
#include <EventSystem.h>
class RadarHook
{
public:
	static bool SkipRadarDraw;

	static Event<> RadarDrawEvent;
	static void PlayMapMovie(const char* movie);
	static void DrawRadar();
	static void DrawBoard();
	static void DrawView();
};