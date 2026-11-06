#ifdef ENABLE_EXT_NETWORK

#include "UdpHook.h"
#include "Debug.h"
#include "Network.h"
#include "GeneralHook.h"
#include <Syringe.h>
#include <Helpers/Macro.h>
#include <WinsockInterfaceClass.h>

// ---------------------------------------------------------------------------
// sendto hook: forward game packets to TCP relay server
// Also drain the receive queue here, since we know the game thread is running
// its network loop when it calls sendto.
// ---------------------------------------------------------------------------
DEFINE_HOOK(0x7C89B0, Hook_SendTo, 6)
{
	GET_STACK(const char*, buf, 8);
	GET_STACK(int, len, 12);

	// Forward to TCP relay server (copy, don't replace)
	if (len > 0 && buf) {
		Network::SendGameData(buf, len);
	}

	// Drain received packets from TCP relay and inject into game recv queue
	// (we're on the game thread here, so it's safe to touch game structures)
	static char recvBuf[1024];
	while (true) {
		int n = Network::PollReceived(recvBuf, sizeof(recvBuf));
		if (n <= 0) break;

#ifdef DEBUG_DISPLAY_PACKET
		Debug::Log("[INJECT] %d bytes (sub=%02X %02X %02X %02X magic=%02X %02X type=%02X seq=%02X)\n",
			n,
			((uint8_t*)recvBuf)[0], ((uint8_t*)recvBuf)[1], ((uint8_t*)recvBuf)[2], ((uint8_t*)recvBuf)[3],
			((uint8_t*)recvBuf)[4], ((uint8_t*)recvBuf)[5],
			((uint8_t*)recvBuf)[6],
			((uint8_t*)recvBuf)[8]);
#endif
		InjectPacket(recvBuf, n);
	}

	// Call original sendto so game network stack works normally
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
void InjectPacket(const void* data, int len)
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
	pBuf->CRC = *(uint32_t*)data;
	int dataLen = len - 4;
	if (dataLen > 640) dataLen = 640;
	memcpy(pBuf->Buffer, (const char*)data + 4, dataLen);
	pBuf->BufferLen = dataLen;

	pBuf->IsBroadcast = 1;
	pBuf->InUse = 1;
	pBuf->Allocated = 0;
	pBuf->SrcPort = 0;

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
