#ifndef _VW_PATH_PROBE_H_
#define _VW_PATH_PROBE_H_

/* Hole-punching control datagrams exchanged directly between players (and
 * through the relay as keepalives). 32 bytes, authenticated with SipHash-2-4
 * keyed by the session token, and recognizable by an 8-byte magic so they are
 * never mistaken for GGPO packets. */

#include "vw_net_types.h"

namespace vwnet {

enum { kProbeSize = 32 };
enum ProbeType { kProbe = 1, kProbeAck = 2, kKeepalive = 3 };

struct Probe {
   int type;
   int from;              /* sending player, 1-4 */
   int to;                /* receiving player, 1-4 */
   uint32_t sequence;     /* PROBE: sender's sequence; PROBE_ACK: echoed */
   uint32_t time_ms;      /* PROBE: sender's clock; PROBE_ACK: echoed */
};

/* True when data starts with the probe magic (cheap demultiplexing). */
bool LooksLikeProbe(const uint8_t *data, size_t length);

void EncodeProbe(const Probe &probe, uint64_t token, uint8_t out[kProbeSize]);

/* Size, magic, reserved byte, session tag, MAC and player range must all
 * match; anything else is rejected. */
bool DecodeProbe(const uint8_t *data, size_t length, uint64_t token, Probe *probe);

}  // namespace vwnet

#endif
