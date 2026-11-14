#ifndef _VW_LEGEND_H_
#define _VW_LEGEND_H_

/* The in-game legend: one row per player with its ship color, platform and,
 * in a matchmade session, the route to that peer ("P2 Android direct"). The
 * model is portable; each renderer only draws the rows. */

#include <stddef.h>

#define VW_LEGEND_MAX_PLAYERS 4

/* Same numbering as the matchmaking protocol's platform field. */
enum VwPlatform {
   kVwPlatformUnknown = 0, kVwPlatformWindows = 1,
   kVwPlatformLinux = 2, kVwPlatformAndroid = 3
};

enum VwRoute { kVwRouteNone = 0, kVwRouteRelay = 1, kVwRouteDirect = 2 };

struct VwLegend {
   int num_players;
   int local_player;                           /* 1-based; 0 = none */
   int platforms[VW_LEGEND_MAX_PLAYERS];       /* VwPlatform */
   int routes[VW_LEGEND_MAX_PLAYERS];          /* VwRoute; none for direct play */
   int relay_rtt_ms[VW_LEGEND_MAX_PLAYERS];    /* -1 until measured */
   int direct_rtt_ms[VW_LEGEND_MAX_PLAYERS];   /* -1 until measured */
};

/* The platform this binary was built for. */
int VwLocalPlatform();

const char *VwPlatformName(int platform);

/* Legend for a session without matchmaking: only the local platform is known. */
void VwLegendInitDirect(VwLegend *legend, int num_players, int local_player);

/* Row text for player index (0-based): "P1 Windows (you)", "P2 Linux relay",
 * "P3 unknown". Returns false for an index outside the session. */
bool VwLegendRow(const VwLegend &legend, int index, char *out, size_t length);

#endif
