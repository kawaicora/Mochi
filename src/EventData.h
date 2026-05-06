#include <GeneralStructures.h>
#include <TargetClass.h>

enum class MochiEventType : unsigned char
{
    // Vanilla game used Events from 0x00 to 0x2F
    // CnCNet reserved Events from 0x30 to 0x3F
    // Ares used Events 0x60 and 0x61
    // 0x40 Start  - 0x59
   
};




#pragma pack(push, 1)

class EventData
{
public:
    EventType Type;
    bool IsExecuted;
    char HouseIndex;
    uint32_t Frame;
    union
    {
        char DataBuffer[104];
        struct { uint8_t Data; } Nothing;

        struct
        {
            int ID;
            int AnimOwner;
            CellStruct Location;
            uint8_t ExtraData;
        } Animation;

        struct
        {
            uint8_t CRC;
            uint16_t CommandCount;
            uint8_t Delay;
            uint8_t ExtraData;
        } FrameInfo;

        struct
        {
            TargetClass Whom;
            uint8_t ExtraData;
        } Target;

        struct
        {
            TargetClass Whom;
            uint8_t Mission;
            uint8_t _gap_;
            TargetClass Target;
            TargetClass Destination;
            TargetClass Follow;
            bool IsPlanningEvent;
            uint8_t ExtraData;
        } MegaMission;

        struct
        {
            TargetClass Whom;
            uint8_t Mission;
            TargetClass Target;
            TargetClass Destination;
            int Speed;
            int MaxSpeed;
            uint8_t ExtraData;
        } MegaMissionF;

        struct
        {
            int RTTI_ID;
            int Heap_ID;
            int IsNaval;
            uint8_t ExtraData;
        } Production;

        struct
        {
            int Unknown_0;
            uint64_t Data;
            int Unknown_C;
            uint8_t ExtraData;
        } UnknownLongLong;

        struct
        {
            int Unknown_0;
            int Unknown_4;
            int Data;
            int Unknown_C;
            uint8_t ExtraData;
        } UnknownTuple;

        struct
        {
            AbstractType RTTIType;
            int HeapID;
            int IsNaval;
            CellStruct Location;
            uint8_t ExtraData;
        } Place;

        struct
        {
            int ID;
            CellStruct Location;
            uint8_t ExtraData;
        } SpecialPlace;

        struct
        {
            AbstractType RTTIType;
            int ID;
            uint8_t ExtraData;
        } Specific;
    };
};
static_assert(sizeof(EventData) == 111);
static_assert(offsetof(EventData, DataBuffer) == 7);
#pragma pack(pop)