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
*	@brief	Returns the entity number and the header bits, representing a masks of what variables to delta.
**/
int MSG_ParseEntityBits( int *bits ) {
	unsigned    b, total;
	int         number;

	total = MSG_ReadUint8( );
	if ( total & U_MOREBITS1 ) {
		b = MSG_ReadUint8( );
		total |= b << 8;
	}
	if ( total & U_MOREBITS2 ) {
		b = MSG_ReadUint8( );
		total |= b << 16;
	}
	if ( total & U_MOREBITS3 ) {
		b = MSG_ReadUint8( );
		total |= b << 24;
	}

	if ( total & U_NUMBER16 )
		number = MSG_ReadInt16( );
	else
		number = MSG_ReadUint8( );

	*bits = total;

	return number;
}

/**
*	@brief	Can go from either a baseline or a previous packet_entity
**/
void MSG_ParseDeltaEntity( const entity_state_t *from,
						  entity_state_t *to,
						  int            number,
						  int            bits,
						  msgEsFlags_t   flags ) {
	if ( !to ) {
		Com_Error( ERR_DROP, "%s: NULL", __func__ );
	}

	if ( number < 1 || number >= MAX_EDICTS ) {
		Com_Error( ERR_DROP, "%s: bad entity number: %d", __func__, number );
	}

	// set everything to the state we are delta'ing from
	if ( !from ) {
		memset( to, 0, sizeof( *to ) );
	} else if ( to != from ) {
		memcpy( to, from, sizeof( *to ) );
	}

	to->number = number;
	to->event = 0;

	if ( !bits ) {
		return;
	}

	if ( bits & U_MODEL ) {
		to->modelindex = MSG_ReadUint8( );
	}
	if ( bits & U_MODEL2 ) {
		to->modelindex2 = MSG_ReadUint8( );
	}
	if ( bits & U_MODEL3 ) {
		to->modelindex3 = MSG_ReadUint8( );
	}
	if ( bits & U_MODEL4 ) {
		to->modelindex4 = MSG_ReadUint8( );
	}

	if ( bits & U_FRAME8 )
		to->frame = MSG_ReadUint8( );
	if ( bits & U_FRAME16 )
		to->frame = MSG_ReadInt16( );

	if ( ( bits & ( U_SKIN8 | U_SKIN16 ) ) == ( U_SKIN8 | U_SKIN16 ) )  //used for laser colors
		to->skinnum = MSG_ReadInt32( );
	else if ( bits & U_SKIN8 )
		to->skinnum = MSG_ReadUint8( );
	else if ( bits & U_SKIN16 )
		to->skinnum = MSG_ReadUint16( );

	if ( ( bits & ( U_EFFECTS8 | U_EFFECTS16 ) ) == ( U_EFFECTS8 | U_EFFECTS16 ) )
		to->effects = MSG_ReadInt32( );
	else if ( bits & U_EFFECTS8 )
		to->effects = MSG_ReadUint8( );
	else if ( bits & U_EFFECTS16 )
		to->effects = MSG_ReadUint16( );

	if ( ( bits & ( U_RENDERFX8 | U_RENDERFX16 ) ) == ( U_RENDERFX8 | U_RENDERFX16 ) )
		to->renderfx = MSG_ReadInt32( );
	else if ( bits & U_RENDERFX8 )
		to->renderfx = MSG_ReadUint8( );
	else if ( bits & U_RENDERFX16 )
		to->renderfx = MSG_ReadUint16( );

	if ( bits & U_ORIGIN1 ) {
		to->origin[ 0 ] = SHORT2COORD( MSG_ReadInt16( ) );
	}
	if ( bits & U_ORIGIN2 ) {
		to->origin[ 1 ] = SHORT2COORD( MSG_ReadInt16( ) );
	}
	if ( bits & U_ORIGIN3 ) {
		to->origin[ 2 ] = SHORT2COORD( MSG_ReadInt16( ) );
	}

	if ( ( flags & MSG_ES_SHORTANGLES ) && ( bits & U_ANGLE16 ) ) {
		if ( bits & U_ANGLE1 )
			to->angles[ 0 ] = MSG_ReadAngle16( );
		if ( bits & U_ANGLE2 )
			to->angles[ 1 ] = MSG_ReadAngle16( );
		if ( bits & U_ANGLE3 )
			to->angles[ 2 ] = MSG_ReadAngle16( );
	} else {
		if ( bits & U_ANGLE1 )
			to->angles[ 0 ] = MSG_ReadAngle8( );
		if ( bits & U_ANGLE2 )
			to->angles[ 1 ] = MSG_ReadAngle8( );
		if ( bits & U_ANGLE3 )
			to->angles[ 2 ] = MSG_ReadAngle8( );
	}

	if ( bits & U_OLDORIGIN ) {
		MSG_ReadPos( to->old_origin );
	}

	if ( bits & U_SOUND ) {
		to->sound = MSG_ReadUint8( );
	}

	if ( bits & U_EVENT ) {
		to->event = MSG_ReadUint8( );
	}

	if ( bits & U_SOLID ) {
		// WID: upgr-solid: ReadLong by default.
		to->solid = MSG_ReadInt32( );
	}
}
#endif // USE_CLIENT
