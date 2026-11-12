#include "sdl_renderer.h"

#include "vw_sdl.h"
#include "vw_bitmap_font.h"
#include "vw_player_colors.h"
#if defined(__ANDROID__)
#include "sdl_touch_controls.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

void SetPlayerColor(SDL_Renderer *renderer, int player)
{
   const unsigned char *color = VwPlayerColor(kVwColorsSdl, player);
   SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], SDL_ALPHA_OPAQUE);
}

/* Text with the embedded 8x8 font, one point per lit pixel. */
void DrawFontText(SDL_Renderer *renderer, int x, int y, const char *text)
{
   SDL_Point points[VW_FONT_SIZE * VW_FONT_SIZE];
   for (; *text; ++text, x += VW_FONT_SIZE) {
      const unsigned char *glyph = VwFontGlyph(*text);
      if (!glyph) continue;
      int count = 0;
      for (int row = 0; row < VW_FONT_SIZE; ++row) {
         for (int column = 0; column < VW_FONT_SIZE; ++column) {
            if (glyph[row] & (1 << column)) {
               points[count].x = x + column;
               points[count].y = y + row;
               ++count;
            }
         }
      }
      if (count) SDL_RenderDrawPoints(renderer, points, count);
   }
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
#if defined(__ANDROID__)
   // The window is the whole screen: stretch the 640x480 arena to it so
   // normalized touch positions and drawing share one coordinate space.
   if (_renderer) {
      int width = 0, height = 0;
      if (SDL_GetRendererOutputSize(_renderer, &width, &height) == 0 && width && height) {
         SDL_RenderSetScale(_renderer, width / 640.0f, height / 480.0f);
      }
   }
#endif
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
SDLVectorWarRenderer::DrawLegend(const VwLegend &legend)
{
   char row[48];
   for (int index = 0; VwLegendRow(legend, index, row, sizeof(row)); ++index) {
      const int y = 28 + index * 12;
      SDL_Rect swatch = { 12, y, 8, 8 };
      SetPlayerColor(_renderer, index);
      SDL_RenderFillRect(_renderer, &swatch);
      SDL_SetRenderDrawColor(_renderer, 200, 200, 200, SDL_ALPHA_OPAQUE);
      DrawFontText(_renderer, 24, y, row);
   }
}

void
SDLVectorWarRenderer::Draw(GameState &state, NonGameState &non_game_state)
{
   if (!_renderer) return;
   SDL_SetRenderDrawColor(_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
   SDL_RenderClear(_renderer);
   for (int player = 0; player < state._num_ships; ++player) {
      DrawShip(state._ships[player], player);
   }
   DrawLegend(non_game_state.legend);
#if defined(__ANDROID__)
   vw_touch::Draw(_renderer, 640, 480);
#endif
   SDL_RenderPresent(_renderer);
}
