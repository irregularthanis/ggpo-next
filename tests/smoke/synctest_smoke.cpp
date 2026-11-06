#include "smoke_session.h"

#include <stdio.h>
#include <string.h>

static int
InputForPlayer(int player, int frame)
{
   return ((frame + 1) * (player * 17 + 3)) ^ (frame << player);
}

static bool
CheckResult(GGPOErrorCode result, const char *operation)
{
   if (!GGPO_SUCCEEDED(result)) {
      fprintf(stderr, "%s failed with GGPO error %d\n", operation, result);
      return false;
   }
   return true;
}

int
main(void)
{
   const int frames = 90;
   GGPOSessionCallbacks callbacks;
   GGPOPlayerHandle handles[2];
   SmokeState expected = { 0, 0, 0 };
   char game_name[] = "ggpo-m0-synctest";
   int frame;

   SmokeReset();
   callbacks = SmokeCallbacks();
   if (!CheckResult(ggpo_start_synctest(&g_smoke_session,
                                        &callbacks,
                                        game_name,
                                        2,
                                        (int)sizeof(int),
                                        1),
                    "ggpo_start_synctest")) {
      return 1;
   }

   for (int player_index = 0; player_index < 2; player_index++) {
      GGPOPlayer player;
      memset(&player, 0, sizeof(player));
      player.size = sizeof(player);
      player.type = GGPO_PLAYERTYPE_LOCAL;
      player.player_num = player_index + 1;
      if (!CheckResult(ggpo_add_player(g_smoke_session,
                                       &player,
                                       &handles[player_index]),
                       "ggpo_add_player")) {
         return 1;
      }
   }

   if (!CheckResult(ggpo_idle(g_smoke_session, 0), "ggpo_idle")) {
      return 1;
   }

   for (frame = 0; frame < frames; frame++) {
      int inputs[2];
      int synchronized_inputs[2] = { 0, 0 };
      int disconnect_flags = 0;

      inputs[0] = InputForPlayer(1, frame);
      inputs[1] = InputForPlayer(2, frame);
      if (!CheckResult(ggpo_add_local_input(g_smoke_session,
                                             handles[0],
                                             &inputs[0],
                                             (int)sizeof(inputs[0])),
                       "ggpo_add_local_input(player 1)") ||
          !CheckResult(ggpo_add_local_input(g_smoke_session,
                                             handles[1],
                                             &inputs[1],
                                             (int)sizeof(inputs[1])),
                       "ggpo_add_local_input(player 2)") ||
          !CheckResult(ggpo_synchronize_input(g_smoke_session,
                                               synchronized_inputs,
                                               (int)sizeof(synchronized_inputs),
                                               &disconnect_flags),
                       "ggpo_synchronize_input")) {
         return 1;
      }

      SmokeApplyInputs(&expected, inputs, 0);
      SmokeApplyInputs(&g_smoke_state, synchronized_inputs, disconnect_flags);
      if (!CheckResult(ggpo_advance_frame(g_smoke_session),
                       "ggpo_advance_frame")) {
         return 1;
      }
   }

   if (memcmp(&g_smoke_state, &expected, sizeof(expected)) != 0) {
      fprintf(stderr,
              "final state mismatch: frame=%d/%d checksum=%08x/%08x\n",
              g_smoke_state.frame,
              expected.frame,
              SmokeChecksum(&g_smoke_state),
              SmokeChecksum(&expected));
      return 1;
   }
   if (g_smoke_metrics.loads == 0 || g_smoke_metrics.replay_frames == 0 ||
       g_smoke_metrics.running_events != 1) {
      fprintf(stderr,
              "rollback path not exercised: loads=%d replay=%d running=%d\n",
              g_smoke_metrics.loads,
              g_smoke_metrics.replay_frames,
              g_smoke_metrics.running_events);
      return 1;
   }

   printf("synctest: frames=%d checksum=%08x saves=%d loads=%d replay=%d\n",
          g_smoke_state.frame,
          SmokeChecksum(&g_smoke_state),
          g_smoke_metrics.saves,
          g_smoke_metrics.loads,
          g_smoke_metrics.replay_frames);

   if (!CheckResult(ggpo_close_session(g_smoke_session),
                    "ggpo_close_session")) {
      return 1;
   }
   g_smoke_session = NULL;
   return 0;
}
