#include "RadarHook.h"
#include <GeneralHook.h>
#include <Debug.h>
#include <EventSystem.h>
#include <YRPP.h>
#include <FPSCounter.h>
typedef DWORD(__fastcall* PlayMovieInININame_t)(const char*, DSurface*, int*);
static PlayMovieInININame_t const s_PlayMovieInINI =
	reinterpret_cast<PlayMovieInININame_t>(0x5C07D0);
void RadarHook::PlayMapMovie(const char* movie)
{
	Debug::Log("RadarHook: play %s into Sidebar radar box \n",
		movie);
	s_PlayMovieInINI(movie, DSurface::Sidebar, 0);
}