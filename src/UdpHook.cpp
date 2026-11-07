#ifdef ENABLE_EXT_NETWORK

#include "UdpHook.h"
#include "Debug.h"
#include "Network.h"
#include "GeneralHook.h"
#include <Syringe.h>
#include <Helpers/Macro.h>
#include <WinsockInterfaceClass.h>

#ifdef DEBUG_DISPLAY_PACKET
static void LogHead(const char* tag, const void* data, int len, int maxBytes = 24)
{
	const uint8_t* p = (const uint8_t*)data;
	char line[128];
	int pos = 0;
	line[0] = 0;
	int n = (len < maxBytes) ? len : maxBytes;
	for (int i = 0; i < n && pos < (int)sizeof(line) - 4; i++)
		pos += sprintf_s(line + pos, sizeof(line) - pos, "%02X ", p[i]);
	Debug::Log("[%s] len=%d: %s\n", tag, len, line);
}

// Offset of the first 36 12 magic pair inside a buffer, or -1.
static int memmem_magic(const void* data, int len)
{
	const uint8_t* p = (const uint8_t*)data;
	for (int i = 0; i + 1 < len; i++)
		if (p[i] == 0x36 && p[i + 1] == 0x12) return i;
	return -1;
}
#endif

// ---------------------------------------------------------------------------
// sendto hook: forward game packets to TCP relay server
// Also drain the receive queue here, since we know the game thread is running
// its network loop when it calls sendto.
// ---------------------------------------------------------------------------
DEFINE_HOOK(0x7C89B0, Hook_SendTo, 6)
{
	GET_STACK(const char*, buf, 8);
	GET_STACK(int, len, 12);

	if (len > 0 && buf) {
#ifdef DEBUG_DISPLAY_PACKET
		LogHead("GAME_SEND", buf, len);
#endif
		// Forward to TCP relay server
		Network::SendGameData(buf, len);
	}

	// Drain received packets from TCP relay and inject into game recv queue
	// (we're on the game thread here, so it's safe to touch game structures)
	static char recvBuf[1024];
	while (true) {
		unsigned int fromId = 0;
		int n = Network::PollReceivedEx(recvBuf, sizeof(recvBuf), &fromId);
		if (n <= 0) break;

#ifdef DEBUG_DISPLAY_PACKET
		Debug::Log("[INJECT] %d bytes from=%u (sub=%02X %02X %02X %02X magic=%02X %02X type=%02X seq=%02X)\n",
			n, fromId,
			((uint8_t*)recvBuf)[0], ((uint8_t*)recvBuf)[1], ((uint8_t*)recvBuf)[2], ((uint8_t*)recvBuf)[3],
			((uint8_t*)recvBuf)[4], ((uint8_t*)recvBuf)[5],
			((uint8_t*)recvBuf)[6],
			((uint8_t*)recvBuf)[8]);
#endif
		InjectPacket(recvBuf, n, fromId);
	}

	return 0;
}

// ---------------------------------------------------------------------------
// Injects a received TCP-relayed UDP packet into the game's network stack.
//
// The game's recv path (sub_7B3AA0, WSAAsyncSelect callback) does:
//   1. recvfrom into ReceiveBuffer
//   2. Fill WinsockBufferType:
//      - CRC = first 4 bytes of UDP data
//      - BufferLen = recv_len - 4
//      - Buffer = bytes 4..n of UDP data
//   3. Call vtable[22] to route to IPXGlobalConnClass
//   4. If accepted, append to InBuffers
//
// Wire format: [4-byte sub-header][IPX header(16 bytes) + payload]
//   - bytes 0-3: sub-header (varies per packet)
//   - bytes 4-5: magic = 0x1236 (constant)
//   - byte 6: type (01 = data)
//   - bytes 8-11: sequence number
//
// We must match this split: first 4 bytes -> CRC field, rest -> Buffer[].
// ---------------------------------------------------------------------------
void InjectPacket(const void* data, int len, unsigned int fromId)
{
	WinsockInterfaceClass* pWinsock = WinsockInterfaceClass::Instance;
	if (!pWinsock) return;

	// Need at least 4 bytes sub-header + some payload
	if (len < 20) return;

	// Get a free slot from StaticInBuffer (ring buffer of 128 pre-allocated slots)
	DWORD pos = pWinsock->StaticInBufferPos;
	WinsockBufferType* pBuf = &pWinsock->StaticInBuffer[pos];

	// Zero the whole buffer first
	memset(pBuf, 0, sizeof(WinsockBufferType));

	// Split at offset 4, matching game's recv path:
	//   bytes 0-3 -> CRC field (offset 0x1C)
	//   bytes 4+  -> Buffer[]  (offset 0x20)
	const uint8_t* p = (const uint8_t*)data;

#ifdef DEBUG_DISPLAY_PACKET
	LogHead("INJECT_RAW", data, len);
#endif

	// Relay payload is user data only: [4B sub-header][magic 36 12 ...].
	// The game's native recv path stores the first 4 bytes into pBuf->CRC and
	// copies the REST starting at Buffer[0], so magic lands at Buffer[0].
	// Reproduce exactly that: do NOT prepend anything, do NOT keep 4 bytes in Buffer.
	pBuf->CRC = *(uint32_t*)p;

	int dataLen = len - 4;
	if (dataLen > 640) dataLen = 640;
	if (dataLen < 0) dataLen = 0;
	memcpy(pBuf->Buffer, p + 4, dataLen);
	pBuf->BufferLen = dataLen;

#ifdef DEBUG_DISPLAY_PACKET
	LogHead("INJECT_BUF", pBuf->Buffer, dataLen);
#endif

	pBuf->IsBroadcast = 1;
	pBuf->InUse = 1;
	pBuf->Allocated = 0;

	// The native recv path also stores the sender's sockaddr into the buffer
	// header (sub_7B3AA0 lines 142-158):
	//   Header[4..7] = source IP, in network byte order
	//   SrcPort      = source port, in host byte order
	// Leaving these at zero makes every relayed player look like the same origin,
	// which is why the game could not tell whose options packet it had received.
	uint32_t fakeIp = 0x0A000000u | ((fromId + 1) & 0x00FFFFFFu);   // 10.x.x.x, unique per player
	*(uint32_t*)(pBuf->Header + 4) = htonl(fakeIp);
	pBuf->SrcPort = (int16_t)(2000 + (fromId & 0x7FFF));

#ifdef DEBUG_DISPLAY_PACKET
	Debug::Log("[INJECT] magic@buf=%d crc=%08X srcIP=%08X srcPort=%d\n",
		(int)(memmem_magic(pBuf->Buffer, dataLen)),
		pBuf->CRC, fakeIp, (int)pBuf->SrcPort);
#endif

	// Run the game's own acceptance filter, exactly like the native recv path does
	// at sub_7B3AA0 line 125:  (*(vtable + 88))(this, pBuf) -> bool accept.
	// Skipping this step let malformed-looking packets reach InBuffers, where the
	// consumer silently dropped them (that is why the big game-options packet never
	// took effect while small chat/room packets happened to survive).
	typedef unsigned __int8 (__thiscall *AcceptProc)(void* self, WinsockBufferType* buf);
	AcceptProc accept = *(AcceptProc*)((*(DWORD*)pWinsock) + 88);
	unsigned __int8 accepted = accept(pWinsock, pBuf);

#ifdef DEBUG_DISPLAY_PACKET
	Debug::Log("[INJECT] accepted=%d (vtable+88)\n", (int)accepted);
#endif

	if (!accepted) {
		// Native path frees the slot and never queues it; mirror that.
		pBuf->InUse = 0;
		return;
	}

	// Add to InBuffers (DynamicVectorClass<WinsockBufferType*>)
	auto& inBufs = pWinsock->InBuffers;
	if (inBufs.Count < inBufs.Capacity) {
		inBufs.Items[inBufs.Count] = pBuf;
		inBufs.Count++;
	}

	// Advance ring buffer position
	pWinsock->StaticInBufferPos = (pos + 1) % 128;
	pWinsock->StaticInBuffersInUse++;
}

// ---------------------------------------------------------------------------
// UdpHookInit: subscribe to events, set up hooks
// ---------------------------------------------------------------------------
void UdpHookInit()
{
	Debug::Log("UdpHookInit: sendto@0x7C89B0 hook installed\n");
}

#endif // ENABLE_EXT_NETWORK
