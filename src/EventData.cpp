
#include "EventData.h"
#include <EventClass.h>

EventData* EventData::EventClass_CTOR() {
	alignas(EventData) char buffer[sizeof(EventData)];
	auto event = reinterpret_cast<EventData*>(buffer);
	memset(event, 0, sizeof(EventData));
	return event;
}
bool EventData::AddEvent()
{
	return EventClass::OutList.Add(*reinterpret_cast<EventClass*>(this));
}
EventData::EventData(const EventData& another)
{
	memcpy(this, &another, sizeof(*this));
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