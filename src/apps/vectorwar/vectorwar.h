#ifndef _VECTORWAR_H
#define _VECTORWAR_H

#include "ggponet.h"
#include "vectorwar_inputs.h"

/*
 * vectorwar.h --
 *
 * Interface to the vector war application.
 *
 */

void VectorWar_Init(HWND hwnd, unsigned short localport, int num_players, GGPOPlayer *players, int num_spectators);
void VectorWar_InitSpectator(HWND hwnd, unsigned short localport, int num_players, char *host_ip, unsigned short host_port);
void VectorWar_DrawCurrentFrame();
void VectorWar_AdvanceFrame(int inputs[], int disconnect_flags);
void VectorWar_RunFrame(HWND hwnd);
void VectorWar_Idle(int time);
void VectorWar_DisconnectPlayer(int player);
void VectorWar_Exit();

#define ARRAY_SIZE(n)      (sizeof(n) / sizeof(n[0]))
#define FRAME_DELAY        2

#endif
