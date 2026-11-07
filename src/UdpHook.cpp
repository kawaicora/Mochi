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

	// Nothing else can be gated here. In the lobby the InBuffers list has no
	// allocation at all yet (Capacity == 0), and it grows on demand through
	// AddItem - the same call the game's own receive path makes. Requiring
	// Capacity > 0 here made injection silently stop everywhere except mid
	// match. The ring slot, StaticInBufferPos and the in-use counter belong to
	// the game's allocator and must not be second-guessed.
	if (pW->InBuffers.Count < 0) return false;
	return true;
}

// ---------------------------------------------------------------------------
// sendto hook: forward game packets to TCP relay server.
// Receive-side injection happens in UdpHookDrain(), called from the logic
// update event (see Mochi.cpp), not from here.
// ---------------------------------------------------------------------------
// stdcall stub standing in for a successful sendto: report the requested length
// as the result and pop the six arguments, exactly as the real function would.
__declspec(naked) static void FakeSendToOk()
{
	__asm {
		mov eax, [esp + 12]   // len
		ret 24                // sendto(s, buf, len, flags, to, tolen)
	}
}

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

	// Do NOT fall through to the real sendto. Under the relay every destination
	// address is synthetic (10.x.x.x from InjectPacket), so a real send always
	// fails, and the game answers that with "ConnectionClass::Service_Send_Queue
	// bad connection" until it considers the player gone. Report success instead.
	return (int)FakeSendToOk;
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

// Per-second counters, printed as one [STAT] line. They answer the only
// question that matters when packets stop reaching the game: is the drain not
// being called, or is it called and finding nothing / no slot?
static DWORD g_statCalls = 0;
static DWORD g_statInjected = 0;
static DWORD g_statEmpty = 0;
static DWORD g_statNoBuf = 0;
static DWORD g_statLastPrint = 0;

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

	// Budget per call. The relay peaks near 200 packets/second and the message
	// pump ticks with the game frame (~15 fps), so a small budget throttled
	// injection to a fraction of the arriving traffic and the match timed out
	// on missing packets. 128 per call is still bounded.
	int budget = 128;
	bool more = false;
	g_statCalls++;

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
		if (n <= 0) { g_statEmpty++; break; }

#ifdef DEBUG_DISPLAY_PACKET
		Debug::Log("[INJECT] %d bytes from=%u (sub=%02X %02X %02X %02X magic=%02X %02X type=%02X seq=%02X)\n",
			n, fromId,
			((uint8_t*)recvBuf)[0], ((uint8_t*)recvBuf)[1], ((uint8_t*)recvBuf)[2], ((uint8_t*)recvBuf)[3],
			((uint8_t*)recvBuf)[4], ((uint8_t*)recvBuf)[5],
			((uint8_t*)recvBuf)[6],
			((uint8_t*)recvBuf)[8]);
#endif
		if (!InjectPacket(recvBuf, n, fromId)) {
			g_statNoBuf++;
			more = true;
			break;
		}
		g_statInjected++;
		if (budget == 0) more = true;
	}

#ifdef DEBUG_DISPLAY_PACKET
	{
		DWORD now = GetTickCount();
		if (now - g_statLastPrint >= 1000) {
			Debug::Log("[STAT] calls=%u injected=%u empty=%u nobuf=%u%s\n",
				g_statCalls, g_statInjected, g_statEmpty, g_statNoBuf,
				more ? " backlog" : "");
			g_statCalls = g_statInjected = g_statEmpty = g_statNoBuf = 0;
			g_statLastPrint = now;
		}
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
// Hands a buffer back the way the native receive path does (sub_7B3AA0
// lines 132-140): a heap buffer is deleted, a static slot only clears InUse.
// StaticInBuffersInUse is intentionally not touched here - the native path on
// this route does not decrement it either, and our own bookkeeping must never
// become a gate again.
static void ReleaseInBuffer(WinsockBufferType* pBuf)
{
	if (pBuf->Allocated) {
		delete pBuf;
	} else {
		pBuf->InUse = 0;
	}
}

// Returns 1 when the packet was handled (injected or rejected by the game's
// filter), 0 when no receive buffer was available and the caller should stop.
int InjectPacket(const void* data, int len, unsigned int fromId)
{
	WinsockInterfaceClass* pWinsock = WinsockInterfaceClass::Instance;
	if (!pWinsock) return 1;

	// Need at least 4 bytes sub-header + some payload
	if (len < 20) return 1;

	// Take a receive buffer from the game exactly the way its own recv path does
	// (sub_7B2240): it walks StaticInBuffer for a free slot, advances
	// StaticInBufferPos itself, and falls back to a heap buffer once all 128
	// static slots are taken. Picking the slot by hand, with our own idea of
	// when the ring was full, is what made injection stop after a few hundred
	// packets and drop the match.
	typedef BYTE* (__thiscall *AllocInBufferProc)(void* self);
	AllocInBufferProc allocInBuffer = (AllocInBufferProc)0x7B2240;
	WinsockBufferType* pBuf = (WinsockBufferType*)allocInBuffer(pWinsock);
	if (!pBuf) return 0;   // no slot right now; ask the caller to stop draining

	// Zero the slot but keep the lifetime right: a heap buffer must still be
	// freed with operator delete, a static one only has InUse cleared. Decide
	// by address so a stale flag in a reused static slot cannot mislead us.
	bool isStaticSlot = (pBuf >= pWinsock->StaticInBuffer &&
		pBuf < pWinsock->StaticInBuffer + 128);
	BYTE wasAllocated = pBuf->Allocated;
	memset(pBuf, 0, sizeof(WinsockBufferType));
	pBuf->Allocated = isStaticSlot ? 0 : 1;
	(void)wasAllocated;

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
		// Native path releases the buffer and never queues it; mirror that.
		ReleaseInBuffer(pBuf);
		return 1;
	}

	// Publish it exactly the way the native path does (sub_7B3AA0 lines 159-171):
	// AddItem grows the vector when needed and appends the pointer.
	auto& inBufs = pWinsock->InBuffers;
	if (!inBufs.AddItem(pBuf)) {
		ReleaseInBuffer(pBuf);
		return 1;
	}
	// StaticInBufferPos / StaticInBuffersInUse are owned by the game's
	// allocator (sub_7B2240); advancing them here would double-count.
	return 1;
}

// ---------------------------------------------------------------------------
// UdpHookInit: subscribe to events, set up hooks
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ConnectionClass::Service_Send_Queue (0x48C3E0) hard-fails a player on two
// counts: retry count above the configured limit, or no packet heard from the
// peer for longer than the configured window. Both set the failure bit and the
// verdict flag, and from then on 0x541820 reports
//   "Error - Connection %d has gone bad"
// and the session ends. Under the relay those two clocks measure TCP relay
// jitter, not a dead peer, so the verdicts are disabled. Only the verdicts are
// patched; sending, ordering and everything else in the function stays intact.
// ---------------------------------------------------------------------------
static void NopCode(DWORD address, int length)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect((void*)address, length, PAGE_EXECUTE_READWRITE, &oldProtect))
		return;
	memset((void*)address, 0x90, length);
	VirtualProtect((void*)address, length, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), (void*)address, length);
}

void UdpHookInit()
{
	Debug::Log("UdpHookInit: sendto@0x7C89B0 hook installed\n");

	// 0x48C51D  mov dword ptr [esp+0x10], 1   (retry-limit verdict)
	// 0x48C525  or al,4 / mov [esi],eax       (set failure bit)
	// 0x48C53F  mov dword ptr [esp+0x10], 1   (silence verdict)
	// 0x48C547  or al,4 / mov [esi],eax       (set failure bit)
	NopCode(0x48C51D, 8);
	NopCode(0x48C525, 4);
	NopCode(0x48C53F, 8);
	NopCode(0x48C547, 4);

	// 0x5421DE  mov [esp+0x24], ebp   v55 = 0 when the first socket's liveness
	//          probe fails, which makes the whole networking service report a
	//          dead session.
	// 0x54221B  mov [esp+0x24], ebp   same, for the third socket.
	// Leaving v55 at 1 means Service keeps returning "healthy", so the upper
	// layers never mark the opponent defeated / fire REMOVEPLAYER.
	NopCode(0x5421DE, 4);
	NopCode(0x54221B, 4);

	// 0x542238  jne 0x542257  (alive check passed). The fall-through sets
	// v55 = 0 and logs "Error - Connection %d has gone bad", which tears the
	// session down. Turn it into an unconditional jump so a failed liveness
	// probe is never treated as fatal.
	{
		DWORD oldProtect = 0;
		if (VirtualProtect((void*)0x542238, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
			*(unsigned char*)0x542238 = 0xEB;   // jne -> jmp
			VirtualProtect((void*)0x542238, 2, oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), (void*)0x542238, 2);
		}
	}

	Debug::Log("UdpHookInit: connection fail verdicts disabled\n");
}

#endif // ENABLE_EXT_NETWORK
