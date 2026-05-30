#pragma once
#include <EventSystem.h>
#include <EventData.h>
class EventHook {
public:
    static Event<EventData*> NetworkingRespondToEvent;
};