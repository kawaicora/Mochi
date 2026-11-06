#include "MochiNetwork.h"
#include "SpyMoneyHook.h"
#include "MoneyReporter.h"
#include <Debug.h>

void MochiNetwork::Initialize()
{
    Debug::Log("MochiNetwork Initializing...\n");

    // Subscribe to money events
    SpyMoneyHook::OnMoneyEvent.Subscribe([](int amount, bool isGain, const char* payMethod) {
        OnMoneyEvent(amount, isGain, payMethod);
    });

    // Start money reporter
    MoneyReporter::Start();

    Debug::Log("MochiNetwork Initialized.\n");
}

void MochiNetwork::Shutdown()
{
    // TODO: cleanup
}

void MochiNetwork::OnMoneyEvent(int amount, bool isGain,const char * payMethpd)
{
    Debug::Log("Money Event: %d %s\n", amount, isGain ? "gain" : "lose" ,payMethpd);
    MoneyReporter::Enqueue(amount, isGain, payMethpd);
}
