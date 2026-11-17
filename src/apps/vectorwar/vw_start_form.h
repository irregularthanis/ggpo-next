#ifndef _VW_START_FORM_H_
#define _VW_START_FORM_H_

/* The start screen shown when Vector War runs without arguments. Every
 * frontend (Win32 dialog, SDL form, Android launcher) collects these fields
 * and turns them into the same arguments as the command line, so a start
 * screen session and a command-line session start identically:
 *
 *   direct:      LOCAL_PORT 2 local IP:PORT      (you are player 1)
 *                LOCAL_PORT 2 IP:PORT local      (you are player 2)
 *   matchmaking: --matchmaking SERVER PLAYERS [LOBBY] LOCAL_PORT
 */

#include <stddef.h>

/* Default matchmaking server, set at build time with the CMake cache variable
 * GGPO_VECTORWAR_MATCHMAKING_HOST; empty means the player types one in. */
#ifndef VW_DEFAULT_MATCHMAKING_HOST
#define VW_DEFAULT_MATCHMAKING_HOST ""
#endif

enum VwStartMode { kVwStartDirect = 0, kVwStartMatchmaking = 1 };

#define VW_START_MAX_ARGS 6
#define VW_START_ARG_SIZE 136

struct VwStartForm {
   char local_port[8];      /* both modes */
   char player_number[4];   /* direct: "1" or "2" */
   char remote[80];         /* direct: the other player's IP:PORT */
   char server[128];        /* matchmaking: HOST[:PORT] */
   char players[4];         /* matchmaking: "2" to "4" */
   char lobby[16];          /* matchmaking: optional code to share */
};

struct VwStartArgs {
   int argc;                                   /* without a program name */
   char *argv[VW_START_MAX_ARGS];
   char storage[VW_START_MAX_ARGS][VW_START_ARG_SIZE];
};

/* Local port 7000, player 1, 2 players, the default server, public queue. */
void VwStartFormDefaults(VwStartForm *form);

/* Validates the fields for mode and fills args. On failure returns false and
 * writes a message for the screen into error. */
bool VwStartFormArgs(const VwStartForm &form, VwStartMode mode, VwStartArgs *args,
                     char *error, size_t error_length);

/* "Waiting for players... 2 of 4" for the matchmaking screens. */
void VwStartWaitingText(int joined, int group_size, char *out, size_t length);

#endif
