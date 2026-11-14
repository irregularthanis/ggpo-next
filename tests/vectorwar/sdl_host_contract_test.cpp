// This executable supplies its own main(). Without this, SDL's Android
// headers rename main to SDL_main for SDLActivity and the link fails.
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "renderer.h"
#include "sdl_host.h"
#include "sdl_renderer.h"
#include "sdl_start_screen.h"

// gamestate.cpp refers to the core's session; this test does not link the core.
GGPOSession *ggpo = NULL;
#include "vw_player_colors.h"

// Linux and Android counterpart of win32_host_contract_test.cpp. Run with
// SDL_VIDEODRIVER=dummy so CI exercises the real SDL window/renderer path
// without a display server.

static SDL_Event Click(const SDL_Rect &rect)
{
   SDL_Event event;
   SDL_zero(event);
   event.type = SDL_MOUSEBUTTONDOWN;
   event.button.button = SDL_BUTTON_LEFT;
   event.button.x = rect.x + rect.w / 2;
   event.button.y = rect.y + rect.h / 2;
   return event;
}

static SDL_Event Key(SDL_Keycode key)
{
   SDL_Event event;
   SDL_zero(event);
   event.type = SDL_KEYDOWN;
   event.key.keysym.sym = key;
   return event;
}

static void Type(SdlStartScreen *screen, const char *text)
{
   SDL_Event event;
   SDL_zero(event);
   event.type = SDL_TEXTINPUT;
   snprintf(event.text.text, sizeof(event.text.text), "%s", text);
   screen->HandleEvent(event);
}

static bool ArgsAre(SdlStartScreen *screen, VwStartMode mode, const char *expected)
{
   VwStartArgs args;
   char error[128] = "";
   char joined[256] = "";
   if (!VwStartFormArgs(screen->Form(), mode, &args, error, sizeof(error))) {
      snprintf(joined, sizeof(joined), "error: %s", error);
   }
   for (int i = 0; i < args.argc; ++i) {
      if (i) strcat(joined, " ");
      strcat(joined, args.argv[i]);
   }
   if (strcmp(joined, expected) != 0) {
      fprintf(stderr, "start screen gave \"%s\", expected \"%s\"\n", joined, expected);
      return false;
   }
   return true;
}

// The start screen: typing, focus, buttons, and the arguments they produce.
static bool StartScreenContract()
{
   SdlStartScreen *screen = new SdlStartScreen();
   bool ok = screen->IsReady();
   if (!ok) fprintf(stderr, "start screen not ready: %s\n", SDL_GetError());
   // Direct: the remote field has focus first.
   ok = ok && screen->FocusedField() == SdlStartScreen::kRemote;
   Type(screen, "192.168.1.20:7001");
   ok = ok && ArgsAre(screen, kVwStartDirect, "7000 2 local 192.168.1.20:7001");
   SDL_Event click = Click(SdlStartScreen::FieldRect(SdlStartScreen::kPlayerNumber));
   screen->HandleEvent(click);
   SDL_Event backspace = Key(SDLK_BACKSPACE);
   screen->HandleEvent(backspace);
   Type(screen, "2");
   ok = ok && ArgsAre(screen, kVwStartDirect, "7000 2 192.168.1.20:7001 local");
   click = Click(SdlStartScreen::ButtonRect(SdlStartScreen::kConnectButton));
   ok = ok && screen->HandleEvent(click) == SdlStartScreen::kConnect;

   // Matchmaking: Tab to the server, type, Enter starts matchmaking.
   click = Click(SdlStartScreen::FieldRect(SdlStartScreen::kRemote));
   screen->HandleEvent(click);
   SDL_Event tab = Key(SDLK_TAB);
   screen->HandleEvent(tab);
   ok = ok && screen->FocusedField() == SdlStartScreen::kServer;
   Type(screen, "vw.example.org");
   screen->HandleEvent(tab);
   screen->HandleEvent(tab);
   Type(screen, "abc12");
   ok = ok && ArgsAre(screen, kVwStartMatchmaking, "--matchmaking vw.example.org 2 ABC12 7000");
   SDL_Event enter = Key(SDLK_RETURN);
   ok = ok && screen->HandleEvent(enter) == SdlStartScreen::kMatchmaking;
   screen->Draw();

   // While waiting only Cancel works; Esc cancels too.
   screen->SetWaiting(true);
   click = Click(SdlStartScreen::ButtonRect(SdlStartScreen::kQuitButton));
   ok = ok && screen->HandleEvent(click) == SdlStartScreen::kNone;
   click = Click(SdlStartScreen::ButtonRect(SdlStartScreen::kCancelButton));
   ok = ok && screen->HandleEvent(click) == SdlStartScreen::kCancel;
   SDL_Event escape = Key(SDLK_ESCAPE);
   ok = ok && screen->HandleEvent(escape) == SdlStartScreen::kCancel;
   screen->SetWaiting(false);
   ok = ok && screen->HandleEvent(escape) == SdlStartScreen::kQuit;
   delete screen;
   if (!ok) fprintf(stderr, "SDL start screen contract failed\n");
   return ok;
}

int
main()
{
#if defined(__ANDROID__)
   // A plain executable has no SDLActivity: declare main ready ourselves, and
   // select the dummy driver here because the adb launcher forwards only
   // GGPO_* variables.
   SDL_SetMainReady();
   SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
#endif
   if (!StartScreenContract()) return 1;

   SDLVectorWarHost host;
   const VectorWarBounds bounds = host.GetGameBounds();
   if (bounds.left != 0 || bounds.top != 0 ||
       bounds.right != 640 || bounds.bottom != 480) {
      fprintf(stderr, "SDL host did not report the 640x480 arena\n");
      return 1;
   }

   Renderer *renderer = host.CreateRenderer();
   if (!renderer || !static_cast<SDLVectorWarRenderer *>(renderer)->IsReady()) {
      fprintf(stderr, "SDL host did not create a ready renderer: %s\n",
              SDL_GetError());
      delete renderer;
      return 1;
   }
   renderer->SetStatusText("Vector War SDL host contract");

   // Legend: one row per player, its swatch in that player's ship color.
   GameState state;
   state.Init(bounds, 2);
   static NonGameState non_game_state;
   memset(&non_game_state, 0, sizeof(non_game_state));
   VwLegendInitDirect(&non_game_state.legend, 2, 1);
   non_game_state.legend.platforms[1] = kVwPlatformWindows;
   non_game_state.legend.routes[1] = kVwRouteDirect;
   renderer->Draw(state, non_game_state);
   SDL_Renderer *sdl = static_cast<SDLVectorWarRenderer *>(renderer)->Handle();
   for (int player = 0; player < 2; ++player) {
      SDL_Rect pixel = { 16, 28 + player * 12 + 4, 1, 1 };
      Uint8 rgba[4] = { 0, 0, 0, 0 };
      if (SDL_RenderReadPixels(sdl, &pixel, SDL_PIXELFORMAT_ABGR8888, rgba, 4) != 0) {
         fprintf(stderr, "SDL legend readback failed: %s\n", SDL_GetError());
         delete renderer;
         return 1;
      }
      const unsigned char *color = VwPlayerColor(kVwColorsSdl, player);
      if (rgba[0] != color[0] || rgba[1] != color[1] || rgba[2] != color[2]) {
         fprintf(stderr, "SDL legend swatch %d is %d,%d,%d, expected %d,%d,%d\n", player + 1,
                 rgba[0], rgba[1], rgba[2], color[0], color[1], color[2]);
         delete renderer;
         return 1;
      }
   }

   // No keys are pressed under the dummy driver, so input must be neutral.
   if (host.ReadLocalInput(0) != 0) {
      fprintf(stderr, "SDL host reported input with no keys pressed\n");
      delete renderer;
      return 1;
   }

   const unsigned int before = host.NowMs();
   host.SleepMs(2);
   if (host.NowMs() < before) {
      fprintf(stderr, "SDL host clock moved backwards\n");
      delete renderer;
      return 1;
   }
   host.InitializePerformanceMonitor();
   host.TogglePerformanceMonitor();
   host.ShutdownPerformanceMonitor();
   delete renderer;
   printf("vectorwar-sdl-host: window, bounds, renderer, legend, start screen, input, clock and diagnostics pass\n");
   return 0;
}
