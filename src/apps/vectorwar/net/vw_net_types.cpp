#include "vw_net_types.h"

#include <stdio.h>
#include <string.h>

namespace vwnet {

Endpoint
MakeEndpoint(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port)
{
   Endpoint endpoint;
   endpoint.ip[0] = a;
   endpoint.ip[1] = b;
   endpoint.ip[2] = c;
   endpoint.ip[3] = d;
   endpoint.port = port;
   return endpoint;
}

bool
SameEndpoint(const Endpoint &left, const Endpoint &right)
{
   return memcmp(left.ip, right.ip, 4) == 0 && left.port == right.port;
}

bool
IsUnset(const Endpoint &endpoint)
{
   static const uint8_t zero[4] = { 0, 0, 0, 0 };
   return memcmp(endpoint.ip, zero, 4) == 0 && endpoint.port == 0;
}

bool
IsPrivateLan(const Endpoint &endpoint)
{
   const uint8_t *ip = endpoint.ip;
   return ip[0] == 10 ||
          (ip[0] == 172 && (ip[1] & 0xF0) == 16) ||
          (ip[0] == 192 && ip[1] == 168);
}

void
FormatEndpoint(const Endpoint &endpoint, char *buffer, size_t length)
{
   if (length == 0) return;
   snprintf(buffer, length, "%u.%u.%u.%u:%u",
            endpoint.ip[0], endpoint.ip[1], endpoint.ip[2], endpoint.ip[3],
            (unsigned)endpoint.port);
}

}  // namespace vwnet
