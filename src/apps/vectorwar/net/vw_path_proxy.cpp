#include "vw_path_proxy.h"

#include "vw_path_probe.h"

#include <string.h>

namespace vwnet {

PathProxy::PathProxy() : _allow_direct(true), _stop(false), _started(false)
{
   memset(&_match, 0, sizeof(_match));
   memset(&_status, 0, sizeof(_status));
   for (int k = 0; k < kMaxPlayers; ++k) {
      _peers[k].remote = false;
      _peers[k].selector = NULL;
      memset(_peers[k].candidates, 0, sizeof(_peers[k].candidates));
      memset(_peers[k].candidate_set, 0, sizeof(_peers[k].candidate_set));
   }
}

PathProxy::~PathProxy()
{
   Stop();
   for (int k = 0; k < kMaxPlayers; ++k) delete _peers[k].selector;
}

bool PathProxy::OpenPublic(uint16_t port) { return _public.Open(port, false); }

bool
PathProxy::Start(const MatchResult &match, uint16_t ggpo_port, bool allow_direct)
{
   if (_started || !_public.IsOpen()) return false;
   _match = match;
   _allow_direct = allow_direct;
   _ggpo = MakeEndpoint(127, 0, 0, 1, ggpo_port);
   for (int k = 0; k < match.count; ++k) {
      Peer &peer = _peers[k];
      peer.remote = k + 1 != match.player;
      if (!peer.remote) continue;
      if (!peer.loopback.Open(0, true)) return false;
      peer.candidates[0] = match.relay[k];
      peer.candidate_set[0] = true;
      int next = 1;
      if (match.have_candidates && allow_direct) {
         const Endpoint &pub = match.public_candidates[k];
         if (!IsUnset(pub)) { peer.candidates[next] = pub; peer.candidate_set[next++] = true; }
         const Endpoint &lan = match.lan_candidates[k];
         if (!IsUnset(lan) && !SameEndpoint(lan, pub)) {
            peer.candidates[next] = lan;
            peer.candidate_set[next++] = true;
         }
      }
      peer.selector = new PathSelector(kCandidates);
   }
   Snapshot();
   _stop = false;
   _started = _thread.Start(ThreadMain, this);
   return _started;
}

void
PathProxy::Stop()
{
   if (!_started) return;
   _stop = true;
   _thread.Join();
   _started = false;
}

uint16_t
PathProxy::LoopbackPort(int player) const
{
   if (player < 1 || player > _match.count || !_peers[player - 1].remote) return 0;
   return _peers[player - 1].loopback.LocalPort();
}

PathStatus
PathProxy::Status(int player)
{
   PathStatus status;
   memset(&status, 0, sizeof(status));
   if (player < 1 || player > kMaxPlayers) return status;
   _lock.Lock();
   status = _status[player - 1];
   _lock.Unlock();
   return status;
}

void PathProxy::ThreadMain(void *self) { ((PathProxy *)self)->Run(); }

void
PathProxy::Run()
{
   UdpSocket *sockets[kMaxPlayers + 1];
   int count = 0;
   sockets[count++] = &_public;
   for (int k = 0; k < _match.count; ++k) {
      if (_peers[k].remote) sockets[count++] = &_peers[k].loopback;
   }
   uint8_t buffer[2048];
   uint32_t last_snapshot = 0;
   while (!_stop) {
      UdpSocket::WaitReadable(sockets, count, 5);
      const uint32_t now = NowMs();
      for (int k = 0; k < _match.count; ++k) {
         if (!_peers[k].remote) continue;
         Endpoint from;
         int size;
         while ((size = _peers[k].loopback.ReceiveFrom(buffer, sizeof(buffer), &from)) > 0) {
            FromGgpo(k, buffer, size, from);
         }
      }
      Endpoint from;
      int size;
      while ((size = _public.ReceiveFrom(buffer, sizeof(buffer), &from)) > 0) {
         FromNetwork(buffer, size, from, now);
      }
      SendProbes(now);
      for (int k = 0; k < _match.count; ++k) {
         if (_peers[k].remote) _peers[k].selector->Update(now);
      }
      if ((int32_t)(now - last_snapshot) >= 100) {
         Snapshot();
         last_snapshot = now;
      }
   }
}

void
PathProxy::FromGgpo(int k, const uint8_t *data, int size, const Endpoint &from)
{
   /* Only GGPO's own socket may send through the proxy. */
   if (from.port != _ggpo.port || from.ip[0] != 127) return;
   Peer &peer = _peers[k];
   const int active = peer.selector->Active();
   const int route = peer.candidate_set[active] ? active : 0;
   _public.SendTo(peer.candidates[route], data, (size_t)size);
}

int
PathProxy::FindCandidate(int k, const Endpoint &from) const
{
   for (int c = 0; c < kCandidates; ++c) {
      if (_peers[k].candidate_set[c] && SameEndpoint(_peers[k].candidates[c], from)) return c;
   }
   return -1;
}

void
PathProxy::FromNetwork(const uint8_t *data, int size, const Endpoint &from, uint32_t now)
{
   if (LooksLikeProbe(data, (size_t)size)) {
      Probe probe;
      if (!DecodeProbe(data, (size_t)size, _match.token, &probe)) return;
      if (probe.to != _match.player || probe.from < 1 || probe.from > _match.count) return;
      const int k = probe.from - 1;
      Peer &peer = _peers[k];
      if (!peer.remote) return;
      int c = FindCandidate(k, from);
      if (c < 0) {
         /* Peer-reflexive candidate: an authenticated probe from an address
          * the server did not advertise (e.g. a different NAT mapping). */
         if (!_allow_direct) return;
         for (int slot = 1; slot < kCandidates && c < 0; ++slot) {
            if (!peer.candidate_set[slot]) {
               peer.candidates[slot] = from;
               peer.candidate_set[slot] = true;
               c = slot;
            }
         }
         if (c < 0) return;
      }
      if (probe.type == kProbe) {
         Probe ack = { kProbeAck, _match.player, probe.from, probe.sequence, probe.time_ms };
         uint8_t wire[kProbeSize];
         EncodeProbe(ack, _match.token, wire);
         _public.SendTo(from, wire, sizeof(wire));
         peer.selector->PeerProbeReceived(c, now);
      } else if (probe.type == kProbeAck) {
         peer.selector->AckReceived(c, probe.sequence, now);
      } else {
         peer.selector->TrafficReceived(c, now);
      }
      return;
   }
   /* GGPO data: from a relay port, or from a validated direct address. */
   for (int k = 0; k < _match.count; ++k) {
      Peer &peer = _peers[k];
      if (!peer.remote) continue;
      const int c = FindCandidate(k, from);
      if (c < 0) continue;
      if (c != 0 && !peer.selector->Validated(c)) return;
      peer.selector->TrafficReceived(c, now);
      peer.loopback.SendTo(_ggpo, data, (size_t)size);
      return;
   }
}

void
PathProxy::SendProbes(uint32_t now)
{
   for (int k = 0; k < _match.count; ++k) {
      Peer &peer = _peers[k];
      if (!peer.remote) continue;
      for (int c = 0; c < kCandidates; ++c) {
         if (!peer.candidate_set[c] || (c != 0 && !_allow_direct)) continue;
         if (!peer.selector->ProbeDue(c, now)) continue;
         Probe probe = { kProbe, _match.player, k + 1, peer.selector->ProbeSent(c, now), now };
         uint8_t wire[kProbeSize];
         EncodeProbe(probe, _match.token, wire);
         _public.SendTo(peer.candidates[c], wire, sizeof(wire));
      }
   }
}

void
PathProxy::Snapshot()
{
   PathStatus status[kMaxPlayers];
   memset(status, 0, sizeof(status));
   for (int k = 0; k < _match.count; ++k) {
      const Peer &peer = _peers[k];
      status[k].remote = peer.remote;
      if (!peer.remote) continue;
      const int active = peer.selector->Active();
      status[k].direct = active != PathSelector::kRelay && peer.candidate_set[active];
      status[k].route = peer.candidates[status[k].direct ? active : 0];
      status[k].relay_rtt_ms = peer.selector->SmoothedRttMs(0);
      status[k].switches_to_direct = peer.selector->SwitchesToDirect();
      status[k].fallbacks = peer.selector->Fallbacks();
      status[k].direct_rtt_ms = -1;
      for (int c = 1; c < kCandidates; ++c) {
         const int rtt = peer.selector->SmoothedRttMs(c);
         if (peer.candidate_set[c] && peer.selector->Validated(c) && rtt >= 0 &&
             (status[k].direct_rtt_ms < 0 || rtt < status[k].direct_rtt_ms)) {
            status[k].direct_rtt_ms = rtt;
         }
      }
   }
   _lock.Lock();
   memcpy(_status, status, sizeof(_status));
   _lock.Unlock();
}

}  // namespace vwnet
