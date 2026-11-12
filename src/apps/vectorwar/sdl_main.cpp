#include "matchmade.h"
#include "sdl_host.h"
#include "sdl_renderer.h"
#include "vectorwar.h"

#include "vw_sdl.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void Syntax(const char *program)
{
   fprintf(stderr,
           "usage: %s LOCAL_PORT NUM_PLAYERS (local | IP:PORT)*\n"
           "       %s --matchmaking HOST[:PORT] PLAYERS [LOBBY] LOCAL_PORT [--relay-only]\n",
           program, program);
}

static bool ParseRemote(const char *argument, GGPOPlayer *player)
{
   char address[128];
   unsigned short port = 0;
   if (sscanf(argument, "%127[^:]:%hu", address, &port) != 2) return false;
   if (strlen(address) >= sizeof(player->u.remote.ip_address)) return false;
   snprintf(player->u.remote.ip_address,
            sizeof(player->u.remote.ip_address), "%s", address);
   player->u.remote.port = port;
   return true;
}

static void RunMainLoop(SDLVectorWarHost *host)
{
   unsigned int next = host->NowMs();
   bool running = true;
   while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
         if (event.type == SDL_QUIT) running = false;
         if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
#if defined(__ANDROID__)
            if (event.key.keysym.sym == SDLK_AC_BACK) running = false;
#endif
            if (event.key.keysym.sym == SDLK_p) VectorWar_TogglePerformanceMonitor();
            if (event.key.keysym.sym >= SDLK_F1 && event.key.keysym.sym <= SDLK_F12) {
               VectorWar_DisconnectPlayer((int)(event.key.keysym.sym - SDLK_F1));
            }
         }
      }
      const unsigned int now = host->NowMs();
      VectorWar_Idle((std::max)(0, (int)(next - now - 1)));
      if (now >= next) {
         VectorWar_RunFrame();
         next = now + (1000 / 60);
      }
   }
}

int main(int argc, char **argv)
{
   if (argc == 2 && strcmp(argv[1], "--frontend-smoke") == 0) {
      SDLVectorWarRenderer renderer;
      if (!renderer.IsReady()) return 1;
      renderer.SetStatusText("Vector War SDL frontend smoke");
#if defined(__ANDROID__)
      // Read by CI from logcat; there is no console on Android.
      SDL_Log("Vector War SDL frontend smoke: OK");
#endif
      return 0;
   }
   if (argc >= 2 && strcmp(argv[1], "--matchmaking") == 0) {
      /* Lobby, relay and hole punching with relay fallback. */
      MatchmadeArgs args;
      if (!ParseMatchmakingArgs(argc, argv, 2, &args)) {
         Syntax(argv[0]);
         return 2;
      }
      GGPOPlayer players[GGPO_MAX_PLAYERS];
      int num_players = 0, local_player = 0;
      unsigned short ggpo_port = 0;
      char error[160];
      MatchmadeSession *session = MatchmadeStart(args, players, &num_players, &ggpo_port,
                                                 &local_player, error, sizeof(error));
      if (!session) {
         fprintf(stderr, "matchmaking failed: %s\n", error);
#if defined(__ANDROID__)
         SDL_Log("matchmaking failed: %s", error);
#endif
         return 1;
      }
      SDLVectorWarHost host;
      VectorWar_Init(&host, ggpo_port, num_players, players, 0);
      RunMainLoop(&host);
      MatchmadeReportPaths(session);
      VectorWar_Exit();
      MatchmadeStop(session);
      return 0;
   }
   if (argc < 4) {
      Syntax(argv[0]);
      return 2;
   }
   const unsigned short local_port = (unsigned short)atoi(argv[1]);
   const int num_players = atoi(argv[2]);
   if (!local_port || num_players < 1 || num_players > GGPO_MAX_PLAYERS) {
      Syntax(argv[0]);
      return 2;
   }

   SDLVectorWarHost host;
   if (argc == 5 && strcasecmp(argv[3], "spectate") == 0) {
      GGPOPlayer remote;
      memset(&remote, 0, sizeof(remote));
      if (!ParseRemote(argv[4], &remote)) {
         Syntax(argv[0]);
         return 2;
      }
      VectorWar_InitSpectator(&host, local_port, num_players,
                              remote.u.remote.ip_address,
                              remote.u.remote.port);
      RunMainLoop(&host);
      VectorWar_Exit();
      return 0;
   }
   if (argc < 3 + num_players) {
      Syntax(argv[0]);
      return 2;
   }

   GGPOPlayer players[GGPO_MAX_PLAYERS + GGPO_MAX_SPECTATORS];
   memset(players, 0, sizeof(players));
   int offset = 3;
   int index = 0;
   for (; index < num_players; ++index) {
      players[index].size = sizeof(players[index]);
      players[index].player_num = index + 1;
      if (strcasecmp(argv[offset], "local") == 0) {
         players[index].type = GGPO_PLAYERTYPE_LOCAL;
      } else {
         players[index].type = GGPO_PLAYERTYPE_REMOTE;
         if (!ParseRemote(argv[offset], &players[index])) {
            Syntax(argv[0]);
            return 2;
         }
      }
      ++offset;
   }
   const int num_spectators = argc - offset;
   for (; offset < argc; ++offset, ++index) {
      players[index].size = sizeof(players[index]);
      players[index].type = GGPO_PLAYERTYPE_SPECTATOR;
      if (!ParseRemote(argv[offset], &players[index])) {
         Syntax(argv[0]);
         return 2;
      }
   }

   VectorWar_Init(&host, local_port, num_players, players, num_spectators);
   RunMainLoop(&host);
   VectorWar_Exit();
   return 0;
}
