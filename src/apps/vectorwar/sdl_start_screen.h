#ifndef _SDL_START_SCREEN_H_
#define _SDL_START_SCREEN_H_

/* The Linux start screen: an SDL form drawn with the embedded font, shown
 * when Vector War runs without arguments. Direct IP:PORT or Matchmaking, the
 * same fields as the Windows dialog and the Android launcher (see
 * vw_start_form.h). Keyboard (Tab, Enter, Esc) and mouse. */

#include "vw_sdl.h"
#include "vw_start_form.h"

class SdlStartScreen {
public:
   enum Action { kNone = 0, kConnect, kMatchmaking, kQuit, kCancel };
   enum Field { kLocalPort = 0, kPlayerNumber, kRemote, kServer, kPlayers, kLobby, kFieldCount };
   enum Button { kConnectButton = 0, kMatchmakingButton, kQuitButton, kCancelButton, kButtonCount };

   /* form = false: a compact screen with only the status and Cancel/Quit,
    * for --matchmaking from the command line or the Android launcher. */
   explicit SdlStartScreen(bool form = true);
   ~SdlStartScreen();
   bool IsReady() const { return _window && _renderer; }

   /* One input event; returns the button chosen, if any. */
   Action HandleEvent(const SDL_Event &event);
   void Draw();

   VwStartForm &Form() { return _form; }
   int FocusedField() const { return _focus; }
   void SetMessage(const char *text, bool error);
   /* While waiting only Cancel (or Esc) does anything. */
   void SetWaiting(bool waiting) { _waiting = waiting; }

   static SDL_Rect FieldRect(int field);
   static SDL_Rect ButtonRect(int button);
   SDL_Renderer *Handle() const { return _renderer; }

private:
   char *FieldText(int field, size_t *capacity);
   void DrawField(int field);
   Action Press(int button);

   SDL_Window *_window;
   SDL_Renderer *_renderer;
   VwStartForm _form;
   int _focus;
   bool _form_visible;
   bool _waiting;
   bool _error;
   char _message[128];
};

#endif
