#include "matchmade.h"
#include "sdl_host.h"
#include "sdl_renderer.h"
#include "sdl_start_screen.h"
#include "vectorwar.h"
#include "vw_start_form.h"

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

/* Matchmaking from the start screen: progress and Cancel while the lobby fills. */
struct StartScreenWait {
   SdlStartScreen *screen;
   bool cancelled;
};

static void StartScreenProgress(int joined, int group_size, void *context)
{
   StartScreenWait *wait = (StartScreenWait *)context;
   char text[64];
   VwStartWaitingText(joined, group_size, text, sizeof(text));
   wait->screen->SetMessage(text, false);
}

static bool StartScreenKeepGoing(void *context)
{
   StartScreenWait *wait = (StartScreenWait *)context;
   SDL_Event event;
   while (SDL_PollEvent(&event)) {
      if (wait->screen->HandleEvent(event) == SdlStartScreen::kCancel) wait->cancelled = true;
   }
   wait->screen->Draw();
   return !wait->cancelled;
}

/* Plays a matchmade session to the end and releases it. */
static int PlayMatchmade(MatchmadeSession *session, GGPOPlayer *players, int num_players,
                         unsigned short ggpo_port)
{
   SDLVectorWarHost host;
   VectorWar_Init(&host, ggpo_port, num_players, players, 0);
   MatchmadeAttachLegend(session);
   RunMainLoop(&host);
   MatchmadeReportPaths(session);
   VectorWar_Exit();
   MatchmadeAttachLegend(NULL);
   MatchmadeStop(session);
   return 0;
}

static int RunWithArgs(int argc, char **argv)
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
      /* A compact screen shows progress and offers Cancel (Esc, Back). */
      SdlStartScreen *screen = new SdlStartScreen(false);
      StartScreenWait wait = { screen, false };
      if (screen->IsReady()) {
         args.progress = StartScreenProgress;
         args.keep_going = StartScreenKeepGoing;
         args.ui_context = &wait;
         screen->SetWaiting(true);
         screen->SetMessage("Contacting the matchmaking server...", false);
         screen->Draw();
      }
      GGPOPlayer players[GGPO_MAX_PLAYERS];
      int num_players = 0, local_player = 0;
      unsigned short ggpo_port = 0;
      char error[160];
      MatchmadeSession *session = MatchmadeStart(args, players, &num_players, &ggpo_port,
                                                 &local_player, error, sizeof(error));
      if (!session) {
         fprintf(stderr, "matchmaking failed: %s\n", wait.cancelled ? "cancelled" : error);
#if defined(__ANDROID__)
         SDL_Log("matchmaking failed: %s", wait.cancelled ? "cancelled" : error);
         /* No console on Android: keep the reason on screen until Quit/Back. */
         if (screen->IsReady() && !wait.cancelled) {
            screen->SetWaiting(false);
            screen->SetMessage(error, true);
            for (bool done = false; !done;) {
               SDL_Event event;
               while (SDL_PollEvent(&event)) {
                  if (screen->HandleEvent(event) == SdlStartScreen::kQuit) done = true;
               }
               screen->Draw();
               SDL_Delay(16);
            }
         }
#endif
         delete screen;
         return 1;
      }
      delete screen;
      return PlayMatchmade(session, players, num_players, ggpo_port);
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

#if !defined(__ANDROID__)
/* No arguments: the start screen. Direct play continues exactly as the
 * command line would; matchmaking waits here, then plays. */
static int RunStartScreen(char *program)
{
   SdlStartScreen *screen = new SdlStartScreen();
   if (!screen->IsReady()) {
      delete screen;
      Syntax(program);
      return 2;
   }
   for (;;) {
      SdlStartScreen::Action action = SdlStartScreen::kNone;
      SDL_Event event;
      while (action == SdlStartScreen::kNone && SDL_PollEvent(&event)) {
         action = screen->HandleEvent(event);
      }
      if (action == SdlStartScreen::kQuit) {
         delete screen;
         return 0;
      }
      if (action == SdlStartScreen::kConnect || action == SdlStartScreen::kMatchmaking) {
         const VwStartMode mode = action == SdlStartScreen::kConnect ? kVwStartDirect : kVwStartMatchmaking;
         VwStartArgs args;
         char error[160];
         if (!VwStartFormArgs(screen->Form(), mode, &args, error, sizeof(error))) {
            screen->SetMessage(error, true);
         } else if (mode == kVwStartDirect) {
            char *argv[VW_START_MAX_ARGS + 1];
            argv[0] = program;
            for (int i = 0; i < args.argc; ++i) argv[i + 1] = args.argv[i];
            delete screen;
            return RunWithArgs(args.argc + 1, argv);
         } else {
            MatchmadeArgs matchmade;
            if (!ParseMatchmakingArgs(args.argc, args.argv, 1, &matchmade)) {
               screen->SetMessage("Check the server, players and lobby code", true);
            } else {
               StartScreenWait wait = { screen, false };
               matchmade.progress = StartScreenProgress;
               matchmade.keep_going = StartScreenKeepGoing;
               matchmade.ui_context = &wait;
               screen->SetWaiting(true);
               screen->SetMessage("Contacting the matchmaking server...", false);
               screen->Draw();
               GGPOPlayer players[GGPO_MAX_PLAYERS];
               int num_players = 0, local_player = 0;
               unsigned short ggpo_port = 0;
               MatchmadeSession *session = MatchmadeStart(matchmade, players, &num_players, &ggpo_port,
                                                          &local_player, error, sizeof(error));
               screen->SetWaiting(false);
               if (session) {
                  delete screen;
                  return PlayMatchmade(session, players, num_players, ggpo_port);
               }
               screen->SetMessage(wait.cancelled ? "Matchmaking cancelled" : error, !wait.cancelled);
            }
         }
      }
      screen->Draw();
      SDL_Delay(16);
   }
}
#endif

int main(int argc, char **argv)
{
#if !defined(__ANDROID__)
   if (argc == 1) return RunStartScreen(argv[0]);
#endif
   return RunWithArgs(argc, argv);
}
