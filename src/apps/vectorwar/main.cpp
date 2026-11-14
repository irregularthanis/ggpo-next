#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_DEBUG)
#   include <crtdbg.h>
#endif
#include "matchmade.h"
#include "vectorwar.h"
#include "win32_host.h"
#include "win32_start_dialog.h"
#include "vw_start_form.h"

LRESULT CALLBACK
MainWindowProc(HWND hwnd,
               UINT uMsg,
               WPARAM wParam,
               LPARAM lParam)
{
   switch (uMsg) {
   case WM_ERASEBKGND:
      return 1;
   case WM_KEYDOWN:
      if (wParam == 'P') {
         VectorWar_TogglePerformanceMonitor();
      } else if (wParam == VK_ESCAPE) {
         VectorWar_Exit();
		 PostQuitMessage(0);
      } else if (wParam >= VK_F1 && wParam <= VK_F12) {
         VectorWar_DisconnectPlayer((int)(wParam - VK_F1));
      }
      return 0;
   case WM_PAINT:
      VectorWar_DrawCurrentFrame();
      ValidateRect(hwnd, NULL);
      return 0;
   case WM_CLOSE:
      PostQuitMessage(0);
      break;
   }
   return CallWindowProc(DefWindowProc, hwnd, uMsg, wParam, lParam);
}

HWND
CreateMainWindow(HINSTANCE hInstance)
{
   HWND hwnd;
   WNDCLASSEX wndclass = { 0 };
   RECT rc;
   int width = 640, height = 480;
   WCHAR titlebuf[128];

   wsprintf(titlebuf, L"(pid:%d) ggpo sdk sample: vector war", GetCurrentProcessId());
   wndclass.cbSize = sizeof(wndclass);
   wndclass.lpfnWndProc = MainWindowProc;
   wndclass.lpszClassName = L"vwwnd";
   RegisterClassEx(&wndclass);
   hwnd = CreateWindow(L"vwwnd",
                       titlebuf,
                       WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       width, height,
                       NULL, NULL, hInstance, NULL);

   GetClientRect(hwnd, &rc);
   SetWindowPos(hwnd, NULL, 0, 0, width + (width - (rc.right - rc.left)), height + (height - (rc.bottom - rc.top)), SWP_NOMOVE);
   return hwnd;
}

void
RunMainLoop()
{
   MSG msg = { 0 };
   int next, now;

   next = now = timeGetTime();
   while(1) {
      while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
         TranslateMessage(&msg); 
         DispatchMessage(&msg);
         if (msg.message == WM_QUIT) {
            return;
         }
      }
      now = timeGetTime();
      VectorWar_Idle(max(0, next - now - 1));
      if (now >= next) {
         VectorWar_RunFrame();
         next = now + (1000 / 60);
      }
   }
}

void
Syntax()
{
   MessageBox(NULL, 
              L"Syntax: vectorwar.exe <local port> <num players> ('local' | <remote ip>:<remote port>)*\n"
              L"        vectorwar.exe --matchmaking <host>[:<port>] <players> [<lobby>] <local port> [--relay-only]\n"
              L"        vectorwar.exe (no arguments: start screen)\n",
              L"Could not start", MB_OK);
}

static const POINT window_offsets[] = {
   { 64,  64 },   // player 1
   { 740, 64 },   // player 2
   { 64,  600 },  // player 3
   { 740, 600 },  // player 4
};

static void
PlaceWindow(HWND hwnd, int slot)
{
   if (slot >= 0 && slot < (int)(sizeof(window_offsets) / sizeof(window_offsets[0]))) {
      ::SetWindowPos(hwnd, NULL, window_offsets[slot].x, window_offsets[slot].y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
   }
}

/* "IP:PORT" into a GGPO remote endpoint. */
static bool
ParseRemote(const char *text, GGPOPlayer *player)
{
   char address[128];
   unsigned short port = 0;
   if (sscanf_s(text, "%127[^:]:%hu", address, (unsigned)sizeof(address), &port) != 2) return false;
   if (strlen(address) >= sizeof(player->u.remote.ip_address)) return false;
   strcpy_s(player->u.remote.ip_address, sizeof(player->u.remote.ip_address), address);
   player->u.remote.port = port;
   return true;
}

/* Plays a matchmade session in a new game window and releases it. */
static int
PlayMatchmade(HINSTANCE instance, MatchmadeSession *session, GGPOPlayer *players,
              int num_players, unsigned short ggpo_port, int local_player)
{
   HWND hwnd = CreateMainWindow(instance);
   Win32VectorWarHost host(hwnd);
   PlaceWindow(hwnd, local_player - 1);
   VectorWar_Init(&host, ggpo_port, num_players, players, 0);
   MatchmadeAttachLegend(session);
   RunMainLoop();
   VectorWar_Exit();
   MatchmadeAttachLegend(NULL);
   MatchmadeStop(session);
   DestroyWindow(hwnd);
   return 0;
}

/* The command line, narrowed to ASCII; argv[0] is the program. */
static int
RunWithArgs(HINSTANCE instance, int argc, char **argv)
{
   if (argc >= 2 && strcmp(argv[1], "--matchmaking") == 0) {
      /* Lobby, relay and hole punching with relay fallback. */
      MatchmadeArgs args;
      if (!ParseMatchmakingArgs(argc, argv, 2, &args)) {
         Syntax();
         return 1;
      }
      GGPOPlayer players[GGPO_MAX_PLAYERS];
      int num_players = 0, local_player = 0;
      unsigned short ggpo_port = 0;
      char error[160];
      MatchmadeSession *session = MatchmadeStart(args, players, &num_players, &ggpo_port,
                                                 &local_player, error, sizeof(error));
      if (!session) {
         MessageBoxA(NULL, error, "Matchmaking failed", MB_OK);
         return 1;
      }
      return PlayMatchmade(instance, session, players, num_players, ggpo_port, local_player);
   }
   if (argc < 3) {
      Syntax();
      return 1;
   }
   int offset = 1, local_player = 0;
   unsigned short local_port = (unsigned short)atoi(argv[offset++]);
   int num_players = atoi(argv[offset++]);
   if (num_players < 0 || argc < offset + num_players) {
      Syntax();
      return 1;
   }
   HWND hwnd = CreateMainWindow(instance);
   Win32VectorWarHost host(hwnd);
   if (offset < argc && strcmp(argv[offset], "spectate") == 0) {
      GGPOPlayer remote;
      memset(&remote, 0, sizeof(remote));
      if (offset + 1 >= argc || !ParseRemote(argv[offset + 1], &remote)) {
         Syntax();
         DestroyWindow(hwnd);
         return 1;
      }
      VectorWar_InitSpectator(&host, local_port, num_players, remote.u.remote.ip_address,
                              remote.u.remote.port);
   } else {
      GGPOPlayer players[GGPO_MAX_SPECTATORS + GGPO_MAX_PLAYERS];
      memset(players, 0, sizeof(players));

      int i;
      for (i = 0; i < num_players; i++) {
         const char *arg = argv[offset++];

         players[i].size = sizeof(players[i]);
         players[i].player_num = i + 1;
         if (!_stricmp(arg, "local")) {
            players[i].type = GGPO_PLAYERTYPE_LOCAL;
            local_player = i;
            continue;
         }
         players[i].type = GGPO_PLAYERTYPE_REMOTE;
         if (!ParseRemote(arg, &players[i])) {
            Syntax();
            DestroyWindow(hwnd);
            return 1;
         }
      }
      // these are spectators...
      int num_spectators = 0;
      while (offset < argc) {
         players[i].size = sizeof(players[i]);
         players[i].type = GGPO_PLAYERTYPE_SPECTATOR;
         if (!ParseRemote(argv[offset++], &players[i])) {
            Syntax();
            DestroyWindow(hwnd);
            return 1;
         }
         i++;
         num_spectators++;
      }

      PlaceWindow(hwnd, local_player);
      VectorWar_Init(&host, local_port, num_players, players, num_spectators);
   }
   RunMainLoop();
   VectorWar_Exit();
   DestroyWindow(hwnd);
   return 0;
}

/* Matchmaking from the start dialog: progress and Cancel while the lobby fills. */
struct StartDialogWait {
   Win32StartDialog *dialog;
   bool cancelled;
};

static void
StartDialogProgress(int joined, int group_size, void *context)
{
   char text[64];
   VwStartWaitingText(joined, group_size, text, sizeof(text));
   ((StartDialogWait *)context)->dialog->SetMessage(text);
}

static bool
StartDialogKeepGoing(void *context)
{
   StartDialogWait *wait = (StartDialogWait *)context;
   if (wait->dialog->Pump(0) == Win32StartDialog::kCancel) wait->cancelled = true;
   return !wait->cancelled;
}

/* No arguments: the start dialog. Direct play continues exactly as the
 * command line would; matchmaking waits in the dialog, then plays. */
static int
RunStartScreen(HINSTANCE instance, char *program)
{
   Win32StartDialog *dialog = new Win32StartDialog(instance);
   if (!dialog->IsReady()) {
      delete dialog;
      Syntax();
      return 1;
   }
   for (;;) {
      const Win32StartDialog::Action action = dialog->Pump(50);
      if (action == Win32StartDialog::kQuit) {
         delete dialog;
         return 0;
      }
      if (action != Win32StartDialog::kConnect && action != Win32StartDialog::kMatchmaking) continue;
      const VwStartMode mode = action == Win32StartDialog::kConnect ? kVwStartDirect : kVwStartMatchmaking;
      VwStartForm form;
      dialog->ReadForm(&form);
      VwStartArgs args;
      char error[160];
      if (!VwStartFormArgs(form, mode, &args, error, sizeof(error))) {
         dialog->SetMessage(error);
         continue;
      }
      if (mode == kVwStartDirect) {
         char *argv[VW_START_MAX_ARGS + 1];
         argv[0] = program;
         for (int i = 0; i < args.argc; ++i) argv[i + 1] = args.argv[i];
         delete dialog;
         return RunWithArgs(instance, args.argc + 1, argv);
      }
      MatchmadeArgs matchmade;
      if (!ParseMatchmakingArgs(args.argc, args.argv, 1, &matchmade)) {
         dialog->SetMessage("Check the server, players and lobby code");
         continue;
      }
      StartDialogWait wait = { dialog, false };
      matchmade.progress = StartDialogProgress;
      matchmade.keep_going = StartDialogKeepGoing;
      matchmade.ui_context = &wait;
      dialog->SetWaiting(true);
      dialog->SetMessage("Contacting the matchmaking server...");
      GGPOPlayer players[GGPO_MAX_PLAYERS];
      int num_players = 0, local_player = 0;
      unsigned short ggpo_port = 0;
      MatchmadeSession *session = MatchmadeStart(matchmade, players, &num_players, &ggpo_port,
                                                 &local_player, error, sizeof(error));
      dialog->SetWaiting(false);
      if (session) {
         delete dialog;
         return PlayMatchmade(instance, session, players, num_players, ggpo_port, local_player);
      }
      dialog->SetMessage(wait.cancelled ? "Matchmaking cancelled" : error);
   }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int)
{
   WSADATA wd = { 0 };
   WSAStartup(MAKEWORD(2, 2), &wd);

#if defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   /* Every argument is plain ASCII (ports, addresses, host names, codes). */
   char narrow[16][160];
   char *argv[16];
   const int argc = __argc < 16 ? __argc : 16;
   for (int i = 0; i < argc; ++i) {
      WideCharToMultiByte(CP_UTF8, 0, __wargv[i], -1, narrow[i], sizeof(narrow[i]), NULL, NULL);
      argv[i] = narrow[i];
   }
   const int code = argc <= 1 ? RunStartScreen(hInstance, argv[0]) : RunWithArgs(hInstance, argc, argv);
   WSACleanup();
   return code;
}
