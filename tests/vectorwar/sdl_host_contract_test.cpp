#include <SDL2/SDL.h>
#include <stdio.h>

#include "renderer.h"
#include "sdl_host.h"
#include "sdl_renderer.h"

// Linux counterpart of win32_host_contract_test.cpp. Run with
// SDL_VIDEODRIVER=dummy so CI exercises the real SDL window/renderer path
// without a display server.
int
main()
{
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
   printf("vectorwar-sdl-host: window, bounds, renderer, input, clock and diagnostics pass\n");
   return 0;
}
