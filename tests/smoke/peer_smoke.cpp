#include <winsock2.h>
#include <windows.h>

#include "smoke_session.h"

#include <stdio.h>
#include <stdlib.h>
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

static bool
WriteResult(const char *path, int player)
{
   FILE *output = NULL;
   if (fopen_s(&output, path, "w") != 0 || !output) {
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
   WSADATA winsock_data;
   int local_player;
   int local_port;
   int remote_port;
   int target_frames;
   DWORD start_time;

   if (argc != 6) {
      fprintf(stderr,
              "usage: %s PLAYER LOCAL_PORT REMOTE_PORT FRAMES RESULT.json\n",
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

   if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
      fprintf(stderr, "WSAStartup failed\n");
      return 1;
   }

   SmokeReset();
   callbacks = SmokeCallbacks();
   if (!CheckResult(ggpo_start_session(&g_smoke_session,
                                       &callbacks,
                                       "ggpo-m0-peer-smoke",
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
         strcpy_s(player.u.remote.ip_address,
                  sizeof(player.u.remote.ip_address),
                  "127.0.0.1");
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

   start_time = GetTickCount();
   while (g_smoke_metrics.running_events == 0) {
      ggpo_idle(g_smoke_session, 1);
      Sleep(1);
      if (GetTickCount() - start_time > 15000) {
         fprintf(stderr, "timed out waiting for GGPO running event\n");
         return 1;
      }
   }

   start_time = GetTickCount();
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
         Sleep(1);
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
      if (GetTickCount() - start_time > 30000) {
         fprintf(stderr, "timed out advancing peer session\n");
         return 1;
      }
   }

   for (int settle = 0; settle < 1000; settle++) {
      ggpo_idle(g_smoke_session, 1);
      Sleep(1);
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
   WSACleanup();
   return 0;
}
