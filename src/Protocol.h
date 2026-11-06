/*
	Protocol.h - TCP protocol between client and central server
	Must match mochi-online-server/src/protocol.py
*/
#pragma once

#include <windows.h>
#include <cstdint>

namespace Protocol {

// Packet magic "MC" = 0x4D43
const uint16_t PACKET_MAGIC = 0x4D43;
const uint8_t PROTOCOL_VERSION = 1;

// Header format: Magic(2) + Version(1) + Type(1) + Length(2) + Reserved(2) = 8 bytes
#pragma pack(push, 1)
struct PacketHeader {
	uint16_t magic;
	uint8_t  version;
	uint8_t  type;
	uint16_t length;
	uint16_t reserved;
};
#pragma pack(pop)

const int HEADER_SIZE = sizeof(PacketHeader);

enum PacketType : uint8_t {
	Hello            = 0x01,
	Welcome          = 0x02,
	PlayerJoin       = 0x03,
	PlayerLeave      = 0x04,
	UdpData          = 0x10,
	UdpBroadcast     = 0x11,
	Heartbeat        = 0x20,
	HeartbeatAck     = 0x21,
	Disconnect       = 0x22,
	LadderRequest    = 0x30,
	LadderResponse   = 0x31,
	LadderSearch     = 0x32,
	GameResult       = 0x34,
	HighscoreRequest = 0x36,
	Error            = 0xFF
};

// Build a packet with given type and body
// Returns heap-allocated buffer, caller frees with delete[]
inline uint8_t* BuildPacket(uint8_t type, const void* body, uint16_t bodyLen, int& outTotalLen) {
	int total = HEADER_SIZE + bodyLen;
	uint8_t* buf = new uint8_t[total];
	PacketHeader* hdr = (PacketHeader*)buf;
	hdr->magic = PACKET_MAGIC;
	hdr->version = PROTOCOL_VERSION;
	hdr->type = type;
	hdr->length = bodyLen;
	hdr->reserved = 0;
	if (body && bodyLen > 0) {
		memcpy(buf + HEADER_SIZE, body, bodyLen);
	}
	outTotalLen = total;
	return buf;
}

// ---- Hello packet ----
// Body: game_version(u32) + game_udp_port(u16) + player_name(char[32] null-terminated)
inline uint8_t* BuildHello(uint32_t gameVersion, uint16_t gameUdpPort, const char* playerName, int& outLen) {
	uint8_t body[64];
	memset(body, 0, sizeof(body));
	*(uint32_t*)body = gameVersion;
	*(uint16_t*)(body + 4) = gameUdpPort;
	strncpy_s((char*)(body + 6), 32, playerName, 31);
	return BuildPacket(Hello, body, 6 + (uint16_t)strlen(playerName) + 1, outLen);
}

// ---- Welcome packet ----
// Body: player_id(u32) + max_players(u32) + current_players(u32)
#pragma pack(push, 1)
struct WelcomeBody {
	uint32_t playerId;
	uint32_t maxPlayers;
	uint32_t currentPlayers;
};

// ---- PlayerJoin / PlayerLeave ----
// Join body: player_id(u32) + player_name(char[32] null-terminated)
struct PlayerJoinBody {
	uint32_t playerId;
	char playerName[32];
};

// ---- UdpData packet ----
// Body: inner_type(u8) + from_player_id(u32) + to_player_id(u32) + payload_len(u16) + payload
const uint32_t BROADCAST_ID = 0xFFFFFFFF;

struct UdpDataHeader {
	uint8_t  innerType;
	uint32_t fromPlayerId;
	uint32_t toPlayerId;
	uint16_t payloadLen;
};
#pragma pack(pop)

inline uint8_t* BuildUdpData(uint8_t innerType, uint32_t fromId, uint32_t toId,
							 const void* payload, uint16_t payloadLen, int& outLen) {
	int bodyLen = sizeof(UdpDataHeader) + payloadLen;
	uint8_t* body = new uint8_t[bodyLen];
	UdpDataHeader* h = (UdpDataHeader*)body;
	h->innerType = innerType;
	h->fromPlayerId = fromId;
	h->toPlayerId = toId;
	h->payloadLen = payloadLen;
	if (payload && payloadLen > 0)
		memcpy(body + sizeof(UdpDataHeader), payload, payloadLen);

	uint8_t* pkt = BuildPacket(UdpData, body, (uint16_t)bodyLen, outLen);
	delete[] body;
	return pkt;
}

// ---- Heartbeat ----
inline uint8_t* BuildHeartbeat(int& outLen) {
	return BuildPacket(Heartbeat, nullptr, 0, outLen);
}

inline uint8_t* BuildHeartbeatAck(int& outLen) {
	return BuildPacket(HeartbeatAck, nullptr, 0, outLen);
}

// ---- Disconnect ----
inline uint8_t* BuildDisconnect(int& outLen) {
	return BuildPacket(Disconnect, nullptr, 0, outLen);
}

// ---- GameResult ----
// Body: winner_id(u32) + loser_id(u32) + winner_kills(u16) + loser_kills(u16) + winner_name + null + loser_name + null
inline uint8_t* BuildGameResult(uint32_t winnerId, uint32_t loserId,
								uint16_t winnerKills, uint16_t loserKills,
								const char* winnerName, const char* loserName, int& outLen) {
	size_t wnLen = strlen(winnerName);
	size_t lnLen = strlen(loserName);
	int bodyLen = 12 + (int)wnLen + 1 + (int)lnLen + 1;
	uint8_t* body = new uint8_t[bodyLen];
	int pos = 0;
	*(uint32_t*)(body + pos) = winnerId; pos += 4;
	*(uint32_t*)(body + pos) = loserId; pos += 4;
	*(uint16_t*)(body + pos) = winnerKills; pos += 2;
	*(uint16_t*)(body + pos) = loserKills; pos += 2;
	memcpy(body + pos, winnerName, wnLen); pos += (int)wnLen;
	body[pos++] = 0;
	memcpy(body + pos, loserName, lnLen); pos += (int)lnLen;
	body[pos++] = 0;

	uint8_t* pkt = BuildPacket(GameResult, body, (uint16_t)bodyLen, outLen);
	delete[] body;
	return pkt;
}

}
