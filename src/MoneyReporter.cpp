#include "MoneyReporter.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <queue>
#include <string>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

static SOCKET g_client = INVALID_SOCKET;
static std::queue<std::string> g_queue;
static CRITICAL_SECTION g_lock;
static HANDLE g_wakeEvent;
static volatile LONG g_started = 0;

void MoneyReporter::Start()
{
    if (InterlockedExchange(&g_started, 1) == 1)
        return;

    InitializeCriticalSection(&g_lock);
    g_wakeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
    CloseHandle(hThread);
}

void MoneyReporter::Enqueue(int amount, bool isGain,const char* payMethod)
{
    if (amount <= 0 || amount > 200000000)
        return;

    char buf[256];
    int frame = 0;
    __try
    {
        frame = *(int*)0x87A734; // Game.CurrentFrame
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        frame = 0;
    }

    sprintf_s(buf, "{\"amount\":%d,\"direction\":\"%s\",\"pay_method\":\"%s\",\"frame\":%d}",
        amount, isGain ? "gain" : "lose",payMethod, frame);

    EnterCriticalSection(&g_lock);
    if (g_queue.size() >= 64)
        g_queue.pop();
    g_queue.push(buf);
    LeaveCriticalSection(&g_lock);

    SetEvent(g_wakeEvent);
}

void MoneyReporter::WorkerThread()
{
    while (true)
    {
        std::string line;
        EnterCriticalSection(&g_lock);
        if (!g_queue.empty())
        {
            line = g_queue.front();
            g_queue.pop();
        }
        LeaveCriticalSection(&g_lock);

        if (line.empty())
        {
            WaitForSingleObject(g_wakeEvent, 500);
            continue;
        }

        __try
        {
            if (g_client == INVALID_SOCKET)
            {
                if (!Connect())
                    continue;
            }

            std::string data = "SPY_MONEY_EVENT " + line + "\n";
            int sent = send(g_client, data.c_str(), (int)data.size(), 0);
            if (sent == SOCKET_ERROR)
            {
                Close();
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Close();
        }
    }
}

bool MoneyReporter::Connect()
{
    Close();

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return false;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(39247);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    DWORD startTime = GetTickCount();
    while (true)
    {
        int result = connect(sock, (sockaddr*)&addr, sizeof(addr));
        if (result != SOCKET_ERROR)
            break;

        if (GetTickCount() - startTime > 800)
        {
            closesocket(sock);
            return false;
        }
        Sleep(10);
    }

    g_client = sock;
    return true;
}

void MoneyReporter::Close()
{
    if (g_client != INVALID_SOCKET)
    {
        closesocket(g_client);
        g_client = INVALID_SOCKET;
    }
}
