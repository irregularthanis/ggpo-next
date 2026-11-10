#ifndef GGPO_TESTS_SMOKE_SESSION_H
#define GGPO_TESTS_SMOKE_SESSION_H

#include "ggponet.h"

// Minimal game-owned state used to verify GGPO save, restore, replay, and peer
// convergence.  It intentionally contains no renderer, input device, or I/O.
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
   int unexpected_disconnects;
};

extern GGPOSession *g_smoke_session;
extern SmokeState g_smoke_state;
extern SmokeMetrics g_smoke_metrics;

void SmokeReset(void);
void SmokeApplyInputs(SmokeState *state, const int inputs[2], int disconnect_flags);
unsigned int SmokeChecksum(const SmokeState *state);
unsigned int SmokeCanonicalChecksum(const SmokeState *state);
GGPOSessionCallbacks SmokeCallbacks(void);

#endif
