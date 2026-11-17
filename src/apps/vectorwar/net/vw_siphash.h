#ifndef _VW_SIPHASH_H_
#define _VW_SIPHASH_H_

#include <stddef.h>
#include <stdint.h>

namespace vwnet {

/* SipHash-2-4 (Aumasson and Bernstein) with a 128-bit key given as two
 * little-endian 64-bit words. */
uint64_t SipHash24(uint64_t k0, uint64_t k1, const void *data, size_t length);

}  // namespace vwnet

#endif
