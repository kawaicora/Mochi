#pragma once
#include <GeneralDefinitions.h>
#include "EventSystem.h"
#include "Utilities/Macro.h"
#include "EventClass.h"
#include "Mochi.h"
#include <Windows.h>
#include "Debug.h"
class EventClass {
public :
	static Event<EventType*> NetworkingRespondToEvent;
    static const char* EventTypeToString(EventType type) {
        switch (type) {
        case EventType::Empty: return "Empty";
        case EventType::PowerOn: return "PowerOn";
        case EventType::PowerOff: return "PowerOff";
        case EventType::Ally: return "Ally";
        case EventType::MegaMission: return "MegaMission";
        case EventType::MegaMissionF: return "MegaMissionF";
        case EventType::Idle: return "Idle";
        case EventType::Scatter: return "Scatter";
        case EventType::Destruct: return "Destruct";
        case EventType::Deploy: return "Deploy";
        case EventType::Detonate: return "Detonate";
        case EventType::Place: return "Place";
        case EventType::Options: return "Options";
        case EventType::GameSpeed: return "GameSpeed";
        case EventType::Produce: return "Produce";
        case EventType::Suspend: return "Suspend";
        case EventType::Abandon: return "Abandon";
        case EventType::Primary: return "Primary";
        case EventType::SpecialPlace: return "SpecialPlace";
        case EventType::Exit: return "Exit";
        case EventType::Animation: return "Animation";
        case EventType::Repair: return "Repair";
        case EventType::Sell: return "Sell";
        case EventType::SellCell: return "SellCell";
        case EventType::Special: return "Special";
        case EventType::FrameSync: return "FrameSync";
        case EventType::Message: return "Message";
        case EventType::ResponseTime: return "ResponseTime";
        case EventType::FrameInfo: return "FrameInfo";
        case EventType::SaveGame: return "SaveGame";
        case EventType::Archive: return "Archive";
        case EventType::AddPlayer: return "AddPlayer";
        case EventType::Timing: return "Timing";
        case EventType::ProcessTime: return "ProcessTime";
        case EventType::PageUser: return "PageUser";
        case EventType::RemovePlayer: return "RemovePlayer";
        case EventType::LatencyFudge: return "LatencyFudge";
        case EventType::MegaFrameInfo: return "MegaFrameInfo";
        case EventType::PacketTiming: return "PacketTiming";
        case EventType::AboutToExit: return "AboutToExit";
        case EventType::FallbackHost: return "FallbackHost";
        case EventType::AddressChange: return "AddressChange";
        case EventType::PlanConnect: return "PlanConnect";
        case EventType::PlanCommit: return "PlanCommit";
        case EventType::PlanNodeDelete: return "PlanNodeDelete";
        case EventType::AllCheer: return "AllCheer";
        case EventType::AbandonAll: return "AbandonAll";
        default: return "Unknown";
        }
    }
};
Event<EventType*> EventClass::NetworkingRespondToEvent;
DEFINE_HOOK(0x4C6CB0, Networking_RespondToEvent, 0x6)
{
    GET(EventType*, eventType, ECX);
    EventClass::NetworkingRespondToEvent.Invoke(eventType);
    return 0;
};
