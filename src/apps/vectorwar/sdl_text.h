#ifndef _SDL_TEXT_H_
#define _SDL_TEXT_H_

/* Text for the SDL frontends with the embedded 8x8 font, in the renderer's
 * current draw color; scale 2 draws 16x16 characters. */

#include "vw_bitmap_font.h"
#include "vw_sdl.h"

static inline void VwSdlDrawText(SDL_Renderer *renderer, int x, int y, const char *text, int scale)
{
   SDL_Rect pixels[VW_FONT_SIZE * VW_FONT_SIZE];
   const int advance = VW_FONT_SIZE * scale;
   for (; *text; ++text, x += advance) {
      const unsigned char *glyph = VwFontGlyph(*text);
      if (!glyph) continue;
      int count = 0;
      for (int row = 0; row < VW_FONT_SIZE; ++row) {
         for (int column = 0; column < VW_FONT_SIZE; ++column) {
            if (glyph[row] & (1 << column)) {
               pixels[count].x = x + column * scale;
               pixels[count].y = y + row * scale;
               pixels[count].w = pixels[count].h = scale;
               ++count;
            }
         }
      }
      if (count) SDL_RenderFillRects(renderer, pixels, count);
   }
}

#endif
