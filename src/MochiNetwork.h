#pragma once

class MochiNetwork
{
public:
    static void Initialize();
    static void Shutdown();
private:
    static void OnMoneyEvent(int amount, bool isGain, const char *);
};
