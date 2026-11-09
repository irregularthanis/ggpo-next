#include "types.h"
#include "sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TestState {
   int frame;
   unsigned int value;
};

struct TestMetrics {
   int saves;
   int loads;
   int frees;
   int replay_frames;
};

static Sync *active_sync = 0;
static TestState state = { 0, 0 };
static TestMetrics metrics = { 0, 0, 0, 0 };
static int failures = 0;

// Record a focused rollback-contract failure.
static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "sync contract failed: %s\n", contract);
      ++failures;
   }
}

// Apply the independent deterministic state transition used by the oracle.
static void Apply(TestState *target, unsigned char local, unsigned char remote)
{
   target->value = target->value * 33u + local * 3u + remote * 5u + target->frame;
   ++target->frame;
}

static bool GGPO_CALL BeginGame(const char *) { return true; }

// Save the complete test state and count ownership transfers.
static bool GGPO_CALL SaveState(unsigned char **buffer, int *len,
                                int *checksum, int)
{
   *len = (int)sizeof(state);
   *buffer = (unsigned char *)malloc(*len);
   if (!*buffer) {
      return false;
   }
   memcpy(*buffer, &state, *len);
   *checksum = (int)state.value;
   ++metrics.saves;
   return true;
}

// Restore the exact state selected by Sync for rollback.
static bool GGPO_CALL LoadState(unsigned char *buffer, int len)
{
   if (len != (int)sizeof(state)) {
      return false;
   }
   memcpy(&state, buffer, len);
   ++metrics.loads;
   return true;
}

static bool GGPO_CALL LogState(char *, unsigned char *, int) { return true; }

// Release every snapshot allocated by SaveState.
static void GGPO_CALL FreeBuffer(void *buffer)
{
   if (buffer) {
      ++metrics.frees;
   }
   free(buffer);
}

// Replay one frame through production Sync after a restored snapshot.
static bool GGPO_CALL AdvanceFrame(int)
{
   unsigned char inputs[2] = { 0, 0 };
   active_sync->SynchronizeInputs(inputs, (int)sizeof(inputs));
   Apply(&state, inputs[0], inputs[1]);
   ++metrics.replay_frames;
   active_sync->IncrementFrame();
   return true;
}

static bool GGPO_CALL OnEvent(GGPOEvent *) { return true; }

// Assemble the callback interface consumed by production Sync.
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

// Construct a one-byte queue input for a specific confirmed frame.
static GameInput MakeInput(int frame, unsigned char value)
{
   char bits[1];
   GameInput input;
   bits[0] = (char)value;
   input.init(frame, bits, 1);
   return input;
}

int main()
{
   UdpMsg::connect_status status[2];
   Sync::Config config;
   TestState expected = { 0, 0 };
   unsigned char values[2];
   GameInput input;

   memset(status, 0, sizeof(status));
   memset(&config, 0, sizeof(config));
   config.callbacks = Callbacks();
   config.num_prediction_frames = MAX_PREDICTION_FRAMES;
   config.num_players = 2;
   config.input_size = 1;

   {
      Sync sync(status);
      active_sync = &sync;
      sync.Init(config);

      // Execute frame zero with confirmed local and remote inputs.
      input = MakeInput(0, 1);
      Check(sync.AddLocalInput(0, input), "frame zero local input is accepted");
      input = MakeInput(0, 2);
      sync.AddRemoteInput(1, input);
      sync.SynchronizeInputs(values, (int)sizeof(values));
      Apply(&state, values[0], values[1]);
      sync.IncrementFrame();

      // Execute two frames using the last remote value as a prediction.
      input = MakeInput(1, 3);
      Check(sync.AddLocalInput(0, input), "frame one local input is accepted");
      sync.SynchronizeInputs(values, (int)sizeof(values));
      Apply(&state, values[0], values[1]);
      sync.IncrementFrame();

      input = MakeInput(2, 4);
      Check(sync.AddLocalInput(0, input), "frame two local input is accepted");
      sync.SynchronizeInputs(values, (int)sizeof(values));
      Apply(&state, values[0], values[1]);
      sync.IncrementFrame();

      // Confirm different remote values and force production rollback/replay.
      input = MakeInput(1, 9);
      sync.AddRemoteInput(1, input);
      input = MakeInput(2, 8);
      sync.AddRemoteInput(1, input);
      sync.CheckSimulation(0);

      // Cause a second mismatch to prove the same pool of snapshots is reusable.
      input = MakeInput(3, 5);
      Check(sync.AddLocalInput(0, input), "frame three local input is accepted");
      sync.SynchronizeInputs(values, (int)sizeof(values));
      Apply(&state, values[0], values[1]);
      sync.IncrementFrame();
      input = MakeInput(3, 7);
      sync.AddRemoteInput(1, input);
      sync.CheckSimulation(0);

      Apply(&expected, 1, 2);
      Apply(&expected, 3, 9);
      Apply(&expected, 4, 8);
      Apply(&expected, 5, 7);
      Check(metrics.loads == 2, "two mismatches load two snapshots");
      Check(metrics.replay_frames == 3, "rollbacks replay exactly three frames");
      Check(sync.GetFrameCount() == 4, "replay returns to the original frame");
      Check(state.frame == expected.frame && state.value == expected.value,
            "replayed state converges with the independent oracle");
   }

   Check(metrics.saves > 0, "sync captured rollback snapshots");
   Check(metrics.frees == metrics.saves, "every snapshot buffer is released");

   // Cycle beyond the saved-state ring and verify overwritten buffers are freed.
   memset(&state, 0, sizeof(state));
   memset(&metrics, 0, sizeof(metrics));
   memset(status, 0, sizeof(status));
   config.num_players = 1;
   config.num_prediction_frames = 64;
   {
      Sync sync(status);
      active_sync = &sync;
      sync.Init(config);
      for (int frame = 0; frame < 12; ++frame) {
         input = MakeInput(frame, (unsigned char)frame);
         Check(sync.AddLocalInput(0, input), "retention input is accepted");
         sync.SynchronizeInputs(values, 1);
         ++state.frame;
         state.value += values[0];
         sync.IncrementFrame();
      }
      Check(metrics.frees >= 3, "snapshot ring frees overwritten buffers");
   }
   Check(metrics.frees == metrics.saves,
         "retained snapshots are released at destruction");

   // Stop accepting local input when the configured prediction window is full.
   memset(&state, 0, sizeof(state));
   memset(&metrics, 0, sizeof(metrics));
   memset(status, 0, sizeof(status));
   config.num_prediction_frames = 2;
   {
      Sync sync(status);
      active_sync = &sync;
      sync.Init(config);
      input = MakeInput(0, 1);
      Check(sync.AddLocalInput(0, input), "first prediction-window input passes");
      sync.IncrementFrame();
      input = MakeInput(1, 2);
      Check(sync.AddLocalInput(0, input), "second prediction-window input passes");
      sync.IncrementFrame();
      input = MakeInput(2, 3);
      Check(!sync.AddLocalInput(0, input),
            "input is rejected at the prediction threshold");
   }
   return failures == 0 ? 0 : 1;
}
