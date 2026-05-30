#pragma once
#include <YRPP.h>
#include <EventSystem.h>

class FactoryHook
{
public:
	static Event<FactoryClass*> ProgressUpdateEvent;
	static Event<FactoryClass*> FactoryClassCreateEvent;
};
