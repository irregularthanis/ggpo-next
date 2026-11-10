#ifndef _SDL_VECTORWAR_HOST_H_
#define _SDL_VECTORWAR_HOST_H_

#include "vectorwar_host.h"

/* Native SDL2 services for the interactive Linux frontend. */
class SDLVectorWarHost : public VectorWarHost {
public:
   SDLVectorWarHost();
   virtual VectorWarBounds GetGameBounds() const;
   virtual Renderer *CreateRenderer();
   virtual int ReadLocalInput(int frame) const;
   virtual unsigned int NowMs() const;
   virtual void SleepMs(unsigned int milliseconds) const;
   virtual void InitializePerformanceMonitor();
   virtual void UpdatePerformanceMonitor(GGPOSession *session,
                                         GGPOPlayerHandle players[],
                                         int num_players);
   virtual void TogglePerformanceMonitor();
   virtual void ShutdownPerformanceMonitor();
};

#endif
