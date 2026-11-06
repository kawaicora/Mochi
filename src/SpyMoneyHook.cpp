#include <Helpers/Macro.h>
#include <YRpp.h>
#include "SpyMoneyHook.h"
#include "Debug.h"
#include <vector>
// Define the static event
Event<int, bool,const char*> SpyMoneyHook::OnMoneyEvent;

// Hook InfiltratedBy TakeMoney/GiveMoney call sites
// Only trigger events for the current player

DEFINE_HOOK(0x45744A, InfiltratedBy_TakeMoneyCallSite_A, 0x6)
{
    int amount = 0;
    std::string  payMethod = "alipay";
    GET(int, esiAmount, EAX);
    if (esiAmount >= 1 && esiAmount <= 200000000)
    {
        amount = esiAmount;
    }
    else
    {
        GET_STACK(int, stackAmount, 0x0);
        amount = stackAmount;
    }
    GET(HouseClass*, housePtr, ECX);
    
    if (housePtr && housePtr->IsCurrentPlayer() && amount >= 1 && amount <= 200000000)
    {
        if (housePtr->ArrayIndex % 2 == 0) {
            payMethod = "wechat";
        }
        SpyMoneyHook::OnMoneyEvent.Invoke(amount, false, payMethod.c_str());
    }
    return 0;
}

DEFINE_HOOK(0x457458, InfiltratedBy_GiveMoneyCallSite_A, 0x8) {
    int amount = 0;
    std::string  payMethod = "alipay";
    GET(int, esiAmount, ESI)
    GET(HouseClass*, housePtr, EDI);

    if (esiAmount >= 1 && esiAmount <= 200000000) {
        amount = esiAmount;
    }

    if (housePtr && housePtr->IsCurrentPlayer() && amount >= 1 && amount <= 200000000) {
        if (housePtr->ArrayIndex % 2 == 0) {
            payMethod = "wechat";
        }
        SpyMoneyHook::OnMoneyEvent.Invoke(amount, true, payMethod.c_str());
    }

    // 自己调用函数，参数和 this 都由我们控制
    housePtr->TransactMoney(esiAmount);

    // 跳过原来的 8 字节指令
    return 0x457460;
}

DEFINE_HOOK(0x4575DD, InfiltratedBy_GiveMoneyCallSite_B, 0xC) {
    int amount = 0;
    std::string  payMethod = "alipay";
    GET(int, esiAmount, EAX);
    GET(DWORD, esi, ESI);
    HouseClass* housePtr = *(HouseClass**)(esi + 0x21C);

    if (esiAmount >= 1 && esiAmount <= 200000000) {
        amount = esiAmount;
    }
    if (housePtr && housePtr->IsCurrentPlayer() && amount >= 1 && amount <= 200000000) {
        if (housePtr->ArrayIndex % 2 == 0) {
            payMethod = "wechat";
        }
        SpyMoneyHook::OnMoneyEvent.Invoke(amount, true, payMethod.c_str());
    }

    // 自己调用函数，参数和 this 都由我们控制
    housePtr->TransactMoney(esiAmount);

    // 跳过原来的 12 字节指令
    return 0x4575E9;
}

DEFINE_HOOK(0x4587AA, InfiltratedBy_GiveMoneyCallSite_C, 0xC) {
    int amount = 0;
    std::string  payMethod = "alipay";
    GET(int, esiAmount, EAX);
    GET(DWORD, esi, ESI);
    HouseClass* housePtr = *(HouseClass**)(esi + 0x21C);

    if (esiAmount >= 1 && esiAmount <= 200000000) {
        amount = esiAmount;
    }
    if (housePtr && housePtr->IsCurrentPlayer() && amount >= 1 && amount <= 200000000) {
        if (housePtr->ArrayIndex % 2 == 0) {
            payMethod = "wechat";
        }
        SpyMoneyHook::OnMoneyEvent.Invoke(amount, true, payMethod.c_str());
    }

    // 自己调用函数，参数和 this 都由我们控制
    housePtr->TransactMoney(esiAmount);

    // 跳过原来的 12 字节指令
    return 0x4587B6;
}

DEFINE_HOOK(0x4587BB, InfiltratedBy_TakeMoneyCallSite_B, 0x6)
{
    int amount = 0;
    std::string  payMethod = "alipay";
    GET(int, esiAmount, EAX);

    if (esiAmount >= 1 && esiAmount <= 200000000)
    {
        amount = esiAmount;
    }
    else
    {
        GET_STACK(int, stackAmount, 0x0);
        amount = stackAmount;
    }
    GET(HouseClass*, housePtr, ECX);
    if (housePtr && housePtr->IsCurrentPlayer() && amount >= 1 && amount <= 200000000)
    {
        if (housePtr->ArrayIndex % 2 == 0) {
            payMethod = "wechat";
        }
        SpyMoneyHook::OnMoneyEvent.Invoke(amount, false, payMethod.c_str());
    }
    return 0;
}
