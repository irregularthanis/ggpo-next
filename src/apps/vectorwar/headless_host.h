#ifndef _VECTORWAR_HEADLESS_HOST_H_
#define _VECTORWAR_HEADLESS_HOST_H_

#include "vectorwar_host.h"

/* Platform-neutral test host: scripted input, no window, renderer, or UI. */
class HeadlessVectorWarHost : public VectorWarHost {
public:
   explicit HeadlessVectorWarHost(int player);

   virtual VectorWarBounds GetGameBounds() const;
   virtual Renderer *CreateRenderer();
   virtual int ReadLocalInput(int frame) const;
   virtual unsigned int NowMs() const;
   virtual void SleepMs(unsigned int milliseconds) const;
   virtual void InitializePerformanceMonitor();
   virtual void UpdatePerformanceMonitor(GGPOSession *, GGPOPlayerHandle [], int);
   virtual void TogglePerformanceMonitor();
   virtual void ShutdownPerformanceMonitor();

private:
   int _player;
};

#endif
