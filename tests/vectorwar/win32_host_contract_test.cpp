#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "renderer.h"
#include "win32_host.h"
#include "win32_start_dialog.h"
#include "Resource.h"

// gamestate.cpp refers to the core's session; this test does not link the core.
GGPOSession *ggpo = NULL;

static bool ArgsAre(Win32StartDialog &dialog, VwStartMode mode, const char *expected)
{
   VwStartForm form;
   dialog.ReadForm(&form);
   VwStartArgs args;
   char error[128] = "";
   char joined[256] = "";
   if (!VwStartFormArgs(form, mode, &args, error, sizeof(error))) {
      sprintf_s(joined, sizeof(joined), "error: %s", error);
   }
   for (int i = 0; i < args.argc; ++i) {
      if (i) strcat_s(joined, sizeof(joined), " ");
      strcat_s(joined, sizeof(joined), args.argv[i]);
   }
   if (strcmp(joined, expected) != 0) {
      fprintf(stderr, "start dialog gave \"%s\", expected \"%s\"\n", joined, expected);
      return false;
   }
   return true;
}

static Win32StartDialog::Action Click(Win32StartDialog &dialog, int id)
{
   SendMessageA(dialog.Handle(), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED),
                (LPARAM)GetDlgItem(dialog.Handle(), id));
   return dialog.Pump(0);
}

// The start dialog from VectorWar.rc: defaults, fields to arguments, buttons.
static bool StartDialogContract()
{
   Win32StartDialog dialog(GetModuleHandle(NULL));
   if (!dialog.IsReady()) {
      fprintf(stderr, "start dialog not created (%lu)\n", GetLastError());
      return false;
   }
   bool ok = true;
   SetDlgItemTextA(dialog.Handle(), IDC_START_REMOTE, "192.168.1.20:7001");
   ok = ok && ArgsAre(dialog, kVwStartDirect, "7000 2 local 192.168.1.20:7001");
   SetDlgItemTextA(dialog.Handle(), IDC_START_PLAYER_NUMBER, "2");
   ok = ok && ArgsAre(dialog, kVwStartDirect, "7000 2 192.168.1.20:7001 local");
   ok = ok && Click(dialog, IDC_START_CONNECT) == Win32StartDialog::kConnect;

   SetDlgItemTextA(dialog.Handle(), IDC_START_SERVER, "vw.example.org");
   SetDlgItemTextA(dialog.Handle(), IDC_START_PLAYERS, "3");
   SetDlgItemTextA(dialog.Handle(), IDC_START_LOBBY, "abc12");
   ok = ok && ArgsAre(dialog, kVwStartMatchmaking, "--matchmaking vw.example.org 3 ABC12 7000");
   ok = ok && Click(dialog, IDC_START_MATCHMAKING) == Win32StartDialog::kMatchmaking;

   // While waiting the start buttons do nothing and Quit becomes Cancel.
   dialog.SetWaiting(true);
   char label[16] = "";
   GetDlgItemTextA(dialog.Handle(), IDCANCEL, label, sizeof(label));
   ok = ok && strcmp(label, "Cancel") == 0;
   ok = ok && Click(dialog, IDC_START_CONNECT) == Win32StartDialog::kNone;
   ok = ok && Click(dialog, IDCANCEL) == Win32StartDialog::kCancel;
   dialog.SetWaiting(false);
   ok = ok && Click(dialog, IDCANCEL) == Win32StartDialog::kQuit;
   if (!ok) fprintf(stderr, "Win32 start dialog contract failed\n");
   return ok;
}

int
main()
{
   if (!StartDialogContract()) return 1;

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
   printf("vectorwar-win32-host: window, start dialog, bounds, renderer, legend, clock and diagnostics pass\n");
   return 0;
}
