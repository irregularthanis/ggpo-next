#include "headless_host.h"
#include "vectorwar_inputs.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#endif

HeadlessVectorWarHost::HeadlessVectorWarHost(int player) : _player(player) { }

VectorWarBounds
HeadlessVectorWarHost::GetGameBounds() const
{
   VectorWarBounds bounds = { 0, 0, 640, 480 };
   return bounds;
}

Renderer *HeadlessVectorWarHost::CreateRenderer() { return NULL; }

int
HeadlessVectorWarHost::ReadLocalInput(int frame) const
{
   /* Rotation exercises distinct per-player inputs without invoking libm in
    * movement, keeping the native cross-platform oracle bit-exact. */
   return ((frame / 15 + _player) % 2) ? INPUT_ROTATE_LEFT : INPUT_ROTATE_RIGHT;
}

unsigned int
HeadlessVectorWarHost::NowMs() const
{
#if defined(_WIN32)
   return (unsigned int)GetTickCount();
#else
   struct timespec current;
   if (clock_gettime(CLOCK_MONOTONIC, &current) != 0) {
      return 0;
   }
   return (unsigned int)((unsigned long long)current.tv_sec * 1000ULL +
                         (unsigned long long)current.tv_nsec / 1000000ULL);
#endif
}

void
HeadlessVectorWarHost::SleepMs(unsigned int milliseconds) const
{
#if defined(_WIN32)
   Sleep((DWORD)milliseconds);
#else
   struct timespec requested;
   requested.tv_sec = milliseconds / 1000;
   requested.tv_nsec = (milliseconds % 1000) * 1000000L;
   while (nanosleep(&requested, &requested) != 0 && errno == EINTR) { }
#endif
}

void HeadlessVectorWarHost::InitializePerformanceMonitor() { }
void HeadlessVectorWarHost::UpdatePerformanceMonitor(GGPOSession *, GGPOPlayerHandle [], int) { }
void HeadlessVectorWarHost::TogglePerformanceMonitor() { }
void HeadlessVectorWarHost::ShutdownPerformanceMonitor() { }
