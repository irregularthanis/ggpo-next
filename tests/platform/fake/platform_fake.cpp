/* Deterministic adapter for core tests. It deliberately performs no native
 * I/O: SyncTest must be able to save, restore, and replay without an OS clock,
 * socket stack, filesystem, or wait primitive. */
#include "types.h"

#include <string.h>

static uint32 fake_time_ms = 0;

struct ggpo_platform::UdpSocket::Impl {
   bool open;
   Impl() : open(false) { }
};

uint32 ggpo_platform::MonotonicTimeMs() { return fake_time_ms; }
void ggpo_platform::WaitMs(int timeout_ms)
{
   if (timeout_ms > 0) {
      fake_time_ms += (uint32)timeout_ms;
   }
}
uint32 ggpo_platform::ProcessId() { return 1; }
int ggpo_platform::GetConfigInt(const char *, int fallback) { return fallback; }
bool ggpo_platform::GetConfigBool(const char *, bool fallback) { return fallback; }
void ggpo_platform::AssertFailed(const char *) { }
bool ggpo_platform::EnsureDirectory(const char *) { return true; }
bool ggpo_platform::NetworkStartup() { return true; }
void ggpo_platform::NetworkShutdown() { }

bool ggpo_platform::ParseUdpEndpoint(const char *, uint16 port,
                                     UdpEndpoint *endpoint)
{
   if (!endpoint) {
      return false;
   }
   memset(endpoint->address, 0, sizeof(endpoint->address));
   endpoint->port = port;
   return true;
}

bool ggpo_platform::SameUdpEndpoint(const UdpEndpoint &left,
                                    const UdpEndpoint &right)
{
   return memcmp(left.address, right.address, sizeof(left.address)) == 0 &&
          left.port == right.port;
}

void ggpo_platform::FormatUdpEndpoint(const UdpEndpoint &, char *buffer,
                                      int length)
{
   if (length > 0) {
      buffer[0] = '\0';
   }
}

ggpo_platform::UdpSocket::UdpSocket() : _impl(new Impl()) { }
ggpo_platform::UdpSocket::~UdpSocket() { delete _impl; }
bool ggpo_platform::UdpSocket::OpenAndBind(uint16 requested_port, int,
                                           uint16 *bound_port)
{
   _impl->open = true;
   if (bound_port) {
      *bound_port = requested_port;
   }
   return true;
}
ggpo_platform::IoResult ggpo_platform::UdpSocket::Send(
   const void *, int, const UdpEndpoint &)
{
   return _impl->open ? IO_OK : IO_ERROR;
}
ggpo_platform::IoResult ggpo_platform::UdpSocket::Receive(
   void *, int, int *, UdpEndpoint *)
{
   return _impl->open ? IO_WOULD_BLOCK : IO_ERROR;
}
void ggpo_platform::UdpSocket::Close() { _impl->open = false; }
