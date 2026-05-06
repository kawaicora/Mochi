#pragma once
#include <GeneralDefinitions.h>
#include "EventSystem.h"
#include "Utilities/Macro.h"
#include <Windows.h>
#include "Debug.h"
#include <EventClass.h>
#include <TargetClass.h>
#include <cstddef>
#include <stdint.h>
enum class MochiEventType : unsigned char
{
    
    // Vanilla game used Events from 0x00 to 0x2F
    // CnCNet reserved Events from 0x30 to 0x3F
    // Ares used Events 0x60 and 0x61
    // 0x40 Start  - 0x59
    CoraCompleteProduction = 0x40,
    CoraSuperWeaponCharge = 0x41,
};


class EventHook {
public:
   

    static Event<EventClass*> NetworkingRespondToEvent;
    static EventClass* EventClass_CTOR() {
        alignas(EventClass) char buffer[sizeof(EventClass)];
        auto event = reinterpret_cast<EventClass*>(buffer);
        memset(event, 0, sizeof(EventClass));
        return event;
    }
    static bool AddEvent(EventClass* event)
    {
        return EventClass::OutList.Add(*reinterpret_cast<EventClass*>(event));
    }

};
Event<EventClass*> EventHook::NetworkingRespondToEvent;
DEFINE_HOOK(0x4C6CB0, Networking_RespondToEvent, 0x6)
{
    GET(DWORD, EventKind, EAX);
    GET(EventClass*, pEvent, ECX);
    EventHook::NetworkingRespondToEvent.Invoke(pEvent);
    return 0;
};