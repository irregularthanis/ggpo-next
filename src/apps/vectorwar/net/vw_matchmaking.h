#ifndef _VW_MATCHMAKING_H_
#define _VW_MATCHMAKING_H_

/* Matchmaking client: JOIN a lobby, BIND to every other player's relay port,
 * then (protocol v3) learn every member's hole-punching candidates. All from
 * one UDP socket, the one the path proxy keeps using afterwards. */

#include "vw_net_protocol.h"
#include "vw_udp.h"

namespace vwnet {

struct MatchResult {
   int player;                         /* 1..count */
   int count;
   uint64_t token;
   int platforms[kMaxPlayers];
   Endpoint relay[kMaxPlayers];        /* server:R[k]; relay[player - 1] unused */
   bool have_candidates;               /* the server answered PEERS_REQUEST */
   Endpoint public_candidates[kMaxPlayers];
   Endpoint lan_candidates[kMaxPlayers];
};

struct MatchmakingOptions {
   int group_size;                     /* 2-4 */
   const char *lobby;                  /* "" for the public queue */
   int platform;
   uint32_t join_timeout_ms;           /* waiting for the group to fill */
   uint32_t server_timeout_ms;         /* no answer at all: server unreachable */
   uint32_t bind_timeout_ms;
   uint32_t candidates_timeout_ms;
   bool advertise_lan;                 /* send our LAN address as a candidate */
   /* Called with (joined, group_size) whenever WAITING changes; may be NULL. */
   void (*progress)(int joined, int group_size, void *context);
   /* Polled about every 50 ms while waiting for the group (a start screen
    * pumps its events here); returning false cancels. May be NULL. */
   bool (*keep_going)(void *context);
   void *context;
   MatchmakingOptions();
};

/* Runs the whole exchange on socket (already open). On failure returns false
 * and writes a short reason into error. */
bool Matchmake(UdpSocket *socket, const Endpoint &server, const MatchmakingOptions &options,
               MatchResult *result, char *error, size_t error_length);

}  // namespace vwnet

#endif
