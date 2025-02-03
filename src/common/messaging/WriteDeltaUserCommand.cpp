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
*   @brief Write a client's delta move command.
**/
const int64_t MSG_WriteDeltaUserCommand( const usercmd_t *from, const usercmd_t *cmd, const int32_t protocolVersion ) {

	if ( !from ) {
		from = &nullUserCmd;
	}

//
// send the movement message
//
	int64_t bits = 0;

	if ( cmd->angles[ 0 ] != from->angles[ 0 ] ) {
		bits |= CM_ANGLE1;
	}
	if ( cmd->angles[ 1 ] != from->angles[ 1 ] ) {
		bits |= CM_ANGLE2;
	}
	if ( cmd->angles[ 2 ] != from->angles[ 2 ] ) {
		bits |= CM_ANGLE3;
	}
	if ( cmd->forwardmove != from->forwardmove ) {
		bits |= CM_FORWARD;
	}
	if ( cmd->sidemove != from->sidemove ) {
		bits |= CM_SIDE;
	}
	if ( cmd->upmove != from->upmove ) {
		bits |= CM_UP;
	}
	if ( cmd->buttons != from->buttons ) {
		bits |= CM_BUTTONS;
	}
	if ( cmd->impulse != from->impulse ) {
		bits |= CM_IMPULSE;
	}

	MSG_WriteUintBase128( bits );

	// Write current angles.
	if ( bits & CM_ANGLE1 ) {
		MSG_WriteFloat( QM_AngleMod( cmd->angles[ 0 ] ) );
	}
	if ( bits & CM_ANGLE2 ) {
		MSG_WriteFloat( QM_AngleMod( cmd->angles[ 1 ] ) );
	}
	if ( bits & CM_ANGLE3 ) {
		MSG_WriteFloat( QM_AngleMod( cmd->angles[ 2 ] ) );
	}

	// Write movement.
	if ( bits & CM_FORWARD ) {
		MSG_WriteFloat( cmd->forwardmove );
	}
	if ( bits & CM_SIDE ) {
		MSG_WriteFloat( cmd->sidemove );
	}
	if ( bits & CM_UP ) {
		MSG_WriteFloat( cmd->upmove );
	}

	// Write buttons.
	if ( bits & CM_BUTTONS ) {
		MSG_WriteUint16( cmd->buttons );
	}
	if ( bits & CM_IMPULSE ) {
		MSG_WriteUint8( cmd->impulse );
	}

	// Write command ran time.
	MSG_WriteUint8( cmd->msec );

	// Read out the current frame number, for possibly deterministics.
	MSG_WriteIntBase128( cmd->frameNumber );

	return bits;
}

