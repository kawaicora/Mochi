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
// Can we hand one more packet to the game right now?
//
// The game's receive ring is WinsockInterfaceClass::StaticInBuffer[128]. We
// inject by claiming the slot at StaticInBufferPos. If that slot is still
// marked InUse, the game has not consumed it yet and writing into it would
// destroy unread data. Same for the pointer list: once InBuffers is full the
// packet could not be published anyway.
// ---------------------------------------------------------------------------
static bool CanInject()
{
	WinsockInterfaceClass* pW = WinsockInterfaceClass::Instance;
	if (!pW) return false;

	// StaticInBufferPos is reset while the scenario loads / the interface is
	// rebuilt; a stale value would make StaticInBuffer[pos] an out-of-bounds
	// access and crash the game. Guard it.
	DWORD pos = pW->StaticInBufferPos;
	if (pos >= 128) return false;

	if (pW->StaticInBuffersInUse >= 128) return false;
	if (pW->StaticInBuffer[pos].InUse != 0) return false;
	if (pW->InBuffers.Count >= pW->InBuffers.Capacity) return false;
	return true;
}

// ---------------------------------------------------------------------------
// sendto hook: forward game packets to TCP relay server.
// Receive-side injection happens in UdpHookDrain(), called from the logic
// update event (see Mochi.cpp), not from here.
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

	// Injecting from inside the game's send path crashed the game right after
	// a packet was sent (out-of-bounds StaticInBufferPos and re-entrancy). The
	// receive queue is therefore drained only from the logic-update events,
	// never from here.

	return 0;
}

// ---------------------------------------------------------------------------
// Drains the relay receive queue and injects packets into the game's network
// stack. Called from the logic-update event and from the sendto hook.
//
// Why not only from sendto: the game can stop sending for a while (loading,
// pause, a slow tick, the other side stalling). The queue then kept growing
// and the next sendto swallowed the whole backlog at once; a 184-packet burst
// in one tick desynced the game and it crashed / dropped the player.
//
// Why budgeted: each call may inject at most 32 packets, so neither a stalled
// game nor a 200-calls-per-second sendto can hand the game a huge burst.
// ---------------------------------------------------------------------------
// Re-entry guard: InjectPacket calls the game's accept filter, which may itself
// end up sending, and that comes straight back into our sendto hook. Without
// this flag the drain would recurse into itself.
static bool g_inDrain = false;

void UdpHookDrain()
{
	if (g_inDrain) return;
	g_inDrain = true;

#ifdef DEBUG_DISPLAY_PACKET
	// Prove from the log that the logic-update hook really runs, at most one
	// line per second.
	{
		static DWORD lastTickLog = 0;
		DWORD now = GetTickCount();
		if (now - lastTickLog >= 1000) {
			Debug::Log("[TICK] drain running\n");
			lastTickLog = now;
		}
	}
#endif

	static char recvBuf[2048];

	// Budget per logic tick. The relay peaks around 200 packets/second; at 15
	// logic fps that is ~14 per tick, so 32 leaves headroom without ever
	// handing the game a huge burst.
	int budget = 32;
	bool more = false;

	while (budget-- > 0) {
		// Stop before taking a packet off our queue when the game side has no
		// free slot: an unread packet must never be overwritten, and the data
		// we already hold is more valuable than one tick of latency.
		if (!CanInject()) {
			more = true;
			break;
		}

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
		if (budget == 0) more = true;
	}

#ifdef DEBUG_DISPLAY_PACKET
	if (more) {
		Debug::Log("[INJECT] backlog remains, will continue next tick\n");
	}
#endif

	g_inDrain = false;
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
	if (pos >= 128) return;   // stale position after interface rebuild
	WinsockBufferType* pBuf = &pWinsock->StaticInBuffer[pos];

	// Never write over a slot the game has not consumed yet.
	if (pBuf->InUse != 0) return;

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
	if (!inBufs.Items || inBufs.Count < 0 || inBufs.Count >= inBufs.Capacity) {
		// Cannot publish this packet. Release the slot and leave the ring
		// position untouched so the next inject retries the same slot.
		pBuf->InUse = 0;
		return;
	}
	inBufs.Items[inBufs.Count] = pBuf;
	inBufs.Count++;

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
