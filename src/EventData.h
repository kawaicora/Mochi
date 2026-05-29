#pragma once

#include <YRPP.h>
#include <EventClass.h>

#pragma pack(push, 1)
class EventData
{
public:



	static EventData* EventClass_CTOR() {
		alignas(EventData) char buffer[sizeof(EventData)];
		auto event = reinterpret_cast<EventData*>(buffer);
		memset(event, 0, sizeof(EventData));
		return event;
	}


	bool AddEvent()
	{
		return EventClass::OutList.Add(*reinterpret_cast<EventClass*>(this));
	}
	explicit EventData(const EventData& another)
	{
		memcpy(this, &another, sizeof(*this));
	}

	EventData& operator=(const EventData& another)
	{
		if (this != &another)
			memcpy(this, &another, sizeof(*this));

		return *this;
	}

	bool operator==(const EventData& q) const
	{
		return memcmp(this, &q, sizeof(q)) == 0;
	};



	EventType Type;
	bool IsExecuted;
	char HouseIndex; // '-1' stands for not a valid house
	unsigned int Frame; // 'Frame' is the frame that the command should execute on.

	union
	{
		char DataBuffer[104];

		struct EMPTY
		{
		} Empty;

		struct POWERON
		{
			TargetClass Target;
		} Poweron;

		struct POWEROFF
		{
			TargetClass Target;
		} Poweroff;

		struct ALLY
		{
			int HouseID;
		} Ally;

		struct MEGAMISSION
		{
			TargetClass Whom;
			unsigned char Mission;
			char _gap_;
			TargetClass Target;
			TargetClass Destination;
			TargetClass Follow;
			bool IsPlanningEvent;
		} MegaMission;

		struct MEGAMISSION_F
		{
			TargetClass Whom;
			unsigned char Mission;
			TargetClass Target;
			TargetClass Destination;
			int Speed;
			int MaxSpeed;
		} MegaMissionF;

		struct IDLE
		{
			TargetClass Whom;
		} Idle;

		struct SCATTER
		{
			TargetClass Whom;
		} Scatter;

		struct DESTRUCT
		{
		} Destruct;

		struct DEPLOY
		{
			TargetClass Whom;
		} Deploy;

		struct DETONATE
		{
			TargetClass Whom;
		} Detonate;

		struct PLACE
		{
			AbstractType RTTIType;
			int HeapID;
			int IsNaval;
			CellStruct Location;
		} Place;

		struct OPTIONS
		{
		} Options;

		struct GAMESPEED
		{
			int GameSpeed;
		} GameSpeed;

		// This event starts production of the specified object type. The house can
		// determine from the type and ID value, what object to begin production on and
		// what factory to use.
		struct PRODUCE
		{
			AbstractType RTTIType;
			int HeapID;
			int IsNaval;
		} Produce;

		struct SUSPEND
		{
			AbstractType RTTIType;
			int HeapID;
			int IsNaval;
		} Suspend;

		// This event is generated when the player cancels production of the specified
		// object type. From the object type, the exact factory can be inferred.
		struct ABANDON
		{
			AbstractType RTTIType;
			int HeapID;
			int IsNaval;
		} Abandon;

		// Toggles the primary factory state of the specified building.
		struct PRIMARY
		{
			TargetClass Whom;

		} Primary;

		struct SPECIAL_PLACE
		{
			int ID;
			CellStruct Location;
		} SpecialPlace;

		struct EXIT
		{
		} Exit;

		struct ANIMATION
		{
			int AnimID;
			int HouseID;
			Point2D Location;
		} Animation;

		// Starts or stops repair on the specified object. This event is triggered by the
		// player clicking the repair wrench on a building.
		struct REPAIR
		{
			TargetClass Whom;
		} Repair;

		// Tells a building/unit to sell. This event is triggered by the player clicking the
		// sell animating cursor over the building or unit.
		struct SELL
		{
			TargetClass Whom;
		} Sell;

		// Used to sell walls
		struct SELLCELL
		{
			CellStruct Location;
		} SellCell;

		// Update the special control flags. This is necessary so that in a multiplayer
		// game, all machines will agree on the rules. If these options change during
		// game play, then all players are informed that options have changed.
		struct SPECIAL
		{
			ScenarioFlags SpecialFlags;
		} Special;

		struct FRAMESYNC
		{
		} FrameSync;

		struct MESSAGE
		{
		} Message;

		struct RESPONSE_TIME
		{
			char unknown;
		} ResponseTime;

		struct FRAMEINFO
		{
			unsigned int CRC;
			unsigned short CommandCount;
			unsigned char Delay;
		} FrameInfo;

		struct SAVEGAME
		{
		} SaveGame;

		// Update the archive target for this building.
		struct ARCHIVE
		{
			TargetClass Whom1;
			TargetClass Whom2;
		} Archive;

		struct ADDPLAYER
		{
			void* unknownPointer;
		} AddPlayer;

		struct TIMING
		{
			unsigned short RequestedFPS;
			unsigned short MaxAhead;
			unsigned char FrameSendRate;
		} Timing;

		struct PROCESS_TIME
		{
			unsigned short Time;
		} ProcessTime;

		struct PAGEUSER
		{
		} PageUser;

		struct REMOVEPLAYER
		{
			int HouseID;
		} RemovePlayer;

		struct LATENCYFUDGE
		{
			int LatencyFudge;
		} LatencyFudge;

		struct MEGAFRAMEINFO
		{
			char Unknown[104];
		} MegafFameInfo;

		struct PACKETTIMING
		{
			char Unknown[64];
		} PacketTiming;

		struct ABOUTTOEXIT
		{
		} AboutToExit;

		struct FALLBACKHOST
		{
			int FallbackHost;
		} FallbackHost;

		struct ADDRESSCHANGE
		{
			char PlayerID;
			DWORD Address;
		} AddressChange;

		struct PLANCONNECT
		{
			TargetClass Target1;
			TargetClass Target2;
		} PlanConnect;

		struct PLANCOMMIT
		{
		} PlanCommit;

		struct PLANNODEDELETE
		{
			TargetClass Target;
		} PlanNodeDelete;

		struct ALLCHEER
		{
			char Unknown[4];
		} AllCheer;

		struct ABANDON_ALL
		{
			char Unknown[12];
		} AbandonAll;
		struct UNLOCK_ALL_TECH
		{
			bool IsUnlock;

		} UnlockAllTech;

		struct Money
		{
			int Amount;

		} Money;
	};
};
#pragma pack(pop)

static_assert(sizeof(EventData) == 111);
static_assert(offsetof(EventData, DataBuffer) == 7);
