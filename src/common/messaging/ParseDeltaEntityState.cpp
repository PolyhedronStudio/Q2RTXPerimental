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
* @brief Reads out entity number of current message in the buffer.
*
* @param remove     Set to true in case a remove bit was send along the wire..
* @param byteMask   Mask containing all the bits of message data to read out.
*
* @return   The entity number.
**/
const int32_t MSG_ReadEntityNumber( bool *remove, uint64_t *byteMask ) {
	int32_t number;

	*remove = false;
	number = (int32_t)MSG_ReadIntBase128( );
	*byteMask = MSG_ReadUintBase128( );

	if ( number < 0 ) {
		number *= -1;
		*remove = true;
	}

	return number;
}

/**
*	@brief	Can go from either a baseline or a previous packet_entity
**/
void MSG_ParseDeltaEntity( const entity_state_t *from,
						  entity_state_t *to,
						  int            number,
						  uint64_t       bits,
						  msgEsFlags_t   flags ) {
	if ( !to ) {
		Com_Error( ERR_DROP, "%s: NULL", __func__ );
	}

	if ( number < 1 || number >= MAX_EDICTS ) {
		Com_Error( ERR_DROP, "%s: bad entity number: %d", __func__, number );
	}

	// Set everything to the state we are delta'ing from.
	if ( !from ) {
		*to = {};
	} else if ( to != from ) {
		*to = *from;
	}

	// Ensure its number is set.
	to->number = number;
	// Always reset the event to begin with.
	//to->event = 0;

	// Don't proceed reading if no other bits were present.
	if ( !bits ) {
		return;
	}

	//if ( bits & U_CLIENT ) {
	//	to->client = MSG_ReadInt16();
	//}
	if ( bits & U_ORIGIN1 ) {
		to->origin[ 0 ] = MSG_ReadFloat( );// SHORT2COORD( MSG_ReadInt16( ) ); // WID: float-movement 
	}
	if ( bits & U_ORIGIN2 ) {
		to->origin[ 1 ] = MSG_ReadFloat( );// SHORT2COORD( MSG_ReadInt16( ) ); // WID: float-movement
	}
	if ( bits & U_ORIGIN3 ) {
		to->origin[ 2 ] = MSG_ReadFloat( ); // SHORT2COORD( MSG_ReadInt16( ) ); // WID: float-movement
	}

	if ( bits & U_ANGLE1 ) {
		to->angles[ 0 ] = MSG_ReadHalfFloat( );
	}
	if ( bits & U_ANGLE2 ) {
		to->angles[ 1 ] = MSG_ReadHalfFloat( );
	}
	if ( bits & U_ANGLE3 ) {
		to->angles[ 2 ] = MSG_ReadHalfFloat( );
	}

	if ( bits & U_OLDORIGIN ) {
		to->old_origin[ 0 ] = MSG_ReadFloat( );
		to->old_origin[ 1 ] = MSG_ReadFloat( );
		to->old_origin[ 2 ] = MSG_ReadFloat( );
	}

	if ( bits & U_MODEL ) {
		to->modelindex = QM_Clamp<int32_t>( MSG_ReadUintBase128( ), 0, MAX_MODELS ); //to->modelindex = MSG_ReadUintBase128( );
	}
	if ( bits & U_MODEL2 ) {
		to->modelindex2 = QM_Clamp<int32_t>( MSG_ReadUintBase128(), 0, MAX_MODELS ); //to->modelindex2 = MSG_ReadUintBase128( );
	}
	if ( bits & U_MODEL3 ) {
		to->modelindex3 = QM_Clamp<int32_t>( MSG_ReadUintBase128(), 0, MAX_MODELS ); //to->modelindex3 = MSG_ReadUintBase128( );
	}
	if ( bits & U_MODEL4 ) {
		to->modelindex4 = QM_Clamp<int32_t>( MSG_ReadUintBase128(), 0, MAX_MODELS ); //to->modelindex4 = MSG_ReadUintBase128( );
	}

	if ( bits & U_ENTITY_TYPE ) {
		to->entityType = MSG_ReadUintBase128();
	}
	if ( bits & U_OTHER_ENTITY_NUMBER ) {
		to->otherEntityNumber = MSG_ReadIntBase128();
	}

	if ( bits & U_FRAME ) {
		to->frame = MSG_ReadUintBase128( );
	}
	//if ( bits & U_OLD_FRAME ) {
	//	to->old_frame = MSG_ReadUintBase128();
	//}

	if ( bits & U_SKIN ) {
		to->skinnum = MSG_ReadIntBase128( );
	}
	if ( bits & U_EFFECTS ) {
		to->effects = MSG_ReadUintBase128( );
	}
	if ( bits & U_RENDERFX ) {
		to->renderfx = MSG_ReadIntBase128( );
	}

	if ( bits & U_SOUND ) {
		to->sound = MSG_ReadIntBase128( );
	}

	if ( bits & U_EVENT ) {
		to->event = MSG_ReadIntBase128( );
	}
	if ( bits & U_EVENT_PARM_0 ) {
		to->eventParm0 = MSG_ReadIntBase128();
	}
	if ( bits & U_EVENT_PARM_1 ) {
		to->eventParm1 = MSG_ReadIntBase128();
	}

	if ( bits & U_SOLID ) {
		// WID: upgr-solid: ReadLong by default.
		to->solid = static_cast<cm_solid_t>( MSG_ReadUintBase128() );
	}
	if ( bits & U_BOUNDINGBOX ) {
		to->bounds = static_cast<uint32_t>( MSG_ReadUintBase128() );
	}
	if ( bits & U_CLIPMASK ) {
		to->clipMask = static_cast<cm_contents_t>( MSG_ReadUintBase128( ) );
	}
	if ( bits & U_HULL_CONTENTS ) {
		to->hullContents = static_cast<cm_contents_t>( MSG_ReadUintBase128() );
	}
	if ( bits & U_OWNER ) {
		to->ownerNumber = MSG_ReadIntBase128( );
	}

	// START ET_SPOTLIGHT:
	if ( bits & U_SPOTLIGHT_RGB ) {
		to->spotlight.rgb[ 0 ] = MSG_ReadUint8( );
		to->spotlight.rgb[ 1 ] = MSG_ReadUint8( );
		to->spotlight.rgb[ 2 ] = MSG_ReadUint8( );
	}
	if ( bits & U_SPOTLIGHT_INTENSITY ) {
		to->spotlight.intensity = MSG_ReadHalfFloat( );
	}
	if ( bits & U_SPOTLIGHT_ANGLE_FALLOFF ) {
		to->spotlight.angle_falloff = MSG_ReadHalfFloat( );
	}
	if ( bits & U_SPOTLIGHT_ANGLE_WIDTH ) {
		to->spotlight.angle_width = MSG_ReadHalfFloat( );
	}
	// END ET_SPOTLIGHT.
}
#endif // USE_CLIENT
