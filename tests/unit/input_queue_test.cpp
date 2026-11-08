#include "types.h"
#include "input_queue.h"

#include <stdio.h>

static int failures = 0;

// Record a precise contract failure without relying on a test framework.
static void Check(bool condition, const char *contract)
{
   if (!condition) {
      fprintf(stderr, "input queue contract failed: %s\n", contract);
      ++failures;
   }
}

// Construct a one-byte input with a deterministic value.
static GameInput MakeInput(int frame, unsigned char value)
{
   char bits[1];
   GameInput input;
   bits[0] = (char)value;
   input.init(frame, bits, 1);
   return input;
}

// Verify confirmed input, prediction, mismatch detection, and recovery.
static void TestPredictionLifecycle()
{
   InputQueue queue(1);
   GameInput input = MakeInput(0, 3);
   GameInput output;

   queue.AddInput(input);
   Check(input.frame == 0, "first input remains frame zero");
   Check(queue.GetInput(0, &output), "confirmed frame returns true");
   Check((unsigned char)output.bits[0] == 3, "confirmed value is preserved");

   Check(!queue.GetInput(1, &output), "missing frame enters prediction");
   Check((unsigned char)output.bits[0] == 3, "prediction repeats last value");
   input = MakeInput(1, 3);
   queue.AddInput(input);
   Check(queue.GetFirstIncorrectFrame() == GameInput::NullFrame,
         "matching confirmation leaves prediction correct");

   Check(!queue.GetInput(2, &output), "next missing frame predicts again");
   input = MakeInput(2, 9);
   queue.AddInput(input);
   Check(queue.GetFirstIncorrectFrame() == 2,
         "first mismatching confirmation is reported");
   queue.ResetPrediction(2);
   Check(queue.GetFirstIncorrectFrame() == GameInput::NullFrame,
         "prediction reset clears mismatch");
   Check(queue.GetInput(2, &output), "confirmed mismatch frame is readable");
   Check((unsigned char)output.bits[0] == 9,
         "confirmed value replaces prediction");
}

// Verify delay padding and ring wrap without exceeding queue capacity.
static void TestDelayAndWrap()
{
   InputQueue delayed(1);
   GameInput input;
   GameInput output;
   int frame;

   delayed.SetFrameDelay(2);
   input = MakeInput(0, 7);
   delayed.AddInput(input);
   Check(input.frame == 2, "frame delay moves the submitted frame");
   Check(delayed.GetConfirmedInput(0, &output), "delay creates first padding frame");
   Check((unsigned char)output.bits[0] == 0, "padding begins with neutral input");
   Check(delayed.GetConfirmedInput(2, &output), "delayed frame is confirmed");
   Check((unsigned char)output.bits[0] == 7, "delayed value is preserved");

   InputQueue wrapped(1);
   for (frame = 0; frame < INPUT_QUEUE_LENGTH; ++frame) {
      input = MakeInput(frame, (unsigned char)(frame & 0xff));
      wrapped.AddInput(input);
   }
   wrapped.DiscardConfirmedFrames(63);
   for (frame = INPUT_QUEUE_LENGTH; frame < INPUT_QUEUE_LENGTH + 64; ++frame) {
      input = MakeInput(frame, (unsigned char)(frame & 0xff));
      wrapped.AddInput(input);
   }
   Check(wrapped.GetConfirmedInput(INPUT_QUEUE_LENGTH + 63, &output),
         "wrapped queue retains newest confirmed frame");
   Check((unsigned char)output.bits[0] ==
            (unsigned char)((INPUT_QUEUE_LENGTH + 63) & 0xff),
         "wrapped queue retains the correct value");
}

int main()
{
   TestPredictionLifecycle();
   TestDelayAndWrap();
   return failures == 0 ? 0 : 1;
}
