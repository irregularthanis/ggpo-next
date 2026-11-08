#include "types.h"
#include "network/udp_msg.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "protocol contract failed: %s\n", contract);
      ++failures;
   }
}

int main()
{
   UdpMsg keepalive(UdpMsg::KeepAlive);
   Check(keepalive.IsValidPacketSize(keepalive.PacketSize()),
         "exact keepalive packet is valid");
   Check(!keepalive.IsValidPacketSize(keepalive.PacketSize() - 1),
         "truncated header is rejected");
   Check(!keepalive.IsValidPacketSize(keepalive.PacketSize() + 1),
         "trailing bytes are rejected");

   UdpMsg input(UdpMsg::Input);
   memset(&input.u.input, 0, sizeof(input.u.input));
   input.u.input.num_bits = 9;
   Check(input.IsValidPacketSize(input.PacketSize()),
         "partial-byte input payload is valid at exact length");
   Check(!input.IsValidPacketSize(input.PacketSize() - 1),
         "truncated input payload is rejected");
   input.hdr.type = 255;
   Check(!input.IsValidPacketSize((int)sizeof(input)),
         "unknown message type is rejected");
   return failures == 0 ? 0 : 1;
}
