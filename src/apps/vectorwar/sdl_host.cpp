#include "sdl_host.h"
#include "sdl_renderer.h"
#include "vectorwar_inputs.h"

#include "vw_sdl.h"
#if defined(__ANDROID__)
#include "sdl_touch_controls.h"
#endif

SDLVectorWarHost::SDLVectorWarHost() { }

VectorWarBounds
SDLVectorWarHost::GetGameBounds() const
{
   VectorWarBounds bounds = { 0, 0, 640, 480 };
   return bounds;
}

Renderer *SDLVectorWarHost::CreateRenderer() { return new SDLVectorWarRenderer(); }

int
SDLVectorWarHost::ReadLocalInput(int) const
{
   SDL_PumpEvents();
   const unsigned char *keys = SDL_GetKeyboardState(NULL);
   int input = 0;
   if (keys[SDL_SCANCODE_UP]) input |= INPUT_THRUST;
   if (keys[SDL_SCANCODE_DOWN]) input |= INPUT_BREAK;
   if (keys[SDL_SCANCODE_LEFT]) input |= INPUT_ROTATE_LEFT;
   if (keys[SDL_SCANCODE_RIGHT]) input |= INPUT_ROTATE_RIGHT;
   if (keys[SDL_SCANCODE_D]) input |= INPUT_FIRE;
   if (keys[SDL_SCANCODE_S]) input |= INPUT_BOMB;
#if defined(__ANDROID__)
   input |= vw_touch::ReadInput();
#endif
   return input;
}

unsigned int SDLVectorWarHost::NowMs() const { return SDL_GetTicks(); }
void SDLVectorWarHost::SleepMs(unsigned int milliseconds) const { SDL_Delay(milliseconds); }
void SDLVectorWarHost::InitializePerformanceMonitor() { }
void SDLVectorWarHost::UpdatePerformanceMonitor(GGPOSession *, GGPOPlayerHandle [], int) { }
void SDLVectorWarHost::UpdatePathMonitor(const VwLegend &) { }
void SDLVectorWarHost::TogglePerformanceMonitor() { }
void SDLVectorWarHost::ShutdownPerformanceMonitor() { }
