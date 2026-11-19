
#include "EventData.h"
#include <EventClass.h>
EventData::EventData(int houseIndex, EventType eventType)
{
	memset(this, 0, sizeof(*this));          // 含 IsExecuted(+1) 和 DataBuffer(+7..110)
	Type = eventType;
	HouseIndex = static_cast<char>(houseIndex);   // -1 = 无效 house
	Frame = Unsorted::CurrentFrame;          // 与原版 ctor 一致（原版也是从 0xA8ED84 取）
}
bool EventData::AddEvent()
{
	return EventClass::OutList.Add(*reinterpret_cast<EventClass*>(this));
}

EventData& EventData::operator=(const EventData& another)
{
	if (this != &another)
		memcpy(this, &another, sizeof(*this));

	return *this;
}

bool EventData::operator==(const EventData& q) const
{
	return memcmp(this, &q, sizeof(q)) == 0;
};