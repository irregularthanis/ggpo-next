#include "types.h"
#include "timesync.h"

#include <stdio.h>

static int failures = 0;

// Record an expected frame-wait recommendation.
static void CheckEqual(int actual, int expected, const char *contract)
{
   if (actual != expected) {
      fprintf(stderr, "time sync contract failed: %s (got %d, expected %d)\n",
              contract, actual, expected);
      ++failures;
   }
}

// Fill a complete history window with controlled frame advantages and inputs.
static void FillWindow(TimeSync *sync, int local, int remote, bool varying)
{
   int frame;
   for (frame = 0; frame < FRAME_WINDOW_SIZE; ++frame) {
      char value = varying ? (char)(frame % 2) : 0;
      GameInput input;
      input.init(frame, &value, 1);
      sync->advance_frame(input, local, remote);
   }
}

int main()
{
   TimeSync balanced;
   TimeSync moderate;
   TimeSync below_threshold;
   TimeSync capped;
   TimeSync active_input;

   CheckEqual(balanced.recommend_frame_wait_duration(false), 0,
              "balanced peers do not wait");

   FillWindow(&moderate, 0, 8, false);
   CheckEqual(moderate.recommend_frame_wait_duration(false), 4,
              "agreed eight-frame difference recommends four frames");
   CheckEqual(moderate.recommend_frame_wait_duration(true), 4,
              "idle input permits the recommendation");

   FillWindow(&below_threshold, 0, 4, false);
   CheckEqual(below_threshold.recommend_frame_wait_duration(false), 0,
              "small difference remains below the correction threshold");

   FillWindow(&capped, 0, 20, false);
   CheckEqual(capped.recommend_frame_wait_duration(false), MAX_FRAME_ADVANTAGE,
              "large difference is capped");

   FillWindow(&active_input, 0, 8, true);
   CheckEqual(active_input.recommend_frame_wait_duration(true), 0,
              "changing input suppresses an idle-only wait");
   return failures == 0 ? 0 : 1;
}
