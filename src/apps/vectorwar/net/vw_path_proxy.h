#ifndef _VW_PATH_PROXY_H_
#define _VW_PATH_PROXY_H_

/* Path proxy: hole punching with relay fallback, without changing GGPO.
 *
 * GGPO binds an ordinary local port and is configured with one loopback
 * endpoint per remote player (127.0.0.1:LoopbackPort(k)). The proxy owns the
 * single public socket used for matchmaking, the relay and the direct paths.
 * On its own thread it forwards each player's GGPO datagrams on that player's
 * current route (relay or validated direct address), delivers datagrams from
 * the relay and from validated direct addresses back to GGPO from the
 * matching loopback endpoint, answers and sends authenticated probes, and
 * runs one PathSelector per remote player. */

#include "vw_matchmaking.h"
#include "vw_path_selector.h"
#include "vw_udp.h"

namespace vwnet {

struct PathStatus {
   bool remote;                        /* false for the local player */
   bool direct;                        /* currently sending directly */
   Endpoint route;                     /* where datagrams go now */
   int relay_rtt_ms;                   /* -1 unknown */
   int direct_rtt_ms;                  /* best validated direct; -1 none */
   int switches_to_direct;
   int fallbacks;
};

class PathProxy {
public:
   PathProxy();
   ~PathProxy();

   /* The public socket; matchmaking runs on it before Start(). */
   bool OpenPublic(uint16_t port);
   UdpSocket *PublicSocket() { return &_public; }

   /* Start forwarding for GGPO bound on 127.0.0.1:ggpo_port (any interface
    * works; datagrams are delivered to loopback). allow_direct=false keeps
    * every player on the relay (no probes to direct candidates). */
   bool Start(const MatchResult &match, uint16_t ggpo_port, bool allow_direct);
   void Stop();

   /* GGPO's endpoint for remote player k (1-based); 0 for the local player. */
   uint16_t LoopbackPort(int player) const;
   PathStatus Status(int player);

private:
   enum { kCandidates = PathSelector::kMaxCandidates };
   struct Peer {
      bool remote;
      UdpSocket loopback;
      Endpoint candidates[kCandidates];   /* 0 relay, 1.. direct */
      bool candidate_set[kCandidates];
      PathSelector *selector;
   };

   static void ThreadMain(void *self);
   void Run();
   void FromGgpo(int k, const uint8_t *data, int size, const Endpoint &from);
   void FromNetwork(const uint8_t *data, int size, const Endpoint &from, uint32_t now);
   void SendProbes(uint32_t now);
   int FindCandidate(int k, const Endpoint &from) const;
   void Snapshot();

   PathProxy(const PathProxy &);
   PathProxy &operator=(const PathProxy &);

   UdpSocket _public;
   MatchResult _match;
   Peer _peers[kMaxPlayers];
   Endpoint _ggpo;
   bool _allow_direct;
   volatile bool _stop;
   Thread _thread;
   Mutex _lock;
   PathStatus _status[kMaxPlayers];
   bool _started;
};

}  // namespace vwnet

#endif
