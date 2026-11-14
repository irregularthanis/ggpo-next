#ifndef _VW_PATH_SELECTOR_H_
#define _VW_PATH_SELECTOR_H_

/* Per-peer route choice for hole punching with relay fallback.
 *
 * Candidate 0 is the relay; candidates 1..n are direct addresses (public and
 * LAN). Every session starts on the relay. A direct candidate is validated
 * once our probe is acknowledged *and* the peer's own probe has arrived from
 * it, i.e. both NAT directions are open. A healthy direct path is preferred
 * even at equal latency (it takes the server off the data path): the selector
 * sends on a validated candidate once its smoothed round-trip time is no more
 * than switch_margin_ms above the relay's for switch_confirmations
 * consecutive acknowledgements. It falls back to the relay after
 * silence_fallback_ms without traffic from it, or when it is more than
 * fallback_margin_ms slower than the relay for the same number of consecutive
 * acknowledgements; the wider fallback margin prevents flapping. After a
 * fallback, direct probing backs off (doubling up to max_backoff_ms).
 *
 * All times are milliseconds from a caller-supplied clock, so tests drive it
 * with a fake clock. Wraparound is handled with signed differences. */

#include "vw_net_types.h"

namespace vwnet {

struct PathConfig {
   uint32_t probe_interval_ms;
   uint32_t relay_probe_interval_ms;
   uint32_t switch_margin_ms;
   uint32_t fallback_margin_ms;
   int switch_confirmations;
   uint32_t silence_fallback_ms;
   uint32_t max_backoff_ms;
   uint32_t ack_window_ms;
   PathConfig();
};

class PathSelector {
public:
   enum { kRelay = 0, kMaxCandidates = 4, kOutstanding = 8 };

   explicit PathSelector(int candidates, const PathConfig &config = PathConfig());

   int Active() const { return _active; }          /* candidate to send on */
   bool Validated(int candidate) const;
   int SmoothedRttMs(int candidate) const;         /* -1 when unknown */
   uint32_t BackoffMs(int candidate) const;
   int SwitchesToDirect() const { return _switches; }   /* relay -> direct */
   int Fallbacks() const { return _fallbacks; }         /* direct -> relay */

   bool ProbeDue(int candidate, uint32_t now) const;
   /* Record a probe sent now; returns the sequence to put in it. */
   uint32_t ProbeSent(int candidate, uint32_t now);
   /* An authenticated PROBE_ACK arrived from the candidate. Accepted only if
    * it echoes a probe we sent to that candidate within ack_window_ms. */
   bool AckReceived(int candidate, uint32_t sequence, uint32_t now);
   /* The peer's authenticated PROBE arrived from the candidate. */
   void PeerProbeReceived(int candidate, uint32_t now);
   /* Any other traffic from the candidate (GGPO data, keepalive). */
   void TrafficReceived(int candidate, uint32_t now);
   /* Periodic check for silence on the active direct path. */
   void Update(uint32_t now);

private:
   struct Candidate {
      bool ack_ok, inbound_ok, heard;
      int srtt;                              /* -1 unknown */
      uint32_t last_rx;
      uint32_t next_probe;
      uint32_t backoff;
      int better, worse;
      uint32_t sent_sequence[kOutstanding];
      uint32_t sent_time[kOutstanding];
      bool sent_valid[kOutstanding];
   };

   void Evaluate(int candidate);
   void FallBack(uint32_t now);
   bool Valid(int candidate) const { return candidate >= 0 && candidate < _count; }

   PathConfig _config;
   Candidate _candidates[kMaxCandidates];
   int _count;
   int _active;
   uint32_t _sequence;
   int _switches;
   int _fallbacks;
};

}  // namespace vwnet

#endif
