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
#include "common/messaging.h"
#include "common/protocol.h"
#include "common/sizebuf.h"
#include "common/math.h"
#include "common/intreadwrite.h"


#if USE_CLIENT
/**
*	@brief	
**/
void MSG_ParseDeltaPlayerstate( const player_state_t *from,
									   player_state_t *to,
									   int            flags ) {
	int         i;
	int         statbits;

	if ( !to ) {
		Com_Error( ERR_DROP, "%s: NULL", __func__ );
	}

	// clear to old value before delta parsing
	if ( !from ) {
		memset( to, 0, sizeof( *to ) );
	} else if ( to != from ) {
		memcpy( to, from, sizeof( *to ) );
	}

	//
	// parse the pmove_state_t
	//
	if ( flags & PS_M_TYPE )
		to->pmove.pm_type = static_cast<pmtype_t>( MSG_ReadUint8( ) ); // WID: C++20: Added cast.

	if ( flags & PS_M_ORIGIN ) {
		to->pmove.origin[ 0 ] = MSG_ReadInt16( );
		to->pmove.origin[ 1 ] = MSG_ReadInt16( );
		to->pmove.origin[ 2 ] = MSG_ReadInt16( );
	}

	if ( flags & PS_M_VELOCITY ) {
		to->pmove.velocity[ 0 ] = MSG_ReadInt16( );
		to->pmove.velocity[ 1 ] = MSG_ReadInt16( );
		to->pmove.velocity[ 2 ] = MSG_ReadInt16( );
	}

	if ( flags & PS_M_TIME )
		to->pmove.pm_time = MSG_ReadUint8( );

	if ( flags & PS_M_FLAGS )
		to->pmove.pm_flags = MSG_ReadUint8( );

	if ( flags & PS_M_GRAVITY )
		to->pmove.gravity = MSG_ReadInt16( );

	if ( flags & PS_M_DELTA_ANGLES ) {
		to->pmove.delta_angles[ 0 ] = MSG_ReadInt16( );
		to->pmove.delta_angles[ 1 ] = MSG_ReadInt16( );
		to->pmove.delta_angles[ 2 ] = MSG_ReadInt16( );
	}

	//
	// parse the rest of the player_state_t
	//
	if ( flags & PS_VIEWOFFSET ) {
		to->viewoffset[ 0 ] = MSG_ReadInt8( ) * 0.25f;
		to->viewoffset[ 1 ] = MSG_ReadInt8( ) * 0.25f;
		to->viewoffset[ 2 ] = MSG_ReadInt8( ) * 0.25f;
	}

	if ( flags & PS_VIEWANGLES ) {
		to->viewangles[ 0 ] = MSG_ReadAngle16( );
		to->viewangles[ 1 ] = MSG_ReadAngle16( );
		to->viewangles[ 2 ] = MSG_ReadAngle16( );
	}

	if ( flags & PS_KICKANGLES ) {
		to->kick_angles[ 0 ] = MSG_ReadInt8( ) * 0.25f;
		to->kick_angles[ 1 ] = MSG_ReadInt8( ) * 0.25f;
		to->kick_angles[ 2 ] = MSG_ReadInt8( ) * 0.25f;
	}

	if ( flags & PS_WEAPONINDEX ) {
		to->gunindex = MSG_ReadUint8( );
	}

	if ( flags & PS_WEAPONFRAME ) {
		to->gunframe = MSG_ReadUint8( );
		to->gunoffset[ 0 ] = MSG_ReadInt8( ) * 0.25f;
		to->gunoffset[ 1 ] = MSG_ReadInt8( ) * 0.25f;
		to->gunoffset[ 2 ] = MSG_ReadInt8( ) * 0.25f;
		to->gunangles[ 0 ] = MSG_ReadInt8( ) * 0.25f;
		to->gunangles[ 1 ] = MSG_ReadInt8( ) * 0.25f;
		to->gunangles[ 2 ] = MSG_ReadInt8( ) * 0.25f;
	}

	if ( flags & PS_WEAPONRATE )
		to->gunrate = MSG_ReadUint8( );

	if ( flags & PS_BLEND ) {
		to->blend[ 0 ] = MSG_ReadUint8( ) / 255.0f;
		to->blend[ 1 ] = MSG_ReadUint8( ) / 255.0f;
		to->blend[ 2 ] = MSG_ReadUint8( ) / 255.0f;
		to->blend[ 3 ] = MSG_ReadUint8( ) / 255.0f;
	}

	if ( flags & PS_FOV )
		to->fov = MSG_ReadUint8( );

	if ( flags & PS_RDFLAGS )
		to->rdflags = MSG_ReadUint8( );

	// parse stats
	statbits = MSG_ReadInt32( );
	for ( i = 0; i < MAX_STATS; i++ )
		if ( statbits & ( 1U << i ) )
			to->stats[ i ] = MSG_ReadInt16( );
}
#endif // USE_CLIENT