#include "vw_legend.h"

#include <stdio.h>
#include <string.h>

int VwLocalPlatform()
{
#if defined(_WIN32)
   return kVwPlatformWindows;
#elif defined(__ANDROID__)
   return kVwPlatformAndroid;
#else
   return kVwPlatformLinux;
#endif
}

const char *VwPlatformName(int platform)
{
   switch (platform) {
   case kVwPlatformWindows: return "Windows";
   case kVwPlatformLinux: return "Linux";
   case kVwPlatformAndroid: return "Android";
   default: return "unknown";
   }
}

void VwLegendInitDirect(VwLegend *legend, int num_players, int local_player)
{
   memset(legend, 0, sizeof(*legend));
   if (num_players < 0) num_players = 0;
   if (num_players > VW_LEGEND_MAX_PLAYERS) num_players = VW_LEGEND_MAX_PLAYERS;
   legend->num_players = num_players;
   if (local_player >= 1 && local_player <= num_players) {
      legend->local_player = local_player;
      legend->platforms[local_player - 1] = VwLocalPlatform();
   }
}

bool VwLegendRow(const VwLegend &legend, int index, char *out, size_t length)
{
   if (!out || length == 0) return false;
   out[0] = '\0';
   if (index < 0 || index >= legend.num_players || index >= VW_LEGEND_MAX_PLAYERS) return false;
   const char *suffix = "";
   if (index + 1 == legend.local_player) {
      suffix = " (you)";
   } else if (legend.routes[index] == kVwRouteDirect) {
      suffix = " direct";
   } else if (legend.routes[index] == kVwRouteRelay) {
      suffix = " relay";
   }
   snprintf(out, length, "P%d %s%s", index + 1, VwPlatformName(legend.platforms[index]), suffix);
   return true;
}
