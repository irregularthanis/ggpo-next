// The start screen's fields become exactly the command-line arguments, so a
// session started from any start screen starts like the command line does.
#include <stdio.h>
#include <string.h>

#include "vw_start_form.h"

static int failures = 0;

static void ExpectArgs(const VwStartForm &form, VwStartMode mode, const char *expected)
{
   VwStartArgs args;
   char error[96] = "";
   char joined[256] = "";
   if (VwStartFormArgs(form, mode, &args, error, sizeof(error))) {
      for (int i = 0; i < args.argc; ++i) {
         if (i) strcat(joined, " ");
         strcat(joined, args.argv[i]);
      }
   } else {
      snprintf(joined, sizeof(joined), "error: %s", error);
   }
   if (strcmp(joined, expected) != 0) {
      fprintf(stderr, "FAIL: got \"%s\", expected \"%s\"\n", joined, expected);
      ++failures;
   }
}

int main()
{
   VwStartForm form;
   VwStartFormDefaults(&form);
   if (strcmp(form.local_port, "7000") || strcmp(form.player_number, "1") ||
       strcmp(form.players, "2") || strcmp(form.server, VW_DEFAULT_MATCHMAKING_HOST) || form.lobby[0]) {
      fprintf(stderr, "FAIL: defaults\n");
      ++failures;
   }

   // Direct: the remote player goes in the other player's slot.
   snprintf(form.remote, sizeof(form.remote), " 192.168.1.20:7001 ");
   ExpectArgs(form, kVwStartDirect, "7000 2 local 192.168.1.20:7001");
   snprintf(form.player_number, sizeof(form.player_number), "2");
   ExpectArgs(form, kVwStartDirect, "7000 2 192.168.1.20:7001 local");
   snprintf(form.player_number, sizeof(form.player_number), "3");
   ExpectArgs(form, kVwStartDirect, "error: You must be player 1 or 2");
   snprintf(form.player_number, sizeof(form.player_number), "1");
   snprintf(form.remote, sizeof(form.remote), "friend.example:7001");
   ExpectArgs(form, kVwStartDirect, "error: Other player must be IP:PORT, like 192.168.1.20:7001");
   snprintf(form.remote, sizeof(form.remote), "10.0.0.300:7001");
   ExpectArgs(form, kVwStartDirect, "error: Other player must be IP:PORT, like 192.168.1.20:7001");
   snprintf(form.remote, sizeof(form.remote), "10.0.0.3:7001");
   snprintf(form.local_port, sizeof(form.local_port), "0");
   ExpectArgs(form, kVwStartDirect, "error: Local port must be 1 to 65535");

   // Matchmaking: lobby optional and upper-cased; port 0 picks any.
   snprintf(form.server, sizeof(form.server), "vw.example.org:7100");
   snprintf(form.players, sizeof(form.players), "4");
   ExpectArgs(form, kVwStartMatchmaking, "--matchmaking vw.example.org:7100 4 0");
   snprintf(form.lobby, sizeof(form.lobby), "fri3nds");
   snprintf(form.local_port, sizeof(form.local_port), "7000");
   ExpectArgs(form, kVwStartMatchmaking, "--matchmaking vw.example.org:7100 4 FRI3NDS 7000");
   snprintf(form.lobby, sizeof(form.lobby), "two words");
   ExpectArgs(form, kVwStartMatchmaking, "error: Lobby code is up to 8 letters or digits");
   form.lobby[0] = '\0';
   snprintf(form.players, sizeof(form.players), "5");
   ExpectArgs(form, kVwStartMatchmaking, "error: Players must be 2, 3 or 4");
   snprintf(form.players, sizeof(form.players), "2");
   snprintf(form.server, sizeof(form.server), "  ");
   ExpectArgs(form, kVwStartMatchmaking, "error: Enter a matchmaking server");

   char text[64];
   VwStartWaitingText(2, 4, text, sizeof(text));
   if (strcmp(text, "Waiting for players... 2 of 4") != 0) {
      fprintf(stderr, "FAIL: waiting text \"%s\"\n", text);
      ++failures;
   }

   if (failures) {
      fprintf(stderr, "vectorwar-start-form: %d failure(s)\n", failures);
      return 1;
   }
   printf("vectorwar-start-form: direct and matchmaking arguments pass\n");
   return 0;
}
