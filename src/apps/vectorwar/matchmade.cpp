#include "matchmade.h"

#include "vw_matchmaking.h"
#include "vw_path_proxy.h"
#include "vw_legend.h"
#include "vectorwar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct MatchmadeSession {
   vwnet::PathProxy proxy;
   vwnet::MatchResult match;
};

namespace {

/* Progress on stdout only: Android test executables are fully static and the
 * NDK has no static liblog; the APK shows progress on its start screen. */
void Say(const char *text)
{
   printf("%s\n", text);
   fflush(stdout);
}

void Progress(int joined, int group_size, void *context)
{
   char text[64];
   snprintf(text, sizeof(text), "vectorwar-match: waiting for players %d/%d", joined, group_size);
   Say(text);
   const MatchmadeArgs *args = (const MatchmadeArgs *)context;
   if (args->progress) args->progress(joined, group_size, args->ui_context);
}

bool KeepGoing(void *context)
{
   const MatchmadeArgs *args = (const MatchmadeArgs *)context;
   return !args->keep_going || args->keep_going(args->ui_context);
}

bool ParsePort(const char *text, unsigned short *port, bool allow_zero)
{
   char *end = NULL;
   const long value = strtol(text, &end, 10);
   if (!*text || *end || value < (allow_zero ? 0 : 1) || value > 65535) return false;
   *port = (unsigned short)value;
   return true;
}

}  // namespace

bool
ParseMatchmakingArgs(int argc, char **argv, int first, MatchmadeArgs *args)
{
   memset(args, 0, sizeof(*args));
   args->server_port = 7100;
   int last = argc;
   if (last > first && strcmp(argv[last - 1], "--relay-only") == 0) {
      args->relay_only = true;
      --last;
   }
   const int count = last - first;          /* HOST PLAYERS [LOBBY] LOCAL_PORT */
   if (count != 3 && count != 4) return false;
   const char *host = argv[first];
   const char *colon = strrchr(host, ':');
   const size_t host_length = colon ? (size_t)(colon - host) : strlen(host);
   if (host_length == 0 || host_length >= sizeof(args->server)) return false;
   memcpy(args->server, host, host_length);
   args->server[host_length] = 0;
   if (colon && !ParsePort(colon + 1, &args->server_port, false)) return false;
   args->players = atoi(argv[first + 1]);
   if (args->players < 2 || args->players > vwnet::kMaxPlayers) return false;
   if (count == 4) {
      if (strlen(argv[first + 2]) >= sizeof(args->lobby)) return false;
      snprintf(args->lobby, sizeof(args->lobby), "%s", strcmp(argv[first + 2], "-") == 0 ? "" : argv[first + 2]);
      char normalized[vwnet::kLobbyCodeSize + 1];
      if (!vwnet::NormalizeLobby(args->lobby, normalized)) return false;
   }
   return ParsePort(argv[last - 1], &args->local_port, true);
}

MatchmadeSession *
MatchmadeStart(const MatchmadeArgs &args, GGPOPlayer players[GGPO_MAX_PLAYERS], int *num_players,
               unsigned short *ggpo_port, int *local_player, char *error, size_t error_length)
{
   if (!vwnet::NetStartup()) {
      snprintf(error, error_length, "network startup failed");
      return NULL;
   }
   vwnet::Endpoint server;
   if (!vwnet::ResolveIPv4(args.server, args.server_port, &server)) {
      snprintf(error, error_length, "cannot resolve the matchmaking server %s", args.server);
      vwnet::NetShutdown();
      return NULL;
   }
   MatchmadeSession *session = new MatchmadeSession();
   if (!session->proxy.OpenPublic(args.local_port)) {
      snprintf(error, error_length, "cannot open UDP port %u", (unsigned)args.local_port);
      MatchmadeStop(session);
      return NULL;
   }
   char text[160];
   snprintf(text, sizeof(text), "vectorwar-match: joining %s:%u for %d players%s%s",
            args.server, (unsigned)args.server_port, args.players,
            args.lobby[0] ? " in lobby " : "", args.lobby);
   Say(text);
   vwnet::MatchmakingOptions options;
   options.group_size = args.players;
   options.lobby = args.lobby;
   options.platform = VwLocalPlatform();  /* same numbering as vwnet */
   options.progress = Progress;
   options.keep_going = KeepGoing;
   options.context = (void *)&args;
   if (!vwnet::Matchmake(session->proxy.PublicSocket(), server, options, &session->match,
                         error, error_length)) {
      MatchmadeStop(session);
      return NULL;
   }

   /* GGPO gets its own free local port; the proxy delivers to it. */
   unsigned short port = 0;
   {
      vwnet::UdpSocket probe;
      if (probe.Open(0, true)) port = probe.LocalPort();
   }
   if (!port || !session->proxy.Start(session->match, port, !args.relay_only)) {
      snprintf(error, error_length, "cannot start the path proxy");
      MatchmadeStop(session);
      return NULL;
   }
   const vwnet::MatchResult &match = session->match;
   snprintf(text, sizeof(text), "vectorwar-match: player=%d of %d candidates=%s direct=%s",
            match.player, match.count, match.have_candidates ? "yes" : "no",
            args.relay_only ? "disabled" : "allowed");
   Say(text);

   memset(players, 0, sizeof(GGPOPlayer) * GGPO_MAX_PLAYERS);
   for (int index = 0; index < match.count; ++index) {
      players[index].size = sizeof(players[index]);
      players[index].player_num = index + 1;
      if (index + 1 == match.player) {
         players[index].type = GGPO_PLAYERTYPE_LOCAL;
      } else {
         players[index].type = GGPO_PLAYERTYPE_REMOTE;
         snprintf(players[index].u.remote.ip_address,
                  sizeof(players[index].u.remote.ip_address), "127.0.0.1");
         players[index].u.remote.port = session->proxy.LoopbackPort(index + 1);
      }
   }
   *num_players = match.count;
   *ggpo_port = port;
   *local_player = match.player;
   return session;
}

void
MatchmadeReportPaths(MatchmadeSession *session)
{
   if (!session) return;
   const vwnet::MatchResult &match = session->match;
   for (int k = 1; k <= match.count; ++k) {
      if (k == match.player) continue;
      const vwnet::PathStatus status = session->proxy.Status(k);
      char route[32];
      vwnet::FormatEndpoint(status.route, route, sizeof(route));
      char text[200];
      snprintf(text, sizeof(text),
               "vectorwar-path: player=%d peer=%d route=%s via=%s relay_rtt_ms=%d direct_rtt_ms=%d"
               " switches=%d fallbacks=%d",
               match.player, k, status.direct ? "direct" : "relay", route,
               status.relay_rtt_ms, status.direct_rtt_ms,
               status.switches_to_direct, status.fallbacks);
      Say(text);
   }
}

void
MatchmadeFillLegend(MatchmadeSession *session, VwLegend *legend)
{
   if (!session || !legend) return;
   const vwnet::MatchResult &match = session->match;
   VwLegendInitDirect(legend, match.count, match.player);
   for (int k = 1; k <= legend->num_players; ++k) {
      legend->platforms[k - 1] = match.platforms[k - 1];
      if (k == match.player) continue;
      legend->routes[k - 1] = session->proxy.Status(k).direct ? kVwRouteDirect : kVwRouteRelay;
   }
}

namespace {
void FillLegendSource(VwLegend *legend, void *context)
{
   MatchmadeFillLegend((MatchmadeSession *)context, legend);
}
}  // namespace

void
MatchmadeAttachLegend(MatchmadeSession *session)
{
   VectorWar_SetLegendSource(session ? FillLegendSource : NULL, session);
}

void
MatchmadeStop(MatchmadeSession *session)
{
   if (!session) return;
   session->proxy.Stop();
   delete session;
   vwnet::NetShutdown();
}
