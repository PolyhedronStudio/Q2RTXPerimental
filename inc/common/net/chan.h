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

#ifndef NET_CHAN_H
#define NET_CHAN_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

#include "common/messaging.h"
#include "common/net/net.h"
#include "common/sizebuf.h"

//typedef enum netchan_type_e {
//    NETCHAN_OLD,
//    NETCHAN_NEW,
//	NETCHAN_Q2RTXPERIMENTAL
//} netchan_type_t;

typedef struct netchan_s {
    //netchan_type_t  type;
    int         protocol;
    size_t      maxpacketlen;

    bool        fatal_error;

    netsrc_t    sock;

    int64_t     dropped;            // between last packet and previous
    uint64_t    total_dropped;      // for statistics
	uint64_t	total_received;

	uint64_t	last_received;      // for timeouts
	uint64_t	last_sent;          // for retransmits

    netadr_t    remote_address;
    int         qport;              // qport value to write when transmitting

    sizebuf_t   message;            // writing buffer for reliable data

	uint64_t	reliable_length;

    bool        reliable_ack_pending;   // set to true each time reliable is received
    bool        fragment_pending;

    // sequencing variables
    int64_t     incoming_sequence;
	int64_t		incoming_acknowledged;
	int64_t		outgoing_sequence;

    bool        incoming_reliable_acknowledged; // single bit
    bool        incoming_reliable_sequence;     // single bit, maintained local
    bool        reliable_sequence;          // single bit
	int64_t		last_reliable_sequence;     // sequence number of last send
	int64_t		fragment_sequence;

    byte        *message_buf;       // leave space for header

    // message is copied to this buffer when it is first transfered
    byte        *reliable_buf;      // unacked reliable message

    sizebuf_t   fragment_in;
    sizebuf_t   fragment_out;
    //byte        *fragment_in_buf;
    //byte        *fragment_out_buf;

    // common methods
    //size_t      (*Transmit)(struct netchan_s *, size_t, const void *);
    //size_t      (*TransmitNextFragment)(struct netchan_s *);
    //bool        (*Process)(struct netchan_s *);
    //bool        (*ShouldUpdate)(struct netchan_s *);
} netchan_t;

extern cvar_t       *net_qport;
extern cvar_t       *net_maxmsglen;
extern cvar_t       *net_chantype;

const char *Netchan_SocketString( netsrc_t socket );
void Netchan_Init(void);
/**
*  @brief  Sends a text message in an out-of-band datagram
**/
void Netchan_OutOfBandPrint(netsrc_t sock, const netadr_t *adr,
                       const char *format, ...) q_printf(3, 4);
/**
*  @brief  Sends a data message in an out-of-band datagram (only used for "connect")
**/
void Netchan_OutOfBandData( netsrc_t sock, const netadr_t *address, byte *format, const int32_t len );

void Netchan_Setup(netchan_t *netchan, netsrc_t sock, /*netchan_type_t type,*/
                   const netadr_t *adr, int qport, size_t maxpacketlen, int protocol);
void Netchan_Close(netchan_t *netchan);

#define OOB_PRINT(sock, addr, data) \
    NET_SendPacket(sock, CONST_STR_LEN("\xff\xff\xff\xff" data), addr)

#include "Q2RTXPerimentalNetChan.h"

#ifdef __cplusplus
};
#endif
#endif // NET_CHAN_H
