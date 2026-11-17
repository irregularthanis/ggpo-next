#include "vw_net_protocol.h"

#include <string.h>

namespace vwnet {

namespace {

const uint8_t kMagic[4] = { 'V', 'W', 'M', 'M' };

void Put16(uint8_t *p, unsigned value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
void Put64(uint8_t *p, uint64_t value)
{
   for (int i = 0; i < 8; ++i) p[i] = (uint8_t)(value >> (8 * i));
}
unsigned Get16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
uint64_t Get64(const uint8_t *p)
{
   uint64_t value = 0;
   for (int i = 7; i >= 0; --i) value = (value << 8) | p[i];
   return value;
}
bool AllZero(const uint8_t *p, size_t length)
{
   for (size_t i = 0; i < length; ++i) if (p[i]) return false;
   return true;
}
void PutEndpoint(uint8_t *p, const Endpoint &endpoint)
{
   memcpy(p, endpoint.ip, 4);
   Put16(p + 4, endpoint.port);
}
Endpoint GetEndpoint(const uint8_t *p)
{
   Endpoint endpoint;
   memcpy(endpoint.ip, p, 4);
   endpoint.port = (uint16_t)Get16(p + 4);
   return endpoint;
}
size_t Header(uint8_t *out, int type, int version)
{
   memcpy(out, kMagic, 4);
   out[4] = (uint8_t)version;
   out[5] = (uint8_t)type;
   out[6] = out[7] = 0;
   return kHeaderSize;
}
bool ValidCount(int count) { return count >= 2 && count <= kMaxPlayers; }

}  // namespace

void
ClearMessage(Message *message, int type)
{
   memset(message, 0, sizeof(*message));
   message->type = type;
}

bool
NormalizeLobby(const char *code, char out[kLobbyCodeSize + 1])
{
   memset(out, 0, kLobbyCodeSize + 1);
   size_t length = 0;
   for (const char *c = code; *c; ++c) {
      char ch = *c;
      if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
      const bool ok = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
      if (!ok || length == kLobbyCodeSize) return false;
      out[length++] = ch;
   }
   return true;
}

size_t
Encode(const Message &m, uint8_t *out, size_t capacity)
{
   if (capacity < kMaxMessageSize) return 0;
   memset(out, 0, kMaxMessageSize);
   uint8_t *body = out + kHeaderSize;
   switch (m.type) {
   case kJoin: {
      char lobby[kLobbyCodeSize + 1];
      if (!NormalizeLobby(m.lobby, lobby) || m.group_size < 2 || m.group_size > kMaxPlayers) return 0;
      Header(out, kJoin, kVersion);
      Put64(body, m.nonce);
      body[8] = (uint8_t)m.platform;
      body[9] = (uint8_t)m.group_size;
      memcpy(body + 12, lobby, kLobbyCodeSize);
      return kJoinSize;
   }
   case kWaiting:
      Header(out, kWaiting, kVersion);
      Put64(body, m.nonce);
      body[8] = (uint8_t)m.joined;
      body[9] = (uint8_t)m.group_size;
      return kSmallSize;
   case kMatch:
      if (!ValidCount(m.count) || m.player < 1 || m.player > m.count) return 0;
      Header(out, kMatch, kVersion);
      Put64(body, m.nonce);
      Put64(body + 8, m.token);
      body[16] = (uint8_t)m.player;
      body[17] = (uint8_t)m.count;
      for (int i = 0; i < m.count; ++i) {
         Put16(body + 20 + 4 * i, m.relay_ports[i]);
         body[20 + 4 * i + 2] = (uint8_t)m.platforms[i];
      }
      return kMatchSize;
   case kBind:
   case kBound:
      if (m.player < 1 || m.player > kMaxPlayers) return 0;
      Header(out, m.type, kVersion);
      Put64(body, m.token);
      body[8] = (uint8_t)m.player;
      return kSmallSize;
   case kHello:
      Header(out, kHello, kVersion);
      Put64(body, m.nonce);
      body[8] = (uint8_t)m.platform;
      return kSmallSize;
   case kError:
      Header(out, kError, kVersion);
      Put64(body, m.nonce);
      body[8] = (uint8_t)m.code;
      return kSmallSize;
   case kPeersRequest:
      if (m.player < 1 || m.player > kMaxPlayers) return 0;
      Header(out, kPeersRequest, kPeersVersion);
      Put64(body, m.token);
      body[8] = (uint8_t)m.player;
      PutEndpoint(body + 12, m.lan);
      return kPeersSize;
   case kPeers:
      if (!ValidCount(m.count)) return 0;
      Header(out, kPeers, kPeersVersion);
      Put64(body, m.token);
      body[8] = (uint8_t)m.count;
      for (int i = 0; i < m.count; ++i) {
         PutEndpoint(body + 12 + 12 * i, m.public_candidates[i]);
         PutEndpoint(body + 12 + 12 * i + 6, m.lan_candidates[i]);
      }
      return kPeersSize;
   default:
      return 0;
   }
}

bool
Decode(const uint8_t *data, size_t length, Message *m)
{
   if (length < kHeaderSize || memcmp(data, kMagic, 4) != 0 || data[6] || data[7]) return false;
   const int type = data[5];
   const int expected = (type == kPeersRequest || type == kPeers) ? kPeersVersion : kVersion;
   if (data[4] != expected) return false;
   const uint8_t *body = data + kHeaderSize;
   const size_t body_length = length - kHeaderSize;
   ClearMessage(m, type);
   switch (type) {
   case kJoin: {
      if (length != kJoinSize || Get16(body + 10) != 0 || !AllZero(body + 20, kJoinSize - 28)) return false;
      m->nonce = Get64(body);
      m->platform = body[8];
      m->group_size = body[9];
      if (!ValidCount(m->group_size)) return false;
      char raw[kLobbyCodeSize + 1];
      memcpy(raw, body + 12, kLobbyCodeSize);
      raw[kLobbyCodeSize] = 0;
      /* Trailing NULs only: a NUL inside the code is not a valid character. */
      size_t used = kLobbyCodeSize;
      while (used > 0 && raw[used - 1] == 0) --used;
      /* On the wire the code is already upper case (the server rejects
       * anything else). */
      for (size_t i = 0; i < used; ++i) {
         const char ch = raw[i];
         if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) return false;
      }
      return NormalizeLobby(raw, m->lobby);
   }
   case kWaiting:
      if (body_length != 12) return false;
      m->nonce = Get64(body);
      m->joined = body[8];
      m->group_size = body[9];
      return true;
   case kMatch:
      if (length != kMatchSize) return false;
      m->nonce = Get64(body);
      m->token = Get64(body + 8);
      m->player = body[16];
      m->count = body[17];
      if (!ValidCount(m->count) || m->player < 1 || m->player > m->count) return false;
      if (!AllZero(body + 20 + 4 * m->count, 4 * (kMaxPlayers - m->count))) return false;
      for (int i = 0; i < m->count; ++i) {
         m->relay_ports[i] = (uint16_t)Get16(body + 20 + 4 * i);
         m->platforms[i] = body[20 + 4 * i + 2];
      }
      return true;
   case kBind:
   case kBound:
      if (body_length != 12) return false;
      m->token = Get64(body);
      m->player = body[8];
      return m->player >= 1 && m->player <= kMaxPlayers;
   case kHello:
      if (body_length != 12) return false;
      m->nonce = Get64(body);
      m->platform = body[8];
      return true;
   case kError:
      if (body_length != 12) return false;
      m->nonce = Get64(body);
      m->code = body[8];
      return true;
   case kPeersRequest:
      if (length != kPeersSize || !AllZero(body + 9, 3) || !AllZero(body + 18, 2) ||
          !AllZero(body + 20, kPeersSize - kHeaderSize - 20)) return false;
      m->token = Get64(body);
      m->player = body[8];
      m->lan = GetEndpoint(body + 12);
      return m->player >= 1 && m->player <= kMaxPlayers;
   case kPeers:
      if (length != kPeersSize || !AllZero(body + 9, 3)) return false;
      m->token = Get64(body);
      m->count = body[8];
      if (!ValidCount(m->count)) return false;
      if (!AllZero(body + 12 + 12 * m->count, 12 * (kMaxPlayers - m->count))) return false;
      for (int i = 0; i < m->count; ++i) {
         m->public_candidates[i] = GetEndpoint(body + 12 + 12 * i);
         m->lan_candidates[i] = GetEndpoint(body + 12 + 12 * i + 6);
      }
      return true;
   default:
      return false;
   }
}

}  // namespace vwnet
