#ifndef _VW_PLAYER_COLORS_H_
#define _VW_PLAYER_COLORS_H_

/* Ship colors, one table per renderer, shared by the ships and the legend so
 * the two cannot drift. Entries are { red, green, blue } per player. */

enum VwColorTable { kVwColorsGdi = 0, kVwColorsSdl = 1 };

static inline const unsigned char *VwPlayerColor(VwColorTable table, int player)
{
   static const unsigned char gdi[4][3] = {
      { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 128, 128, 128 }
   };
   static const unsigned char sdl[4][3] = {
      { 255, 64, 64 }, { 64, 255, 64 }, { 64, 128, 255 }, { 192, 192, 192 }
   };
   if (player < 0 || player > 3) player = 3;
   return table == kVwColorsGdi ? gdi[player] : sdl[player];
}

#endif
