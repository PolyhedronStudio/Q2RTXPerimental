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
*   @brief Read a client's delta move command.
**/
void MSG_ParseDeltaUserCommand( const usercmd_t *from, usercmd_t *to ) {
	int bits;

	if ( from ) {
		memcpy( to, from, sizeof( *to ) );
	} else {
		memset( to, 0, sizeof( *to ) );
	}

	bits = MSG_ReadUint8( );

// read current angles
	if ( bits & CM_ANGLE1 )
		to->angles[ 0 ] = MSG_ReadFloat( );
	if ( bits & CM_ANGLE2 )
		to->angles[ 1 ] = MSG_ReadFloat( );
	if ( bits & CM_ANGLE3 )
		to->angles[ 2 ] = MSG_ReadFloat( );

// read movement
	if ( bits & CM_FORWARD )
		to->forwardmove = MSG_ReadInt16( );
	if ( bits & CM_SIDE )
		to->sidemove = MSG_ReadInt16( );
	if ( bits & CM_UP )
		to->upmove = MSG_ReadInt16( );

// read buttons
	if ( bits & CM_BUTTONS )
		to->buttons = MSG_ReadUint8( );

	if ( bits & CM_IMPULSE )
		to->impulse = MSG_ReadUint8( );

// read time to run command
	to->msec = MSG_ReadUint8( );
}