#pragma once
#include <EventSystem.h>

class SpyMoneyHook
{
public:
    // 金钱事件：参数是 (amount, isGain)
    static Event<int, bool, const char*> OnMoneyEvent;
};
