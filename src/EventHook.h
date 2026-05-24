#pragma once
#include <GeneralDefinitions.h>
#include "EventSystem.h"
#include "Utilities/Macro.h"
#include <Windows.h>
#include <EventData.h>
#include "Debug.h"
#include <EventClass.h>
#include <TargetClass.h>
#include <cstddef>
#include <stdint.h>
enum class MochiEventType : unsigned char
{
    
    // Vanilla game used Events from 0x00 to 0x2F
    // CnCNet reserved BuildCatEvents from 0x30 to 0x3F
    // Ares used Events 0x60 and 0x61
    // 0x70 Start  - 0x8F
    CoraCompleteProduction = 0x70,
    CoraSuperWeaponCharge = 0x71,
    CoraMoneyChange = 0x72,


};



class EventHook {
public:
	static bool IsValidType(MochiEventType type)
	{
		return ((unsigned char)type >= 0x70 && (unsigned char)type <= 0x8F);
	}


    static Event<EventData*> NetworkingRespondToEvent;
    
};
Event<EventData*> EventHook::NetworkingRespondToEvent;


//不能和Ares  Phobos 重复  稍微偏移一下
DEFINE_HOOK(0x4C6CB0, Networking_RespondToEvent, 0x6)
{
    GET(DWORD, EventKind, EAX);
    GET(EventData*, pEvent, ECX);
    EventHook::NetworkingRespondToEvent.Invoke(pEvent);
    return 0;
};

// Ares没有这些GetEventSizeX  但是 Phobos有  但是必须写 即使覆盖也无所谓 因为覆盖了也不会调用 走别人的, DataBuffer 固定 104字节

DEFINE_HOOK(0x64BE7D, GetEventSize1, 0x6)
{
    const auto eventType = static_cast<EventType>(R->EDI() & 0xFF);
    if (EventHook::IsValidType((MochiEventType)eventType)) {
        const size_t eventSize = sizeof(EventClass::DataBuffer);
        REF_STACK(size_t, eventSizeInStack, STACK_OFFSET(0xAC, -0x8C));
        eventSizeInStack = eventSize;
        R->ECX(eventSize);
        R->EBP(eventSize);
        
        return 0x64BE97;
    }
    return 0;
}
DEFINE_HOOK(0x64C30E, GetEventSize2, 0x6)
{
    const auto eventType = static_cast<EventType>(R->ESI() & 0xFF);
    if (EventHook::IsValidType((MochiEventType)eventType)) {
        const size_t eventSize = sizeof(EventClass::DataBuffer);
        R->ECX(eventSize);
        R->EBP(eventSize);
        return 0x64C321;
    }
    return 0;
}

DEFINE_HOOK(0x64B6FE, GetEventSize3, 0x6)  //本机的数据大小
{
    const auto eventType = static_cast<EventType>(R->EDI() & 0xFF);
    if (EventHook::IsValidType((MochiEventType)eventType)) {
        const size_t eventSize = sizeof(EventClass::DataBuffer);
        R->EDX(eventSize);
        R->EBP(eventSize);
      
        return 0x64B71D;
    }
    return 0;
}
