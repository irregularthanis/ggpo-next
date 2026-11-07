#include "smoke_session.h"
#include "types.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// One instance represents one player in the loopback test.  The Python
// orchestrator launches two instances with opposite local/remote ports, then
// compares their result files after real Winsock synchronization and rollback.
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

static bool
WriteResult(const char *path, int player)
{
   FILE *output = fopen(path, "w");
   if (!output) {
      fprintf(stderr, "could not open result file: %s\n", path);
      return false;
   }
   fprintf(output,
           "{\"player\":%d,\"frame\":%d,\"checksum\":%u,"
           "\"saves\":%d,\"loads\":%d,\"replay_frames\":%d,"
           "\"running_events\":%d}\n",
           player,
           g_smoke_state.frame,
           SmokeChecksum(&g_smoke_state),
           g_smoke_metrics.saves,
           g_smoke_metrics.loads,
           g_smoke_metrics.replay_frames,
           g_smoke_metrics.running_events);
   fclose(output);
   return true;
}

int
main(int argc, char **argv)
{
   GGPOSessionCallbacks callbacks;
   GGPOPlayerHandle local_handle = GGPO_INVALID_HANDLE;
   int local_player;
   int local_port;
   int remote_port;
   int target_frames;
   uint32 start_time;

   if (argc != 6 && argc != 7) {
      fprintf(stderr,
              "usage: %s PLAYER LOCAL_PORT REMOTE_PORT FRAMES RESULT.json "
              "[REMOTE_IP]\n",
              argv[0]);
      return 2;
   }
   local_player = atoi(argv[1]);
   local_port = atoi(argv[2]);
   remote_port = atoi(argv[3]);
   target_frames = atoi(argv[4]);
   if (local_player < 1 || local_player > 2 || local_port <= 0 ||
       remote_port <= 0 || target_frames <= 0) {
      fprintf(stderr, "invalid peer-smoke arguments\n");
      return 2;
   }

   SmokeReset();
   callbacks = SmokeCallbacks();
   if (!CheckResult(ggpo_start_session(&g_smoke_session,
                                       &callbacks,
                                       "ggpo-peer-smoke",
                                       2,
                                       (int)sizeof(int),
                                       (unsigned short)local_port),
                    "ggpo_start_session")) {
      return 1;
   }

   for (int player_number = 1; player_number <= 2; player_number++) {
      GGPOPlayer player;
      GGPOPlayerHandle handle;
      memset(&player, 0, sizeof(player));
      player.size = sizeof(player);
      player.player_num = player_number;
      if (player_number == local_player) {
         player.type = GGPO_PLAYERTYPE_LOCAL;
      } else {
         player.type = GGPO_PLAYERTYPE_REMOTE;
         const char *remote_ip = argc == 7 ? argv[6] : "127.0.0.1";
         snprintf(player.u.remote.ip_address,
                  sizeof(player.u.remote.ip_address), "%s", remote_ip);
         player.u.remote.port = (unsigned short)remote_port;
      }
      if (!CheckResult(ggpo_add_player(g_smoke_session, &player, &handle),
                       "ggpo_add_player")) {
         return 1;
      }
      if (player_number == local_player) {
         local_handle = handle;
      }
   }

   // Do not submit inputs until both GGPO peers complete their handshake.
   start_time = ggpo_platform::MonotonicTimeMs();
   while (g_smoke_metrics.running_events == 0) {
      ggpo_idle(g_smoke_session, 1);
      ggpo_platform::WaitMs(1);
      if (ggpo_platform::MonotonicTimeMs() - start_time > 15000) {
         fprintf(stderr, "timed out waiting for GGPO running event\n");
         return 1;
      }
   }

   // Prediction-threshold and not-synchronized responses are normal while the
   // remote process catches up; pump GGPO and retry the same logical frame.
   start_time = ggpo_platform::MonotonicTimeMs();
   while (g_smoke_state.frame < target_frames) {
      int local_input = InputForPlayer(local_player, g_smoke_state.frame);
      int synchronized_inputs[2] = { 0, 0 };
      int disconnect_flags = 0;
      GGPOErrorCode result = ggpo_add_local_input(g_smoke_session,
                                                   local_handle,
                                                   &local_input,
                                                   (int)sizeof(local_input));
      if (result == GGPO_ERRORCODE_PREDICTION_THRESHOLD ||
          result == GGPO_ERRORCODE_NOT_SYNCHRONIZED) {
         ggpo_idle(g_smoke_session, 1);
         ggpo_platform::WaitMs(1);
         continue;
      }
      if (!CheckResult(result, "ggpo_add_local_input") ||
          !CheckResult(ggpo_synchronize_input(g_smoke_session,
                                               synchronized_inputs,
                                               (int)sizeof(synchronized_inputs),
                                               &disconnect_flags),
                       "ggpo_synchronize_input")) {
         return 1;
      }
      SmokeApplyInputs(&g_smoke_state,
                       synchronized_inputs,
                       disconnect_flags);
      if (!CheckResult(ggpo_advance_frame(g_smoke_session),
                       "ggpo_advance_frame")) {
         return 1;
      }
      ggpo_idle(g_smoke_session, 0);
      if (ggpo_platform::MonotonicTimeMs() - start_time > 30000) {
         fprintf(stderr, "timed out advancing peer session\n");
         return 1;
      }
   }

   // Give late packets and callbacks a bounded drain window before recording
   // the result and closing the session.
   for (int settle = 0; settle < 1000; settle++) {
      ggpo_idle(g_smoke_session, 1);
      ggpo_platform::WaitMs(1);
   }

   if (!WriteResult(argv[5], local_player)) {
      return 1;
   }
   printf("peer-smoke: player=%d frame=%d checksum=%08x loads=%d replay=%d\n",
          local_player,
          g_smoke_state.frame,
          SmokeChecksum(&g_smoke_state),
          g_smoke_metrics.loads,
          g_smoke_metrics.replay_frames);

   if (!CheckResult(ggpo_close_session(g_smoke_session),
                    "ggpo_close_session")) {
      return 1;
   }
   g_smoke_session = NULL;
   return 0;
}
