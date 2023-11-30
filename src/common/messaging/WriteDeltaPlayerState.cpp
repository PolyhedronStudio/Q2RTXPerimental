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


/**
*   @brief	Pack a player state(in) encoding it into player_packed_t(out)
**/
void MSG_PackPlayer( player_packed_t *out, const player_state_t *in ) {
	int i;

	out->pmove = in->pmove;
	//out->viewangles[ 0 ] = ANGLE2SHORT( in->viewangles[ 0 ] );
	//out->viewangles[ 1 ] = ANGLE2SHORT( in->viewangles[ 1 ] );
	//out->viewangles[ 2 ] = ANGLE2SHORT( in->viewangles[ 2 ] );
	out->viewoffset[ 0 ] = OFFSET2CHAR( in->viewoffset[ 0 ] );
	out->viewoffset[ 1 ] = OFFSET2CHAR( in->viewoffset[ 1 ] );
	out->viewoffset[ 2 ] = OFFSET2CHAR( in->viewoffset[ 2 ] );
	out->kick_angles[ 0 ] = OFFSET2CHAR( in->kick_angles[ 0 ] );
	out->kick_angles[ 1 ] = OFFSET2CHAR( in->kick_angles[ 1 ] );
	out->kick_angles[ 2 ] = OFFSET2CHAR( in->kick_angles[ 2 ] );
	out->gunoffset[ 0 ] = OFFSET2CHAR( in->gunoffset[ 0 ] );
	out->gunoffset[ 1 ] = OFFSET2CHAR( in->gunoffset[ 1 ] );
	out->gunoffset[ 2 ] = OFFSET2CHAR( in->gunoffset[ 2 ] );
	out->gunangles[ 0 ] = OFFSET2CHAR( in->gunangles[ 0 ] );
	out->gunangles[ 1 ] = OFFSET2CHAR( in->gunangles[ 1 ] );
	out->gunangles[ 2 ] = OFFSET2CHAR( in->gunangles[ 2 ] );
	out->gunindex = in->gunindex;
	out->gunframe = in->gunframe;
	// WID: 40hz.
	out->gunrate = ( in->gunrate == 10 ) ? 0 : in->gunrate;
	// WID: 40hz.
	out->blend[ 0 ] = BLEND2BYTE( in->blend[ 0 ] );
	out->blend[ 1 ] = BLEND2BYTE( in->blend[ 1 ] );
	out->blend[ 2 ] = BLEND2BYTE( in->blend[ 2 ] );
	out->blend[ 3 ] = BLEND2BYTE( in->blend[ 3 ] );
	out->fov = (int)in->fov;
	out->rdflags = in->rdflags;
	for ( i = 0; i < MAX_STATS; i++ )
		out->stats[ i ] = in->stats[ i ];
}

/**
*   @brief Writes the delta player state.
**/
void MSG_WriteDeltaPlayerstate( const player_packed_t *from, const player_packed_t *to ) {
	int     i;
	uint64_t pflags;

	if ( !to )
		Com_Error( ERR_DROP, "%s: NULL", __func__ );

	if ( !from )
		from = &nullPlayerState;

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if ( to->pmove.pm_type != from->pmove.pm_type )
		pflags |= PS_M_TYPE;

	if ( !VectorCompare( to->pmove.origin, from->pmove.origin ) )
		pflags |= PS_M_ORIGIN;

	if ( !VectorCompare( to->pmove.velocity, from->pmove.velocity ) )
		pflags |= PS_M_VELOCITY;

	if ( to->pmove.pm_time != from->pmove.pm_time )
		pflags |= PS_M_TIME;

	if ( to->pmove.pm_flags != from->pmove.pm_flags )
		pflags |= PS_M_FLAGS;

	if ( to->pmove.gravity != from->pmove.gravity )
		pflags |= PS_M_GRAVITY;

	if ( !VectorCompare( to->pmove.delta_angles, from->pmove.delta_angles ) )
		pflags |= PS_M_DELTA_ANGLES;

	if ( !VectorCompare( to->viewoffset, from->viewoffset ) )
		pflags |= PS_VIEWOFFSET;

	if ( !VectorCompare( to->viewangles, from->viewangles ) )
		pflags |= PS_VIEWANGLES;

	if ( !VectorCompare( to->kick_angles, from->kick_angles ) )
		pflags |= PS_KICKANGLES;

	if ( !Vector4Compare( to->blend, from->blend ) )
		pflags |= PS_BLEND;

	if ( to->fov != from->fov )
		pflags |= PS_FOV;

	if ( to->rdflags != from->rdflags )
		pflags |= PS_RDFLAGS;

	if ( to->gunframe != from->gunframe ||
		!VectorCompare( to->gunoffset, from->gunoffset ) ||
		!VectorCompare( to->gunangles, from->gunangles ) )
		pflags |= PS_WEAPONFRAME;

	if ( to->gunindex != from->gunindex )
		pflags |= PS_WEAPONINDEX;

	if ( to->gunrate != from->gunrate )
		pflags |= PS_WEAPONRATE;

	//
	// write it
	//
	MSG_WriteUintBase128( pflags );

	//
	// write the pmove_state_t
	//
	if ( pflags & PS_M_TYPE )
		MSG_WriteUint8( to->pmove.pm_type );

	if ( pflags & PS_M_ORIGIN ) {
		MSG_WriteFloat( to->pmove.origin[ 0 ] ); //MSG_WriteInt16( to->pmove.origin[ 0 ] ); // WID: float-movement
		MSG_WriteFloat( to->pmove.origin[ 1 ] ); //MSG_WriteInt16( to->pmove.origin[ 1 ] ); // WID: float-movement
		MSG_WriteFloat( to->pmove.origin[ 2 ] );//MSG_WriteInt16( to->pmove.origin[ 2 ] ); // WID: float-movement
	}

	if ( pflags & PS_M_VELOCITY ) {
		MSG_WriteFloat( to->pmove.velocity[ 0 ] ); //MSG_WriteInt16( to->pmove.velocity[ 0 ] ); // WID: float-movement
		MSG_WriteFloat( to->pmove.velocity[ 1 ] ); //MSG_WriteInt16( to->pmove.velocity[ 1 ] ); // WID: float-movement
		MSG_WriteFloat( to->pmove.velocity[ 2 ] ); //MSG_WriteInt16( to->pmove.velocity[ 2 ] ); // WID: float-movement
	}

	if ( pflags & PS_M_TIME )
		MSG_WriteUint8( to->pmove.pm_time );

	if ( pflags & PS_M_FLAGS )
		MSG_WriteUint8( to->pmove.pm_flags );

	if ( pflags & PS_M_GRAVITY )
		MSG_WriteInt16( to->pmove.gravity );

	if ( pflags & PS_M_DELTA_ANGLES ) {
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 0 ] );
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 1 ] );
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 2 ] );
		//MSG_WriteInt16( to->pmove.delta_angles[ 0 ] );
		//MSG_WriteInt16( to->pmove.delta_angles[ 1 ] );
		//MSG_WriteInt16( to->pmove.delta_angles[ 2 ] );
	}

	//
	// write the rest of the player_state_t
	//
	if ( pflags & PS_VIEWOFFSET ) {
		MSG_WriteInt8( to->viewoffset[ 0 ] );
		MSG_WriteInt8( to->viewoffset[ 1 ] );
		MSG_WriteInt8( to->viewoffset[ 2 ] );
	}

	if ( pflags & PS_VIEWANGLES ) {
		MSG_WriteHalfFloat( to->viewangles[ 0 ] );
		MSG_WriteHalfFloat( to->viewangles[ 1 ] );
		MSG_WriteHalfFloat( to->viewangles[ 2 ] );
	}

	if ( pflags & PS_KICKANGLES ) {
		MSG_WriteInt8( to->kick_angles[ 0 ] );
		MSG_WriteInt8( to->kick_angles[ 1 ] );
		MSG_WriteInt8( to->kick_angles[ 2 ] );
	}

	if ( pflags & PS_WEAPONINDEX )
		MSG_WriteUint8( to->gunindex );

	if ( pflags & PS_WEAPONFRAME ) {
		MSG_WriteUint8( to->gunframe );
		MSG_WriteInt8( to->gunoffset[ 0 ] );
		MSG_WriteInt8( to->gunoffset[ 1 ] );
		MSG_WriteInt8( to->gunoffset[ 2 ] );
		MSG_WriteInt8( to->gunangles[ 0 ] );
		MSG_WriteInt8( to->gunangles[ 1 ] );
		MSG_WriteInt8( to->gunangles[ 2 ] );
	}

	if ( pflags & PS_WEAPONRATE ) {
		MSG_WriteInt8( to->gunrate );
	}

	if ( pflags & PS_BLEND ) {
		MSG_WriteUint8( to->blend[ 0 ] );
		MSG_WriteUint8( to->blend[ 1 ] );
		MSG_WriteUint8( to->blend[ 2 ] );
		MSG_WriteUint8( to->blend[ 3 ] );
	}

	if ( pflags & PS_FOV )
		MSG_WriteUint8( to->fov );

	if ( pflags & PS_RDFLAGS )
		MSG_WriteUint8( to->rdflags );

	// send stats
	int64_t statbits = 0;
	for ( i = 0; i < MAX_STATS; i++ )
		if ( to->stats[ i ] != from->stats[ i ] )
			statbits |= 1ULL << i;

	MSG_WriteIntBase128( statbits );
	for ( i = 0; i < MAX_STATS; i++ )
		if ( statbits & ( 1ULL << i ) )
			MSG_WriteInt16( to->stats[ i ] );
}