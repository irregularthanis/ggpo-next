// This executable supplies its own main(). Without this, SDL's Android
// headers rename main to SDL_main for SDLActivity and the link fails.
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "renderer.h"
#include "sdl_host.h"
#include "sdl_renderer.h"
#include "vw_player_colors.h"

// Linux and Android counterpart of win32_host_contract_test.cpp. Run with
// SDL_VIDEODRIVER=dummy so CI exercises the real SDL window/renderer path
// without a display server.
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
   printf("vectorwar-sdl-host: window, bounds, renderer, legend, input, clock and diagnostics pass\n");
   return 0;
}
