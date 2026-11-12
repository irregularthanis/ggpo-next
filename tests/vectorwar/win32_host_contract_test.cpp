#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "renderer.h"
#include "win32_host.h"

int
main()
{
   const wchar_t *class_name = L"GGPOVectorWarHostContract";
   WNDCLASSW window_class = { 0 };
   window_class.lpfnWndProc = DefWindowProcW;
   window_class.hInstance = GetModuleHandle(NULL);
   window_class.lpszClassName = class_name;
   if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      fprintf(stderr, "could not register test window\n");
      return 1;
   }
   HWND window = CreateWindowW(class_name, L"Vector War host contract",
                               WS_OVERLAPPEDWINDOW, 0, 0, 640, 480,
                               NULL, NULL, window_class.hInstance, NULL);
   if (!window) {
      fprintf(stderr, "could not create test window\n");
      return 1;
   }

   RECT client = { 0 };
   GetClientRect(window, &client);
   Win32VectorWarHost host(window);
   const VectorWarBounds bounds = host.GetGameBounds();
   if (bounds.left != client.left || bounds.top != client.top ||
       bounds.right != client.right || bounds.bottom != client.bottom) {
      fprintf(stderr, "Win32 host did not preserve client bounds\n");
      DestroyWindow(window);
      return 1;
   }
   Renderer *renderer = host.CreateRenderer();
   if (!renderer) {
      fprintf(stderr, "Win32 host did not create its renderer\n");
      DestroyWindow(window);
      return 1;
   }
   // Draw one frame with a legend through the GDI path.
   GameState state;
   state.Init(bounds, 2);
   static NonGameState non_game_state;
   memset(&non_game_state, 0, sizeof(non_game_state));
   VwLegendInitDirect(&non_game_state.legend, 2, 1);
   non_game_state.legend.platforms[1] = kVwPlatformAndroid;
   non_game_state.legend.routes[1] = kVwRouteRelay;
   renderer->Draw(state, non_game_state);
   delete renderer;

   const unsigned int before = host.NowMs();
   host.SleepMs(2);
   if (host.NowMs() < before) {
      fprintf(stderr, "Win32 host clock moved backwards\n");
      DestroyWindow(window);
      return 1;
   }
   host.InitializePerformanceMonitor();
   host.ShutdownPerformanceMonitor();
   DestroyWindow(window);
   printf("vectorwar-win32-host: window, bounds, renderer, legend, clock and diagnostics pass\n");
   return 0;
}
