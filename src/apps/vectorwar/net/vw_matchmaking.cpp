#include "vw_matchmaking.h"

#include <stdio.h>
#include <string.h>

namespace vwnet {

namespace {

void SetError(char *error, size_t length, const char *text)
{
   if (error && length) snprintf(error, length, "%s", text);
}

uint64_t MakeNonce(const UdpSocket *socket)
{
   /* Unique per attempt, not secret: it only pairs replies with requests. */
   uint64_t nonce = ((uint64_t)NowMs() << 32) ^ ((uint64_t)socket->LocalPort() << 16);
   nonce ^= (uint64_t)(uintptr_t)socket * 0x9E3779B97F4A7C15ULL;
   return nonce ? nonce : 1;
}

int Elapsed(uint32_t now, uint32_t since) { return (int)(int32_t)(now - since); }

bool Receive(UdpSocket *socket, Message *message, Endpoint *from, int wait_ms)
{
   UdpSocket *sockets[1] = { socket };
   UdpSocket::WaitReadable(sockets, 1, wait_ms);
   uint8_t buffer[512];
   for (;;) {
      const int size = socket->ReceiveFrom(buffer, sizeof(buffer), from);
      if (size <= 0) return false;
      if (Decode(buffer, (size_t)size, message)) return true;
      /* Other players' early traffic or probes: not ours, keep reading. */
   }
}

}  // namespace

MatchmakingOptions::MatchmakingOptions() :
   group_size(2), lobby(""), platform(kPlatformUnknown),
   join_timeout_ms(120000), bind_timeout_ms(10000), candidates_timeout_ms(3000),
   advertise_lan(true), progress(NULL), context(NULL)
{
}

bool
Matchmake(UdpSocket *socket, const Endpoint &server, const MatchmakingOptions &options,
          MatchResult *result, char *error, size_t error_length)
{
   memset(result, 0, sizeof(*result));
   uint8_t wire[kMaxMessageSize];

   /* 1. JOIN until MATCH. */
   Message join;
   ClearMessage(&join, kJoin);
   join.nonce = MakeNonce(socket);
   join.platform = options.platform;
   join.group_size = options.group_size;
   if (!NormalizeLobby(options.lobby ? options.lobby : "", join.lobby)) {
      SetError(error, error_length, "lobby code must be up to 8 letters or digits");
      return false;
   }
   const size_t join_size = Encode(join, wire, sizeof(wire));
   if (!join_size) {
      SetError(error, error_length, "invalid group size");
      return false;
   }
   const uint32_t join_start = NowMs();
   uint32_t last_join = join_start - 1000;
   int last_joined = -1;
   Message match;
   bool matched = false;
   while (!matched) {
      const uint32_t now = NowMs();
      if (Elapsed(now, join_start) > (int)options.join_timeout_ms) {
         SetError(error, error_length, "timed out waiting for other players");
         return false;
      }
      if (Elapsed(now, last_join) >= 500) {
         socket->SendTo(server, wire, join_size);
         last_join = now;
      }
      Message reply;
      Endpoint from;
      if (!Receive(socket, &reply, &from, 50) || !SameEndpoint(from, server)) continue;
      if (reply.type == kWaiting && reply.nonce == join.nonce) {
         if (reply.joined != last_joined && options.progress) {
            options.progress(reply.joined, reply.group_size, options.context);
         }
         last_joined = reply.joined;
      } else if (reply.type == kMatch && reply.nonce == join.nonce) {
         match = reply;
         matched = true;
      } else if (reply.type == kError && reply.nonce == join.nonce &&
                 reply.code == kErrorVersionMismatch) {
         SetError(error, error_length, "the server speaks another protocol version");
         return false;
      }
   }
   result->player = match.player;
   result->count = match.count;
   result->token = match.token;
   for (int i = 0; i < match.count; ++i) {
      result->platforms[i] = match.platforms[i];
      result->relay[i] = server;
      result->relay[i].port = match.relay_ports[i];
   }

   /* 2. BIND to every other player's relay port until BOUND. */
   Message bind;
   ClearMessage(&bind, kBind);
   bind.token = match.token;
   bind.player = match.player;
   const size_t bind_size = Encode(bind, wire, sizeof(wire));
   bool bound[kMaxPlayers] = { false, false, false, false };
   bound[match.player - 1] = true;
   const uint32_t bind_start = NowMs();
   uint32_t last_bind = bind_start - 1000;
   for (;;) {
      bool all = true;
      for (int k = 0; k < match.count; ++k) all = all && bound[k];
      if (all) break;
      const uint32_t now = NowMs();
      if (Elapsed(now, bind_start) > (int)options.bind_timeout_ms) {
         SetError(error, error_length, "timed out binding to the relay");
         return false;
      }
      if (Elapsed(now, last_bind) >= 200) {
         for (int k = 0; k < match.count; ++k) {
            if (!bound[k]) socket->SendTo(result->relay[k], wire, bind_size);
         }
         last_bind = now;
      }
      Message reply;
      Endpoint from;
      if (!Receive(socket, &reply, &from, 20)) continue;
      if (reply.type != kBound || reply.token != match.token || reply.player != match.player) continue;
      for (int k = 0; k < match.count; ++k) {
         if (SameEndpoint(from, result->relay[k])) bound[k] = true;
      }
   }

   /* 3. Hole-punching candidates (protocol v3); optional. */
   Message request;
   ClearMessage(&request, kPeersRequest);
   request.token = match.token;
   request.player = match.player;
   Endpoint lan;
   if (options.advertise_lan && PrimaryLanAddress(&lan) && IsPrivateLan(lan)) {
      request.lan = lan;
      request.lan.port = socket->LocalPort();
   }
   const size_t request_size = Encode(request, wire, sizeof(wire));
   const uint32_t peers_start = NowMs();
   uint32_t last_request = peers_start - 1000;
   while (Elapsed(NowMs(), peers_start) <= (int)options.candidates_timeout_ms) {
      const uint32_t now = NowMs();
      if (Elapsed(now, last_request) >= 250) {
         socket->SendTo(server, wire, request_size);
         last_request = now;
      }
      Message reply;
      Endpoint from;
      if (!Receive(socket, &reply, &from, 20) || reply.type != kPeers ||
          reply.token != match.token || reply.count != match.count || !SameEndpoint(from, server)) {
         continue;
      }
      result->have_candidates = true;
      bool complete = true;
      for (int k = 0; k < reply.count; ++k) {
         result->public_candidates[k] = reply.public_candidates[k];
         result->lan_candidates[k] = reply.lan_candidates[k];
         complete = complete && !IsUnset(reply.public_candidates[k]);
      }
      if (complete) break;
   }
   return true;
}

}  // namespace vwnet
