#include "types.h"
#include "network/udp_msg.h"
#include "network/udp_proto.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

class StatsProtocol : public UdpProtocol
{
public:
   void UpdateAtInitialTick()
   {
      _stats_start_time = 0;
      _packets_sent = 0;
      _bytes_sent = 0;
      UpdateNetworkStats();
   }

   int KbpsSent() const { return _kbps_sent; }
};

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

   StatsProtocol stats;
   stats.UpdateAtInitialTick();
   Check(stats.KbpsSent() == 0,
         "network stats remain finite before elapsed time or payload bytes");
   return failures == 0 ? 0 : 1;
}
