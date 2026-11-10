#include "smoke_session.h"
#include "types.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#endif

// One instance represents one player in the loopback test.  The Python
// orchestrator launches two instances with opposite local/remote ports, then
// compares their result files after real Winsock synchronization and rollback.
static int
InputForPlayer(int player, int frame)
{
   return ((frame + 1) * (player * 17 + 3)) ^ (frame << player);
}

// Enable one explicit fault used only by negative harness regression tests.
static bool
FaultEnabled(const char *expected)
{
   const char *fault = getenv("GGPO_SMOKE_FAULT");
   return fault && strcmp(fault, expected) == 0;
}

// Read a monotonic test deadline without depending on a GGPO adapter revision.
static uint32
TestTimeMs()
{
#if defined(_WIN32)
   return (uint32)GetTickCount();
#else
   struct timespec current;
   if (clock_gettime(CLOCK_MONOTONIC, &current) != 0) {
      return 0;
   }
   return (uint32)((unsigned long long)current.tv_sec * 1000ULL +
                   (unsigned long long)current.tv_nsec / 1000000ULL);
#endif
}

// Yield briefly while peers exchange packets, retrying interrupted POSIX waits.
static void
TestWaitMs(int timeout_ms)
{
#if defined(_WIN32)
   Sleep((DWORD)timeout_ms);
#else
   struct timespec requested;
   requested.tv_sec = timeout_ms / 1000;
   requested.tv_nsec = (timeout_ms % 1000) * 1000000L;
   while (nanosleep(&requested, &requested) != 0 && errno == EINTR) { }
#endif
}

// Convert a GGPO result into a readable test failure.
static bool
CheckResult(GGPOErrorCode result, const char *operation)
{
   if (!GGPO_SUCCEEDED(result)) {
      fprintf(stderr, "%s failed with GGPO error %d\n", operation, result);
      return false;
   }
   return true;
}

// Accept only identity strings that can be embedded in fixture JSON without
// escaping; orchestration rejects all other values before launching peers.
static bool
SafeIdentity(const char *value)
{
   const unsigned char *cursor = (const unsigned char *)value;
   if (!value || !*value) {
      return false;
   }
   while (*cursor) {
      if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_' &&
          *cursor != '.') {
         return false;
      }
      cursor++;
   }
   return true;
}

// Publish the current deterministic state for the test-only coordinator.
static bool
WriteReadyState(const char *path)
{
   FILE *marker = fopen(path, "w");
   if (!marker) {
      fprintf(stderr, "could not create marker: %s\n", path);
      return false;
   }
   fprintf(marker,
           "{\"frame\":%d,\"canonical_checksum\":%u}\n",
           g_smoke_state.frame,
           SmokeCanonicalChecksum(&g_smoke_state));
   fclose(marker);
   return true;
}

// Check a coordinator marker without changing the production GGPO protocol.
static bool
MarkerExists(const char *path)
{
   FILE *marker = fopen(path, "r");
   if (!marker) {
      return false;
   }
   fclose(marker);
   return true;
}

// Persist the final peer state for independent cross-host comparison.
static bool
WriteResult(const char *path, int player, bool session_closed)
{
   const char *scenario = getenv("GGPO_SMOKE_SCENARIO");
   const char *run_id = getenv("GGPO_SMOKE_RUN_ID");
   const char *session_id = getenv("GGPO_SMOKE_SESSION_ID");
   const char *platform = getenv("GGPO_SMOKE_PLATFORM");
   bool schema2 = scenario || run_id || session_id || platform;
   FILE *output = fopen(path, "w");
   if (!output) {
      fprintf(stderr, "could not open result file: %s\n", path);
      return false;
   }
   if (schema2) {
      if (!SafeIdentity(scenario) || !SafeIdentity(run_id) ||
          !SafeIdentity(session_id) || !SafeIdentity(platform)) {
         fprintf(stderr, "invalid schema 2 identity\n");
         fclose(output);
         return false;
      }
      fprintf(output,
              "{\"schema\":2,\"scenario\":\"%s\","
              "\"run_id\":\"%s\",\"session_id\":\"%s\","
              "\"player\":%d,\"platform\":\"%s\","
              "\"frames\":%d,\"canonical_checksum\":%u,"
              "\"native_checksum\":%u,\"saves\":%d,\"loads\":%d,"
              "\"replay_frames\":%d,\"running_events\":%d,"
              "\"unexpected_disconnects\":%d,"
              "\"session_closed\":%s,\"exit_code\":0}\n",
              scenario, run_id, session_id, player, platform,
              g_smoke_state.frame,
              SmokeCanonicalChecksum(&g_smoke_state),
              SmokeChecksum(&g_smoke_state),
              g_smoke_metrics.saves,
              g_smoke_metrics.loads,
              g_smoke_metrics.replay_frames,
              g_smoke_metrics.running_events,
              g_smoke_metrics.unexpected_disconnects,
              session_closed ? "true" : "false");
   } else {
      fprintf(output,
              "{\"player\":%d,\"frame\":%d,\"checksum\":%u,"
              "\"saves\":%d,\"loads\":%d,\"replay_frames\":%d,"
              "\"running_events\":%d}\n",
              player,
              g_smoke_state.frame,
              SmokeChecksum(&g_smoke_state),
              g_smoke_metrics.saves,
              g_smoke_metrics.loads,
              g_smoke_metrics.replay_frames,
              g_smoke_metrics.running_events);
   }
   fclose(output);
   return true;
}

// Run one native peer through handshake, synchronized frames, and shutdown.
int
main(int argc, char **argv)
{
   GGPOSessionCallbacks callbacks;
   GGPOPlayerHandle local_handle = GGPO_INVALID_HANDLE;
#if defined(_WIN32)
   WSADATA winsock_data;
#endif
   int local_player;
   int local_port;
   int remote_port;
   int target_frames;
   uint32 start_time;

   if (argc != 6 && argc != 7) {
      fprintf(stderr,
              "usage: %s PLAYER LOCAL_PORT REMOTE_PORT FRAMES RESULT.json "
              "[REMOTE_IP]\n",
              argv[0]);
      return 2;
   }
   local_player = atoi(argv[1]);
   local_port = atoi(argv[2]);
   remote_port = atoi(argv[3]);
   target_frames = atoi(argv[4]);
   if (local_player < 1 || local_player > 2 || local_port <= 0 ||
       remote_port <= 0 || target_frames <= 0) {
      fprintf(stderr, "invalid peer-smoke arguments\n");
      return 2;
   }

   if (FaultEnabled("startup")) {
      fprintf(stderr, "fault: startup\n");
      return 91;
   }
   if (FaultEnabled("timeout")) {
      fprintf(stderr, "fault: timeout\n");
      TestWaitMs(60000);
      return 93;
   }
   if (FaultEnabled("mismatch")) {
      SmokeReset();
      g_smoke_state.frame = target_frames;
      g_smoke_state.accumulator = (unsigned int)local_player;
      g_smoke_metrics.running_events = 1;
      return WriteResult(argv[5], local_player, false) ? 0 : 1;
   }
   if (FaultEnabled("shutdown")) {
      fprintf(stderr, "fault: shutdown\n");
      return 92;
   }

#if defined(_WIN32)
   // Preserve the caller-side Winsock contract required by frozen GGPO DLLs.
   if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
      fprintf(stderr, "WSAStartup failed\n");
      return 1;
   }
#endif

   SmokeReset();
   callbacks = SmokeCallbacks();
   if (!CheckResult(ggpo_start_session(&g_smoke_session,
                                       &callbacks,
                                       "ggpo-peer-smoke",
                                       2,
                                       (int)sizeof(int),
                                       (unsigned short)local_port),
                    "ggpo_start_session")) {
      return 1;
   }

   for (int player_number = 1; player_number <= 2; player_number++) {
      GGPOPlayer player;
      GGPOPlayerHandle handle;
      memset(&player, 0, sizeof(player));
      player.size = sizeof(player);
      player.player_num = player_number;
      if (player_number == local_player) {
         player.type = GGPO_PLAYERTYPE_LOCAL;
      } else {
         player.type = GGPO_PLAYERTYPE_REMOTE;
         const char *remote_ip = argc == 7 ? argv[6] : "127.0.0.1";
         snprintf(player.u.remote.ip_address,
                  sizeof(player.u.remote.ip_address), "%s", remote_ip);
         player.u.remote.port = (unsigned short)remote_port;
      }
      if (!CheckResult(ggpo_add_player(g_smoke_session, &player, &handle),
                       "ggpo_add_player")) {
         return 1;
      }
      if (player_number == local_player) {
         local_handle = handle;
      }
   }

   // Do not submit inputs until both GGPO peers complete their handshake.
   start_time = TestTimeMs();
   while (g_smoke_metrics.running_events == 0) {
      ggpo_idle(g_smoke_session, 1);
      TestWaitMs(1);
      if (TestTimeMs() - start_time > 15000) {
         fprintf(stderr, "timed out waiting for GGPO running event\n");
         return 1;
      }
   }

   // Prediction-threshold and not-synchronized responses are normal while the
   // remote process catches up; pump GGPO and retry the same logical frame.
   start_time = TestTimeMs();
   while (g_smoke_state.frame < target_frames) {
      int local_input = InputForPlayer(local_player, g_smoke_state.frame);
      int synchronized_inputs[2] = { 0, 0 };
      int disconnect_flags = 0;
      GGPOErrorCode result = ggpo_add_local_input(g_smoke_session,
                                                   local_handle,
                                                   &local_input,
                                                   (int)sizeof(local_input));
      if (result == GGPO_ERRORCODE_PREDICTION_THRESHOLD ||
          result == GGPO_ERRORCODE_NOT_SYNCHRONIZED) {
         ggpo_idle(g_smoke_session, 1);
         TestWaitMs(1);
         continue;
      }
      if (!CheckResult(result, "ggpo_add_local_input") ||
          !CheckResult(ggpo_synchronize_input(g_smoke_session,
                                               synchronized_inputs,
                                               (int)sizeof(synchronized_inputs),
                                               &disconnect_flags),
                       "ggpo_synchronize_input")) {
         return 1;
      }
      SmokeApplyInputs(&g_smoke_state,
                       synchronized_inputs,
                       disconnect_flags);
      if (!CheckResult(ggpo_advance_frame(g_smoke_session),
                       "ggpo_advance_frame")) {
         return 1;
      }
      ggpo_idle(g_smoke_session, 0);
      if (TestTimeMs() - start_time > 30000) {
         fprintf(stderr, "timed out advancing peer session\n");
         return 1;
      }
   }

   // E2E runs use a coordinator barrier so neither peer records success while
   // its counterpart is still advancing. Legacy loopback retains its bounded
   // drain to preserve existing invocations.
   const char *ready_path = getenv("GGPO_SMOKE_READY_FILE");
   const char *finalize_path = getenv("GGPO_SMOKE_FINALIZE_FILE");
   if (ready_path || finalize_path) {
      if (!ready_path || !finalize_path || !WriteReadyState(ready_path)) {
         fprintf(stderr, "incomplete completion-barrier configuration\n");
         return 1;
      }
      start_time = TestTimeMs();
      while (!MarkerExists(finalize_path)) {
         ggpo_idle(g_smoke_session, 1);
         if (!WriteReadyState(ready_path)) {
            return 1;
         }
         TestWaitMs(1);
         if (TestTimeMs() - start_time > 15000) {
            fprintf(stderr, "timed out waiting for completion barrier\n");
            return 1;
         }
      }
   } else {
      for (int settle = 0; settle < 1000; settle++) {
         ggpo_idle(g_smoke_session, 1);
         TestWaitMs(1);
      }
   }

   if (!CheckResult(ggpo_close_session(g_smoke_session),
                    "ggpo_close_session")) {
      return 1;
   }
   g_smoke_session = NULL;
#if defined(_WIN32)
   // Balance the fixture's startup reference after GGPO closes its session.
   WSACleanup();
#endif
   if (!WriteResult(argv[5], local_player, true)) {
      return 1;
   }
   printf("peer-smoke: player=%d frame=%d checksum=%08x loads=%d replay=%d\n",
          local_player,
          g_smoke_state.frame,
          SmokeCanonicalChecksum(&g_smoke_state),
          g_smoke_metrics.loads,
          g_smoke_metrics.replay_frames);
   return 0;
}
