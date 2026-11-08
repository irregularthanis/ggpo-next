#include "types.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

// Report a platform-contract mismatch consistently on every adapter.
static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "platform conformance failed: %s\n", contract);
      ++failures;
   }
}

int main()
{
   uint32 before = ggpo_platform::MonotonicTimeMs();
   ggpo_platform::WaitMs(1);
   uint32 after = ggpo_platform::MonotonicTimeMs();
   Check(after >= before, "monotonic clock never moves backwards");
   Check(ggpo_platform::ProcessId() != 0, "process identity is nonzero");
   Check(ggpo_platform::GetConfigInt("GGPO_CONFORMANCE_MISSING_INT", 37) == 37,
         "missing integer configuration preserves fallback");
   Check(ggpo_platform::GetConfigBool("GGPO_CONFORMANCE_MISSING_BOOL", true),
         "missing boolean configuration preserves fallback");
   Check(ggpo_platform::EnsureDirectory("."), "existing directory is accepted");
   void (*assert_failed)(const char *) = &ggpo_platform::AssertFailed;
   Check(assert_failed != 0, "assertion reporter is linked");
   // Do not invoke it here: the Windows contract intentionally displays a
   // modal diagnostic, which is unsuitable for unattended CI.

   ggpo_platform::UdpEndpoint endpoint;
   ggpo_platform::UdpEndpoint same;
   char formatted[64];
   Check(!ggpo_platform::ParseUdpEndpoint("not-an-address", 7000, &endpoint),
         "invalid IPv4 text is rejected");
   Check(ggpo_platform::ParseUdpEndpoint("127.0.0.1", 7000, &endpoint),
         "loopback endpoint parses");
   Check(ggpo_platform::ParseUdpEndpoint("127.0.0.1", 7000, &same),
         "equivalent endpoint parses");
   Check(ggpo_platform::SameUdpEndpoint(endpoint, same),
         "equivalent endpoints compare equal");
   ggpo_platform::FormatUdpEndpoint(endpoint, formatted, sizeof(formatted));
   Check(strcmp(formatted, "127.0.0.1:7000") == 0,
         "endpoint formatting is stable and host-order aware");

   Check(ggpo_platform::NetworkStartup(), "network startup succeeds");
   {
      ggpo_platform::UdpSocket socket;
      uint16 port = (uint16)(40000 + (ggpo_platform::ProcessId() % 10000));
      uint16 bound = 0;
      char received[8];
      int bytes = 0;
      ggpo_platform::UdpEndpoint from;

      Check(socket.Send("x", 1, endpoint) == ggpo_platform::IO_ERROR,
            "closed socket rejects send");
      Check(socket.OpenAndBind(port, 100, &bound), "socket binds with retries");
      Check(ggpo_platform::ParseUdpEndpoint("127.0.0.1", bound, &endpoint),
            "bound endpoint parses");
      Check(socket.Send("ok", 2, endpoint) == ggpo_platform::IO_OK,
            "open socket sends a datagram");
      ggpo_platform::IoResult result = ggpo_platform::IO_WOULD_BLOCK;
      for (int attempt = 0; attempt < 100 && result == ggpo_platform::IO_WOULD_BLOCK;
           ++attempt) {
         result = socket.Receive(received, sizeof(received), &bytes, &from);
         if (result == ggpo_platform::IO_WOULD_BLOCK) {
            ggpo_platform::WaitMs(1);
         }
      }
      Check(result == ggpo_platform::IO_OK && bytes == 2 &&
               memcmp(received, "ok", 2) == 0,
            "sent datagram is received without data loss");
      socket.Close();
      Check(socket.Receive(received, sizeof(received), &bytes, &from) ==
               ggpo_platform::IO_ERROR,
            "closed socket rejects receive");
   }
   ggpo_platform::NetworkShutdown();
   return failures == 0 ? 0 : 1;
}
