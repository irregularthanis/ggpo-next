/* Path proxy end to end without GGPO: matchmake against the real server,
 * start a PathProxy, and stand in for GGPO with one loopback socket that sends
 * tagged datagrams to every remote player's loopback endpoint and checks that
 * each player's datagrams come back from that player's endpoint.
 *
 * usage: net_proxy_tool SERVER_PORT GROUP_SIZE LOBBY direct|relay SECONDS RESULT.json
 *        net_proxy_tool --client-errors SILENT_PORT
 * The second form checks the errors a start screen shows: an unreachable
 * server, and a cancel from the keep_going hook. */

#include "vw_matchmaking.h"
#include "vw_path_proxy.h"
#include "vw_udp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace vwnet;

static bool StopAfter200Ms(void *context)
{
   return (int32_t)(NowMs() - *(uint32_t *)context) < 200;
}

static int ClientErrors(uint16_t silent_port)
{
   UdpSocket socket;
   if (!socket.Open(0, true)) return 1;
   const Endpoint nobody = MakeEndpoint(127, 0, 0, 1, silent_port);
   MatchResult match;
   char error[128];

   MatchmakingOptions options;
   options.server_timeout_ms = 1000;
   uint32_t start = NowMs();
   if (Matchmake(&socket, nobody, options, &match, error, sizeof(error)) ||
       strcmp(error, "matchmaking server unreachable") != 0 ||
       (int32_t)(NowMs() - start) > 3000) {
      fprintf(stderr, "unreachable server not reported promptly: %s\n", error);
      return 1;
   }

   MatchmakingOptions cancel;
   start = NowMs();
   cancel.keep_going = StopAfter200Ms;
   cancel.context = &start;
   if (Matchmake(&socket, nobody, cancel, &match, error, sizeof(error)) ||
       strcmp(error, "cancelled") != 0 || (int32_t)(NowMs() - start) > 1000) {
      fprintf(stderr, "cancel not honoured promptly: %s\n", error);
      return 1;
   }
   printf("net-proxy-tool: unreachable server and cancel reported\n");
   return 0;
}

int main(int argc, char **argv)
{
   if (argc == 3 && strcmp(argv[1], "--client-errors") == 0) {
      if (!NetStartup()) return 1;
      return ClientErrors((uint16_t)atoi(argv[2]));
   }
   if (argc != 7) {
      fprintf(stderr, "usage: %s SERVER_PORT GROUP_SIZE LOBBY direct|relay SECONDS RESULT.json\n", argv[0]);
      return 2;
   }
   const uint16_t server_port = (uint16_t)atoi(argv[1]);
   const bool allow_direct = strcmp(argv[4], "direct") == 0;
   const uint32_t run_ms = (uint32_t)atoi(argv[5]) * 1000u;
   if (!NetStartup()) return 1;

   PathProxy proxy;
   if (!proxy.OpenPublic(0)) { fprintf(stderr, "cannot open public socket\n"); return 1; }
   MatchmakingOptions options;
   options.group_size = atoi(argv[2]);
   options.lobby = strcmp(argv[3], "-") == 0 ? "" : argv[3];
   options.platform = kPlatformLinux;
   options.join_timeout_ms = 30000;
   options.advertise_lan = false;           /* loopback test: public = 127.0.0.1 */
   MatchResult match;
   char error[128];
   if (!Matchmake(proxy.PublicSocket(), MakeEndpoint(127, 0, 0, 1, server_port), options,
                  &match, error, sizeof(error))) {
      fprintf(stderr, "matchmaking failed: %s\n", error);
      return 1;
   }

   UdpSocket ggpo;                          /* stands in for GGPO's socket */
   if (!ggpo.Open(0, true) || !proxy.Start(match, ggpo.LocalPort(), allow_direct)) {
      fprintf(stderr, "cannot start the proxy\n");
      return 1;
   }

   int received[kMaxPlayers] = { 0, 0, 0, 0 };
   int wrong_source = 0;
   int sent = 0;
   const uint32_t start = NowMs();
   uint32_t last_send = start;
   while ((int32_t)(NowMs() - start) < (int32_t)run_ms) {
      const uint32_t now = NowMs();
      if ((int32_t)(now - last_send) >= 10) {
         last_send = now;
         for (int k = 1; k <= match.count; ++k) {
            if (k == match.player) continue;
            char payload[64];
            const int length = snprintf(payload, sizeof(payload), "vw %d %d %d", match.player, k, sent);
            ggpo.SendTo(MakeEndpoint(127, 0, 0, 1, proxy.LoopbackPort(k)), payload, (size_t)length);
         }
         ++sent;
      }
      UdpSocket *sockets[1] = { &ggpo };
      UdpSocket::WaitReadable(sockets, 1, 2);
      char buffer[128];
      Endpoint from;
      int size;
      while ((size = ggpo.ReceiveFrom(buffer, sizeof(buffer) - 1, &from)) > 0) {
         buffer[size] = 0;
         int sender = 0, target = 0, sequence = 0;
         if (sscanf(buffer, "vw %d %d %d", &sender, &target, &sequence) != 3 ||
             target != match.player || sender < 1 || sender > match.count ||
             from.port != proxy.LoopbackPort(sender)) {
            ++wrong_source;
            continue;
         }
         ++received[sender - 1];
      }
   }

   FILE *output = fopen(argv[6], "w");
   if (!output) return 1;
   fprintf(output, "{\"player\":%d,\"count\":%d,\"have_candidates\":%s,\"sent\":%d,"
                   "\"wrong_source\":%d,\"peers\":[",
           match.player, match.count, match.have_candidates ? "true" : "false", sent, wrong_source);
   bool first = true;
   for (int k = 1; k <= match.count; ++k) {
      if (k == match.player) continue;
      const PathStatus status = proxy.Status(k);
      fprintf(output, "%s{\"player\":%d,\"received\":%d,\"direct\":%s,\"relay_rtt_ms\":%d,"
                      "\"direct_rtt_ms\":%d}",
              first ? "" : ",", k, received[k - 1], status.direct ? "true" : "false",
              status.relay_rtt_ms, status.direct_rtt_ms);
      first = false;
   }
   fprintf(output, "]}\n");
   fclose(output);
   proxy.Stop();
   NetShutdown();
   return 0;
}
