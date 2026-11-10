#include <stdio.h>
#include <string.h>

#include "ggponet.h"
#include "gamestate.h"
#include "state_contract.h"

GGPOSession *ggpo = 0;

static bool
ExpectShip(const GameState &state, int ship, double x, double y, int heading)
{
   const Ship &actual = state._ships[ship];
   if (actual.position.x != x || actual.position.y != y ||
       actual.heading != heading) {
      fprintf(stderr,
              "ship %d: expected (%.0f,%.0f,%d), got (%.17g,%.17g,%d)\n",
              ship, x, y, heading, actual.position.x, actual.position.y,
              actual.heading);
      return false;
   }
   return true;
}

int
main()
{
   const VectorWarBounds bounds = { 0, 0, 640, 480 };
   GameState state;

   memset(&state, 0, sizeof(state));
   state.Init(bounds, 2);
   if (!ExpectShip(state, 0, 428, 232, 180) ||
       !ExpectShip(state, 1, 196, 232, 0)) {
      return 1;
   }

   memset(&state, 0, sizeof(state));
   state.Init(bounds, 4);
   if (!ExpectShip(state, 0, 428, 232, 180) ||
       !ExpectShip(state, 1, 312, 348, 270) ||
       !ExpectShip(state, 2, 196, 232, 0) ||
       !ExpectShip(state, 3, 312, 116, 90)) {
      return 1;
   }

   printf("vectorwar-gameplay: exact cardinal initialization checksum=%u\n",
          VectorWarCanonicalChecksum(state));
   return 0;
}
