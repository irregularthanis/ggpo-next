#include "types.h"
#include "bitvector.h"
#include "ring_buffer.h"
#include "static_buffer.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

// Record a utility invariant failure.
static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "core utility contract failed: %s\n", contract);
      ++failures;
   }
}

// Verify FIFO order and index translation across ring wrap.
static void TestRingBuffer()
{
   RingBuffer<int, 4> ring;
   ring.push(10);
   ring.push(20);
   ring.push(30);
   Check(ring.size() == 3, "ring reaches its documented usable capacity");
   Check(ring.front() == 10, "ring front starts at first item");
   ring.pop();
   ring.push(40);
   Check(ring.item(0) == 20 && ring.item(1) == 30 && ring.item(2) == 40,
         "ring preserves FIFO order after wrap");
}

// Verify fixed storage retains insertion order up to its usable capacity.
static void TestStaticBuffer()
{
   StaticBuffer<int, 4> buffer;
   buffer.push_back(4);
   buffer.push_back(5);
   buffer.push_back(6);
   Check(buffer.size() == 3, "static buffer reports inserted elements");
   Check(buffer[0] == 4 && buffer[2] == 6,
         "static buffer retains insertion order");
}

// Verify bit and byte operations cross an underlying-byte boundary.
static void TestBitVector()
{
   uint8 bytes[3];
   int offset = 7;
   int read_offset = 7;
   memset(bytes, 0, sizeof(bytes));

   BitVector_SetBit(bytes, &offset);
   BitVector_WriteNibblet(bytes, 0xa5, &offset);
   Check(BitVector_ReadBit(bytes, &read_offset) == 1,
         "single bit survives a byte boundary");
   Check(BitVector_ReadNibblet(bytes, &read_offset) == 0xa5,
         "nibblet round-trips across bytes");

   offset = 7;
   BitVector_ClearBit(bytes, &offset);
   read_offset = 7;
   Check(BitVector_ReadBit(bytes, &read_offset) == 0,
         "clear resets a previously set bit");
}

int main()
{
   TestRingBuffer();
   TestStaticBuffer();
   TestBitVector();
   return failures == 0 ? 0 : 1;
}
