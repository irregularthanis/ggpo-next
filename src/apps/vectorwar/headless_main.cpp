#include "headless_host.h"
#include "vectorwar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#endif

static bool
WriteResult(const char *path, int player, int frame, unsigned int checksum,
            int saves, int loads, int replay_frames, int running_events)
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
   fprintf(output,
           "{\"player\":%d,\"frame\":%d,\"checksum\":%u,"
           "\"saves\":%d,\"loads\":%d,\"replay_frames\":%d,"
           "\"running_events\":%d}\n",
           player, frame, checksum, saves, loads, replay_frames, running_events);
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
                         (unsigned int)local_player, 1, 0, 1, 1) ? 0 : 1;
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

   /* Keep pumping both peers so late authoritative inputs can complete any
    * rollback before the independent result files are compared. */
   for (int settle = 0; settle < 1000; ++settle) {
      VectorWar_Idle(1);
      host.SleepMs(1);
   }

   const int frame = VectorWar_FrameNumber();
   const unsigned int checksum = VectorWar_StateChecksum();
   const int saves = VectorWar_SaveCount();
   const int loads = VectorWar_LoadCount();
   int replay_frames = VectorWar_ReplayFrameCount();
   const int running_events = VectorWar_RunningEventCount();
   if (fault && strcmp(fault, "no_rollback") == 0) {
      replay_frames = 0;
   }
   VectorWar_Exit();
#if defined(_WIN32)
   WSACleanup();
#endif
   if (fault && strcmp(fault, "shutdown") == 0) {
      fprintf(stderr, "fault: shutdown\n");
      return 92;
   }
   if (!WriteResult(argv[5], local_player, frame, checksum, saves, loads,
                    replay_frames, running_events)) {
      return 1;
   }
   printf("vectorwar-headless: player=%d frame=%d checksum=%08x loads=%d replay=%d\n",
          local_player, frame, checksum, loads, replay_frames);
   return 0;
}
