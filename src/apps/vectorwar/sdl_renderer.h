#ifndef _SDL_RENDERER_H_
#define _SDL_RENDERER_H_

#include "renderer.h"

struct SDL_Renderer;
struct SDL_Window;

/* SDL2 renderer used by the interactive Linux Vector War frontend. */
class SDLVectorWarRenderer : public Renderer {
public:
   SDLVectorWarRenderer();
   virtual ~SDLVectorWarRenderer();

   virtual void Draw(GameState &state, NonGameState &non_game_state);
   virtual void SetStatusText(const char *text);
   bool IsReady() const;

private:
   void DrawShip(const Ship &ship, int player);

   SDL_Window *_window;
   SDL_Renderer *_renderer;
   char _status[256];
};

#endif
