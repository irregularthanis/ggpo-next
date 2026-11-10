#include <stdint.h>
#include <string.h>
#include "state_contract.h"

namespace {

/* FNV-1a keeps the format compact while making every serialized byte visible. */
void HashByte(uint32_t &hash, unsigned char value)
{
   hash ^= value;
   hash *= 16777619u;
}

void HashU32(uint32_t &hash, uint32_t value)
{
   for (int shift = 0; shift < 32; shift += 8) {
      HashByte(hash, (unsigned char)((value >> shift) & 0xff));
   }
}

void HashU64(uint32_t &hash, uint64_t value)
{
   for (int shift = 0; shift < 64; shift += 8) {
      HashByte(hash, (unsigned char)((value >> shift) & 0xff));
   }
}

void HashDouble(uint32_t &hash, double value)
{
   uint64_t bits = 0;
   if (sizeof(value) == sizeof(bits)) {
      memcpy(&bits, &value, sizeof(bits));
   }
   HashU64(hash, bits);
}

void HashPosition(uint32_t &hash, const Position &position)
{
   HashDouble(hash, position.x);
   HashDouble(hash, position.y);
}

void HashVelocity(uint32_t &hash, const Velocity &velocity)
{
   HashDouble(hash, velocity.dx);
   HashDouble(hash, velocity.dy);
}

}  // namespace

unsigned int VectorWarCanonicalChecksum(const GameState &state)
{
   uint32_t hash = 2166136261u;
   HashU32(hash, (uint32_t)state._framenumber);
   HashU32(hash, (uint32_t)state._bounds.left);
   HashU32(hash, (uint32_t)state._bounds.top);
   HashU32(hash, (uint32_t)state._bounds.right);
   HashU32(hash, (uint32_t)state._bounds.bottom);
   HashU32(hash, (uint32_t)state._num_ships);

   for (int i = 0; i < MAX_SHIPS; ++i) {
      const Ship &ship = state._ships[i];
      HashPosition(hash, ship.position);
      HashVelocity(hash, ship.velocity);
      HashU32(hash, (uint32_t)ship.radius);
      HashU32(hash, (uint32_t)ship.heading);
      HashU32(hash, (uint32_t)ship.health);
      HashU32(hash, (uint32_t)ship.speed);
      HashU32(hash, (uint32_t)ship.cooldown);
      for (int j = 0; j < MAX_BULLETS; ++j) {
         const Bullet &bullet = ship.bullets[j];
         HashByte(hash, bullet.active ? 1 : 0);
         HashPosition(hash, bullet.position);
         HashVelocity(hash, bullet.velocity);
      }
      HashU32(hash, (uint32_t)ship.score);
   }
   return hash;
}
