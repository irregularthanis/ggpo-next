#include "ggponet.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int running_events = 0;

// Record an API contract failure without adding a framework dependency.
static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "public API contract failed: %s\n", contract);
      ++failures;
   }
}

static bool GGPO_CALL BeginGame(const char *) { return true; }

// Allocate a minimal valid snapshot for backend construction and cleanup.
static bool GGPO_CALL SaveState(unsigned char **buffer, int *len,
                                int *checksum, int frame)
{
   *len = (int)sizeof(frame);
   *buffer = (unsigned char *)malloc(*len);
   if (!*buffer) {
      return false;
   }
   memcpy(*buffer, &frame, *len);
   *checksum = frame;
   return true;
}

static bool GGPO_CALL LoadState(unsigned char *, int) { return true; }
static bool GGPO_CALL LogState(char *, unsigned char *, int) { return true; }
static void GGPO_CALL FreeBuffer(void *buffer) { free(buffer); }
static bool GGPO_CALL AdvanceFrame(int) { return true; }

// Observe the documented SyncTest transition to running.
static bool GGPO_CALL OnEvent(GGPOEvent *event)
{
   if (event && event->code == GGPO_EVENTCODE_RUNNING) {
      ++running_events;
   }
   return true;
}

// Assemble a complete callback table accepted by every backend constructor.
static GGPOSessionCallbacks Callbacks()
{
   GGPOSessionCallbacks callbacks;
   memset(&callbacks, 0, sizeof(callbacks));
   callbacks.begin_game = BeginGame;
   callbacks.save_game_state = SaveState;
   callbacks.load_game_state = LoadState;
   callbacks.log_game_state = LogState;
   callbacks.free_buffer = FreeBuffer;
   callbacks.advance_frame = AdvanceFrame;
   callbacks.on_event = OnEvent;
   return callbacks;
}

// Exercise ggpo_logv through a type-safe variadic wrapper.
static void LogvWrapper(GGPOSession *session, const char *format, ...)
{
   va_list args;
   va_start(args, format);
   ggpo_logv(session, format, args);
   va_end(args);
}

// Verify every error-returning wrapper rejects a null session consistently.
static void TestInvalidSessionGuards()
{
   GGPOPlayer player;
   GGPOPlayerHandle handle = GGPO_INVALID_HANDLE;
   GGPONetworkStats stats;
   char input = 0;
   int disconnect_flags = 0;

   memset(&player, 0, sizeof(player));
   memset(&stats, 0, sizeof(stats));
   Check(ggpo_add_player(0, &player, &handle) == GGPO_ERRORCODE_INVALID_SESSION,
         "add player rejects null session");
   Check(ggpo_set_frame_delay(0, 0, 1) == GGPO_ERRORCODE_INVALID_SESSION,
         "frame delay rejects null session");
   Check(ggpo_idle(0, 0) == GGPO_ERRORCODE_INVALID_SESSION,
         "idle rejects null session");
   Check(ggpo_add_local_input(0, 0, &input, 1) == GGPO_ERRORCODE_INVALID_SESSION,
         "local input rejects null session");
   Check(ggpo_synchronize_input(0, &input, 1, &disconnect_flags) ==
            GGPO_ERRORCODE_INVALID_SESSION,
         "synchronize input rejects null session");
   Check(ggpo_disconnect_player(0, 0) == GGPO_ERRORCODE_INVALID_SESSION,
         "disconnect rejects null session");
   Check(ggpo_advance_frame(0) == GGPO_ERRORCODE_INVALID_SESSION,
         "advance rejects null session");
   Check(ggpo_get_network_stats(0, 0, &stats) == GGPO_ERRORCODE_INVALID_SESSION,
         "network stats reject null session");
   Check(ggpo_set_disconnect_timeout(0, 1000) == GGPO_ERRORCODE_INVALID_SESSION,
         "disconnect timeout rejects null session");
   Check(ggpo_set_disconnect_notify_start(0, 500) ==
            GGPO_ERRORCODE_INVALID_SESSION,
         "disconnect notification rejects null session");
   Check(ggpo_close_session(0) == GGPO_ERRORCODE_INVALID_SESSION,
         "close rejects null session");
   ggpo_log(0, "null session log is ignored");
   LogvWrapper(0, "null session logv is ignored");
}

// Verify valid construction and terminal cleanup for all three backends.
static void TestBackendLifecycle()
{
   GGPOSessionCallbacks callbacks = Callbacks();
   GGPOSession *session = 0;
   GGPOPlayer player;
   GGPOPlayerHandle handle = GGPO_INVALID_HANDLE;
   char game[] = "api-contract";
   char host[] = "127.0.0.1";

   Check(ggpo_start_session(&session, &callbacks, game, 2, 1, 7000) == GGPO_OK,
         "P2P session starts");
   Check(session != 0, "P2P session returns an object");
   ggpo_log(session, "P2P logging remains callable");
   LogvWrapper(session, "P2P logv remains callable");

   memset(&player, 0, sizeof(player));
   player.type = GGPO_PLAYERTYPE_LOCAL;
   player.player_num = 1;
   Check(ggpo_add_player(session, &player, &handle) == GGPO_OK,
         "P2P registers a local player");
   Check(ggpo_set_frame_delay(session, handle, 1) == GGPO_OK,
         "P2P accepts frame delay for a valid player");
   Check(ggpo_set_frame_delay(session, 99, 1) ==
            GGPO_ERRORCODE_INVALID_PLAYER_HANDLE,
         "P2P rejects frame delay for an invalid player");
   Check(ggpo_add_local_input(session, handle, host, 1) ==
            GGPO_ERRORCODE_NOT_SYNCHRONIZED,
         "P2P rejects input before synchronization");
   Check(ggpo_get_network_stats(session, 99, 0) ==
            GGPO_ERRORCODE_INVALID_PLAYER_HANDLE,
         "P2P rejects stats for an invalid player before dereferencing output");
   Check(ggpo_set_disconnect_timeout(session, 1000) == GGPO_OK,
         "P2P accepts disconnect timeout configuration");
   Check(ggpo_set_disconnect_notify_start(session, 500) == GGPO_OK,
         "P2P accepts disconnect notification configuration");
   Check(ggpo_disconnect_player(session, 99) ==
            GGPO_ERRORCODE_INVALID_PLAYER_HANDLE,
         "P2P rejects disconnect for an invalid player");
   Check(ggpo_disconnect_player(session, handle) == GGPO_OK,
         "P2P disconnects a valid local player");
   // Repeated local-player disconnect behavior is not frozen here: unlike a
   // remote endpoint, the current implementation does not mark it disconnected.
   Check(ggpo_close_session(session) == GGPO_OK, "P2P session closes");

   session = 0;
   Check(ggpo_start_synctest(&session, &callbacks, game, 2, 1, 1) == GGPO_OK,
         "SyncTest session starts");
   memset(&player, 0, sizeof(player));
   player.player_num = 0;
   Check(ggpo_add_player(session, &player, &handle) ==
            GGPO_ERRORCODE_PLAYER_OUT_OF_RANGE,
         "SyncTest rejects player zero");
   player.player_num = 1;
   Check(ggpo_add_player(session, &player, &handle) == GGPO_OK && handle == 0,
         "SyncTest registers player one");
   player.player_num = 3;
   Check(ggpo_add_player(session, &player, &handle) ==
            GGPO_ERRORCODE_PLAYER_OUT_OF_RANGE,
         "SyncTest rejects player above capacity");
   Check(ggpo_idle(session, 0) == GGPO_OK && running_events == 1,
         "SyncTest idle publishes one running event");
   Check(ggpo_set_frame_delay(session, 0, 1) == GGPO_ERRORCODE_UNSUPPORTED,
         "SyncTest explicitly reports unsupported frame delay");
   Check(ggpo_close_session(session) == GGPO_OK, "SyncTest session closes");

   session = 0;
   Check(ggpo_start_spectating(&session, &callbacks, game, 2, 1, 7001,
                                host, 7002) == GGPO_OK,
         "spectator session starts");
   Check(session != 0, "spectator session returns an object");
   Check(ggpo_close_session(session) == GGPO_OK, "spectator session closes");
}

int main()
{
   TestInvalidSessionGuards();
   TestBackendLifecycle();
   return failures == 0 ? 0 : 1;
}
