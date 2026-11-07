#include <ggponet.h>

// Compile every public callback through the installed C11 header. The call to
// ggpo_idle also forces the linker to resolve a real SDK export.
static bool GGPO_CALL begin_game(const char *game) { return game != 0; }
static bool GGPO_CALL save_state(unsigned char **buffer, int *len,
                                 int *checksum, int frame)
{
   (void)buffer; (void)len; (void)checksum; (void)frame;
   return true;
}
static bool GGPO_CALL load_state(unsigned char *buffer, int len)
{
   (void)buffer; (void)len;
   return true;
}
static bool GGPO_CALL log_state(char *filename, unsigned char *buffer, int len)
{
   (void)filename; (void)buffer; (void)len;
   return true;
}
static void GGPO_CALL free_buffer(void *buffer) { (void)buffer; }
static bool GGPO_CALL advance_frame(int flags) { return flags == 0; }
static bool GGPO_CALL on_event(GGPOEvent *event) { return event != 0; }

int main(void)
{
   GGPOSessionCallbacks callbacks = {
      begin_game, save_state, load_state, log_state,
      free_buffer, advance_frame, on_event
   };
   (void)callbacks;
   return ggpo_idle((GGPOSession *)0, 0) == GGPO_ERRORCODE_INVALID_SESSION ? 0 : 1;
}
