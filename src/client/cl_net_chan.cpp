/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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
// cl_main.c  -- client main loop
#include "shared/shared.h"
#include "cl_client.h"


#define	CL_ENCODE_START		12
#define CL_DECODE_START		4

//#define LEGACY_PROTOCOL
//#ifdef LEGACY_PROTOCOL
//
////==============
////CL_Netchan_Encode
////
////	// first 12 bytes of the data are always:
////	long serverId;
////	long messageAcknowledge;
////	long reliableAcknowledge;
////
////==============
//static void CL_Netchan_Encode( sizebuf_t *msg ) {
//	int serverId, messageAcknowledge, reliableAcknowledge;
//	int i, index, srdc, sbit, soob;
//	byte key, *string;
//
//	if ( msg->cursize <= CL_ENCODE_START ) {
//		return;
//	}
//
//	srdc = msg->readcount;
//	sbit = msg->bit;
//	soob = msg->oob;
//
//	msg->bit = 0;
//	msg->readcount = 0;
//	msg->oob = 0;
//
//	serverId = MSG_ReadInt32( );
//	messageAcknowledge = MSG_ReadInt32( );
//	reliableAcknowledge = MSG_ReadInt32( );
//
//	msg->oob = soob;
//	msg->bit = sbit;
//	msg->readcount = srdc;
//
//	string = (byte *)cls.serverCommands[ reliableAcknowledge & ( MAX_RELIABLE_COMMANDS - 1 ) ];
//	index = 0;
//	//
//	key = cls.challenge ^ serverId ^ messageAcknowledge;
//	for ( i = CL_ENCODE_START; i < msg->cursize; i++ ) {
//		// modify the key with the last received now acknowledged server command
//		if ( !string[ index ] )
//			index = 0;
//		if ( string[ index ] > 127 || string[ index ] == '%' ) {
//			key ^= '.' << ( i & 1 );
//		} else {
//			key ^= string[ index ] << ( i & 1 );
//		}
//		index++;
//		// encode the data with this key
//		*( msg->data + i ) = ( *( msg->data + i ) ) ^ key;
//	}
//}
//
////==============
////CL_Netchan_Decode
////
////	// first four bytes of the data are always:
////	long reliableAcknowledge;
////
////==============
//static void CL_Netchan_Decode( sizebuf_t *msg ) {
//	long reliableAcknowledge, i, index;
//	byte key, *string;
//	int	srdc, sbit, soob;
//
//	srdc = msg->readcount;
//	sbit = msg->bit;
//	soob = msg->oob;
//
//	msg->oob = 0;
//
//	reliableAcknowledge = MSG_ReadInt32( );
//
//	msg->oob = soob;
//	msg->bit = sbit;
//	msg->readcount = srdc;
//
//	string = (byte *)cls.reliableCommands[ reliableAcknowledge & ( MAX_RELIABLE_COMMANDS - 1 ) ];
//	index = 0;
//	// xor the client challenge with the netchan sequence number (need something that changes every message)
//	key = cls.challenge ^ LittleLong( *(unsigned *)msg->data );
//	for ( i = msg->readcount + CL_DECODE_START; i < msg->cursize; i++ ) {
//		// modify the key with the last sent and with this message acknowledged client command
//		if ( !string[ index ] )
//			index = 0;
//		if ( string[ index ] > 127 || string[ index ] == '%' ) {
//			key ^= '.' << ( i & 1 );
//		} else {
//			key ^= string[ index ] << ( i & 1 );
//		}
//		index++;
//		// decode the data with this key
//		*( msg->data + i ) = *( msg->data + i ) ^ key;
//	}
//}
//#endif
//
////=================
////CL_Netchan_TransmitNextFragment
////=================
//qboolean CL_Netchan_TransmitNextFragment( netchan_t *chan ) {
//	if ( chan->fragment_pending ) {
//		NetchanQ2RTXPerimental_TransmitNextFragment( chan );
//		return qtrue;
//	}
//
//	return qfalse;
//}
//
//
////===============
////CL_Netchan_Transmit
////================
//void CL_Netchan_Transmit( netchan_t *chan, sizebuf_t *msg ) {
//	MSG_WriteUint8( clc_EOF );
//
//	#ifdef LEGACY_PROTOCOL
//	if ( chan->compat )
//		CL_Netchan_Encode( msg );
//	#endif
//
//	NetchanQ2RTXPerimental_Transmit( chan, msg->cursize, msg->data );
//
//	// Transmit all fragments without delay
//	while ( CL_Netchan_TransmitNextFragment( chan ) ) {
//		Com_DPrintf( "WARNING: #462 unsent fragments (not supposed to happen!)\n" );
//	}
//}
//
////=================
////CL_Netchan_Process
////=================
//qboolean CL_Netchan_Process( netchan_t *chan, sizebuf_t *msg ) {
//	int ret;
//
//	sizebuf_t temp = msg_read;
//	msg_read = *msg;
//	ret = NetchanQ2RTXPerimental_Process( chan );
//	msg_read = temp;
//
//	if ( !ret )
//		return qfalse;
//
//	#ifdef LEGACY_PROTOCOL
//	if ( chan->compat )
//		CL_Netchan_Decode( msg );
//	#endif
//
//	return qtrue;
//}
