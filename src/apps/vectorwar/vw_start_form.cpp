#include "vw_start_form.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

/* Trimmed copy: fields come from text boxes. */
void Trim(const char *in, char *out, size_t length)
{
   while (*in && isspace((unsigned char)*in)) ++in;
   size_t n = strlen(in);
   while (n && isspace((unsigned char)in[n - 1])) --n;
   if (n >= length) n = length - 1;
   memcpy(out, in, n);
   out[n] = '\0';
}

bool IsNumber(const char *text, long low, long high)
{
   if (!*text) return false;
   for (const char *p = text; *p; ++p) {
      if (!isdigit((unsigned char)*p)) return false;
   }
   if (strlen(text) > 5) return false;
   const long value = strtol(text, NULL, 10);
   return value >= low && value <= high;
}

/* IP:PORT with a dotted IPv4 address, as GGPO's endpoints need. */
bool IsRemote(const char *text)
{
   unsigned int a, b, c, d;
   char port[8];
   char extra;
   if (sscanf(text, "%u.%u.%u.%u:%7[0-9]%c", &a, &b, &c, &d, port, &extra) != 5) return false;
   return a < 256 && b < 256 && c < 256 && d < 256 && IsNumber(port, 1, 65535);
}

bool IsLobby(const char *text)
{
   if (strlen(text) > 8) return false;
   for (const char *p = text; *p; ++p) {
      if (!isalnum((unsigned char)*p)) return false;
   }
   return true;
}

void Add(VwStartArgs *args, const char *value)
{
   char *slot = args->storage[args->argc];
   snprintf(slot, VW_START_ARG_SIZE, "%s", value);
   args->argv[args->argc++] = slot;
}

bool Fail(char *error, size_t length, const char *message)
{
   if (error && length) snprintf(error, length, "%s", message);
   return false;
}

}  // namespace

void VwStartFormDefaults(VwStartForm *form)
{
   memset(form, 0, sizeof(*form));
   snprintf(form->local_port, sizeof(form->local_port), "7000");
   snprintf(form->player_number, sizeof(form->player_number), "1");
   snprintf(form->server, sizeof(form->server), "%s", VW_DEFAULT_MATCHMAKING_HOST);
   snprintf(form->players, sizeof(form->players), "2");
}

bool VwStartFormArgs(const VwStartForm &form, VwStartMode mode, VwStartArgs *args,
                     char *error, size_t error_length)
{
   memset(args, 0, sizeof(*args));
   char local_port[sizeof(form.local_port)];
   Trim(form.local_port, local_port, sizeof(local_port));

   if (mode == kVwStartDirect) {
      char number[sizeof(form.player_number)], remote[sizeof(form.remote)];
      Trim(form.player_number, number, sizeof(number));
      Trim(form.remote, remote, sizeof(remote));
      if (!IsNumber(local_port, 1, 65535)) return Fail(error, error_length, "Local port must be 1 to 65535");
      if (strcmp(number, "1") != 0 && strcmp(number, "2") != 0) {
         return Fail(error, error_length, "You must be player 1 or 2");
      }
      if (!IsRemote(remote)) return Fail(error, error_length, "Other player must be IP:PORT, like 192.168.1.20:7001");
      Add(args, local_port);
      Add(args, "2");
      Add(args, number[0] == '1' ? "local" : remote);
      Add(args, number[0] == '1' ? remote : "local");
      return true;
   }

   char server[sizeof(form.server)], players[sizeof(form.players)], lobby[sizeof(form.lobby)];
   Trim(form.server, server, sizeof(server));
   Trim(form.players, players, sizeof(players));
   Trim(form.lobby, lobby, sizeof(lobby));
   if (!server[0] || strchr(server, ' ')) return Fail(error, error_length, "Enter a matchmaking server");
   if (!IsNumber(players, 2, 4)) return Fail(error, error_length, "Players must be 2, 3 or 4");
   if (!IsLobby(lobby)) return Fail(error, error_length, "Lobby code is up to 8 letters or digits");
   if (!IsNumber(local_port, 0, 65535)) return Fail(error, error_length, "Local port must be 0 to 65535");
   for (char *p = lobby; *p; ++p) *p = (char)toupper((unsigned char)*p);
   Add(args, "--matchmaking");
   Add(args, server);
   Add(args, players);
   if (lobby[0]) Add(args, lobby);
   Add(args, local_port);
   return true;
}

void VwStartWaitingText(int joined, int group_size, char *out, size_t length)
{
   if (joined > 0 && group_size > 0) {
      snprintf(out, length, "Waiting for players... %d of %d", joined, group_size);
   } else {
      snprintf(out, length, "Contacting the matchmaking server...");
   }
}
