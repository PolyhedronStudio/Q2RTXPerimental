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
*   @brief Writes the delta player state.
**/
void MSG_WriteDeltaPlayerstate( const player_state_t *from, const player_state_t *to ) {
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
	if ( to->pmove.speed != from->pmove.speed ) {
		pflags |= PS_M_SPEED;
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
	//if ( to->gunframe != from->gunframe ||
	//	!VectorCompare( to->gunoffset, from->gunoffset ) ||
	//	!VectorCompare( to->gunangles, from->gunangles ) ) {
	if ( to->gun.animationID != from->gun.animationID ) {
		pflags |= PS_GUN_ANIMATION;
	}
	if ( to->gun.modelIndex != from->gun.modelIndex ) {
		pflags |= PS_GUN_MODELINDEX;
	}

	if ( to->eventSequence != from->eventSequence ) {
		pflags |= PS_EVENT_SEQUENCE;
	}
	if ( to->events[ 0 ] != from->events[ 0 ] ) {
		pflags |= PS_EVENT_FIRST;
	}
	if ( to->events[ 1 ] != from->events[ 1 ] ) {
		pflags |= PS_EVENT_SECOND;
	}
	if ( to->eventParms[ 0 ] != from->eventParms[ 0 ] ) {
		pflags |= PS_EVENT_FIRST_PARM;
	}
	if ( to->eventParms[ 1 ] != from->eventParms[ 1 ] ) {
		pflags |= PS_EVENT_SECOND_PARM;
	}

	if ( to->externalEvent != from->externalEvent ) {
		pflags |= PS_EXTERNAL_EVENT;
	}
	if ( to->externalEventParm0 != from->externalEventParm0 ) {
		pflags |= PS_EXTERNAL_EVENT_PARM0;
	}
	if ( to->externalEventParm1 != from->externalEventParm1 ) {
		pflags |= PS_EXTERNAL_EVENT_PARM1;
	}

	//
	// write it
	//
	MSG_WriteUintBase128( pflags );

	//
	// write the pmove_state_t
	//
	// Write out the pmove_state_t.
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
		//MSG_WriteUint16( to->pmove.pm_time );
		MSG_WriteHalfFloat( to->pmove.pm_time );
	}
	if ( pflags & PS_M_FLAGS ) {
		MSG_WriteUintBase128( to->pmove.pm_flags );
	}
	if ( pflags & PS_M_SPEED ) {
		MSG_WriteInt16( to->pmove.speed );
	}
	// Write out the gravity.
	if ( pflags & PS_M_GRAVITY ) {
		MSG_WriteInt16( to->pmove.gravity );
	}
	// Write out the delta angles.
	if ( pflags & PS_M_DELTA_ANGLES ) {
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 0 ] );
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 1 ] );
		MSG_WriteHalfFloat( to->pmove.delta_angles[ 2 ] );
	}
	// Write out the viewheight.
	if ( pflags & PS_M_VIEWHEIGHT ) {
		MSG_WriteInt8( to->pmove.viewheight );
	}
	
	// Write out the bobcycle.
	if ( pflags & PS_BOB_CYCLE ) {
		MSG_WriteUint8( (uint8_t)to->bobCycle );
	}

	// Sequenced Events:
	if ( pflags & PS_EVENT_SEQUENCE ) {
		MSG_WriteUint8( to->eventSequence );
	}
	if ( pflags & PS_EVENT_FIRST ) {
		MSG_WriteIntBase128( to->events[ 0 ] );
	}
	if ( pflags & PS_EVENT_FIRST_PARM ) {
		MSG_WriteIntBase128( to->eventParms[ 0 ] );
	}
	if ( pflags & PS_EVENT_SECOND ) {
		MSG_WriteIntBase128( to->events[ 1 ] );
	}
	if ( pflags & PS_EVENT_SECOND_PARM ) {
		MSG_WriteIntBase128( to->eventParms[ 1 ] );
	}
	
	// Write out external event.
	if ( pflags & PS_EXTERNAL_EVENT ) {
		MSG_WriteIntBase128( to->externalEvent );
	}
	if ( pflags & PS_EXTERNAL_EVENT_PARM0 ) {
		MSG_WriteIntBase128( to->externalEventParm0 );
	}
	if ( pflags & PS_EXTERNAL_EVENT_PARM1 ) {
		MSG_WriteIntBase128( to->externalEventParm1 );
	}


	//
	// write the rest of the player_state_t
	//
	// Write out view offsets.
	if ( pflags & PS_VIEWOFFSET ) {
		MSG_WriteHalfFloat( to->viewoffset[ 0 ] );
		MSG_WriteHalfFloat( to->viewoffset[ 1 ] );
		MSG_WriteHalfFloat( to->viewoffset[ 2 ] );
	}
	// Write out view angles.
	if ( pflags & PS_VIEWANGLES ) {
		MSG_WriteHalfFloat( QM_AngleMod( to->viewangles[ 0 ] ) );
		MSG_WriteHalfFloat( QM_AngleMod( to->viewangles[ 1 ] ) );
		MSG_WriteHalfFloat( QM_AngleMod( to->viewangles[ 2 ] ) );
	}
	// Write out kick angles.
	if ( pflags & PS_KICKANGLES ) {
		MSG_WriteHalfFloat( to->kick_angles[ 0 ] );
		MSG_WriteHalfFloat( to->kick_angles[ 1 ] );
		MSG_WriteHalfFloat( to->kick_angles[ 2 ] );
	}
	// Write out gun info.
	if ( pflags & PS_GUN_MODELINDEX ) {
		MSG_WriteUintBase128( to->gun.modelIndex );
	}
	// Write out gun animation.
	if ( pflags & PS_GUN_ANIMATION ) {
		MSG_WriteUint8( to->gun.animationID );
		// Moved to CLGame.
		//MSG_WriteInt16( to->gunoffset[ 0 ] );
		//MSG_WriteInt16( to->gunoffset[ 1 ] );
		//MSG_WriteInt16( to->gunoffset[ 2 ] );
		//MSG_WriteInt16( to->gunangles[ 0 ] );
		//MSG_WriteInt16( to->gunangles[ 1 ] );
		//MSG_WriteInt16( to->gunangles[ 2 ] );
	}

	// Write out screen blend.
	if ( pflags & PS_BLEND ) {
		MSG_WriteUint8( BLEND2BYTE( to->screen_blend[ 0 ] ) );
		MSG_WriteUint8( BLEND2BYTE( to->screen_blend[ 1 ] ) );
		MSG_WriteUint8( BLEND2BYTE( to->screen_blend[ 2 ] ) );
		MSG_WriteUint8( BLEND2BYTE( to->screen_blend[ 3 ] ) );
	}
	// Write out fov.
	if ( pflags & PS_FOV ) {
		MSG_WriteUint8( (uint8_t)to->fov );
	}
	// Write out rdflags.
	if ( pflags & PS_RDFLAGS ) {
		MSG_WriteIntBase128( to->rdflags );
	}

	// Send stats
	int64_t statBits = 0;
	for ( int64_t i = 0; i < MAX_STATS; i++ ) {
		if ( to->stats[ i ] != from->stats[ i ] ) {
			statBits |= 1ULL << i;
		}
	}

	MSG_WriteIntBase128( statBits );
	for ( int64_t i = 0; i < MAX_STATS; i++ ) {
		if ( statBits & ( 1ULL << i ) ) {
			MSG_WriteIntBase128( to->stats[ i ] );
		}
	}
}