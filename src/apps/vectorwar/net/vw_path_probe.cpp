#include "vw_path_probe.h"

#include "vw_siphash.h"

#include <string.h>

namespace vwnet {

namespace {

const uint8_t kProbeMagic[8] = { 'V', 'W', 'P', 'A', 'T', 'H', '0', '1' };
const uint64_t kKeyMix = 0x3130485441505756ULL;   /* "VWPATH01" little-endian */
const char kTagLabel[] = "vw-session-tag";

void Put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
uint32_t Get32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint64_t Mac(uint64_t token, const uint8_t *data, size_t length)
{
   return SipHash24(token, token ^ kKeyMix, data, length);
}

uint32_t Tag(uint64_t token)
{
   return (uint32_t)Mac(token, (const uint8_t *)kTagLabel, sizeof(kTagLabel) - 1);
}

}  // namespace

bool
LooksLikeProbe(const uint8_t *data, size_t length)
{
   return length >= sizeof(kProbeMagic) && memcmp(data, kProbeMagic, sizeof(kProbeMagic)) == 0;
}

void
EncodeProbe(const Probe &probe, uint64_t token, uint8_t out[kProbeSize])
{
   memset(out, 0, kProbeSize);
   memcpy(out, kProbeMagic, sizeof(kProbeMagic));
   out[8] = (uint8_t)probe.type;
   out[9] = (uint8_t)probe.from;
   out[10] = (uint8_t)probe.to;
   Put32(out + 12, probe.sequence);
   Put32(out + 16, probe.time_ms);
   Put32(out + 20, Tag(token));
   const uint64_t mac = Mac(token, out, 24);
   for (int i = 0; i < 8; ++i) out[24 + i] = (uint8_t)(mac >> (8 * i));
}

bool
DecodeProbe(const uint8_t *data, size_t length, uint64_t token, Probe *probe)
{
   if (length != kProbeSize || !LooksLikeProbe(data, length) || data[11] != 0) return false;
   if (Get32(data + 20) != Tag(token)) return false;
   uint64_t mac = 0;
   for (int i = 7; i >= 0; --i) mac = (mac << 8) | data[24 + i];
   if (mac != Mac(token, data, 24)) return false;
   const int type = data[8];
   if (type < kProbe || type > kKeepalive) return false;
   if (data[9] < 1 || data[9] > 4 || data[10] < 1 || data[10] > 4 || data[9] == data[10]) return false;
   probe->type = type;
   probe->from = data[9];
   probe->to = data[10];
   probe->sequence = Get32(data + 12);
   probe->time_ms = Get32(data + 16);
   return true;
}

}  // namespace vwnet
