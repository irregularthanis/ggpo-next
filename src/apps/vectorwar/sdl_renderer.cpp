#include "sdl_renderer.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

void SetPlayerColor(SDL_Renderer *renderer, int player)
{
   static const unsigned char colors[4][3] = {
      { 255, 64, 64 }, { 64, 255, 64 },
      { 64, 128, 255 }, { 192, 192, 192 }
   };
   SDL_SetRenderDrawColor(renderer, colors[player][0], colors[player][1],
                          colors[player][2], SDL_ALPHA_OPAQUE);
}

} // namespace

SDLVectorWarRenderer::SDLVectorWarRenderer() :
   _window(NULL), _renderer(NULL)
{
   _status[0] = '\0';
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
      fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
      return;
   }
   _window = SDL_CreateWindow(
      "GGPO Vector War", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      640, 480, SDL_WINDOW_SHOWN);
   if (_window) {
      _renderer = SDL_CreateRenderer(
         _window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
      if (!_renderer) {
         _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_SOFTWARE);
      }
   }
   if (!_window || !_renderer) {
      fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
   }
}

SDLVectorWarRenderer::~SDLVectorWarRenderer()
{
   if (_renderer) SDL_DestroyRenderer(_renderer);
   if (_window) SDL_DestroyWindow(_window);
   SDL_Quit();
}

bool SDLVectorWarRenderer::IsReady() const { return _window && _renderer; }

void
SDLVectorWarRenderer::SetStatusText(const char *text)
{
   if (!text) text = "";
   snprintf(_status, sizeof(_status), "%s", text);
   if (_window) SDL_SetWindowTitle(_window, _status[0] ? _status : "GGPO Vector War");
}

void
SDLVectorWarRenderer::DrawShip(const Ship &ship, int player)
{
   SDL_Point points[5];
   const int shape[5][2] = {
      { SHIP_RADIUS, 0 }, { -SHIP_RADIUS, SHIP_WIDTH },
      { SHIP_TUCK - SHIP_RADIUS, 0 }, { -SHIP_RADIUS, -SHIP_WIDTH },
      { SHIP_RADIUS, 0 }
   };
   const double theta = (double)ship.heading * PI / 180.0;
   const double cosine = cos(theta);
   const double sine = sin(theta);
   for (int index = 0; index < 5; ++index) {
      points[index].x = (int)(shape[index][0] * cosine - shape[index][1] * sine + ship.position.x);
      points[index].y = (int)(shape[index][0] * sine + shape[index][1] * cosine + ship.position.y);
   }
   SetPlayerColor(_renderer, player);
   SDL_RenderDrawLines(_renderer, points, 5);

   SDL_SetRenderDrawColor(_renderer, 255, 255, 0, SDL_ALPHA_OPAQUE);
   for (int index = 0; index < MAX_BULLETS; ++index) {
      if (ship.bullets[index].active) {
         SDL_Rect bullet;
         bullet.x = (int)ship.bullets[index].position.x - 1;
         bullet.y = (int)ship.bullets[index].position.y - 1;
         bullet.w = bullet.h = 3;
         SDL_RenderFillRect(_renderer, &bullet);
      }
   }

   SDL_Rect health;
   health.x = player < 2 ? 12 : 428;
   health.y = player % 2 == 0 ? 12 : 452;
   health.w = ship.health > 0 ? ship.health * 2 : 0;
   health.h = 8;
   SetPlayerColor(_renderer, player);
   SDL_RenderFillRect(_renderer, &health);
}

void
SDLVectorWarRenderer::Draw(GameState &state, NonGameState &)
{
   if (!_renderer) return;
   SDL_SetRenderDrawColor(_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
   SDL_RenderClear(_renderer);
   for (int player = 0; player < state._num_ships; ++player) {
      DrawShip(state._ships[player], player);
   }
   SDL_RenderPresent(_renderer);
}
