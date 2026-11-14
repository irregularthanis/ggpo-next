#ifndef _VW_UDP_H_
#define _VW_UDP_H_

/* Minimal non-blocking IPv4 UDP sockets for Windows and POSIX, plus a mutex
 * and a thread, for the matchmaking client and the path proxy. */

#include "vw_net_types.h"

namespace vwnet {

bool NetStartup();          /* WSAStartup on Windows; reference counted */
void NetShutdown();

class UdpSocket {
public:
   UdpSocket();
   ~UdpSocket();
   /* Bind port (0 = any) on all interfaces, or on 127.0.0.1 only. */
   bool Open(uint16_t port, bool loopback_only);
   void Close();
   bool IsOpen() const;
   uint16_t LocalPort() const;
   bool SendTo(const Endpoint &to, const void *data, size_t length);
   /* Returns the datagram size, 0 when nothing is waiting, -1 on error. */
   int ReceiveFrom(void *buffer, size_t capacity, Endpoint *from);
   /* Wait until any socket is readable or timeout_ms passes. */
   static void WaitReadable(UdpSocket *const *sockets, int count, int timeout_ms);
private:
   UdpSocket(const UdpSocket &);
   UdpSocket &operator=(const UdpSocket &);
   intptr_t _handle;
};

/* host is "a.b.c.d" or a name; resolved to the first IPv4 address. */
bool ResolveIPv4(const char *host, uint16_t port, Endpoint *out);
/* The address this host would use to reach the Internet (no packets sent);
 * false when there is no route. */
bool PrimaryLanAddress(Endpoint *out);
uint32_t NowMs();
void SleepMs(uint32_t milliseconds);

class Mutex {
public:
   Mutex();
   ~Mutex();
   void Lock();
   void Unlock();
private:
   Mutex(const Mutex &);
   Mutex &operator=(const Mutex &);
   void *_impl;
};

class Thread {
public:
   typedef void (*Function)(void *argument);
   Thread();
   ~Thread();
   bool Start(Function function, void *argument);
   void Join();
private:
   Thread(const Thread &);
   Thread &operator=(const Thread &);
   void *_impl;
};

}  // namespace vwnet

#endif
