#include "win32_host.h"
#include "gdi_renderer.h"
#include "ggpo_perfmon.h"
#include "vectorwar_inputs.h"

Win32VectorWarHost::Win32VectorWarHost(HWND hwnd) :
   _hwnd(hwnd)
{
}

VectorWarBounds
Win32VectorWarHost::GetGameBounds() const
{
   RECT rect = { 0 };
   GetClientRect(_hwnd, &rect);
   VectorWarBounds bounds = {
      (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom
   };
   return bounds;
}

Renderer *
Win32VectorWarHost::CreateRenderer()
{
   return new GDIRenderer(_hwnd);
}

int
Win32VectorWarHost::ReadLocalInput(int) const
{
   static const struct {
      int key;
      int input;
   } inputtable[] = {
      { VK_UP, INPUT_THRUST },
      { VK_DOWN, INPUT_BREAK },
      { VK_LEFT, INPUT_ROTATE_LEFT },
      { VK_RIGHT, INPUT_ROTATE_RIGHT },
      { 'D', INPUT_FIRE },
      { 'S', INPUT_BOMB },
   };
   int inputs = 0;
   if (GetForegroundWindow() == _hwnd) {
      for (int i = 0; i < (int)(sizeof(inputtable) / sizeof(inputtable[0])); ++i) {
         if (GetAsyncKeyState(inputtable[i].key)) {
            inputs |= inputtable[i].input;
         }
      }
   }
   return inputs;
}

unsigned int
Win32VectorWarHost::NowMs() const
{
   return timeGetTime();
}

void
Win32VectorWarHost::SleepMs(unsigned int milliseconds) const
{
   Sleep(milliseconds);
}

void
Win32VectorWarHost::InitializePerformanceMonitor()
{
   ggpoutil_perfmon_init(_hwnd);
}

void
Win32VectorWarHost::UpdatePerformanceMonitor(GGPOSession *session,
                                              GGPOPlayerHandle players[],
                                              int num_players)
{
   ggpoutil_perfmon_update(session, players, num_players);
}

void
Win32VectorWarHost::TogglePerformanceMonitor()
{
   ggpoutil_perfmon_toggle();
}

void
Win32VectorWarHost::ShutdownPerformanceMonitor()
{
   ggpoutil_perfmon_exit();
}
