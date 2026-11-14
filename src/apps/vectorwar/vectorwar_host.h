#ifndef _VECTORWAR_HOST_H_
#define _VECTORWAR_HOST_H_

#include "gamestate.h"
#include "ggponet.h"
#include "vw_legend.h"

class Renderer;

/* Services supplied by an interactive or headless application host. */
class VectorWarHost {
public:
   virtual ~VectorWarHost() { }

   virtual VectorWarBounds GetGameBounds() const = 0;
   virtual Renderer *CreateRenderer() = 0;
   virtual int ReadLocalInput(int frame) const = 0;
   virtual unsigned int NowMs() const = 0;
   virtual void SleepMs(unsigned int milliseconds) const = 0;

   virtual void InitializePerformanceMonitor() = 0;
   virtual void UpdatePerformanceMonitor(GGPOSession *session,
                                         GGPOPlayerHandle players[],
                                         int num_players) = 0;
   virtual void UpdatePathMonitor(const VwLegend &legend) = 0;
   virtual void TogglePerformanceMonitor() = 0;
   virtual void ShutdownPerformanceMonitor() = 0;
};

#endif
