#include "vw_udp.h"

#include <string.h>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
typedef SOCKET NativeSocket;
#  define VW_INVALID_SOCKET INVALID_SOCKET
#  define VW_CLOSE closesocket
#else
#  include <arpa/inet.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <pthread.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <time.h>
#  include <unistd.h>
typedef int NativeSocket;
#  define VW_INVALID_SOCKET (-1)
#  define VW_CLOSE close
#endif

namespace vwnet {

namespace {

int g_startups = 0;

sockaddr_in ToSockaddr(const Endpoint &endpoint)
{
   sockaddr_in address;
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   memcpy(&address.sin_addr, endpoint.ip, 4);
   address.sin_port = htons(endpoint.port);
   return address;
}

Endpoint FromSockaddr(const sockaddr_in &address)
{
   Endpoint endpoint;
   memcpy(endpoint.ip, &address.sin_addr, 4);
   endpoint.port = ntohs(address.sin_port);
   return endpoint;
}

NativeSocket Native(intptr_t handle) { return (NativeSocket)handle; }

}  // namespace

bool
NetStartup()
{
#if defined(_WIN32)
   if (g_startups == 0) {
      WSADATA data;
      if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
   }
#endif
   ++g_startups;
   return true;
}

void
NetShutdown()
{
   if (g_startups <= 0) return;
   --g_startups;
#if defined(_WIN32)
   if (g_startups == 0) WSACleanup();
#endif
}

UdpSocket::UdpSocket() : _handle((intptr_t)VW_INVALID_SOCKET) {}
UdpSocket::~UdpSocket() { Close(); }

bool
UdpSocket::Open(uint16_t port, bool loopback_only)
{
   Close();
   NativeSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (s == VW_INVALID_SOCKET) return false;
   sockaddr_in address;
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = htonl(loopback_only ? INADDR_LOOPBACK : INADDR_ANY);
   address.sin_port = htons(port);
   if (bind(s, (sockaddr *)&address, sizeof(address)) != 0) {
      VW_CLOSE(s);
      return false;
   }
#if defined(_WIN32)
   u_long nonblocking = 1;
   ioctlsocket(s, FIONBIO, &nonblocking);
   /* Do not report ICMP port unreachable as a receive error. */
   BOOL report = FALSE;
   DWORD returned = 0;
   WSAIoctl(s, _WSAIOW(IOC_VENDOR, 12), &report, sizeof(report), NULL, 0, &returned, NULL, NULL);
#else
   fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
   _handle = (intptr_t)s;
   return true;
}

void
UdpSocket::Close()
{
   if (Native(_handle) != VW_INVALID_SOCKET) {
      VW_CLOSE(Native(_handle));
      _handle = (intptr_t)VW_INVALID_SOCKET;
   }
}

bool UdpSocket::IsOpen() const { return Native(_handle) != VW_INVALID_SOCKET; }

uint16_t
UdpSocket::LocalPort() const
{
   sockaddr_in address;
   socklen_t length = sizeof(address);
   if (!IsOpen() || getsockname(Native(_handle), (sockaddr *)&address, &length) != 0) return 0;
   return ntohs(address.sin_port);
}

bool
UdpSocket::SendTo(const Endpoint &to, const void *data, size_t length)
{
   if (!IsOpen()) return false;
   sockaddr_in address = ToSockaddr(to);
   /* A failed send is a lost datagram, as GGPO treats it. */
   return sendto(Native(_handle), (const char *)data, (int)length, 0,
                 (sockaddr *)&address, sizeof(address)) == (int)length;
}

int
UdpSocket::ReceiveFrom(void *buffer, size_t capacity, Endpoint *from)
{
   if (!IsOpen()) return -1;
   sockaddr_in address;
   socklen_t length = sizeof(address);
   const int received = (int)recvfrom(Native(_handle), (char *)buffer, (int)capacity, 0,
                                      (sockaddr *)&address, &length);
   if (received < 0) {
#if defined(_WIN32)
      const int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK || error == WSAECONNRESET || error == WSAEMSGSIZE) return 0;
#else
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNREFUSED || errno == EINTR) return 0;
#endif
      return -1;
   }
   if (from) *from = FromSockaddr(address);
   return received;
}

void
UdpSocket::WaitReadable(UdpSocket *const *sockets, int count, int timeout_ms)
{
   fd_set readable;
   FD_ZERO(&readable);
   NativeSocket highest = 0;
   for (int i = 0; i < count; ++i) {
      if (!sockets[i] || !sockets[i]->IsOpen()) continue;
      const NativeSocket s = Native(sockets[i]->_handle);
      FD_SET(s, &readable);
      if (s > highest) highest = s;
   }
   timeval timeout;
   timeout.tv_sec = timeout_ms / 1000;
   timeout.tv_usec = (timeout_ms % 1000) * 1000;
   select((int)highest + 1, &readable, NULL, NULL, &timeout);
}

bool
ResolveIPv4(const char *host, uint16_t port, Endpoint *out)
{
   addrinfo hints;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   addrinfo *result = NULL;
   if (getaddrinfo(host, NULL, &hints, &result) != 0 || !result) return false;
   *out = FromSockaddr(*(const sockaddr_in *)result->ai_addr);
   out->port = port;
   freeaddrinfo(result);
   return true;
}

bool
PrimaryLanAddress(Endpoint *out)
{
   NativeSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (s == VW_INVALID_SOCKET) return false;
   /* connect() on UDP picks a route and source address without sending. */
   sockaddr_in remote = ToSockaddr(MakeEndpoint(192, 0, 2, 1, 9));
   bool ok = connect(s, (sockaddr *)&remote, sizeof(remote)) == 0;
   sockaddr_in local;
   socklen_t length = sizeof(local);
   ok = ok && getsockname(s, (sockaddr *)&local, &length) == 0;
   VW_CLOSE(s);
   if (!ok) return false;
   *out = FromSockaddr(local);
   out->port = 0;
   return !IsUnset(*out);
}

uint32_t
NowMs()
{
#if defined(_WIN32)
   return (uint32_t)GetTickCount();
#else
   timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (uint32_t)((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u);
#endif
}

void
SleepMs(uint32_t milliseconds)
{
#if defined(_WIN32)
   Sleep(milliseconds);
#else
   timespec request;
   request.tv_sec = milliseconds / 1000;
   request.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
   nanosleep(&request, NULL);
#endif
}

#if defined(_WIN32)
Mutex::Mutex() : _impl(new CRITICAL_SECTION) { InitializeCriticalSection((CRITICAL_SECTION *)_impl); }
Mutex::~Mutex() { DeleteCriticalSection((CRITICAL_SECTION *)_impl); delete (CRITICAL_SECTION *)_impl; }
void Mutex::Lock() { EnterCriticalSection((CRITICAL_SECTION *)_impl); }
void Mutex::Unlock() { LeaveCriticalSection((CRITICAL_SECTION *)_impl); }

namespace {
struct ThreadStart { Thread::Function function; void *argument; HANDLE handle; };
DWORD WINAPI ThreadMain(LPVOID parameter)
{
   ThreadStart *start = (ThreadStart *)parameter;
   start->function(start->argument);
   return 0;
}
}

Thread::Thread() : _impl(NULL) {}
Thread::~Thread() { Join(); }
bool
Thread::Start(Function function, void *argument)
{
   ThreadStart *start = new ThreadStart();
   start->function = function;
   start->argument = argument;
   start->handle = CreateThread(NULL, 0, ThreadMain, start, 0, NULL);
   if (!start->handle) { delete start; return false; }
   _impl = start;
   return true;
}
void
Thread::Join()
{
   if (!_impl) return;
   ThreadStart *start = (ThreadStart *)_impl;
   WaitForSingleObject(start->handle, INFINITE);
   CloseHandle(start->handle);
   delete start;
   _impl = NULL;
}
#else
Mutex::Mutex() : _impl(new pthread_mutex_t) { pthread_mutex_init((pthread_mutex_t *)_impl, NULL); }
Mutex::~Mutex() { pthread_mutex_destroy((pthread_mutex_t *)_impl); delete (pthread_mutex_t *)_impl; }
void Mutex::Lock() { pthread_mutex_lock((pthread_mutex_t *)_impl); }
void Mutex::Unlock() { pthread_mutex_unlock((pthread_mutex_t *)_impl); }

namespace {
struct ThreadStart { Thread::Function function; void *argument; pthread_t handle; };
void *ThreadMain(void *parameter)
{
   ThreadStart *start = (ThreadStart *)parameter;
   start->function(start->argument);
   return NULL;
}
}

Thread::Thread() : _impl(NULL) {}
Thread::~Thread() { Join(); }
bool
Thread::Start(Function function, void *argument)
{
   ThreadStart *start = new ThreadStart();
   start->function = function;
   start->argument = argument;
   if (pthread_create(&start->handle, NULL, ThreadMain, start) != 0) { delete start; return false; }
   _impl = start;
   return true;
}
void
Thread::Join()
{
   if (!_impl) return;
   ThreadStart *start = (ThreadStart *)_impl;
   pthread_join(start->handle, NULL);
   delete start;
   _impl = NULL;
}
#endif

}  // namespace vwnet
