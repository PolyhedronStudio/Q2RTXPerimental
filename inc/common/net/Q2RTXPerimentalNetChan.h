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

#ifndef NET_CHAN_Q2RTXPERIMENTAL_H
#define NET_CHAN_Q2RTXPERIMENTAL_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

#include "common/msg.h"
#include "common/net/net.h"
#include "common/sizebuf.h"

//============================================================================

// WID: net-code: Our own NetChan struct.
typedef struct netchan_q2rtxperimental_s {
    netchan_t   pub;

// sequencing variables
    int         incoming_reliable_acknowledged; // single bit
    int         incoming_reliable_sequence;     // single bit, maintained local
    int         reliable_sequence;          // single bit
    int         last_reliable_sequence;     // sequence number of last send

    byte        *message_buf;       // leave space for header

// message is copied to this buffer when it is first transfered
    byte        *reliable_buf;  // unacked reliable message
} netchan_q2rtxperimental_t;

/*
===============
NetchanQ2RTXPerimental_Transmit
===============
*/
size_t NetchanQ2RTXPerimental_TransmitNextFragment( netchan_t *netchan );

/*
===============
NetchanQ2RTXPerimental_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
size_t NetchanQ2RTXPerimental_Transmit( netchan_t *netchan, size_t length, const void *data, int numpackets );

/*
=================
NetchanQ2RTXPerimental_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
bool NetchanQ2RTXPerimental_Process( netchan_t *netchan );
/*
===============
NetchanQ2RTXPerimental_ShouldUpdate
================
*/
bool NetchanQ2RTXPerimental_ShouldUpdate( netchan_t *netchan );

/*
==============
NetchanQ2RTXPerimental_Setup

called to open a channel to a remote system
==============
*/
netchan_t *NetchanQ2RTXPerimental_Setup( netsrc_t sock, const netadr_t *adr, int qport, size_t maxpacketlen );


// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif

#endif // NET_CHAN_H
