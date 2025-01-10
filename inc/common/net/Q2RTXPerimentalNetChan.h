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


#include "common/messaging.h"
#include "common/net/net.h"
#include "common/sizebuf.h"


/**
*	@brief
**/
size_t NetchanQ2RTXPerimental_TransmitNextFragment( netchan_t *chan );

/**
*	@brief	Tries to send an unreliable message to a connection, and handles the
*			transmition / retransmition of the reliable messages.
*
*			A 0 length will still generate a packet and deal with the reliable messages.
**/
size_t NetchanQ2RTXPerimental_Transmit( netchan_t *chan, size_t length, const void *data );

/**
*	@brief	Called when the current net_message is from remote_address.
*			Modifies net_message so that it points to the packet payload
**/
bool NetchanQ2RTXPerimental_Process( netchan_t *chan );
/**
*	@brief
**/
bool NetchanQ2RTXPerimental_ShouldUpdate( netchan_t *chan );


#endif // NET_CHAN_H
