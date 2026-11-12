#ifndef _VW_NET_TYPES_H_
#define _VW_NET_TYPES_H_

#include <stddef.h>
#include <stdint.h>

namespace vwnet {

/* IPv4 endpoint: ip holds the four address bytes a.b.c.d in order. */
struct Endpoint {
   uint8_t ip[4];
   uint16_t port;
};

Endpoint MakeEndpoint(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port);
bool SameEndpoint(const Endpoint &left, const Endpoint &right);
bool IsUnset(const Endpoint &endpoint);           /* 0.0.0.0:0 */
bool IsPrivateLan(const Endpoint &endpoint);      /* RFC 1918 */
/* "a.b.c.d:port" into buffer (22 bytes are always enough). */
void FormatEndpoint(const Endpoint &endpoint, char *buffer, size_t length);

}  // namespace vwnet

#endif
