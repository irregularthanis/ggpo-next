#include "ggponet.h"

#include <stddef.h>
#include <stdio.h>

// Freeze the consumer-visible Windows ABI before platform extraction.  Each
// record is both human-readable evidence and a hard assertion against changes
// to public type sizes, field/callback positions, or selected enum values.
// Exported symbol names are checked separately by the CI evidence script.
static int failures = 0;

static void
Record(const char *name, size_t actual, size_t expected)
{
   printf("%s=%lu\n", name, (unsigned long)actual);
   if (actual != expected) {
      fprintf(stderr,
              "%s mismatch: expected %lu, got %lu\n",
              name,
              (unsigned long)expected,
              (unsigned long)actual);
      failures++;
   }
}

int
main(void)
{
   GGPOPlayer player;
   const size_t pointer_size = sizeof(void *);
   const size_t remote_port_offset =
      (const char *)&player.u.remote.port - (const char *)&player;

   Record("pointer_size", pointer_size, pointer_size);
   Record("sizeof.GGPOPlayerHandle", sizeof(GGPOPlayerHandle), 4);
   Record("sizeof.GGPOPlayerType", sizeof(GGPOPlayerType), 4);
   Record("sizeof.GGPOErrorCode", sizeof(GGPOErrorCode), 4);
   Record("sizeof.GGPOPlayer", sizeof(GGPOPlayer), 48);
   Record("offsetof.GGPOPlayer.type", offsetof(GGPOPlayer, type), 4);
   Record("offsetof.GGPOPlayer.player_num", offsetof(GGPOPlayer, player_num), 8);
   Record("offsetof.GGPOPlayer.u", offsetof(GGPOPlayer, u), 12);
   Record("offsetof.GGPOPlayer.remote.port", remote_port_offset, 44);

   Record("sizeof.GGPOEvent", sizeof(GGPOEvent), 16);
   Record("offsetof.GGPOEvent.u", offsetof(GGPOEvent, u), 4);
   Record("sizeof.GGPONetworkStats", sizeof(GGPONetworkStats), 24);
   Record("offsetof.GGPONetworkStats.timesync",
          offsetof(GGPONetworkStats, timesync),
          16);

   Record("sizeof.GGPOSessionCallbacks",
          sizeof(GGPOSessionCallbacks),
          pointer_size * 7);
   Record("offsetof.callbacks.begin_game",
          offsetof(GGPOSessionCallbacks, begin_game),
          pointer_size * 0);
   Record("offsetof.callbacks.save_game_state",
          offsetof(GGPOSessionCallbacks, save_game_state),
          pointer_size * 1);
   Record("offsetof.callbacks.load_game_state",
          offsetof(GGPOSessionCallbacks, load_game_state),
          pointer_size * 2);
   Record("offsetof.callbacks.log_game_state",
          offsetof(GGPOSessionCallbacks, log_game_state),
          pointer_size * 3);
   Record("offsetof.callbacks.free_buffer",
          offsetof(GGPOSessionCallbacks, free_buffer),
          pointer_size * 4);
   Record("offsetof.callbacks.advance_frame",
          offsetof(GGPOSessionCallbacks, advance_frame),
          pointer_size * 5);
   Record("offsetof.callbacks.on_event",
          offsetof(GGPOSessionCallbacks, on_event),
          pointer_size * 6);

   Record("enum.GGPO_PLAYERTYPE_LOCAL", GGPO_PLAYERTYPE_LOCAL, 0);
   Record("enum.GGPO_PLAYERTYPE_REMOTE", GGPO_PLAYERTYPE_REMOTE, 1);
   Record("enum.GGPO_PLAYERTYPE_SPECTATOR", GGPO_PLAYERTYPE_SPECTATOR, 2);
   Record("enum.GGPO_EVENTCODE_CONNECTED_TO_PEER",
          GGPO_EVENTCODE_CONNECTED_TO_PEER,
          1000);
   Record("enum.GGPO_EVENTCODE_CONNECTION_RESUMED",
          GGPO_EVENTCODE_CONNECTION_RESUMED,
          1007);
   Record("enum.GGPO_ERRORCODE_INVALID_REQUEST",
          GGPO_ERRORCODE_INVALID_REQUEST,
          11);

   return failures == 0 ? 0 : 1;
}
