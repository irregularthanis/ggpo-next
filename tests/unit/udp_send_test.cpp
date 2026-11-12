// A failed UDP send is a dropped packet, not a crash.
//
// GGPO's ASSERT calls exit(0), so an assertion here would end the process
// with a success code. CTest therefore also requires the final pass line.

#include "types.h"
#include "network/udp.h"
#include "poll.h"
#include "platform_fake.h"

#include <stdio.h>

static int failures = 0;

static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "udp send contract failed: %s\n", contract);
      ++failures;
   }
}

struct NullCallbacks : public Udp::Callbacks {
   virtual void OnMsg(ggpo_platform::UdpEndpoint &, UdpMsg *, int) { }
};

int main()
{
   ggpo_test::ResetFakePlatform();

   Poll poll;
   NullCallbacks callbacks;
   Udp udp;
   udp.Init(7000, &poll, &callbacks);

   ggpo_platform::UdpEndpoint peer;
   Check(ggpo_platform::ParseUdpEndpoint("192.0.2.1", 7001, &peer), "peer endpoint parses");
   char packet[8] = { 'g', 'g', 'p', 'o', 0, 0, 0, 0 };

   ggpo_test::FailNextSocketSend(ggpo_platform::IO_WOULD_BLOCK);
   udp.SendTo(packet, sizeof(packet), peer);
   Check(ggpo_test::AssertFailureCount() == 0, "a full send buffer drops the packet without asserting");

   ggpo_test::FailNextSocketSend(ggpo_platform::IO_ERROR);
   udp.SendTo(packet, sizeof(packet), peer);
   Check(ggpo_test::AssertFailureCount() == 0, "a send error drops the packet without asserting");

   udp.SendTo(packet, sizeof(packet), peer);
   Check(ggpo_test::AssertFailureCount() == 0, "sending works again after failures");

   if (failures == 0) {
      printf("udp send contract: pass\n");
   }
   return failures == 0 ? 0 : 1;
}
