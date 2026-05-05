#pragma once
#include <GeneralDefinitions.h>
#include "EventSystem.h"
#include "Utilities/Macro.h"
class EventClass {
public :
	static Event<EventType*> NetworkingRespondToEvent;
	static const char* EventTypeToString(EventType type);
};
