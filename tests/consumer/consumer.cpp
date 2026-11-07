#include <ggponet.h>

// Keep a C++98 consumer beside the C11 fixture so both supported declaration
// paths resolve the same installed library symbol.
int main()
{
   GGPOPlayer player = {};
   player.size = sizeof(player);
   return ggpo_idle(0, 0) == GGPO_ERRORCODE_INVALID_SESSION ? 0 : 1;
}
