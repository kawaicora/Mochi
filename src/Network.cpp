#ifdef ENABLE_EXT_NETWORK

#include "Network.h"
#include "Debug.h"
#include "Protocol.h"
#include <cstdio>

using namespace Protocol;

SOCKET Network::g_sock = INVALID_SOCKET;
CRITICAL_SECTION Network::g_lock;
unsigned int Network::g_playerId = 0;
bool Network::g_ready = false;
char Network::g_queueData[Network::QUEUE_SIZE][Network::MAX_PKT];
int Network::g_queueLen[Network::QUEUE_SIZE];
int Network::g_queueHead = 0;
int Network::g_queueTail = 0;

static bool g_wsaStarted = false;
static bool g_running = false;

static const char* TypeName(uint8_t t) {
	switch (t) {
	case Hello: return "Hello";
	case Welcome: return "Welcome";
	case PlayerJoin: return "PlayerJoin";
	case PlayerLeave: return "PlayerLeave";
	case UdpData: return "UdpData";
	case UdpBroadcast: return "UdpBroadcast";
	case Heartbeat: return "Heartbeat";
	case HeartbeatAck: return "HeartbeatAck";
	case Disconnect: return "Disconnect";
	case Error: return "Error";
	default: return "Unknown";
	}
}

void HexDump(const void* data, int len, const char* title)
{
	if (title && title[0]) Debug::Log("%s\n", title);
	const uint8_t* p = (const uint8_t*)data;
	for (int i = 0; i < len; i += 16) {
		char line[80];
		int pos = 0;
		pos += sprintf_s(line + pos, sizeof(line) - pos, "  ");
		int rowLen = (len - i < 16) ? (len - i) : 16;
		for (int j = 0; j < rowLen; j++) {
			pos += sprintf_s(line + pos, sizeof(line) - pos, "%02X ", p[i + j]);
		}
		for (int j = rowLen; j < 16; j++) {
			pos += sprintf_s(line + pos, sizeof(line) - pos, "   ");
		}
		Debug::Log("%s ", line);
		char ascii[17];
		for (int j = 0; j < rowLen; j++) {
			uint8_t c = p[i + j];
			ascii[j] = (c >= 32 && c < 127) ? c : '.';
		}
		ascii[rowLen] = 0;
		Debug::Log("|%s|\n", ascii);
	}
}

static bool SendAll(SOCKET s, const char* data, int len) {
	int sent = 0;
	while (sent < len) {
		int n = send(s, data + sent, len - sent, 0);
		if (n == SOCKET_ERROR) return false;
		sent += n;
	}
	return true;
}

bool Network::Connect(const char* host, unsigned short port)
{
	struct addrinfo hints, *result = NULL, *rp;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char portStr[16];
	_itoa_s(port, portStr, 10);

	int err = getaddrinfo(host, portStr, &hints, &result);
	if (err != 0) {
		Debug::Log("getaddrinfo(%s) failed: %d\n", host, err);
		return false;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		g_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (g_sock == INVALID_SOCKET) continue;
		if (connect(g_sock, rp->ai_addr, (int)rp->ai_addrlen) != SOCKET_ERROR) break;
		closesocket(g_sock);
		g_sock = INVALID_SOCKET;
	}
	freeaddrinfo(result);

	if (g_sock == INVALID_SOCKET) {
		Debug::Log("connect to %s:%d failed: %d\n", host, port, WSAGetLastError());
		return false;
	}
	return true;
}

bool Network::Init(const char* host, unsigned short port)
{
	if (!g_wsaStarted) {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
		g_wsaStarted = true;
	}

	InitializeCriticalSection(&g_lock);
	g_running = true;

	Debug::Log("Connecting to %s:%d...\n", host, port);
	if (!Connect(host, port)) return false;
	Debug::Log("Connected\n");

	int helloLen;
	uint8_t* helloPkt = BuildHello(0x00010000, 1234, "Player", helloLen);
	SendAll(g_sock, (char*)helloPkt, helloLen);
#ifdef DEBUG_DISPLAY_PACKET
	HexDump(helloPkt, helloLen, "[SEND] Hello");
#endif
	delete[] helloPkt;

	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, NULL, 0, NULL);
	CloseHandle(hThread);
	return true;
}

void Network::Shutdown()
{
	g_running = false;
	if (g_sock != INVALID_SOCKET) {
		int len;
		uint8_t* pkt = BuildDisconnect(len);
		SendAll(g_sock, (char*)pkt, len);
#ifdef DEBUG_DISPLAY_PACKET
		HexDump(pkt, len, "[SEND] Disconnect");
#endif
		delete[] pkt;
		shutdown(g_sock, SD_BOTH);
		closesocket(g_sock);
		g_sock = INVALID_SOCKET;
	}
	DeleteCriticalSection(&g_lock);
	if (g_wsaStarted) WSACleanup();
}

bool Network::SendGameData(const void* data, int len)
{
	if (g_sock == INVALID_SOCKET || !g_ready) return false;

	int pktLen;
	uint8_t* pkt = BuildUdpData(0x01, g_playerId, BROADCAST_ID, data, (uint16_t)len, pktLen);
	bool ok = SendAll(g_sock, (char*)pkt, pktLen) != 0;
#ifdef DEBUG_DISPLAY_PACKET
	if (ok) HexDump(pkt, pktLen, "[SEND] UdpData");
#endif
	delete[] pkt;
	return ok;
}

int Network::PollReceived(void* outBuf, int maxLen)
{
	EnterCriticalSection(&g_lock);
	if (g_queueHead == g_queueTail) { LeaveCriticalSection(&g_lock); return 0; }

	int len = g_queueLen[g_queueHead];
	if (len > maxLen) len = maxLen;
	memcpy(outBuf, g_queueData[g_queueHead], len);
	g_queueHead = (g_queueHead + 1) % QUEUE_SIZE;

	LeaveCriticalSection(&g_lock);
	return len;
}

bool Network::IsReady() { return g_ready; }
unsigned int Network::GetPlayerId() { return g_playerId; }

void Network::WorkerThread()
{
	char buf[8192];
	PacketHeader hdr;

	while (g_running && g_sock != INVALID_SOCKET) {
		int total = 0;
		while (total < HEADER_SIZE && g_running) {
			int n = recv(g_sock, (char*)&hdr + total, HEADER_SIZE - total, 0);
			if (n <= 0) goto disconnected;
			total += n;
		}

		if (hdr.magic != PACKET_MAGIC) goto disconnected;

		int bodyLen = hdr.length;
		if (bodyLen > (int)sizeof(buf)) bodyLen = sizeof(buf);
		total = 0;
		while (total < bodyLen && g_running) {
			int n = recv(g_sock, buf + total, bodyLen - total, 0);
			if (n <= 0) goto disconnected;
			total += n;
		}

		Debug::Log("[RECV] %s (%d bytes)\n", TypeName(hdr.type), bodyLen);
#ifdef DEBUG_DISPLAY_PACKET
		HexDump(&hdr, HEADER_SIZE + bodyLen, NULL);
#endif

		switch (hdr.type) {
		case Welcome: {
			if (bodyLen >= 12) {
				g_playerId = ((WelcomeBody*)buf)->playerId;
				g_ready = true;
				Debug::Log("Welcome! playerId=%u\n", g_playerId);
			}
			break;
		}
		case UdpData: {
			if (bodyLen >= (int)sizeof(UdpDataHeader)) {
				UdpDataHeader* uh = (UdpDataHeader*)buf;
				int payloadLen = uh->payloadLen;
				if (payloadLen > bodyLen - (int)sizeof(UdpDataHeader))
					payloadLen = bodyLen - (int)sizeof(UdpDataHeader);
				if (payloadLen > MAX_PKT) payloadLen = MAX_PKT;

				EnterCriticalSection(&g_lock);
				int nextTail = (g_queueTail + 1) % QUEUE_SIZE;
				if (nextTail != g_queueHead) { // queue not full
					memcpy(g_queueData[g_queueTail], buf + sizeof(UdpDataHeader), payloadLen);
					g_queueLen[g_queueTail] = payloadLen;
					g_queueTail = nextTail;
				}
				LeaveCriticalSection(&g_lock);
			}
			break;
		}
		case HeartbeatAck:
			break;
		case Disconnect:
			goto disconnected;
		default:
			break;
		}
	}

disconnected:
	Debug::Log("Worker ended\n");
	g_ready = false;
	g_sock = INVALID_SOCKET;
}

#endif // ENABLE_EXT_NETWORK
