/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GGPO_PLATFORM_H_
#define _GGPO_PLATFORM_H_

// Semantic services required by the portable core. Native handles, error
// values, path separators, and socket structures stay inside an adapter.
namespace ggpo_platform {

uint32 MonotonicTimeMs();
void WaitMs(int timeout_ms);
uint32 ProcessId();
int GetConfigInt(const char *name, int fallback);
bool GetConfigBool(const char *name, bool fallback);
void AssertFailed(const char *message);
bool EnsureDirectory(const char *path);

struct UdpEndpoint {
   uint8 address[4];
   uint16 port; // Host byte order.
};

enum IoResult {
   IO_OK,
   IO_WOULD_BLOCK,
   IO_ERROR
};

bool ParseUdpEndpoint(const char *address, uint16 port, UdpEndpoint *endpoint);
bool SameUdpEndpoint(const UdpEndpoint &left, const UdpEndpoint &right);
void FormatUdpEndpoint(const UdpEndpoint &endpoint, char *buffer, int length);

bool NetworkStartup();
void NetworkShutdown();

class UdpSocket {
public:
   UdpSocket();
   ~UdpSocket();

   bool OpenAndBind(uint16 requested_port, int retries, uint16 *bound_port);
   IoResult Send(const void *data, int bytes, const UdpEndpoint &to);
   IoResult Receive(void *data, int capacity, int *bytes, UdpEndpoint *from);
   void Close();

private:
   UdpSocket(const UdpSocket &);
   UdpSocket &operator=(const UdpSocket &);
   struct Impl;
   Impl *_impl;
};

}

#endif
