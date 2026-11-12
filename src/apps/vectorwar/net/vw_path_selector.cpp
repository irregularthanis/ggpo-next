#include "vw_path_selector.h"

#include <string.h>

namespace vwnet {

namespace {
int32_t Since(uint32_t now, uint32_t then) { return (int32_t)(now - then); }
}

PathConfig::PathConfig() :
   probe_interval_ms(250),
   relay_probe_interval_ms(500),
   switch_margin_ms(5),
   switch_confirmations(3),
   silence_fallback_ms(500),
   max_backoff_ms(5000),
   ack_window_ms(2000)
{
}

PathSelector::PathSelector(int candidates, const PathConfig &config) :
   _config(config),
   _count(candidates < 1 ? 1 : (candidates > kMaxCandidates ? kMaxCandidates : candidates)),
   _active(kRelay),
   _sequence(0)
{
   memset(_candidates, 0, sizeof(_candidates));
   for (int i = 0; i < kMaxCandidates; ++i) _candidates[i].srtt = -1;
}

bool
PathSelector::Validated(int candidate) const
{
   if (!Valid(candidate)) return false;
   if (candidate == kRelay) return _candidates[kRelay].ack_ok;
   return _candidates[candidate].ack_ok && _candidates[candidate].inbound_ok;
}

int
PathSelector::SmoothedRttMs(int candidate) const
{
   return Valid(candidate) ? _candidates[candidate].srtt : -1;
}

uint32_t
PathSelector::BackoffMs(int candidate) const
{
   return Valid(candidate) ? _candidates[candidate].backoff : 0;
}

bool
PathSelector::ProbeDue(int candidate, uint32_t now) const
{
   if (!Valid(candidate)) return false;
   return Since(now, _candidates[candidate].next_probe) >= 0;
}

uint32_t
PathSelector::ProbeSent(int candidate, uint32_t now)
{
   if (!Valid(candidate)) return 0;
   Candidate &c = _candidates[candidate];
   const uint32_t sequence = ++_sequence;
   const int slot = (int)(sequence % kOutstanding);
   c.sent_sequence[slot] = sequence;
   c.sent_time[slot] = now;
   c.sent_valid[slot] = true;
   uint32_t interval = candidate == kRelay ? _config.relay_probe_interval_ms
                                           : _config.probe_interval_ms;
   if (candidate != kRelay && c.backoff > interval) interval = c.backoff;
   c.next_probe = now + interval;
   /* While a fallen-back candidate stays unvalidated, each attempt doubles
    * the wait (up to max_backoff_ms); validation resets it. */
   if (candidate != kRelay && c.backoff > 0 && !Validated(candidate)) {
      c.backoff = c.backoff * 2 > _config.max_backoff_ms ? _config.max_backoff_ms : c.backoff * 2;
   }
   return sequence;
}

bool
PathSelector::AckReceived(int candidate, uint32_t sequence, uint32_t now)
{
   if (!Valid(candidate)) return false;
   Candidate &c = _candidates[candidate];
   const int slot = (int)(sequence % kOutstanding);
   if (!c.sent_valid[slot] || c.sent_sequence[slot] != sequence) return false;
   const int32_t rtt = Since(now, c.sent_time[slot]);
   c.sent_valid[slot] = false;
   if (rtt < 0 || (uint32_t)rtt > _config.ack_window_ms) return false;
   c.srtt = c.srtt < 0 ? rtt : (7 * c.srtt + rtt) / 8;
   const bool was_validated = Validated(candidate);
   c.ack_ok = true;
   c.last_rx = now;
   c.heard = true;
   if (!was_validated && Validated(candidate)) c.backoff = 0;
   Evaluate(candidate);
   return true;
}

void
PathSelector::PeerProbeReceived(int candidate, uint32_t now)
{
   if (!Valid(candidate)) return;
   Candidate &c = _candidates[candidate];
   const bool was_validated = Validated(candidate);
   c.inbound_ok = true;
   c.last_rx = now;
   c.heard = true;
   if (!was_validated && Validated(candidate)) c.backoff = 0;
}

void
PathSelector::TrafficReceived(int candidate, uint32_t now)
{
   if (!Valid(candidate)) return;
   _candidates[candidate].last_rx = now;
   _candidates[candidate].heard = true;
}

void
PathSelector::Evaluate(int candidate)
{
   const int relay_rtt = _candidates[kRelay].srtt;
   if (_active == kRelay) {
      if (candidate == kRelay || !Validated(candidate)) return;
      Candidate &c = _candidates[candidate];
      const bool faster = relay_rtt < 0 ||
                          c.srtt + (int)_config.switch_margin_ms < relay_rtt;
      c.better = faster ? c.better + 1 : 0;
      if (c.better >= _config.switch_confirmations) {
         _active = candidate;
         c.better = 0;
         c.worse = 0;
      }
      return;
   }
   if (candidate != _active || relay_rtt < 0) return;
   Candidate &c = _candidates[candidate];
   const bool worse = c.srtt > relay_rtt + (int)_config.switch_margin_ms;
   c.worse = worse ? c.worse + 1 : 0;
}

void
PathSelector::Update(uint32_t now)
{
   if (_active == kRelay) return;
   const Candidate &c = _candidates[_active];
   if (c.worse >= _config.switch_confirmations ||
       Since(now, c.last_rx) > (int32_t)_config.silence_fallback_ms) {
      FallBack(now);
   }
}

void
PathSelector::FallBack(uint32_t now)
{
   Candidate &c = _candidates[_active];
   c.ack_ok = false;
   c.inbound_ok = false;
   c.better = 0;
   c.worse = 0;
   const uint32_t first = 2 * _config.probe_interval_ms;
   c.backoff = c.backoff == 0 ? first : c.backoff * 2;
   if (c.backoff > _config.max_backoff_ms) c.backoff = _config.max_backoff_ms;
   c.next_probe = now + c.backoff;
   _active = kRelay;
}

}  // namespace vwnet
