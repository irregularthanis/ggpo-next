#ifndef _GGPOUTIL_PERFMON_H
#define _GGPOUTIL_PERFMON_H

#include "ggponet.h"
#include "vw_legend.h"

void ggpoutil_perfmon_init(HWND hwnd);
void ggpoutil_perfmon_update(GGPOSession *ggpo, GGPOPlayerHandle players[], int num_players);
void ggpoutil_perfmon_update_paths(const VwLegend &legend);
void ggpoutil_perfmon_toggle();
void ggpoutil_perfmon_exit();


#endif
