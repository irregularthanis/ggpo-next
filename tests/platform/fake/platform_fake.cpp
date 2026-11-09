/* Deterministic adapter for core tests. It deliberately performs no native
 * I/O: SyncTest must be able to save, restore, and replay without an OS clock,
 * socket stack, filesystem, or wait primitive. */
#include "types.h"
#include "platform_fake.h"

#include <string.h>

static uint32 fake_time_ms = 0;
static bool fail_next_socket_open = false;
static int network_startups = 0;
static int network_shutdowns = 0;

struct ggpo_platform::UdpSocket::Impl {
   bool open;
   char data[256];
   int bytes;
   UdpEndpoint from;
   Impl() : open(false), bytes(0) { memset(&from, 0, sizeof(from)); }
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
bool ggpo_platform::NetworkStartup()
{
   ++network_startups;
   return true;
}
void ggpo_platform::NetworkShutdown() { ++network_shutdowns; }

bool ggpo_platform::ParseUdpEndpoint(const char *address, uint16 port,
                                     UdpEndpoint *endpoint)
{
   unsigned int a, b, c, d;
   char trailing;
   if (!address || !endpoint ||
       sscanf(address, "%u.%u.%u.%u%c", &a, &b, &c, &d, &trailing) != 4 ||
       a > 255 || b > 255 || c > 255 || d > 255) {
      return false;
   }
   endpoint->address[0] = (uint8)a;
   endpoint->address[1] = (uint8)b;
   endpoint->address[2] = (uint8)c;
   endpoint->address[3] = (uint8)d;
   endpoint->port = port;
   return true;
}

bool ggpo_platform::SameUdpEndpoint(const UdpEndpoint &left,
                                    const UdpEndpoint &right)
{
   return memcmp(left.address, right.address, sizeof(left.address)) == 0 &&
          left.port == right.port;
}

void ggpo_platform::FormatUdpEndpoint(const UdpEndpoint &endpoint, char *buffer,
                                      int length)
{
   if (buffer && length > 0) {
      snprintf(buffer, length, "%u.%u.%u.%u:%u",
               endpoint.address[0], endpoint.address[1],
               endpoint.address[2], endpoint.address[3], endpoint.port);
      buffer[length - 1] = '\0';
   }
}

ggpo_platform::UdpSocket::UdpSocket() : _impl(new Impl()) { }
ggpo_platform::UdpSocket::~UdpSocket() { delete _impl; }
bool ggpo_platform::UdpSocket::OpenAndBind(uint16 requested_port, int,
                                           uint16 *bound_port)
{
   if (fail_next_socket_open) {
      fail_next_socket_open = false;
      return false;
   }
   _impl->open = true;
   if (bound_port) {
      *bound_port = requested_port;
   }
   return true;
}
ggpo_platform::IoResult ggpo_platform::UdpSocket::Send(
   const void *data, int bytes, const UdpEndpoint &to)
{
   if (!_impl->open || !data || bytes < 0 || bytes > (int)sizeof(_impl->data)) {
      return IO_ERROR;
   }
   memcpy(_impl->data, data, bytes);
   _impl->bytes = bytes;
   _impl->from = to;
   return IO_OK;
}
ggpo_platform::IoResult ggpo_platform::UdpSocket::Receive(
   void *data, int capacity, int *bytes, UdpEndpoint *from)
{
   if (!_impl->open) {
      return IO_ERROR;
   }
   if (_impl->bytes == 0) {
      return IO_WOULD_BLOCK;
   }
   if (!data || capacity < _impl->bytes || !bytes || !from) {
      return IO_ERROR;
   }
   memcpy(data, _impl->data, _impl->bytes);
   *bytes = _impl->bytes;
   *from = _impl->from;
   _impl->bytes = 0;
   return IO_OK;
}
void ggpo_platform::UdpSocket::Close()
{
   _impl->open = false;
   _impl->bytes = 0;
}

void ggpo_test::ResetFakePlatform()
{
   fake_time_ms = 0;
   fail_next_socket_open = false;
   network_startups = 0;
   network_shutdowns = 0;
}
void ggpo_test::FailNextSocketOpen() { fail_next_socket_open = true; }
int ggpo_test::NetworkStartupCount() { return network_startups; }
int ggpo_test::NetworkShutdownCount() { return network_shutdowns; }
