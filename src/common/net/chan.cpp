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
#include "common/messaging.h"
#include "common/net/chan.h"
#include "common/net/Q2RTXPerimentalNetChan.h"
#include "common/net/net.h"
#include "common/protocol.h"
#include "common/sizebuf.h"
#include "common/huffman.h"
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
cvar_t *showpackets; // WID: net-code: removed 'static' keyword, we're sharing these to Q2RTXPerimentalNetChan.cpp
cvar_t *showdrop; // WID: net-code: removed 'static' keyword, we're sharing these to Q2RTXPerimentalNetChan.cpp
#define SHOWPACKET(...) \
    if (showpackets->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#define SHOWDROP(...) \
    if (showdrop->integer) \
        Com_LPrintf(PRINT_DEVELOPER, __VA_ARGS__)
#else
#define SHOWPACKET(...)
#define SHOWDROP(...)
#endif

cvar_t      *net_qport;
cvar_t      *net_maxmsglen;
cvar_t      *net_chantype;


#if USE_DEBUG
static const char socketnames[ 3 ][ 16 ] = { "Client", "Server", "Unknown" };

const char *Netchan_SocketString( netsrc_t socket ) {

	if ( socket == NS_CLIENT )
		return socketnames[ 0 ];
	else if ( socket == NS_SERVER )
		return socketnames[ 1 ];
	else
		return socketnames[ 2 ];
}
#endif

// allow either 0 (no hard limit), or an integer between 512 and 4086
static void net_maxmsglen_changed(cvar_t *self)
{
    if (self->integer) {
        Cvar_ClampInteger(self, MIN_PACKETLEN, MAX_PACKETLEN_WRITABLE);
    }
}

/*
===============
Netchan_Init

===============
*/
void Netchan_Init(void)
{
    int     port;

#if USE_DEBUG
    showpackets = Cvar_Get("showpackets", "0", 0);
    showdrop = Cvar_Get("showdrop", "0", 0);
#endif

    // pick a port value that should be nice and random
    port = Sys_Milliseconds() & 0xffff;
    net_qport = Cvar_Get("qport", va("%d", port), 0);
    net_maxmsglen = Cvar_Get("net_maxmsglen", va("%d", MAX_PACKETLEN_WRITABLE_DEFAULT), 0);
    net_maxmsglen->changed = net_maxmsglen_changed;
    net_chantype = Cvar_Get("net_chantype", "1", 0);
}

/**
*  @brief  Sends a text message in an out-of-band datagram
**/
void Netchan_OutOfBandPrint(netsrc_t sock, const netadr_t *address, const char *format, ...)
{
    va_list     argptr;
    char        data[MAX_PACKETLEN_DEFAULT];
    size_t      len;

    // write the packet header
    memset(data, 0xff, 4);

    va_start(argptr, format);
    len = Q_vsnprintf(data + 4, sizeof(data) - 4, format, argptr);
    va_end(argptr);

    if (len >= sizeof(data) - 4) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }

    // send the datagram
    NET_SendPacket(sock, data, len + 4, address);
}

/**
*  @brief  Sends a data message in an out-of-band datagram (only used for "connect")
**/
void Netchan_OutOfBandData( netsrc_t sock, const netadr_t *address, byte *format, const int32_t len ) {
    // Made static to make the 'heap' happy.
    static byte string[ MAX_MSGLEN * 2 ];
    // Make sure however, that previous data is zerod out.
    memset( string, 0, MAX_MSGLEN * 2 );

    // set the header
    string[ 0 ] = 0xff;
    string[ 1 ] = 0xff;
    string[ 2 ] = 0xff;
    string[ 3 ] = 0xff;

    for ( int32_t i = 0; i < len; i++ ) {
        string[ i + 4 ] = format[ i ];
    }
    
    sizebuf_t   szbuf;
    szbuf.data = string;
    szbuf.cursize = len + 4;
    Huff_Compress( &szbuf, 12 );

    // send the datagram
    NET_SendPacket( sock, szbuf.data, szbuf.cursize, address );
}

// ============================================================================


// ============================================================================


/*
==============
Netchan_Setup
==============
*/
void Netchan_Setup(netchan_t *chan, netsrc_t sock, /*netchan_type_t type,*/
                   const netadr_t *adr, int qport, size_t maxpacketlen, int protocol)
{
    memtag_t tag = sock == NS_SERVER ? TAG_SERVER : TAG_GENERAL;

    Q_assert(chan);
    Q_assert(!chan->message_buf);
    Q_assert(adr);
    Q_assert(maxpacketlen >= MIN_PACKETLEN);
    Q_assert(maxpacketlen <= MAX_PACKETLEN_WRITABLE);

    //chan->type = type;
    chan->protocol = protocol;
    chan->sock = sock;
    chan->remote_address = *adr;
    chan->qport = qport;
    chan->maxpacketlen = maxpacketlen;
    chan->last_received = com_localTime;
    chan->last_sent = com_localTime;
    chan->incoming_sequence = 0;
    chan->outgoing_sequence = 1;

	//chan->message_buf = static_cast<byte *>( Z_TagMalloc( maxpacketlen * 2, tag ) );
	//chan->reliable_buf = chan->message_buf + maxpacketlen;

	//SZ_Init( &chan->message, chan->message_buf, maxpacketlen );
	chan->message_buf = static_cast<byte*>( Z_TagMalloc( MAX_MSGLEN * 4, tag ) );
	chan->reliable_buf = chan->message_buf + MAX_MSGLEN;
	chan->fragment_in_buf = chan->message_buf + MAX_MSGLEN * 2;
	chan->fragment_out_buf = chan->message_buf + MAX_MSGLEN * 3;

	SZ_Init( &chan->message, chan->message_buf, MAX_MSGLEN );
	SZ_TagInit( &chan->fragment_in, chan->fragment_in_buf, MAX_MSGLEN, "nc_frg_in" );
	SZ_TagInit( &chan->fragment_out, chan->fragment_out_buf, MAX_MSGLEN, "nc_frg_out" );
}

/*
==============
Netchan_Close
==============
*/
void Netchan_Close(netchan_t *chan)
{
    Q_assert(chan);
    Z_Free(chan->message_buf);
    memset(chan, 0, sizeof(*chan));
}

