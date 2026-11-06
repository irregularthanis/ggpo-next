#include "types.h"
#include "network/udp_msg.h"

#include <stdio.h>

// UdpMsg's inline size helpers use ASSERT for an unknown message type.  This
// fixture exercises known types without linking GGPO's private logging object,
// so provide the assertion logger expected by the header-only code path.
void
Log(const char *, ...)
{
}

static int failures = 0;

static void
Record(const char *name, size_t actual, size_t expected)
{
   printf("%s=%lu\n", name, (unsigned long)actual);
   if (actual != expected) {
      fprintf(stderr,
              "%s mismatch: expected %lu, got %lu\n",
              name,
              (unsigned long)expected,
              (unsigned long)actual);
      failures++;
   }
}

int
main(void)
{
   UdpMsg message(UdpMsg::Input);
   const size_t union_offset =
      (const char *)&message.u - (const char *)&message;
   const size_t input_bits_offset =
      (const char *)&message.u.input.bits - (const char *)&message;

   Record("sizeof.UdpMsg.connect_status", sizeof(UdpMsg::connect_status), 4);
   Record("sizeof.UdpMsg.header", sizeof(message.hdr), 5);
   Record("offsetof.UdpMsg.union", union_offset, 5);
   Record("sizeof.UdpMsg.sync_request", sizeof(message.u.sync_request), 7);
   Record("sizeof.UdpMsg.sync_reply", sizeof(message.u.sync_reply), 4);
   Record("sizeof.UdpMsg.quality_report", sizeof(message.u.quality_report), 5);
   Record("sizeof.UdpMsg.quality_reply", sizeof(message.u.quality_reply), 4);
   Record("sizeof.UdpMsg.input_ack", sizeof(message.u.input_ack), 4);
   Record("offsetof.UdpMsg.input.bits", input_bits_offset, 32);
   Record("sizeof.UdpMsg.input", sizeof(message.u.input), 4123);
   Record("sizeof.UdpMsg", sizeof(UdpMsg), 4128);

   message.u.input.num_bits = 0;
   Record("payload.UdpMsg.input.empty", message.PayloadSize(), 27);
   Record("packet.UdpMsg.input.empty", message.PacketSize(), 32);
   message.u.input.num_bits = 9;
   Record("payload.UdpMsg.input.9bits", message.PayloadSize(), 29);
   Record("packet.UdpMsg.input.9bits", message.PacketSize(), 34);

   return failures == 0 ? 0 : 1;
}
