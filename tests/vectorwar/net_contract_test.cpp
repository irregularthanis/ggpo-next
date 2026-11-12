/* Vector War networking: matchmaking codec (against the server's shared
 * vectors), SipHash, hole-punching probes, and the per-peer path selector on a
 * fake clock. No sockets; runs on every platform. */

#include "vw_net_protocol.h"
#include "vw_path_probe.h"
#include "vw_path_selector.h"
#include "vw_siphash.h"

#include <stdio.h>
#include <string.h>

using namespace vwnet;

static int failures = 0;
#define CHECK(condition, what) \
   do { if (!(condition)) { fprintf(stderr, "FAIL %s\n", what); ++failures; } } while (0)

#include "net_vectors.inc"

static void TestSipHashReference()
{
   /* SipHash-2-4 paper, appendix A: key 00..0f, message 00..0e. */
   uint8_t message[15];
   for (int i = 0; i < 15; ++i) message[i] = (uint8_t)i;
   const uint64_t k0 = 0x0706050403020100ULL, k1 = 0x0f0e0d0c0b0a0908ULL;
   CHECK(SipHash24(k0, k1, message, sizeof(message)) == 0xa129ca6149be45e5ULL, "siphash reference");
}

static void TestProtocolRejections()
{
   Message join;
   ClearMessage(&join, kJoin);
   join.nonce = 7;
   join.platform = kPlatformLinux;
   join.group_size = 2;
   strcpy(join.lobby, "ab12");
   uint8_t wire[kMaxMessageSize];
   const size_t size = Encode(join, wire, sizeof(wire));
   CHECK(size == kJoinSize, "join size");
   Message decoded;
   CHECK(Decode(wire, size, &decoded) && strcmp(decoded.lobby, "AB12") == 0, "lobby upper-cased");

   uint8_t bad[kMaxMessageSize];
   memcpy(bad, wire, size);
   bad[20] = 'a';                                 /* lower case on the wire */
   CHECK(!Decode(bad, size, &decoded), "lower-case lobby rejected");
   memcpy(bad, wire, size);
   bad[4] = kPeersVersion;
   CHECK(!Decode(bad, size, &decoded), "JOIN with version 3 rejected");
   memcpy(bad, wire, size);
   bad[size - 1] = 1;
   CHECK(!Decode(bad, size, &decoded), "JOIN padding rejected");
   CHECK(!Decode(wire, size - 1, &decoded), "truncated JOIN rejected");

   char normalized[kLobbyCodeSize + 1];
   CHECK(!NormalizeLobby("TOOLONGCODE", normalized), "long lobby code rejected");
   CHECK(!NormalizeLobby("dash-1", normalized), "invalid lobby character rejected");
   strcpy(join.lobby, "AB-1");
   CHECK(Encode(join, wire, sizeof(wire)) == 0, "invalid lobby not encodable");

   Message request;
   ClearMessage(&request, kPeersRequest);
   request.token = 9;
   request.player = 2;
   request.lan = MakeEndpoint(192, 168, 1, 20, 7000);
   const size_t request_size = Encode(request, wire, sizeof(wire));
   CHECK(request_size == kPeersSize && wire[4] == kPeersVersion, "PEERS_REQUEST is v3, 68 bytes");
   memcpy(bad, wire, request_size);
   bad[4] = kVersion;
   CHECK(!Decode(bad, request_size, &decoded), "PEERS_REQUEST with version 2 rejected");
}

static void TestEndpoints()
{
   CHECK(IsPrivateLan(MakeEndpoint(10, 1, 2, 3, 1)), "10/8 private");
   CHECK(IsPrivateLan(MakeEndpoint(172, 31, 0, 1, 1)), "172.16/12 private");
   CHECK(!IsPrivateLan(MakeEndpoint(172, 32, 0, 1, 1)), "172.32 public");
   CHECK(!IsPrivateLan(MakeEndpoint(100, 64, 0, 1, 1)), "CGNAT space not LAN");
   char text[32];
   FormatEndpoint(MakeEndpoint(198, 51, 100, 7, 40001), text, sizeof(text));
   CHECK(strcmp(text, "198.51.100.7:40001") == 0, "format endpoint");
}

static void TestProbes()
{
   const uint64_t token = 0x1122334455667788ULL;
   Probe probe = { kProbe, 1, 2, 42, 123456 };
   uint8_t wire[kProbeSize];
   EncodeProbe(probe, token, wire);
   CHECK(LooksLikeProbe(wire, sizeof(wire)), "probe magic");
   Probe decoded;
   CHECK(DecodeProbe(wire, sizeof(wire), token, &decoded) && decoded.sequence == 42 &&
         decoded.time_ms == 123456 && decoded.from == 1 && decoded.to == 2, "probe round trip");
   CHECK(!DecodeProbe(wire, sizeof(wire), token + 1, &decoded), "wrong token rejected");
   for (int i = 8; i < kProbeSize; ++i) {
      uint8_t tampered[kProbeSize];
      memcpy(tampered, wire, sizeof(wire));
      tampered[i] ^= 0x01;
      CHECK(!DecodeProbe(tampered, sizeof(tampered), token, &decoded), "tampered byte rejected");
   }
   CHECK(!DecodeProbe(wire, sizeof(wire) - 1, token, &decoded), "short probe rejected");
   const uint8_t ggpo_like[16] = { 0x12, 0x34, 0, 0 };
   CHECK(!LooksLikeProbe(ggpo_like, sizeof(ggpo_like)), "GGPO data is not a probe");
}

/* Drive one direct candidate (1) and the relay (0) with fixed round trips. */
struct Harness {
   PathSelector selector;
   uint32_t now;
   Harness() : selector(2), now(1000) {}
   void RoundTrip(int candidate, uint32_t rtt)
   {
      const uint32_t sequence = selector.ProbeSent(candidate, now);
      now += rtt;
      selector.AckReceived(candidate, sequence, now);
   }
};

static void TestSelectorStartsOnRelayAndSwitchesWhenFaster()
{
   Harness h;
   CHECK(h.selector.Active() == PathSelector::kRelay, "starts on relay");
   h.RoundTrip(0, 60);
   h.RoundTrip(1, 20);
   CHECK(h.selector.Active() == PathSelector::kRelay, "ack alone does not validate");
   h.selector.PeerProbeReceived(1, h.now);
   CHECK(h.selector.Validated(1), "validated both ways");
   h.RoundTrip(1, 20);
   h.RoundTrip(1, 20);
   CHECK(h.selector.Active() == PathSelector::kRelay, "two confirmations are not enough");
   h.RoundTrip(1, 20);
   CHECK(h.selector.Active() == 1, "switches after three faster acks");
}

static void TestSelectorPrefersHealthyDirectAtEqualLatency()
{
   Harness h;
   h.RoundTrip(0, 30);
   h.selector.PeerProbeReceived(1, h.now);
   for (int i = 0; i < 3; ++i) h.RoundTrip(1, 34);   /* within the 5 ms margin */
   CHECK(h.selector.Active() == 1, "healthy direct preferred within the margin");
}

static void TestSelectorStaysOnRelayWhenDirectIsSlower()
{
   Harness h;
   h.RoundTrip(0, 30);
   h.selector.PeerProbeReceived(1, h.now);
   for (int i = 0; i < 6; ++i) h.RoundTrip(1, 50);   /* 20 ms slower */
   CHECK(h.selector.Active() == PathSelector::kRelay, "slower direct path not used");
}

static void TestSelectorSymmetricNatNeverValidates()
{
   Harness h;
   h.RoundTrip(0, 40);
   for (int i = 0; i < 10; ++i) {
      h.selector.ProbeSent(1, h.now);              /* never answered */
      h.now += 250;
      h.selector.Update(h.now);
   }
   CHECK(h.selector.Active() == PathSelector::kRelay && !h.selector.Validated(1),
         "unanswered direct path stays on relay");
}

static void TestSelectorFallsBackOnSilenceAndBacksOff()
{
   Harness h;
   h.RoundTrip(0, 60);
   h.selector.PeerProbeReceived(1, h.now);
   for (int i = 0; i < 3; ++i) h.RoundTrip(1, 20);
   CHECK(h.selector.Active() == 1, "direct before the outage");
   h.now += 400;
   h.selector.Update(h.now);
   CHECK(h.selector.Active() == 1, "400 ms of silence is tolerated");
   h.now += 200;
   h.selector.Update(h.now);
   CHECK(h.selector.Active() == PathSelector::kRelay, "falls back after 500 ms of silence");
   CHECK(!h.selector.Validated(1), "fallen-back path must revalidate");
   CHECK(h.selector.BackoffMs(1) == 500, "first backoff 500 ms");
   CHECK(!h.selector.ProbeDue(1, h.now + 499) && h.selector.ProbeDue(1, h.now + 500), "probe waits for backoff");
   const uint32_t expected[] = { 1000, 2000, 4000, 5000, 5000 };
   for (int i = 0; i < 5; ++i) {
      h.selector.ProbeSent(1, h.now);
      CHECK(h.selector.BackoffMs(1) == expected[i], "backoff doubles up to 5 s");
   }
   h.selector.PeerProbeReceived(1, h.now);
   h.RoundTrip(1, 20);
   CHECK(h.selector.BackoffMs(1) == 0, "revalidation resets backoff");
}

static void TestSelectorFallsBackWhenDirectDegrades()
{
   Harness h;
   h.RoundTrip(0, 40);
   h.selector.PeerProbeReceived(1, h.now);
   for (int i = 0; i < 3; ++i) h.RoundTrip(1, 10);
   CHECK(h.selector.Active() == 1, "direct first");
   for (int i = 0; i < 40; ++i) {
      h.RoundTrip(1, 200);                         /* congested direct path */
      h.selector.Update(h.now);
      if (h.selector.Active() == PathSelector::kRelay) break;
   }
   CHECK(h.selector.Active() == PathSelector::kRelay, "sustained degradation falls back");
}

static void TestSelectorRejectsUnknownOrStaleAcks()
{
   Harness h;
   CHECK(!h.selector.AckReceived(1, 999, h.now), "unknown sequence rejected");
   const uint32_t sequence = h.selector.ProbeSent(1, h.now);
   CHECK(!h.selector.AckReceived(1, sequence, h.now + 2001), "ack outside the window rejected");
   CHECK(!h.selector.AckReceived(1, sequence, h.now + 10), "each probe acknowledged once");
   const uint32_t other = h.selector.ProbeSent(0, h.now);
   CHECK(!h.selector.AckReceived(1, other, h.now + 5), "ack must come from the probed candidate");
}

int main()
{
   const int vectors = CheckProtocolVectors();
   CHECK(vectors >= 13, "all shared vectors present");
   TestSipHashReference();
   TestProtocolRejections();
   TestEndpoints();
   TestProbes();
   TestSelectorStartsOnRelayAndSwitchesWhenFaster();
   TestSelectorPrefersHealthyDirectAtEqualLatency();
   TestSelectorStaysOnRelayWhenDirectIsSlower();
   TestSelectorSymmetricNatNeverValidates();
   TestSelectorFallsBackOnSilenceAndBacksOff();
   TestSelectorFallsBackWhenDirectDegrades();
   TestSelectorRejectsUnknownOrStaleAcks();
   if (failures) {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("vectorwar-net: %d shared vectors, siphash, probes and path selector pass\n", vectors);
   return 0;
}
