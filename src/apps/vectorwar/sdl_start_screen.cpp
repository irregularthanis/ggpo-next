#include "sdl_start_screen.h"

#include "sdl_text.h"

#include <stdio.h>
#include <string.h>

namespace {

const char *const kLabels[SdlStartScreen::kFieldCount] = {
   "Local port", "You are player", "Other player", "Server", "Players", "Lobby code"
};
const char *const kButtons[SdlStartScreen::kButtonCount] = {
   "Connect", "Matchmaking", "Quit", "Cancel"
};

/* Waiting: only Cancel. Compact: only Quit. Form: Connect, Matchmaking, Quit. */
bool ButtonShown(int button, bool waiting, bool form)
{
   if (waiting) return button == SdlStartScreen::kCancelButton;
   if (!form) return button == SdlStartScreen::kQuitButton;
   return button != SdlStartScreen::kCancelButton;
}

bool Inside(const SDL_Rect &rect, int x, int y)
{
   return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

}  // namespace

SdlStartScreen::SdlStartScreen(bool form) :
   _window(NULL), _renderer(NULL), _focus(kRemote), _form_visible(form), _waiting(false),
   _error(false)
{
   VwStartFormDefaults(&_form);
   _message[0] = '\0';
   if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return;
   _window = SDL_CreateWindow("GGPO Vector War", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              640, 480, SDL_WINDOW_SHOWN);
   if (_window) {
      _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_SOFTWARE);
   }
   /* Full-screen on Android: draw and hit-test in 640x480 either way. */
   if (_renderer) SDL_RenderSetLogicalSize(_renderer, 640, 480);
   if (_form_visible) SDL_StartTextInput();
}

SdlStartScreen::~SdlStartScreen()
{
   if (_form_visible) SDL_StopTextInput();
   if (_renderer) SDL_DestroyRenderer(_renderer);
   if (_window) SDL_DestroyWindow(_window);
   SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

SDL_Rect SdlStartScreen::FieldRect(int field)
{
   /* Two groups: direct (first three) and matchmaking (last three). */
   const int top = field < kServer ? 76 : 236;
   const int row = field < kServer ? field : field - kServer;
   SDL_Rect rect = { 248, top + row * 40, 368, 26 };
   return rect;
}

SDL_Rect SdlStartScreen::ButtonRect(int button)
{
   static const SDL_Rect rects[kButtonCount] = {
      { 24, 420, 176, 36 }, { 216, 420, 208, 36 }, { 440, 420, 176, 36 }, { 216, 420, 208, 36 }
   };
   return rects[button];
}

char *SdlStartScreen::FieldText(int field, size_t *capacity)
{
   switch (field) {
   case kLocalPort: *capacity = sizeof(_form.local_port); return _form.local_port;
   case kPlayerNumber: *capacity = sizeof(_form.player_number); return _form.player_number;
   case kRemote: *capacity = sizeof(_form.remote); return _form.remote;
   case kServer: *capacity = sizeof(_form.server); return _form.server;
   case kPlayers: *capacity = sizeof(_form.players); return _form.players;
   default: *capacity = sizeof(_form.lobby); return _form.lobby;
   }
}

void SdlStartScreen::SetMessage(const char *text, bool error)
{
   snprintf(_message, sizeof(_message), "%s", text ? text : "");
   _error = error;
}

SdlStartScreen::Action SdlStartScreen::Press(int button)
{
   if (_waiting) return button == kCancelButton ? kCancel : kNone;
   switch (button) {
   case kConnectButton: return kConnect;
   case kMatchmakingButton: return kMatchmaking;
   case kQuitButton: return kQuit;
   default: return kNone;
   }
}

SdlStartScreen::Action SdlStartScreen::HandleEvent(const SDL_Event &event)
{
   switch (event.type) {
   case SDL_QUIT:
      return _waiting ? kCancel : kQuit;
   case SDL_MOUSEBUTTONDOWN:
      for (int button = 0; button < kButtonCount; ++button) {
         if (ButtonShown(button, _waiting, _form_visible) &&
             Inside(ButtonRect(button), event.button.x, event.button.y)) {
            return Press(button);
         }
      }
      if (!_waiting && _form_visible) {
         for (int field = 0; field < kFieldCount; ++field) {
            if (Inside(FieldRect(field), event.button.x, event.button.y)) _focus = field;
         }
      }
      return kNone;
   case SDL_TEXTINPUT: {
      if (_waiting || !_form_visible) return kNone;
      size_t capacity = 0;
      char *text = FieldText(_focus, &capacity);
      for (const char *in = event.text.text; *in; ++in) {
         const unsigned char c = (unsigned char)*in;
         if (c < 32 || c > 126) continue;          /* the font is ASCII only */
         const size_t length = strlen(text);
         if (length + 1 >= capacity) break;
         text[length] = (char)c;
         text[length + 1] = '\0';
      }
      return kNone;
   }
   case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
      case SDLK_AC_BACK:
         return _waiting ? kCancel : kQuit;
      case SDLK_TAB:
         if (!_waiting && _form_visible) {
            const bool back = (event.key.keysym.mod & KMOD_SHIFT) != 0;
            _focus = (_focus + (back ? kFieldCount - 1 : 1)) % kFieldCount;
         }
         return kNone;
      case SDLK_BACKSPACE:
         if (!_waiting && _form_visible) {
            size_t capacity = 0;
            char *text = FieldText(_focus, &capacity);
            const size_t length = strlen(text);
            if (length) text[length - 1] = '\0';
         }
         return kNone;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
         /* Enter starts the group the focused field belongs to. */
         if (_waiting || !_form_visible) return kNone;
         return _focus < kServer ? kConnect : kMatchmaking;
      default:
         return kNone;
      }
   default:
      return kNone;
   }
}

void SdlStartScreen::Draw()
{
   if (!_renderer) return;
   SDL_SetRenderDrawColor(_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
   SDL_RenderClear(_renderer);

   SDL_SetRenderDrawColor(_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
   VwSdlDrawText(_renderer, 24, 16, "GGPO Vector War", 2);
   for (int field = 0; _form_visible && field < kFieldCount; ++field) {
      DrawField(field);
   }
   if (_form_visible) {
      SDL_SetRenderDrawColor(_renderer, 160, 160, 160, SDL_ALPHA_OPAQUE);
      VwSdlDrawText(_renderer, 24, 52, "Direct: two players, IP:PORT", 1);
      VwSdlDrawText(_renderer, 24, 212, "Matchmaking: 2 to 4 players, share the lobby code", 1);
   }

   if (_message[0]) {
      if (_error) {
         SDL_SetRenderDrawColor(_renderer, 255, 96, 96, SDL_ALPHA_OPAQUE);
      } else {
         SDL_SetRenderDrawColor(_renderer, 255, 220, 0, SDL_ALPHA_OPAQUE);
      }
      VwSdlDrawText(_renderer, 24, _form_visible ? 384 : 220, _message, _form_visible ? 1 : 2);
   }

   for (int button = 0; button < kButtonCount; ++button) {
      if (!ButtonShown(button, _waiting, _form_visible)) continue;
      const SDL_Rect rect = ButtonRect(button);
      SDL_SetRenderDrawColor(_renderer, 48, 48, 64, SDL_ALPHA_OPAQUE);
      SDL_RenderFillRect(_renderer, &rect);
      SDL_SetRenderDrawColor(_renderer, 200, 200, 255, SDL_ALPHA_OPAQUE);
      SDL_RenderDrawRect(_renderer, &rect);
      const int width = (int)strlen(kButtons[button]) * 16;
      VwSdlDrawText(_renderer, rect.x + (rect.w - width) / 2, rect.y + 10, kButtons[button], 2);
   }
   SDL_RenderPresent(_renderer);
}

void SdlStartScreen::DrawField(int field)
{
   const SDL_Rect rect = FieldRect(field);
   SDL_SetRenderDrawColor(_renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
   VwSdlDrawText(_renderer, 24, rect.y + 5, kLabels[field], 2);
   if (field == _focus && !_waiting) {
      SDL_SetRenderDrawColor(_renderer, 255, 220, 0, SDL_ALPHA_OPAQUE);
   } else {
      SDL_SetRenderDrawColor(_renderer, 96, 96, 96, SDL_ALPHA_OPAQUE);
   }
   SDL_RenderDrawRect(_renderer, &rect);
   size_t capacity = 0;
   const char *text = FieldText(field, &capacity);
   const size_t visible = (size_t)((rect.w - 8) / 16) - 1;   /* room for the caret */
   const size_t length = strlen(text);
   const char *shown = length > visible ? text + length - visible : text;
   SDL_SetRenderDrawColor(_renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
   VwSdlDrawText(_renderer, rect.x + 4, rect.y + 5, shown, 2);
   if (field == _focus && !_waiting && (SDL_GetTicks() / 500) % 2 == 0) {
      SDL_Rect caret = { rect.x + 4 + (int)strlen(shown) * 16, rect.y + 4, 2, 18 };
      SDL_RenderFillRect(_renderer, &caret);
   }
}
