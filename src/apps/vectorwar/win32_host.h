#ifndef _VECTORWAR_WIN32_HOST_H_
#define _VECTORWAR_WIN32_HOST_H_

#include <windows.h>
#include "vectorwar_host.h"

/* Existing Windows behavior behind the portable VectorWarHost contract. */
class Win32VectorWarHost : public VectorWarHost {
public:
   explicit Win32VectorWarHost(HWND hwnd);

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

private:
   HWND _hwnd;
};

#endif
