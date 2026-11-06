#ifndef GGPO_TESTS_SMOKE_SESSION_H
#define GGPO_TESTS_SMOKE_SESSION_H

#include "ggponet.h"

struct SmokeState {
   int frame;
   unsigned int accumulator;
   int disconnect_flags;
};

struct SmokeMetrics {
   int saves;
   int loads;
   int replay_frames;
   int running_events;
};

extern GGPOSession *g_smoke_session;
extern SmokeState g_smoke_state;
extern SmokeMetrics g_smoke_metrics;

void SmokeReset(void);
void SmokeApplyInputs(SmokeState *state, const int inputs[2], int disconnect_flags);
unsigned int SmokeChecksum(const SmokeState *state);
GGPOSessionCallbacks SmokeCallbacks(void);

#endif
