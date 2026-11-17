#ifndef _VW_NET_PROTOCOL_H_
#define _VW_NET_PROTOCOL_H_

/* Vector War matchmaking protocol (client side). Mirrors
 * services/matchmaking/matchmaking/protocol.py byte for byte; both sides'
 * tests decode services/matchmaking/protocol_vectors.json. */

#include "vw_net_types.h"

namespace vwnet {

enum {
   kVersion = 2,
   kPeersVersion = 3,            /* PEERS_REQUEST and PEERS only */
   kMaxPlayers = 4,
   kHeaderSize = 8,
   kJoinSize = 64,
   kMatchSize = 44,
   kSmallSize = 20,              /* WAITING, BIND, BOUND, HELLO, ERROR */
   kPeersSize = 68,              /* PEERS_REQUEST and PEERS */
   kMaxMessageSize = kPeersSize,
   kLobbyCodeSize = 8
};

enum MessageType {
   kJoin = 1, kWaiting = 2, kMatch = 3, kBind = 4, kBound = 5,
   kHello = 6, kError = 7, kPeersRequest = 8, kPeers = 9
};

enum Platform { kPlatformUnknown = 0, kPlatformWindows = 1, kPlatformLinux = 2, kPlatformAndroid = 3 };
enum ErrorCode { kErrorVersionMismatch = 1, kErrorRateLimited = 2, kErrorServerFull = 3 };

/* One message; only the fields of its type are meaningful. */
struct Message {
   int type;
   uint64_t nonce;               /* JOIN, WAITING, MATCH, HELLO, ERROR */
   uint64_t token;               /* MATCH, BIND, BOUND, PEERS_REQUEST, PEERS */
   int platform;                 /* JOIN, HELLO */
   int group_size;               /* JOIN, WAITING */
   char lobby[kLobbyCodeSize + 1];   /* JOIN: upper case, NUL terminated */
   int joined;                   /* WAITING */
   int player;                   /* MATCH, BIND, BOUND, PEERS_REQUEST */
   int count;                    /* MATCH, PEERS */
   uint16_t relay_ports[kMaxPlayers];   /* MATCH */
   int platforms[kMaxPlayers];          /* MATCH */
   int code;                     /* ERROR */
   Endpoint lan;                 /* PEERS_REQUEST */
   Endpoint public_candidates[kMaxPlayers];  /* PEERS */
   Endpoint lan_candidates[kMaxPlayers];     /* PEERS */
};

void ClearMessage(Message *message, int type);

/* Upper-case and validate a lobby code (up to 8 of A-Z 0-9; "" is the public
 * queue). Returns false for anything else. */
bool NormalizeLobby(const char *code, char out[kLobbyCodeSize + 1]);

/* Encode into out (kMaxMessageSize bytes are always enough). Returns the
 * datagram size, or 0 if the message is invalid. */
size_t Encode(const Message &message, uint8_t *out, size_t capacity);

/* Decode exact wire bytes with the server's rejection rules. */
bool Decode(const uint8_t *data, size_t length, Message *message);

}  // namespace vwnet

#endif
