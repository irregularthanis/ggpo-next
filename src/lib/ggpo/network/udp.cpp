/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "types.h"
#include "udp.h"

Udp::Udp() :
   _network_started(false),
   _callbacks(NULL)
{
}

Udp::~Udp(void)
{
   _socket.Close();
   if (_network_started) {
      ggpo_platform::NetworkShutdown();
      _network_started = false;
   }
}

void
Udp::Init(uint16 port, Poll *poll, Callbacks *callbacks)
{
   _callbacks = callbacks;

   _poll = poll;
   _poll->RegisterLoop(this);

   Log("binding udp socket to port %d.\n", port);
   _network_started = ggpo_platform::NetworkStartup();
   uint16 bound_port = 0;
   const bool opened = _network_started &&
      _socket.OpenAndBind(port, 0, &bound_port);
   ASSERT(opened);
   Log("Udp bound to port: %d.\n", bound_port);
}

void
Udp::SendTo(char *buffer, int len,
            const ggpo_platform::UdpEndpoint &destination)
{
   const ggpo_platform::IoResult result = _socket.Send(buffer, len, destination);
   char formatted[64];
   ggpo_platform::FormatUdpEndpoint(destination, formatted, ARRAY_SIZE(formatted));
   if (result != ggpo_platform::IO_OK) {
      // UDP may drop any datagram, and the protocol already retransmits sync
      // requests, inputs and acks. A full send buffer or a transient error is
      // therefore a lost packet, not a fatal condition.
      Log("send of %d bytes to %s failed (%s, native error %d); dropping packet.\n",
          len, formatted,
          result == ggpo_platform::IO_WOULD_BLOCK ? "would block" : "error",
          _socket.LastError());
      return;
   }
   Log("sent packet length %d to %s.\n", len, formatted);
}

bool
Udp::OnLoopPoll(void *cookie)
{
   uint8          recv_buf[MAX_UDP_PACKET_SIZE];
   ggpo_platform::UdpEndpoint source;

   for (;;) {
      int len = 0;
      const ggpo_platform::IoResult result =
         _socket.Receive(recv_buf, MAX_UDP_PACKET_SIZE, &len, &source);
      if (result == ggpo_platform::IO_WOULD_BLOCK) {
         break;
      }
      if (result == ggpo_platform::IO_ERROR) {
         Log("receive failed.\n");
         break;
      }
      if (len > 0) {
         char formatted[64];
         ggpo_platform::FormatUdpEndpoint(source, formatted, ARRAY_SIZE(formatted));
         Log("recvfrom returned (len:%d from:%s).\n", len, formatted);
         UdpMsg *msg = (UdpMsg *)recv_buf;
         _callbacks->OnMsg(source, msg, len);
      }
   }
   return true;
}


void
Udp::Log(const char *fmt, ...)
{
   char buf[1024];
   size_t offset;
   va_list args;

   snprintf(buf, ARRAY_SIZE(buf), "%s", "udp | ");
   offset = strlen(buf);
   va_start(args, fmt);
   vsnprintf(buf + offset, ARRAY_SIZE(buf) - offset - 1, fmt, args);
   buf[ARRAY_SIZE(buf)-1] = '\0';
   ::Log(buf);
   va_end(args);
}
