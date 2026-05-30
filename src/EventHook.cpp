#include "EventHook.h"
#include <Helpers/Macro.h>
#include <EventClass.h>
#include <MochiEvent.h>
#include <MochiEventType.h>
Event<EventData*> EventHook::NetworkingRespondToEvent;
//不能和Ares  Phobos 重复  稍微偏移一下
DEFINE_HOOK(0x4C6CB0, Networking_RespondToEvent, 0x6)
{
    GET(DWORD, EventKind, EAX);
    GET(EventData*, pEvent, ECX);
    EventHook::NetworkingRespondToEvent.Invoke(pEvent);
    return 0;
};
DEFINE_HOOK(0x64BE7D, GetEventSize1, 0x6)
{
    const auto eventType = static_cast<EventType>(R->EDI() & 0xFF);
    if (MochiEvent::IsValidType((MochiEventType)eventType)) {
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
    if (MochiEvent::IsValidType((MochiEventType)eventType)) {
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
    if (MochiEvent::IsValidType((MochiEventType)eventType)) {
        const size_t eventSize = sizeof(EventClass::DataBuffer);
        R->EDX(eventSize);
        R->EBP(eventSize);

        return 0x64B71D;
    }
    return 0;
}
