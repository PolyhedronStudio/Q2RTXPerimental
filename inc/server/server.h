/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#pragma once


#include "common/net/net.h"

/**
 * @brief Client bandwidth throttling thresholds, in bytes per second. Clients
 * may actually request that the server drops messages for them above a certain
 * bandwidth saturation point in order to maintain some level of connectivity.
 * However, they must accept at least 16KB/s. (Used to be 8.)
 */
#define CLIENT_RATE_MIN		( 8192*2 )

// if this is changed, Q2PRO protocol version must be changed too!
typedef enum {
    ss_dead,            // no map loaded
    ss_loading,         // spawning level edicts
    ss_game,            // actively running
    ss_pic,             // showing static picture
    ss_cinematic,       // playing a cinematic
} server_state_t;

#if USE_ICMP
void SV_ErrorEvent(netadr_t *from, int ee_errno, int ee_info);
#endif
void SV_Init(void);
void SV_Shutdown(const char *finalmsg, error_type_t type);
int64_t SV_Frame( uint64_t msec );
#if USE_SYSCON
void SV_SetConsoleTitle(void);
#endif
//void SV_ConsoleOutput(const char *msg);

#if USE_CLIENT
char *SV_GetSaveInfo(const char *dir);
#endif
