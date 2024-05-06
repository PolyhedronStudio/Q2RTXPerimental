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

//! Used for 'wiring' various view offsets.
static inline int16_t scaled_short( float x, int scale ) {
	return constclamp( x * scale, -32768, 32767 );
}

/**
*   @brief	Pack a player state(in) encoding it into player_packed_t(out)
**/
void MSG_PackPlayer( player_packed_t *out, const player_state_t *in ) {
	int i;

	out->pmove = in->pmove;

	out->bobCycle = (uint8_t)in->bobCycle;

	out->viewangles[ 0 ] = AngleMod( in->viewangles[ 0 ] );
	out->viewangles[ 1 ] = AngleMod( in->viewangles[ 1 ] );
	out->viewangles[ 2 ] = AngleMod( in->viewangles[ 2 ] );
	out->viewoffset[ 0 ] = scaled_short( in->viewoffset[ 0 ], 16 ); // WID: new-pmove OFFSET2CHAR( in->viewoffset[ 0 ] );
	out->viewoffset[ 1 ] = scaled_short( in->viewoffset[ 1 ], 16 ); // WID: new-pmove OFFSET2CHAR( in->viewoffset[ 1 ] );
	out->viewoffset[ 2 ] = scaled_short( in->viewoffset[ 2 ], 16 ); // WID: new-pmove OFFSET2CHAR( in->viewoffset[ 2 ] );
	out->kick_angles[ 0 ] = scaled_short( in->kick_angles[ 0 ], 1024 ); // WID: new-pmove OFFSET2CHAR( in->kick_angles[ 0 ] );
	out->kick_angles[ 1 ] = scaled_short( in->kick_angles[ 1 ], 1024 ); // WID: new-pmove OFFSET2CHAR( in->kick_angles[ 1 ] );
	out->kick_angles[ 2 ] = scaled_short( in->kick_angles[ 2 ], 1024 ); // WID: new-pmove OFFSET2CHAR( in->kick_angles[ 2 ] );

	out->gunoffset[ 0 ] = COORD2SHORT( in->gunoffset[ 0 ] );
	out->gunoffset[ 1 ] = COORD2SHORT( in->gunoffset[ 1 ] );
	out->gunoffset[ 2 ] = COORD2SHORT( in->gunoffset[ 2 ] );
	out->gunangles[ 0 ] = ANGLE2SHORT( in->gunangles[ 0 ] );// WID: new-pmove OFFSET2CHAR( in->gunangles[ 0 ] );
	out->gunangles[ 1 ] = ANGLE2SHORT( in->gunangles[ 1 ] );// WID: new-pmove OFFSET2CHAR( in->gunangles[ 1 ] );
	out->gunangles[ 2 ] = ANGLE2SHORT( in->gunangles[ 2 ] );// WID: new-pmove OFFSET2CHAR( in->gunangles[ 2 ] );
	out->gunindex = in->gunindex;
	out->gunframe = in->gunframe;
	out->gunrate = ( in->gunrate == 10 ) ? 0 : in->gunrate;

	out->screen_blend[ 0 ] = BLEND2BYTE( in->screen_blend[ 0 ] );
	out->screen_blend[ 1 ] = BLEND2BYTE( in->screen_blend[ 1 ] );
	out->screen_blend[ 2 ] = BLEND2BYTE( in->screen_blend[ 2 ] );
	out->screen_blend[ 3 ] = BLEND2BYTE( in->screen_blend[ 3 ] );
	out->rdflags = in->rdflags;
	out->fov = (int)in->fov;

	for ( i = 0; i < MAX_STATS; i++ )
		out->stats[ i ] = in->stats[ i ];
}

/**
*   @brief Writes the delta player state.
**/
void MSG_WriteDeltaPlayerstate( const player_packed_t *from, const player_packed_t *to ) {
	uint64_t pflags;

	if ( !to )
		Com_Error( ERR_DROP, "%s: NULL", __func__ );

	if ( !from )
		from = &nullPlayerState;

	//
	// determine what needs to be sent
	//
	pflags = 0;

	// pmove_state_t:
	if ( to->pmove.pm_type != from->pmove.pm_type ) {
		pflags |= PS_M_TYPE;
	}
	if ( !VectorCompare( to->pmove.origin, from->pmove.origin ) ) {
		pflags |= PS_M_ORIGIN;
	}
	if ( !VectorCompare( to->pmove.velocity, from->pmove.velocity ) ) {
		pflags |= PS_M_VELOCITY;
	}
	if ( to->pmove.pm_time != from->pmove.pm_time ) {
		pflags |= PS_M_TIME;
	}
	if ( to->pmove.pm_flags != from->pmove.pm_flags ) {
		pflags |= PS_M_FLAGS;
	}
	if ( to->pmove.gravity != from->pmove.gravity ) {
		pflags |= PS_M_GRAVITY;
	}
	if ( !VectorCompare( to->pmove.delta_angles, from->pmove.delta_angles ) ) {
		pflags |= PS_M_DELTA_ANGLES;
	}
	if ( to->pmove.viewheight != from->pmove.viewheight ) {
		pflags |= PS_M_VIEWHEIGHT;
	}
	if ( to->bobCycle != from->bobCycle ) {
		pflags |= PS_BOB_CYCLE;
	}

	// rest of the player_state_t:
	if ( !VectorCompare( to->viewoffset, from->viewoffset ) ) {
		pflags |= PS_VIEWOFFSET;
	}
	if ( !VectorCompare( to->viewangles, from->viewangles ) ) {
		pflags |= PS_VIEWANGLES;
	}
	if ( !VectorCompare( to->kick_angles, from->kick_angles ) ) {
		pflags |= PS_KICKANGLES;
	}
	if ( !Vector4Compare( to->screen_blend, from->screen_blend ) ) {
		pflags |= PS_BLEND;
	}
	if ( to->fov != from->fov ) {
		pflags |= PS_FOV;
	}
	if ( to->rdflags != from->rdflags ) {
		pflags |= PS_RDFLAGS;
	}
	if ( to->gunframe != from->gunframe ||
		!VectorCompare( to->gunoffset, from->gunoffset ) ||
		!VectorCompare( to->gunangles, from->gunangles ) ) {
		pflags |= PS_WEAPONFRAME;
	}
	if ( to->gunindex != from->gunindex ) {
		pflags |= PS_WEAPONINDEX;
	}
	if ( to->gunrate != from->gunrate ) {
		pflags |= PS_WEAPONRATE;
	}


	//
	// write it
	//
	MSG_WriteUintBase128( pflags );

	//
	// write the pmove_state_t
	//
	if ( pflags & PS_M_TYPE ) {
		MSG_WriteUint8( to->pmove.pm_type );
	}
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
	if ( pflags & PS_M_TIME ) {
		MSG_WriteUint16( to->pmove.pm_time );
	}
	if ( pflags & PS_M_FLAGS ) {
		MSG_WriteUintBase128( to->pmove.pm_flags );
	}
	if ( pflags & PS_M_GRAVITY ) {
		MSG_WriteInt16( to->pmove.gravity );
	}
	if ( pflags & PS_M_DELTA_ANGLES ) {
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 0 ] );
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 1 ] );
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 2 ] );
	}
	if ( pflags & PS_M_VIEWHEIGHT ) {
		MSG_WriteInt8( to->pmove.viewheight );
	}
	if ( pflags & PS_BOB_CYCLE ) {
		MSG_WriteUint8( to->bobCycle );
	}
	//// Sequenced Events:
	//if ( pflags & PS_M_EVENT_SEQUENCE ) {
	//	MSG_WriteUint8( to->pmove.eventSequence );
	//}
	//if ( pflags & PS_M_EVENT_FIRST ) {
	//	MSG_WriteUint8( to->pmove.events[ 0 ] );
	//}
	//if ( pflags & PS_M_EVENT_FIRST_PARM ) {
	//	MSG_WriteUint8( to->pmove.eventParms[ 0 ] );
	//}
	//if ( pflags & PS_M_EVENT_SECOND ) {
	//	MSG_WriteUint8( to->pmove.events[ 1 ] );
	//}
	//if ( pflags & PS_M_EVENT_SECOND_PARM ) {
	//	MSG_WriteUint8( to->pmove.eventParms[ 1 ] );
	//}


	//
	// write the rest of the player_state_t
	//
	if ( pflags & PS_VIEWOFFSET ) {
		MSG_WriteInt16( to->viewoffset[ 0 ] );
		MSG_WriteInt16( to->viewoffset[ 1 ] );
		MSG_WriteInt16( to->viewoffset[ 2 ] );
	}
	if ( pflags & PS_VIEWANGLES ) {
		MSG_WriteHalfFloat( to->viewangles[ 0 ] );
		MSG_WriteHalfFloat( to->viewangles[ 1 ] );
		MSG_WriteHalfFloat( to->viewangles[ 2 ] );
	}
	if ( pflags & PS_KICKANGLES ) {
		MSG_WriteInt16( to->kick_angles[ 0 ] );
		MSG_WriteInt16( to->kick_angles[ 1 ] );
		MSG_WriteInt16( to->kick_angles[ 2 ] );
	}
	if ( pflags & PS_WEAPONINDEX ) {
		MSG_WriteUintBase128( to->gunindex );
	}
	if ( pflags & PS_WEAPONFRAME ) {
		MSG_WriteUintBase128( to->gunframe );
		MSG_WriteInt16( to->gunoffset[ 0 ] );
		MSG_WriteInt16( to->gunoffset[ 1 ] );
		MSG_WriteInt16( to->gunoffset[ 2 ] );
		MSG_WriteInt16( to->gunangles[ 0 ] );
		MSG_WriteInt16( to->gunangles[ 1 ] );
		MSG_WriteInt16( to->gunangles[ 2 ] );
	}
	if ( pflags & PS_WEAPONRATE ) {
		MSG_WriteInt8( to->gunrate );
	}
	if ( pflags & PS_BLEND ) {
		MSG_WriteUint8( to->screen_blend[ 0 ] );
		MSG_WriteUint8( to->screen_blend[ 1 ] );
		MSG_WriteUint8( to->screen_blend[ 2 ] );
		MSG_WriteUint8( to->screen_blend[ 3 ] );
	}
	if ( pflags & PS_FOV ) {
		MSG_WriteUint8( to->fov );
	}
	if ( pflags & PS_RDFLAGS ) {
		MSG_WriteIntBase128( to->rdflags );
	}

	// Send stats
	int64_t statbits = 0;
	for ( int32_t i = 0; i < MAX_STATS; i++ ) {
		if ( to->stats[ i ] != from->stats[ i ] ) {
			statbits |= 1ULL << i;
		}
	}

	MSG_WriteIntBase128( statbits );
	for ( int32_t i = 0; i < MAX_STATS; i++ ) {
		if ( statbits & ( 1ULL << i ) ) {
			MSG_WriteIntBase128( to->stats[ i ] );
		}
	}
}