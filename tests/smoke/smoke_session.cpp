#include "smoke_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GGPOSession *g_smoke_session = NULL;
SmokeState g_smoke_state = { 0, 0, 0 };
SmokeMetrics g_smoke_metrics = { 0, 0, 0, 0 };

void
SmokeReset(void)
{
   memset(&g_smoke_state, 0, sizeof(g_smoke_state));
   memset(&g_smoke_metrics, 0, sizeof(g_smoke_metrics));
   g_smoke_session = NULL;
}

void
SmokeApplyInputs(SmokeState *state, const int inputs[2], int disconnect_flags)
{
   state->accumulator = state->accumulator * 33u;
   state->accumulator += (unsigned int)(inputs[0] * 3);
   state->accumulator += (unsigned int)(inputs[1] * 5);
   state->accumulator += (unsigned int)state->frame;
   state->disconnect_flags = disconnect_flags;
   state->frame++;
}

unsigned int
SmokeChecksum(const SmokeState *state)
{
   const unsigned char *bytes = (const unsigned char *)state;
   unsigned int checksum = 2166136261u;
   size_t i;
   for (i = 0; i < sizeof(*state); i++) {
      checksum ^= bytes[i];
      checksum *= 16777619u;
   }
   return checksum;
}

static bool __cdecl
SmokeBeginGame(const char *)
{
   return true;
}

static bool __cdecl
SmokeSaveState(unsigned char **buffer, int *len, int *checksum, int)
{
   *len = (int)sizeof(g_smoke_state);
   *buffer = (unsigned char *)malloc(*len);
   if (!*buffer) {
      return false;
   }
   memcpy(*buffer, &g_smoke_state, *len);
   *checksum = (int)SmokeChecksum(&g_smoke_state);
   g_smoke_metrics.saves++;
   return true;
}

static bool __cdecl
SmokeLoadState(unsigned char *buffer, int len)
{
   if (len != (int)sizeof(g_smoke_state)) {
      return false;
   }
   memcpy(&g_smoke_state, buffer, len);
   g_smoke_metrics.loads++;
   return true;
}

static bool __cdecl
SmokeLogState(char *, unsigned char *, int)
{
   return true;
}

static void __cdecl
SmokeFreeBuffer(void *buffer)
{
   free(buffer);
}

static bool __cdecl
SmokeAdvanceFrame(int)
{
   int inputs[2] = { 0, 0 };
   int disconnect_flags = 0;
   GGPOErrorCode result = ggpo_synchronize_input(g_smoke_session,
                                                  inputs,
                                                  (int)sizeof(inputs),
                                                  &disconnect_flags);
   if (!GGPO_SUCCEEDED(result)) {
      return false;
   }
   SmokeApplyInputs(&g_smoke_state, inputs, disconnect_flags);
   g_smoke_metrics.replay_frames++;
   return GGPO_SUCCEEDED(ggpo_advance_frame(g_smoke_session));
}

static bool __cdecl
SmokeOnEvent(GGPOEvent *info)
{
   if (info->code == GGPO_EVENTCODE_RUNNING) {
      g_smoke_metrics.running_events++;
   }
   return true;
}

GGPOSessionCallbacks
SmokeCallbacks(void)
{
   GGPOSessionCallbacks callbacks;
   memset(&callbacks, 0, sizeof(callbacks));
   callbacks.begin_game = SmokeBeginGame;
   callbacks.save_game_state = SmokeSaveState;
   callbacks.load_game_state = SmokeLoadState;
   callbacks.log_game_state = SmokeLogState;
   callbacks.free_buffer = SmokeFreeBuffer;
   callbacks.advance_frame = SmokeAdvanceFrame;
   callbacks.on_event = SmokeOnEvent;
   return callbacks;
}
