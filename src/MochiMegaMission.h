#pragma once
#include <YRPP.h>
#include <TargetClass.h>
#include <EventData.h>
class MochiMegaMission
{
public:
	static DWORD Get(TargetClass* obj);
	static void ShowMegaMissionInfo(EventData* data);
	static void ShowMegaMissionFInfo(EventData* data);
};

