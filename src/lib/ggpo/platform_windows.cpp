/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "platform_windows.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ggpo_platform::UdpSocket::Impl {
   SOCKET socket;
   int last_error;
   Impl() : socket(INVALID_SOCKET), last_error(0) { }
};

BOOL WINAPI
DllMain(HINSTANCE, DWORD, LPVOID)
{
   srand(ggpo_platform::MonotonicTimeMs() + ggpo_platform::ProcessId());
   return TRUE;
}

uint32
ggpo_platform::MonotonicTimeMs()
{
   return timeGetTime();
}

void
ggpo_platform::WaitMs(int timeout_ms)
{
   if (timeout_ms > 0) {
      Sleep((DWORD)timeout_ms);
   }
}

uint32
ggpo_platform::ProcessId()
{
   return (uint32)GetCurrentProcessId();
}

void
ggpo_platform::AssertFailed(const char *message)
{
   MessageBoxA(NULL, message, "GGPO Assertion Failed", MB_OK | MB_ICONEXCLAMATION);
}

bool
ggpo_platform::EnsureDirectory(const char *path)
{
   return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

int
ggpo_platform::GetConfigInt(const char *name, int fallback)
{
   char buf[1024];
   if (GetEnvironmentVariable(name, buf, ARRAY_SIZE(buf)) == 0) {
      return fallback;
   }
   return atoi(buf);
}

bool
ggpo_platform::GetConfigBool(const char *name, bool fallback)
{
   char buf[1024];
   if (GetEnvironmentVariable(name, buf, ARRAY_SIZE(buf)) == 0) {
      return fallback;
   }
   return atoi(buf) != 0 || _stricmp(buf, "true") == 0;
}

bool
ggpo_platform::ParseUdpEndpoint(const char *address, uint16 port, UdpEndpoint *endpoint)
{
   IN_ADDR native_address;
   if (!endpoint || InetPtonA(AF_INET, address, &native_address) != 1) {
      return false;
   }
   const uint32 host_address = ntohl(native_address.S_un.S_addr);
   endpoint->address[0] = (uint8)((host_address >> 24) & 0xff);
   endpoint->address[1] = (uint8)((host_address >> 16) & 0xff);
   endpoint->address[2] = (uint8)((host_address >> 8) & 0xff);
   endpoint->address[3] = (uint8)(host_address & 0xff);
   endpoint->port = port;
   return true;
}

bool
ggpo_platform::SameUdpEndpoint(const UdpEndpoint &left, const UdpEndpoint &right)
{
   return memcmp(left.address, right.address, sizeof(left.address)) == 0 &&
          left.port == right.port;
}

void
ggpo_platform::FormatUdpEndpoint(const UdpEndpoint &endpoint, char *buffer, int length)
{
   if (length > 0) {
      // C99 snprintf (MSVC 2015+ and MinGW-w64) always terminates within
      // length and truncates like _snprintf_s(..., _TRUNCATE, ...).
      snprintf(buffer, length, "%u.%u.%u.%u:%u",
               endpoint.address[0], endpoint.address[1],
               endpoint.address[2], endpoint.address[3], endpoint.port);
   }
}

bool
ggpo_platform::NetworkStartup()
{
   WSADATA data;
   return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void
ggpo_platform::NetworkShutdown()
{
   WSACleanup();
}

ggpo_platform::UdpSocket::UdpSocket() :
   _impl(new Impl())
{
}

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
   if (_impl->socket == INVALID_SOCKET) {
      return false;
   }

   int enabled = 1;
   setsockopt(_impl->socket, SOL_SOCKET, SO_REUSEADDR,
              (const char *)&enabled, sizeof(enabled));
   setsockopt(_impl->socket, SOL_SOCKET, SO_DONTLINGER,
              (const char *)&enabled, sizeof(enabled));
   u_long nonblocking = 1;
   if (ioctlsocket(_impl->socket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
      Close();
      return false;
   }

   sockaddr_in native_address;
   memset(&native_address, 0, sizeof(native_address));
   native_address.sin_family = AF_INET;
   native_address.sin_addr.s_addr = htonl(INADDR_ANY);
   for (int offset = 0; offset <= retries; ++offset) {
      const uint16 port = (uint16)(requested_port + offset);
      native_address.sin_port = htons(port);
      if (bind(_impl->socket, (sockaddr *)&native_address,
               sizeof(native_address)) != SOCKET_ERROR) {
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
   sockaddr_in native_address;
   memset(&native_address, 0, sizeof(native_address));
   native_address.sin_family = AF_INET;
   native_address.sin_port = htons(to.port);
   const uint32 host_address = ((uint32)to.address[0] << 24) |
      ((uint32)to.address[1] << 16) | ((uint32)to.address[2] << 8) |
      (uint32)to.address[3];
   native_address.sin_addr.s_addr = htonl(host_address);

   const int result = sendto(_impl->socket, (const char *)data, bytes, 0,
                             (sockaddr *)&native_address, sizeof(native_address));
   if (result != SOCKET_ERROR) {
      return IO_OK;
   }
   _impl->last_error = WSAGetLastError();
   return _impl->last_error == WSAEWOULDBLOCK ? IO_WOULD_BLOCK : IO_ERROR;
}

ggpo_platform::IoResult
ggpo_platform::UdpSocket::Receive(void *data, int capacity, int *bytes,
                                  UdpEndpoint *from)
{
   sockaddr_in native_address;
   int native_length = sizeof(native_address);
   const int result = recvfrom(_impl->socket, (char *)data, capacity, 0,
                               (sockaddr *)&native_address, &native_length);
   if (result == SOCKET_ERROR) {
      _impl->last_error = WSAGetLastError();
      return _impl->last_error == WSAEWOULDBLOCK ? IO_WOULD_BLOCK : IO_ERROR;
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

int
ggpo_platform::UdpSocket::LastError() const
{
   return _impl->last_error;
}

void
ggpo_platform::UdpSocket::Close()
{
   if (_impl->socket != INVALID_SOCKET) {
      closesocket(_impl->socket);
      _impl->socket = INVALID_SOCKET;
   }
}
