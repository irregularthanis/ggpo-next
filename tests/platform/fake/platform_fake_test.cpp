#include "platform_fake.h"

#include <stdio.h>

static int failures = 0;

static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "fake platform contract failed: %s\n", contract);
      ++failures;
   }
}

int main()
{
   ggpo_test::ResetFakePlatform();
   Check(ggpo_platform::MonotonicTimeMs() == 0, "clock starts at zero");
   ggpo_platform::WaitMs(0);
   Check(ggpo_platform::MonotonicTimeMs() == 0, "zero wait is nonblocking");
   ggpo_platform::WaitMs(7);
   Check(ggpo_platform::MonotonicTimeMs() == 7, "wait advances virtual time");
   Check(ggpo_platform::GetConfigInt("missing", 42) == 42,
         "integer fallback is preserved");
   Check(ggpo_platform::GetConfigBool("missing", true),
         "boolean fallback is preserved");

   ggpo_platform::UdpEndpoint endpoint;
   Check(ggpo_platform::ParseUdpEndpoint("127.0.0.1", 7000, &endpoint),
         "endpoint parses");
   Check(endpoint.port == 7000, "endpoint port uses host byte order");

   ggpo_platform::UdpSocket socket;
   Check(socket.Send("x", 1, endpoint) == ggpo_platform::IO_ERROR,
         "closed socket rejects send");
   ggpo_test::FailNextSocketOpen();
   Check(!socket.OpenAndBind(7000, 0, 0), "scripted open failure is visible");
   Check(socket.OpenAndBind(7000, 0, 0), "socket can recover after failure");
   Check(socket.Send("x", 1, endpoint) == ggpo_platform::IO_OK,
         "open socket accepts send");
   char received[1];
   int bytes = 0;
   ggpo_platform::UdpEndpoint from;
   Check(socket.Receive(received, sizeof(received), &bytes, &from) ==
            ggpo_platform::IO_OK && bytes == 1 && received[0] == 'x',
         "queued datagram is delivered once");
   Check(socket.Receive(received, sizeof(received), &bytes, &from) ==
            ggpo_platform::IO_WOULD_BLOCK,
         "empty receive is explicitly would-block");
   socket.Close();
   Check(socket.Send("x", 1, endpoint) == ggpo_platform::IO_ERROR,
         "close releases socket state");

   Check(ggpo_platform::NetworkStartup(), "network startup succeeds");
   ggpo_platform::NetworkShutdown();
   Check(ggpo_test::NetworkStartupCount() == 1, "startup is counted");
   Check(ggpo_test::NetworkShutdownCount() == 1, "shutdown is balanced");
   return failures == 0 ? 0 : 1;
}
