#ifndef _VECTORWAR_STATE_CONTRACT_H_
#define _VECTORWAR_STATE_CONTRACT_H_

#include "gamestate.h"

/* Hash an explicit byte representation; native padding is never included. */
unsigned int VectorWarCanonicalChecksum(const GameState &state);

#endif
