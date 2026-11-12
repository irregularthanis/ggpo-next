#ifndef _MATCHMADE_H_
#define _MATCHMADE_H_

/* Matchmade sessions for every Vector War frontend: join a lobby on the
 * matchmaking server, start the path proxy (hole punching with relay
 * fallback), and describe the GGPO session to start. GGPO is unchanged: it
 * binds its own local port and sees each remote player at a loopback
 * endpoint owned by the proxy. See doc/architecture/Matchmaking.md. */

#include <stddef.h>

#include "ggponet.h"

struct MatchmadeArgs {
   char server[128];             /* host name or IPv4 */
   unsigned short server_port;   /* default 7100 */
   int players;                  /* 2-4 */
   char lobby[16];               /* "" = public queue */
   unsigned short local_port;    /* public UDP port; 0 = any */
   bool relay_only;
};

/* Parses "HOST[:PORT] PLAYERS [LOBBY] LOCAL_PORT [--relay-only]" from
 * argv[first..argc). */
bool ParseMatchmakingArgs(int argc, char **argv, int first, MatchmadeArgs *args);

struct MatchmadeSession;

/* Matchmakes (blocking, printing progress) and starts the proxy. On success
 * fills players[0..*num_players), *ggpo_port and *local_player. */
MatchmadeSession *MatchmadeStart(const MatchmadeArgs &args, GGPOPlayer players[GGPO_MAX_PLAYERS],
                                 int *num_players, unsigned short *ggpo_port, int *local_player,
                                 char *error, size_t error_length);

/* One "vectorwar-path:" line per remote player: route and round-trip times. */
void MatchmadeReportPaths(MatchmadeSession *session);

struct VwLegend;

/* Fills the legend from the match (platforms) and the proxy (routes). */
void MatchmadeFillLegend(MatchmadeSession *session, VwLegend *legend);

/* Makes the in-game legend follow this session; call after VectorWar_Init. */
void MatchmadeAttachLegend(MatchmadeSession *session);

/* Stops the proxy and releases the session; NULL is ignored. */
void MatchmadeStop(MatchmadeSession *session);

#endif
