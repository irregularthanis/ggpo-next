#include <stdio.h>
#include <string.h>
#include "ggponet.h"
#include "gamestate.h"
#include "state_contract.h"
#include "vectorwar_inputs.h"

GGPOSession *ggpo = 0;

struct ExpectedCheckpoint {
   int frame;
   unsigned int checksum;
};

static const ExpectedCheckpoint kExpected[] = {
   { 1, 2171217371u },
   { 30, 3703441302u },
   { 60, 3529340798u },
   { 90, 3925171079u },
   { 120, 3746018346u },
   { 150, 3780635078u },
   { 180, 225756240u },
};

/* Keep inputs deterministic while exercising movement, rotation and bullets. */
static int ScriptedInput(int player, int frame)
{
   int input = 0;
   if (((frame + player * 7) % 5) < 3) {
      input |= INPUT_THRUST;
   }
   if (((frame / 11) + player) % 2) {
      input |= INPUT_ROTATE_LEFT;
   } else {
      input |= INPUT_ROTATE_RIGHT;
   }
   if (((frame + player * 13) % 17) == 0) {
      input |= INPUT_FIRE;
   }
   return input;
}

int main()
{
   if (sizeof(double) != 8) {
      fprintf(stderr, "Vector War canonical state requires 64-bit double\n");
      return 1;
   }

   GameState state;
   memset(&state, 0, sizeof(state));
   const VectorWarBounds bounds = { 0, 0, 640, 480 };
   state.Init(bounds, 2);

   int checkpoint = 0;
   for (int frame = 0; frame < 180; ++frame) {
      int inputs[MAX_SHIPS] = { 0 };
      inputs[0] = ScriptedInput(0, frame);
      inputs[1] = ScriptedInput(1, frame);
      state.Update(inputs, 0);
      if (checkpoint < (int)(sizeof(kExpected) / sizeof(kExpected[0])) &&
          state._framenumber == kExpected[checkpoint].frame) {
         const unsigned int actual = VectorWarCanonicalChecksum(state);
         if (actual != kExpected[checkpoint].checksum) {
            fprintf(stderr,
                    "Vector War checkpoint %d: expected %u, got %u\n",
                    state._framenumber, kExpected[checkpoint].checksum, actual);
            return 1;
         }
         ++checkpoint;
      }
   }

   const unsigned int checksum = VectorWarCanonicalChecksum(state);
   printf("frames=%d checksum=%u\n", state._framenumber, checksum);
   if (state._framenumber != 180 ||
       checkpoint != (int)(sizeof(kExpected) / sizeof(kExpected[0]))) {
      fprintf(stderr, "Vector War state contract stopped at frame %d\n",
              state._framenumber);
      return 1;
   }
   return 0;
}
