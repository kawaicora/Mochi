#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

class Network
{
public:
	static bool Init(const char* host, unsigned short port);
	static void Shutdown();
	static bool SendGameData(const void* data, int len);
	static int PollReceived(void* outBuf, int maxLen);
	// Same as PollReceived but also reports which player the packet came from,
	// so the injector can fill the source IP/port fields the native recv path sets.
	static int PollReceivedEx(void* outBuf, int maxLen, unsigned int* fromId);
	static bool IsReady();
	static unsigned int GetPlayerId();

private:
	static void WorkerThread();
	static bool Connect(const char* host, unsigned short port);
	static void Close();

	static SOCKET g_sock;
	static CRITICAL_SECTION g_lock;
	static unsigned int g_playerId;
	static bool g_ready;

	// Packet queue: each received game UDP packet stored separately.
	// Same depth as the game's own receive ring (StaticInBuffer[128]), so the
	// relay side and the game side stay one-to-one.
	static const int QUEUE_SIZE = 128;
	static const int MAX_PKT = 2048;
	static char g_queueData[QUEUE_SIZE][MAX_PKT];
	static int g_queueLen[QUEUE_SIZE];
	static unsigned int g_queueFromId[QUEUE_SIZE];
	static int g_queueHead;  // read index
	static int g_queueTail;  // write index
	static int g_dropped;    // dropped packets counter
};

void HexDump(const void* data, int len, const char* title);
