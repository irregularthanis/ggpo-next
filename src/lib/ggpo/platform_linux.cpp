/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct ggpo_platform::UdpSocket::Impl {
   int socket;
   Impl() : socket(-1) { }
};

uint32
ggpo_platform::MonotonicTimeMs()
{
   struct timespec current;
   if (clock_gettime(CLOCK_MONOTONIC, &current) != 0) {
      return 0;
   }
   const unsigned long long milliseconds =
      (unsigned long long)current.tv_sec * 1000ULL +
      (unsigned long long)current.tv_nsec / 1000000ULL;
   return (uint32)milliseconds;
}

void
ggpo_platform::WaitMs(int timeout_ms)
{
   if (timeout_ms <= 0) {
      return;
   }
   struct timespec requested;
   requested.tv_sec = timeout_ms / 1000;
   requested.tv_nsec = (timeout_ms % 1000) * 1000000L;
   while (nanosleep(&requested, &requested) != 0 && errno == EINTR) { }
}

uint32 ggpo_platform::ProcessId() { return (uint32)getpid(); }

void
ggpo_platform::AssertFailed(const char *message)
{
   fprintf(stderr, "GGPO assertion failed: %s\n", message ? message : "");
}

bool
ggpo_platform::EnsureDirectory(const char *path)
{
   return path && (mkdir(path, 0755) == 0 || errno == EEXIST);
}

int
ggpo_platform::GetConfigInt(const char *name, int fallback)
{
   const char *value = name ? getenv(name) : NULL;
   return value ? atoi(value) : fallback;
}

bool
ggpo_platform::GetConfigBool(const char *name, bool fallback)
{
   const char *value = name ? getenv(name) : NULL;
   if (!value) {
      return fallback;
   }
   return atoi(value) != 0 || strcasecmp(value, "true") == 0;
}

bool
ggpo_platform::ParseUdpEndpoint(const char *address, uint16 port,
                                UdpEndpoint *endpoint)
{
   struct in_addr native_address;
   if (!address || !endpoint || inet_pton(AF_INET, address, &native_address) != 1) {
      return false;
   }
   const uint32 host_address = ntohl(native_address.s_addr);
   endpoint->address[0] = (uint8)((host_address >> 24) & 0xff);
   endpoint->address[1] = (uint8)((host_address >> 16) & 0xff);
   endpoint->address[2] = (uint8)((host_address >> 8) & 0xff);
   endpoint->address[3] = (uint8)(host_address & 0xff);
   endpoint->port = port;
   return true;
}

bool
ggpo_platform::SameUdpEndpoint(const UdpEndpoint &left,
                               const UdpEndpoint &right)
{
   return memcmp(left.address, right.address, sizeof(left.address)) == 0 &&
          left.port == right.port;
}

void
ggpo_platform::FormatUdpEndpoint(const UdpEndpoint &endpoint, char *buffer,
                                 int length)
{
   if (buffer && length > 0) {
      snprintf(buffer, length, "%u.%u.%u.%u:%u",
               endpoint.address[0], endpoint.address[1],
               endpoint.address[2], endpoint.address[3], endpoint.port);
      buffer[length - 1] = '\0';
   }
}

bool ggpo_platform::NetworkStartup() { return true; }
void ggpo_platform::NetworkShutdown() { }

ggpo_platform::UdpSocket::UdpSocket() : _impl(new Impl()) { }

ggpo_platform::UdpSocket::~UdpSocket()
{
   Close();
   delete _impl;
}

bool
ggpo_platform::UdpSocket::OpenAndBind(uint16 requested_port, int retries,
                                      uint16 *bound_port)
{
   Close();
   _impl->socket = socket(AF_INET, SOCK_DGRAM, 0);
   if (_impl->socket < 0) {
      return false;
   }

   int enabled = 1;
   setsockopt(_impl->socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
   const int flags = fcntl(_impl->socket, F_GETFL, 0);
   if (flags < 0 || fcntl(_impl->socket, F_SETFL, flags | O_NONBLOCK) != 0) {
      Close();
      return false;
   }

   struct sockaddr_in native_address;
   memset(&native_address, 0, sizeof(native_address));
   native_address.sin_family = AF_INET;
   native_address.sin_addr.s_addr = htonl(INADDR_ANY);
   for (int offset = 0; offset <= retries; ++offset) {
      const uint16 port = (uint16)(requested_port + offset);
      native_address.sin_port = htons(port);
      if (bind(_impl->socket, (struct sockaddr *)&native_address,
               sizeof(native_address)) == 0) {
         if (bound_port) {
            *bound_port = port;
         }
         return true;
      }
   }
   Close();
   return false;
}

ggpo_platform::IoResult
ggpo_platform::UdpSocket::Send(const void *data, int bytes,
                               const UdpEndpoint &to)
{
   if (_impl->socket < 0) {
      return IO_ERROR;
   }
   struct sockaddr_in native_address;
   memset(&native_address, 0, sizeof(native_address));
   native_address.sin_family = AF_INET;
   native_address.sin_port = htons(to.port);
   const uint32 host_address = ((uint32)to.address[0] << 24) |
      ((uint32)to.address[1] << 16) | ((uint32)to.address[2] << 8) |
      (uint32)to.address[3];
   native_address.sin_addr.s_addr = htonl(host_address);

   const int result = (int)sendto(_impl->socket, data, bytes, 0,
                                  (struct sockaddr *)&native_address,
                                  sizeof(native_address));
   if (result >= 0) {
      return IO_OK;
   }
   return errno == EAGAIN || errno == EWOULDBLOCK ? IO_WOULD_BLOCK : IO_ERROR;
}

ggpo_platform::IoResult
ggpo_platform::UdpSocket::Receive(void *data, int capacity, int *bytes,
                                  UdpEndpoint *from)
{
   if (_impl->socket < 0) {
      return IO_ERROR;
   }
   struct sockaddr_in native_address;
   socklen_t native_length = sizeof(native_address);
   const int result = (int)recvfrom(_impl->socket, data, capacity, 0,
                                    (struct sockaddr *)&native_address,
                                    &native_length);
   if (result < 0) {
      return errno == EAGAIN || errno == EWOULDBLOCK ? IO_WOULD_BLOCK : IO_ERROR;
   }
   if (bytes) {
      *bytes = result;
   }
   if (from) {
      const uint32 host_address = ntohl(native_address.sin_addr.s_addr);
      from->address[0] = (uint8)((host_address >> 24) & 0xff);
      from->address[1] = (uint8)((host_address >> 16) & 0xff);
      from->address[2] = (uint8)((host_address >> 8) & 0xff);
      from->address[3] = (uint8)(host_address & 0xff);
      from->port = ntohs(native_address.sin_port);
   }
   return IO_OK;
}

void
ggpo_platform::UdpSocket::Close()
{
   if (_impl->socket >= 0) {
      close(_impl->socket);
      _impl->socket = -1;
   }
}
