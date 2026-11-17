#include "vw_siphash.h"

namespace vwnet {

namespace {

inline uint64_t Rotl(uint64_t value, int bits)
{
   return (value << bits) | (value >> (64 - bits));
}

inline void Round(uint64_t &v0, uint64_t &v1, uint64_t &v2, uint64_t &v3)
{
   v0 += v1; v1 = Rotl(v1, 13); v1 ^= v0; v0 = Rotl(v0, 32);
   v2 += v3; v3 = Rotl(v3, 16); v3 ^= v2;
   v0 += v3; v3 = Rotl(v3, 21); v3 ^= v0;
   v2 += v1; v1 = Rotl(v1, 17); v1 ^= v2; v2 = Rotl(v2, 32);
}

inline uint64_t Load64(const uint8_t *p)
{
   return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
          ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
          ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

}  // namespace

uint64_t
SipHash24(uint64_t k0, uint64_t k1, const void *data, size_t length)
{
   const uint8_t *in = (const uint8_t *)data;
   uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
   uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
   uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
   uint64_t v3 = k1 ^ 0x7465646279746573ULL;

   const size_t whole = length & ~(size_t)7;
   for (size_t offset = 0; offset < whole; offset += 8) {
      const uint64_t m = Load64(in + offset);
      v3 ^= m;
      Round(v0, v1, v2, v3);
      Round(v0, v1, v2, v3);
      v0 ^= m;
   }

   uint64_t last = (uint64_t)length << 56;
   const uint8_t *tail = in + whole;
   switch (length & 7) {
   case 7: last |= (uint64_t)tail[6] << 48;  /* fall through */
   case 6: last |= (uint64_t)tail[5] << 40;  /* fall through */
   case 5: last |= (uint64_t)tail[4] << 32;  /* fall through */
   case 4: last |= (uint64_t)tail[3] << 24;  /* fall through */
   case 3: last |= (uint64_t)tail[2] << 16;  /* fall through */
   case 2: last |= (uint64_t)tail[1] << 8;   /* fall through */
   case 1: last |= (uint64_t)tail[0];        break;
   default: break;
   }
   v3 ^= last;
   Round(v0, v1, v2, v3);
   Round(v0, v1, v2, v3);
   v0 ^= last;

   v2 ^= 0xff;
   Round(v0, v1, v2, v3);
   Round(v0, v1, v2, v3);
   Round(v0, v1, v2, v3);
   Round(v0, v1, v2, v3);
   return v0 ^ v1 ^ v2 ^ v3;
}

}  // namespace vwnet
