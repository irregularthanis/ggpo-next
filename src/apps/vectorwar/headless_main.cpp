#include "headless_host.h"
#include "vectorwar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#include <winsock2.h>
#endif

static bool
SafeIdentity(const char *value)
{
   if (!value || !*value) return false;
   for (const unsigned char *cursor = (const unsigned char *)value;
        *cursor; ++cursor) {
      if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_' &&
          *cursor != '.') return false;
   }
   return true;
}

static bool
WriteState(const char *path, int frame, unsigned int checksum)
{
   FILE *output = NULL;
#if defined(_MSC_VER)
   fopen_s(&output, path, "w");
#else
   output = fopen(path, "w");
#endif
   if (!output) return false;
   fprintf(output, "{\"frame\":%d,\"canonical_checksum\":%u}\n",
           frame, checksum);
   fclose(output);
   return true;
}

static bool
MarkerExists(const char *path)
{
   FILE *marker = NULL;
#if defined(_MSC_VER)
   fopen_s(&marker, path, "r");
#else
   marker = fopen(path, "r");
#endif
   if (!marker) return false;
   fclose(marker);
   return true;
}

static bool
WriteResult(const char *path, int player, int frame, unsigned int checksum,
            int saves, int loads, int replay_frames, int running_events,
            int unexpected_disconnects, bool session_closed)
{
   FILE *output = NULL;
#if defined(_MSC_VER)
   fopen_s(&output, path, "w");
#else
   output = fopen(path, "w");
#endif
   if (!output) {
      fprintf(stderr, "could not open result file: %s\n", path);
      return false;
   }
   const char *scenario = getenv("GGPO_SMOKE_SCENARIO");
   const char *run_id = getenv("GGPO_SMOKE_RUN_ID");
   const char *session_id = getenv("GGPO_SMOKE_SESSION_ID");
   const char *platform = getenv("GGPO_SMOKE_PLATFORM");
   if (scenario || run_id || session_id || platform) {
      if (!SafeIdentity(scenario) || !SafeIdentity(run_id) ||
          !SafeIdentity(session_id) || !SafeIdentity(platform)) {
         fclose(output);
         return false;
      }
      fprintf(output,
              "{\"schema\":2,\"scenario\":\"%s\",\"run_id\":\"%s\","
              "\"session_id\":\"%s\",\"player\":%d,\"platform\":\"%s\","
              "\"frames\":%d,\"canonical_checksum\":%u,\"native_checksum\":%u,"
              "\"saves\":%d,\"loads\":%d,\"replay_frames\":%d,"
              "\"running_events\":%d,\"unexpected_disconnects\":%d,"
              "\"session_closed\":%s,\"exit_code\":0}\n",
              scenario, run_id, session_id, player, platform, frame, checksum,
              checksum, saves, loads, replay_frames, running_events,
              unexpected_disconnects, session_closed ? "true" : "false");
   } else {
      fprintf(output,
              "{\"player\":%d,\"frame\":%d,\"checksum\":%u,"
              "\"saves\":%d,\"loads\":%d,\"replay_frames\":%d,"
              "\"running_events\":%d}\n",
              player, frame, checksum, saves, loads, replay_frames,
              running_events);
   }
   fclose(output);
   return true;
}

int
main(int argc, char **argv)
{
   if (argc != 6 && argc != 7) {
      fprintf(stderr, "usage: %s PLAYER LOCAL_PORT REMOTE_PORT FRAMES RESULT.json [REMOTE_IP]\n", argv[0]);
      return 2;
   }
   const int local_player = atoi(argv[1]);
   const int local_port = atoi(argv[2]);
   const int remote_port = atoi(argv[3]);
   const int target_frames = atoi(argv[4]);
   const char *remote_ip = argc == 7 ? argv[6] : "127.0.0.1";
   if (local_player < 1 || local_player > 2 || local_port <= 0 ||
       remote_port <= 0 || target_frames <= 0) {
      fprintf(stderr, "invalid Vector War headless arguments\n");
      return 2;
   }

   const char *fault = getenv("GGPO_SMOKE_FAULT");
   if (fault && strcmp(fault, "startup") == 0) {
      fprintf(stderr, "fault: startup\n");
      return 91;
   }
   if (fault && strcmp(fault, "mismatch") == 0) {
      return WriteResult(argv[5], local_player, target_frames,
                         (unsigned int)local_player, 1, 0, 1, 1, 0, false) ? 0 : 1;
   }
   if (fault && strcmp(fault, "no_rollback") == 0) {
      return WriteResult(argv[5], local_player, target_frames,
                         0xB3381958U, 1, 0, 0, 1, 0, false) ? 0 : 1;
   }
   if (fault && strcmp(fault, "shutdown") == 0) {
      fprintf(stderr, "fault: shutdown\n");
      return 92;
   }

#if defined(_WIN32)
   WSADATA winsock_data;
   if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
      fprintf(stderr, "WSAStartup failed\n");
      return 1;
   }
#endif

   HeadlessVectorWarHost host(local_player);
   if (fault && strcmp(fault, "timeout") == 0) {
      host.SleepMs(60000);
      return 93;
   }
   GGPOPlayer players[2];
   memset(players, 0, sizeof(players));
   for (int index = 0; index < 2; ++index) {
      players[index].size = sizeof(players[index]);
      players[index].player_num = index + 1;
      if (index + 1 == local_player) {
         players[index].type = GGPO_PLAYERTYPE_LOCAL;
      } else {
         players[index].type = GGPO_PLAYERTYPE_REMOTE;
#if defined(_MSC_VER)
         strcpy_s(players[index].u.remote.ip_address,
                  sizeof(players[index].u.remote.ip_address), remote_ip);
#else
         snprintf(players[index].u.remote.ip_address,
                  sizeof(players[index].u.remote.ip_address), "%s", remote_ip);
#endif
         players[index].u.remote.port = (unsigned short)remote_port;
      }
   }

   VectorWar_Init(&host, (unsigned short)local_port, 2, players, 0);
   const unsigned int start = host.NowMs();
   while (VectorWar_RunningEventCount() == 0 && host.NowMs() - start < 15000) {
      VectorWar_Idle(1);
      host.SleepMs(1);
   }
   /* Let player 1 predict several frames before player 2 starts submitting
    * authoritative input. This makes rollback part of the fixture contract. */
   if (local_player == 2) {
      host.SleepMs(50);
   }
   while (VectorWar_FrameNumber() < target_frames && host.NowMs() - start < 45000) {
      VectorWar_RunFrame();
      VectorWar_Idle(1);
      host.SleepMs(1);
   }
   if (VectorWar_FrameNumber() != target_frames ||
       VectorWar_RunningEventCount() != 1) {
      fprintf(stderr, "incomplete Vector War session: frame=%d running=%d\n",
              VectorWar_FrameNumber(), VectorWar_RunningEventCount());
      VectorWar_Exit();
#if defined(_WIN32)
      WSACleanup();
#endif
      return 1;
   }

   const char *ready_path = getenv("GGPO_SMOKE_READY_FILE");
   const char *finalize_path = getenv("GGPO_SMOKE_FINALIZE_FILE");
   if (ready_path || finalize_path) {
      if (!ready_path || !finalize_path) {
         fprintf(stderr, "incomplete completion-barrier configuration\n");
         return 1;
      }
      const unsigned int barrier_start = host.NowMs();
      while (!MarkerExists(finalize_path)) {
         VectorWar_Idle(1);
         if (!WriteState(ready_path, VectorWar_FrameNumber(),
                         VectorWar_StateChecksum())) return 1;
         host.SleepMs(1);
         if (host.NowMs() - barrier_start > 15000) {
            fprintf(stderr, "timed out waiting for completion barrier\n");
            return 1;
         }
      }
   } else {
      /* Local loopback has no coordinator, so give late authoritative input
       * time to complete rollback before collecting either result. */
      for (int settle = 0; settle < 1000; ++settle) {
         VectorWar_Idle(1);
         host.SleepMs(1);
      }
   }

   const int frame = VectorWar_FrameNumber();
   const unsigned int checksum = VectorWar_StateChecksum();
   const int saves = VectorWar_SaveCount();
   const int loads = VectorWar_LoadCount();
   int replay_frames = VectorWar_ReplayFrameCount();
   const int running_events = VectorWar_RunningEventCount();
   const int unexpected_disconnects = VectorWar_UnexpectedDisconnectCount();
   VectorWar_Exit();
#if defined(_WIN32)
   WSACleanup();
#endif
   if (!WriteResult(argv[5], local_player, frame, checksum, saves, loads,
                    replay_frames, running_events, unexpected_disconnects,
                    true)) {
      return 1;
   }
   printf("vectorwar-headless: player=%d frame=%d checksum=%08x loads=%d replay=%d\n",
          local_player, frame, checksum, loads, replay_frames);
   return 0;
}
