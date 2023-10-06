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

#include "shared/shared.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/msg.h"
#include "common/net/chan.h"
#include "common/net/Q2RTXPerimentalNetChan.h"
#include "common/net/net.h"
#include "common/protocol.h"
#include "common/sizebuf.h"
#include "common/zone.h"
#include "system/system.h"

/*

packet header
-------------
31  sequence
1   does this message contain a reliable payload
31  acknowledge sequence
1   acknowledge receipt of even/odd message
16  qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct evon/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get theref.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.


The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.


If there is no information that needs to be transfered on a given frame,
such as during the connection stage while waiting for the client to load,
then a packet only needs to be delivered if there is something in the
unacknowledged reliable
*/

#if USE_DEBUG
// WID: net-code: already defined in chan.c
extern cvar_t *showpackets;
extern cvar_t *showfragments;
extern cvar_t *showdrop;
extern cvar_t *showdrop;
#define SHOWPACKET(...) \
    if (showpackets->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#define SHOWFRAGMENT(...) \
    if (showfragments->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#define SHOWDROP(...) \
    if (showdrop->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#else
#define SHOWPACKET(...)
#define SHOWDROP(...)
#endif

#define SOCK_TAG(sock)  ((sock) == NS_SERVER ? TAG_SERVER : TAG_GENERAL)

/*
==============
NetchanQ2RTXPerimental_Setup

called to open a channel to a remote system
==============
*/
netchan_t *NetchanQ2RTXPerimental_Setup( netsrc_t sock, const netadr_t * adr,
								   int qport, size_t maxpacketlen ) {
	netchan_q2rtxperimental_t *chan;
	netchan_t *netchan;

	chan = static_cast<netchan_q2rtxperimental_t *>( Z_TagMallocz( sizeof( *chan ), SOCK_TAG( sock ) ) );
	netchan = (netchan_t *)chan;
	netchan->sock = sock;
	netchan->remote_address = *adr;
	netchan->qport = qport;
	netchan->maxpacketlen = maxpacketlen;
	netchan->last_received = com_localTime;
	netchan->last_sent = com_localTime;
	netchan->incoming_sequence = 0;
	netchan->outgoing_sequence = 1;

	netchan->Process = NetchanQ2RTXPerimental_Process;
	netchan->Transmit = NetchanQ2RTXPerimental_Transmit;
	netchan->TransmitNextFragment = NetchanQ2RTXPerimental_TransmitNextFragment;
	netchan->ShouldUpdate = NetchanQ2RTXPerimental_ShouldUpdate;

	chan->message_buf = static_cast<byte *>( Z_TagMalloc( maxpacketlen, SOCK_TAG( sock ) ) );
	SZ_Init( &netchan->message, chan->message_buf, maxpacketlen );

	chan->reliable_buf = static_cast<byte *>( Z_TagMalloc( maxpacketlen, SOCK_TAG( sock ) ) );

	return netchan;
}

// ============================================================================

size_t NetchanQ2RTXPerimental_TransmitNextFragment( netchan_t *netchan ) {
	Q_assert( !"not implemented" );

	if ( showfragments->integer & 1 && netchan->sock == NS_CLIENT ) {
		SHOWFRAGMENT( "Transmit fragment (%s) (id:%i)\n", Netchan_SocketString( netchan->sock ), netchan->outgoing_sequence );
	}
	if ( showfragments->integer & 2 && netchan->sock == NS_SERVER ) {
		SHOWFRAGMENT( "Transmit fragment (%s) (id:%i)\n", Netchan_SocketString( netchan->sock ), netchan->outgoing_sequence );
	}

	return 0;
}

/*
===============
NetchanQ2RTXPerimental_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
size_t NetchanQ2RTXPerimental_Transmit( netchan_t *netchan, size_t length, const void *data, int numpackets ) {
	netchan_q2rtxperimental_t *chan = (netchan_q2rtxperimental_t *)netchan;
	sizebuf_t   send;
	byte        send_buf[ MAX_PACKETLEN ];
	bool        send_reliable;
	uint32_t    w1, w2;
	int         i;

// check for message overflow
	if ( netchan->message.overflowed ) {
		netchan->fatal_error = true;
		Com_WPrintf( "%s: outgoing message overflow\n",
					NET_AdrToString( &netchan->remote_address ) );
		return 0;
	}

	send_reliable = false;

	// if the remote side dropped the last reliable message, resend it
	if ( netchan->incoming_acknowledged > chan->last_reliable_sequence &&
		chan->incoming_reliable_acknowledged != chan->reliable_sequence ) {
		send_reliable = true;
	}

// if the reliable transmit buffer is empty, copy the current message out
	if ( !netchan->reliable_length && netchan->message.cursize ) {
		send_reliable = true;
		memcpy( chan->reliable_buf, chan->message_buf,
			   netchan->message.cursize );
		netchan->reliable_length = netchan->message.cursize;
		netchan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
	}

// write the packet header
	w1 = ( netchan->outgoing_sequence & 0x7FFFFFFF ) |
		( (unsigned)send_reliable << 31 );
	w2 = ( netchan->incoming_sequence & 0x7FFFFFFF ) |
		( (unsigned)chan->incoming_reliable_sequence << 31 );

	SZ_TagInit( &send, send_buf, sizeof( send_buf ), "nc_send_old" );

	SZ_WriteLong( &send, w1 );
	SZ_WriteLong( &send, w2 );

	#if USE_CLIENT
		// send the qport if we are a client
	if ( netchan->sock == NS_CLIENT ) {
		if ( netchan->protocol < PROTOCOL_VERSION_R1Q2 ) {
			SZ_WriteShort( &send, netchan->qport );
		} else if ( netchan->qport ) {
			SZ_WriteByte( &send, netchan->qport );
		}
	}
	#endif

	// copy the reliable message to the packet first
	if ( send_reliable ) {
		SZ_Write( &send, chan->reliable_buf, netchan->reliable_length );
		chan->last_reliable_sequence = netchan->outgoing_sequence;
	}

// add the unreliable part if space is available
	if ( send.maxsize - send.cursize >= length )
		SZ_Write( &send, data, length );
	else
		Com_WPrintf( "%s: dumped unreliable\n",
					NET_AdrToString( &netchan->remote_address ) );

	SHOWPACKET( "send %4zu : s=%d ack=%d rack=%d",
			   send.cursize,
			   netchan->outgoing_sequence,
			   netchan->incoming_sequence,
			   chan->incoming_reliable_sequence );
	if ( send_reliable ) {
		SHOWPACKET( " reliable=%i", chan->reliable_sequence );
	}
	SHOWPACKET( "\n" );

	// send the datagram
	for ( i = 0; i < numpackets; i++ ) {
		NET_SendPacket( netchan->sock, send.data, send.cursize,
					   &netchan->remote_address );
	}

	netchan->outgoing_sequence++;
	netchan->reliable_ack_pending = false;
	netchan->last_sent = com_localTime;

	return send.cursize * numpackets;
}

/*
=================
NetchanQ2RTXPerimental_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
bool NetchanQ2RTXPerimental_Process( netchan_t *netchan ) {
	netchan_q2rtxperimental_t *chan = (netchan_q2rtxperimental_t *)netchan;
	uint32_t    sequence, sequence_ack;
	uint32_t    reliable_ack, reliable_message;

// get sequence numbers
	MSG_BeginReading( );
	sequence = MSG_ReadLong( );
	sequence_ack = MSG_ReadLong( );

	// read the qport if we are a server
	#if USE_CLIENT
	if ( netchan->sock == NS_SERVER )
		#endif
	{
		if ( netchan->protocol < PROTOCOL_VERSION_R1Q2 ) {
			MSG_ReadShort( );
		} else if ( netchan->qport ) {
			MSG_ReadByte( );
		}
	}

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= 0x7FFFFFFF;
	sequence_ack &= 0x7FFFFFFF;

	SHOWPACKET( "recv %4zu : s=%d ack=%d rack=%d",
			   msg_read.cursize,
			   sequence,
			   sequence_ack,
			   reliable_ack );
	if ( reliable_message ) {
		SHOWPACKET( " reliable=%d", chan->incoming_reliable_sequence ^ 1 );
	}
	SHOWPACKET( "\n" );

//
// discard stale or duplicated packets
//
	if ( sequence <= netchan->incoming_sequence ) {
		SHOWDROP( "%s: out of order packet %i at %i\n",
				 NET_AdrToString( &netchan->remote_address ),
				 sequence, netchan->incoming_sequence );
		return false;
	}

//
// dropped packets don't keep the message from being used
//
	netchan->dropped = sequence - ( netchan->incoming_sequence + 1 );
	if ( netchan->dropped > 0 ) {
		SHOWDROP( "%s: dropped %i packets at %i\n",
				 NET_AdrToString( &netchan->remote_address ),
				 netchan->dropped, sequence );
	}

//
// if the current outgoing reliable message has been acknowledged
// clear the buffer to make way for the next
//
	chan->incoming_reliable_acknowledged = reliable_ack;
	if ( reliable_ack == chan->reliable_sequence )
		netchan->reliable_length = 0;   // it has been received

//
// if this message contains a reliable message, bump incoming_reliable_sequence
//
	netchan->incoming_sequence = sequence;
	netchan->incoming_acknowledged = sequence_ack;
	if ( reliable_message ) {
		netchan->reliable_ack_pending = true;
		chan->incoming_reliable_sequence ^= 1;
	}

//
// the message can now be read from the current message pointer
//
	netchan->last_received = com_localTime;

	netchan->total_dropped += netchan->dropped;
	netchan->total_received += netchan->dropped + 1;

	return true;
}

/*
===============
NetchanQ2RTXPerimental_ShouldUpdate
================
*/
bool NetchanQ2RTXPerimental_ShouldUpdate( netchan_t *netchan ) {
	if ( netchan->message.cursize || netchan->reliable_ack_pending ||
		com_localTime - netchan->last_sent > 1000 ) {
		return true;
	}

	return false;
}



// ============================================================================

