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

/* Two players start at 0 and 180 degrees. */
static const ExpectedCheckpoint kTwoPlayers[] = {
   { 1, 2171217371u },
   { 30, 3703441302u },
   { 60, 3529340798u },
   { 90, 3925171079u },
   { 120, 3746018346u },
   { 150, 3780635078u },
   { 180, 225756240u },
};

/* Three and four players start off the cardinal headings or turn through
 * every angle with more ships: the trigonometry must come out bit-identical
 * on every platform (see vw_trig.h). */
static const ExpectedCheckpoint kThreePlayers[] = {
   { 1, 1038753304u },
   { 90, 2721557853u },
   { 180, 3886490988u },
};

static const ExpectedCheckpoint kFourPlayers[] = {
   { 1, 4272662235u },
   { 90, 1788193191u },
   { 180, 1064521386u },
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

static int RunContract(int players, const ExpectedCheckpoint *expected, int count)
{
   GameState state;
   memset(&state, 0, sizeof(state));
   const VectorWarBounds bounds = { 0, 0, 640, 480 };
   state.Init(bounds, players);

   int checkpoint = 0;
   for (int frame = 0; frame < 180; ++frame) {
      int inputs[MAX_SHIPS] = { 0 };
      for (int player = 0; player < players; ++player) {
         inputs[player] = ScriptedInput(player, frame);
      }
      state.Update(inputs, 0);
      if (checkpoint < count && state._framenumber == expected[checkpoint].frame) {
         const unsigned int actual = VectorWarCanonicalChecksum(state);
         if (actual != expected[checkpoint].checksum) {
            fprintf(stderr,
                    "Vector War %d players, checkpoint %d: expected %u, got %u\n",
                    players, state._framenumber, expected[checkpoint].checksum, actual);
            return 1;
         }
         ++checkpoint;
      }
   }

   printf("players=%d frames=%d checksum=%u\n", players, state._framenumber,
          VectorWarCanonicalChecksum(state));
   if (state._framenumber != 180 || checkpoint != count) {
      fprintf(stderr, "Vector War %d-player state contract stopped at frame %d\n",
              players, state._framenumber);
      return 1;
   }
   return 0;
}

#define COUNT(a) ((int)(sizeof(a) / sizeof(a[0])))

int main()
{
   if (sizeof(double) != 8) {
      fprintf(stderr, "Vector War canonical state requires 64-bit double\n");
      return 1;
   }
   int failed = 0;
   failed |= RunContract(2, kTwoPlayers, COUNT(kTwoPlayers));
   failed |= RunContract(3, kThreePlayers, COUNT(kThreePlayers));
   failed |= RunContract(4, kFourPlayers, COUNT(kFourPlayers));
   return failed;
}
