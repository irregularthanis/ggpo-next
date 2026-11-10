#include <stdio.h>

#include "headless_host.h"
#include "vectorwar_inputs.h"

int
main()
{
   HeadlessVectorWarHost player_one(1);
   HeadlessVectorWarHost player_two(2);
   const VectorWarBounds bounds = player_one.GetGameBounds();
   if (bounds.left != 0 || bounds.top != 0 ||
       bounds.right != 640 || bounds.bottom != 480) {
      fprintf(stderr, "native host returned incorrect bounds\n");
      return 1;
   }
   if (player_one.CreateRenderer() != 0) {
      fprintf(stderr, "headless host unexpectedly created a renderer\n");
      return 1;
   }
   if (player_one.ReadLocalInput(0) != INPUT_ROTATE_LEFT ||
       player_two.ReadLocalInput(0) != INPUT_ROTATE_RIGHT ||
       player_one.ReadLocalInput(15) != INPUT_ROTATE_RIGHT) {
      fprintf(stderr, "native host scripted input contract changed\n");
      return 1;
   }

   const unsigned int before = player_one.NowMs();
   player_one.SleepMs(2);
   const unsigned int after = player_one.NowMs();
   if (after < before || after - before > 1000) {
      fprintf(stderr, "native host monotonic clock/sleep contract failed\n");
      return 1;
   }

   player_one.InitializePerformanceMonitor();
   player_one.UpdatePerformanceMonitor(0, 0, 0);
   player_one.TogglePerformanceMonitor();
   player_one.ShutdownPerformanceMonitor();
   printf("vectorwar-native-host: bounds, input, clock, sleep and no-op UI pass\n");
   return 0;
}
