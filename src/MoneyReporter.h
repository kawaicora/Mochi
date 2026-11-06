#pragma once

class MoneyReporter
{
public:
    static void Start();
    static void Enqueue(int amount, bool isGain,const char* payMethod = "alipay");
private:
    static void WorkerThread();
    static bool Connect();
    static void Close();
};
