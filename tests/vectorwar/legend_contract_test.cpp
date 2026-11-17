// The in-game legend's model, colors and font, without a renderer: row text
// for direct and matchmade sessions, platform numbering shared with the
// matchmaking protocol, one color table per renderer, and the bitmap font.
#include <stdio.h>
#include <string.h>

#include "vw_bitmap_font.h"
#include "vw_legend.h"
#include "vw_net_protocol.h"
#include "vw_player_colors.h"

static int failures = 0;

static void Expect(bool condition, const char *what)
{
   if (!condition) {
      fprintf(stderr, "FAIL: %s\n", what);
      ++failures;
   }
}

static void ExpectRow(const VwLegend &legend, int index, const char *expected)
{
   char row[48];
   const bool ok = VwLegendRow(legend, index, row, sizeof(row));
   if (!ok || strcmp(row, expected) != 0) {
      fprintf(stderr, "FAIL: row %d is \"%s\", expected \"%s\"\n", index, ok ? row : "(none)", expected);
      ++failures;
   }
}

int main()
{
   // Platform numbering is the protocol's: MATCH platforms go straight in.
   Expect((int)kVwPlatformUnknown == (int)vwnet::kPlatformUnknown, "unknown numbering");
   Expect((int)kVwPlatformWindows == (int)vwnet::kPlatformWindows, "Windows numbering");
   Expect((int)kVwPlatformLinux == (int)vwnet::kPlatformLinux, "Linux numbering");
   Expect((int)kVwPlatformAndroid == (int)vwnet::kPlatformAndroid, "Android numbering");

   // Direct play: only the local platform is known; no routes.
   VwLegend legend;
   VwLegendInitDirect(&legend, 2, 2);
   Expect(legend.relay_rtt_ms[0] == -1 && legend.direct_rtt_ms[0] == -1,
          "route round-trip times start unknown");
   char local[48];
   snprintf(local, sizeof(local), "P2 %s (you)", VwPlatformName(VwLocalPlatform()));
   ExpectRow(legend, 0, "P1 unknown");
   ExpectRow(legend, 1, local);
   char row[48];
   Expect(!VwLegendRow(legend, 2, row, sizeof(row)) && row[0] == '\0', "no row past the session");
   Expect(!VwLegendRow(legend, -1, row, sizeof(row)), "no negative row");

   // Matchmade: platforms from MATCH, routes from the path proxy.
   VwLegendInitDirect(&legend, 4, 1);
   legend.platforms[0] = kVwPlatformWindows;
   legend.platforms[1] = kVwPlatformAndroid;
   legend.platforms[2] = kVwPlatformLinux;
   legend.platforms[3] = 9;  // a future platform still renders
   legend.routes[1] = kVwRouteDirect;
   legend.routes[2] = kVwRouteRelay;
   legend.routes[3] = kVwRouteRelay;
   ExpectRow(legend, 0, "P1 Windows (you)");
   ExpectRow(legend, 1, "P2 Android direct");
   ExpectRow(legend, 2, "P3 Linux relay");
   ExpectRow(legend, 3, "P4 unknown relay");

   // Out-of-range sizes clamp; a spectator has no local row.
   VwLegendInitDirect(&legend, 9, 0);
   Expect(legend.num_players == VW_LEGEND_MAX_PLAYERS && legend.local_player == 0, "clamped spectator legend");
   char tiny[4];
   Expect(VwLegendRow(legend, 0, tiny, sizeof(tiny)) && strcmp(tiny, "P1 ") == 0, "truncated row");

   // Colors: the tables the ships used before the legend existed.
   const unsigned char *gdi_red = VwPlayerColor(kVwColorsGdi, 0);
   const unsigned char *sdl_blue = VwPlayerColor(kVwColorsSdl, 2);
   Expect(gdi_red[0] == 255 && gdi_red[1] == 0 && gdi_red[2] == 0, "GDI player 1 red");
   Expect(sdl_blue[0] == 64 && sdl_blue[1] == 128 && sdl_blue[2] == 255, "SDL player 3 blue");
   for (int table = 0; table < 2; ++table) {
      for (int a = 0; a < 4; ++a) {
         for (int b = a + 1; b < 4; ++b) {
            Expect(memcmp(VwPlayerColor((VwColorTable)table, a), VwPlayerColor((VwColorTable)table, b), 3) != 0,
                   "distinct player colors");
         }
      }
   }

   // Font: printable ASCII only, space blank, every other glyph drawn.
   Expect(VwFontGlyph('\n') == 0 && VwFontGlyph((char)127) == 0, "no glyph outside printable ASCII");
   static const unsigned char letter_a[8] = { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 };
   Expect(memcmp(VwFontGlyph('A'), letter_a, 8) == 0, "glyph A");
   for (int code = VW_FONT_FIRST; code <= VW_FONT_LAST; ++code) {
      const unsigned char *glyph = VwFontGlyph((char)code);
      int lit = 0;
      for (int row_index = 0; row_index < VW_FONT_SIZE; ++row_index) lit |= glyph[row_index];
      Expect(code == ' ' ? lit == 0 : lit != 0, "glyph coverage");
   }

   if (failures) {
      fprintf(stderr, "vectorwar-legend: %d failure(s)\n", failures);
      return 1;
   }
   printf("vectorwar-legend: rows, platforms, colors and font pass\n");
   return 0;
}
