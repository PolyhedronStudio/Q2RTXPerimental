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
*   @brief	Pack an entity state(in) encoding it into entity_packed_t(out)
**/
void MSG_PackEntity( entity_packed_t *out, const entity_state_t *in, bool short_angles ) {
	// allow 0 to accomodate empty baselines
	if ( in->number < 0 || in->number >= MAX_EDICTS )
		Com_Error( ERR_DROP, "%s: bad number: %d", __func__, in->number );

	out->number = in->number;
	out->origin[ 0 ] = COORD2SHORT( in->origin[ 0 ] );
	out->origin[ 1 ] = COORD2SHORT( in->origin[ 1 ] );
	out->origin[ 2 ] = COORD2SHORT( in->origin[ 2 ] );
	//if ( short_angles ) {
		out->angles[ 0 ] = ANGLE2SHORT( in->angles[ 0 ] );
		out->angles[ 1 ] = ANGLE2SHORT( in->angles[ 1 ] );
		out->angles[ 2 ] = ANGLE2SHORT( in->angles[ 2 ] );
	//} else {
	//	// pack angles8 akin to angles16 to make delta compression happy when
	//	// precision suddenly changes between entity updates
	//	out->angles[ 0 ] = ANGLE2BYTE( in->angles[ 0 ] ) << 8;
	//	out->angles[ 1 ] = ANGLE2BYTE( in->angles[ 1 ] ) << 8;
	//	out->angles[ 2 ] = ANGLE2BYTE( in->angles[ 2 ] ) << 8;
	//}
	out->old_origin[ 0 ] = COORD2SHORT( in->old_origin[ 0 ] );
	out->old_origin[ 1 ] = COORD2SHORT( in->old_origin[ 1 ] );
	out->old_origin[ 2 ] = COORD2SHORT( in->old_origin[ 2 ] );
	out->modelindex = in->modelindex;
	out->modelindex2 = in->modelindex2;
	out->modelindex3 = in->modelindex3;
	out->modelindex4 = in->modelindex4;
	out->skinnum = in->skinnum;
	out->effects = in->effects;
	out->renderfx = in->renderfx;
	out->solid.u = in->solid;
	out->frame = in->frame;
	out->sound = in->sound;
	out->event = in->event;
}

/**
*   @brief  Writes entity number, remove bit, and the byte mask to buffer.
**/
void MSG_WriteEntityNumber( const int32_t number, const bool remove, const uint64_t byteMask) {
    MSG_WriteIntBase128(number * (remove ? -1 : 1));
    MSG_WriteUintBase128(byteMask);
}

/**
*   @brief Writes the delta values of the entity state.
**/
void MSG_WriteDeltaEntity( const entity_packed_t *from,
						  const entity_packed_t *to,
						  msgEsFlags_t          flags ) {
	uint64_t    bits, mask;

	if ( !to ) {
		if ( !from )
			Com_Error( ERR_DROP, "%s: NULL", __func__ );

		if ( from->number < 1 || from->number >= MAX_PACKET_ENTITIES )
			Com_Error( ERR_DROP, "%s: bad number: %d", __func__, from->number );

		// Write out entity number with a remove bit set.
		MSG_WriteEntityNumber( from->number, true, 0 );

		return; // remove entity
	}

	if ( to->number < 1 || to->number >= MAX_EDICTS )
		Com_Error( ERR_DROP, "%s: bad number: %d", __func__, to->number );

	if ( !from )
		from = &nullEntityState;

// send an update
	bits = 0;

	if ( !( flags & MSG_ES_FIRSTPERSON ) ) {
		if ( to->origin[ 0 ] != from->origin[ 0 ] )
			bits |= U_ORIGIN1;
		if ( to->origin[ 1 ] != from->origin[ 1 ] )
			bits |= U_ORIGIN2;
		if ( to->origin[ 2 ] != from->origin[ 2 ] )
			bits |= U_ORIGIN3;

		//if ( flags & MSG_ES_SHORTANGLES ) {
			if ( to->angles[ 0 ] != from->angles[ 0 ] )
				bits |= U_ANGLE1;// | U_ANGLE16;
			if ( to->angles[ 1 ] != from->angles[ 1 ] )
				bits |= U_ANGLE2;// | U_ANGLE16;
			if ( to->angles[ 2 ] != from->angles[ 2 ] )
				bits |= U_ANGLE3;// | U_ANGLE16;
		//} else {
		//	if ( to->angles[ 0 ] != from->angles[ 0 ] )
		//		bits |= U_ANGLE1;
		//	if ( to->angles[ 1 ] != from->angles[ 1 ] )
		//		bits |= U_ANGLE2;
		//	if ( to->angles[ 2 ] != from->angles[ 2 ] )
		//		bits |= U_ANGLE3;
		//}

		if ( ( flags & MSG_ES_NEWENTITY ) && !VectorCompare( to->old_origin, from->origin ) )
			bits |= U_OLDORIGIN;
	}

	if ( flags & MSG_ES_UMASK )
		mask = 0xffff0000;
	else
		mask = 0xffff8000;  // don't confuse old clients

	if ( to->skinnum != from->skinnum ) {
		if ( to->skinnum & mask )
			bits |= U_SKIN8 | U_SKIN16;
		else if ( to->skinnum & 0x0000ff00 )
			bits |= U_SKIN16;
		else
			bits |= U_SKIN8;
	}

	if ( to->frame != from->frame ) {
		if ( to->frame & 0xff00 )
			bits |= U_FRAME16;
		else
			bits |= U_FRAME8;
	}

	if ( to->effects != from->effects ) {
		if ( to->effects & mask )
			bits |= U_EFFECTS8 | U_EFFECTS16;
		else if ( to->effects & 0x0000ff00 )
			bits |= U_EFFECTS16;
		else
			bits |= U_EFFECTS8;
	}

	if ( to->renderfx != from->renderfx ) {
		if ( to->renderfx & mask )
			bits |= U_RENDERFX8 | U_RENDERFX16;
		else if ( to->renderfx & 0x0000ff00 )
			bits |= U_RENDERFX16;
		else
			bits |= U_RENDERFX8;
	}

	if ( to->solid.u != from->solid.u )
		bits |= U_SOLID;

	// event is not delta compressed, just 0 compressed
	if ( to->event )
		bits |= U_EVENT;

	if ( to->modelindex != from->modelindex )
		bits |= U_MODEL;
	if ( to->modelindex2 != from->modelindex2 )
		bits |= U_MODEL2;
	if ( to->modelindex3 != from->modelindex3 )
		bits |= U_MODEL3;
	if ( to->modelindex4 != from->modelindex4 )
		bits |= U_MODEL4;

	if ( to->sound != from->sound ) {
		if ( to->sound & 0xff00 )
			bits |= U_SOUND16;
		else
			bits |= U_SOUND8;
	}

	if ( to->renderfx & RF_FRAMELERP ) {
		bits |= U_OLDORIGIN;
	} else if ( to->renderfx & RF_BEAM ) {
		if ( flags & MSG_ES_BEAMORIGIN ) {
			if ( !VectorCompare( to->old_origin, from->old_origin ) )
				bits |= U_OLDORIGIN;
		} else {
			bits |= U_OLDORIGIN;
		}
	}

	//
	// write the message
	//
	if ( !bits && !( flags & MSG_ES_FORCE ) )
		return;     // nothing to send!

	MSG_WriteEntityNumber( to->number, false, bits );

	if ( bits & U_MODEL )
		MSG_WriteUint8( to->modelindex );
	if ( bits & U_MODEL2 )
		MSG_WriteUint8( to->modelindex2 );
	if ( bits & U_MODEL3 )
		MSG_WriteUint8( to->modelindex3 );
	if ( bits & U_MODEL4 )
		MSG_WriteUint8( to->modelindex4 );

	if ( bits & U_FRAME8 )
		MSG_WriteUint8( to->frame );
	else if ( bits & U_FRAME16 )
		MSG_WriteInt16( to->frame );

	if ( ( bits & ( U_SKIN8 | U_SKIN16 ) ) == ( U_SKIN8 | U_SKIN16 ) )  //used for laser colors
		MSG_WriteInt32( to->skinnum );
	else if ( bits & U_SKIN8 )
		MSG_WriteUint8( to->skinnum );
	else if ( bits & U_SKIN16 )
		MSG_WriteInt16( to->skinnum );

	if ( ( bits & ( U_EFFECTS8 | U_EFFECTS16 ) ) == ( U_EFFECTS8 | U_EFFECTS16 ) )
		MSG_WriteInt32( to->effects );
	else if ( bits & U_EFFECTS8 )
		MSG_WriteUint8( to->effects );
	else if ( bits & U_EFFECTS16 )
		MSG_WriteInt16( to->effects );

	if ( ( bits & ( U_RENDERFX8 | U_RENDERFX16 ) ) == ( U_RENDERFX8 | U_RENDERFX16 ) )
		MSG_WriteInt32( to->renderfx );
	else if ( bits & U_RENDERFX8 )
		MSG_WriteUint8( to->renderfx );
	else if ( bits & U_RENDERFX16 )
		MSG_WriteInt16( to->renderfx );

	if ( bits & U_ORIGIN1 )
		MSG_WriteInt16( to->origin[ 0 ] );
	if ( bits & U_ORIGIN2 )
		MSG_WriteInt16( to->origin[ 1 ] );
	if ( bits & U_ORIGIN3 )
		MSG_WriteInt16( to->origin[ 2 ] );

	//if ( ( flags & MSG_ES_SHORTANGLES ) && ( bits & U_ANGLE16 ) ) {
		if ( bits & U_ANGLE1 )
			MSG_WriteInt16( to->angles[ 0 ] );
		if ( bits & U_ANGLE2 )
			MSG_WriteInt16( to->angles[ 1 ] );
		if ( bits & U_ANGLE3 )
			MSG_WriteInt16( to->angles[ 2 ] );
	//} else {
	//	if ( bits & U_ANGLE1 )
	//		MSG_WriteUint8( to->angles[ 0 ] >> 8 );
	//	if ( bits & U_ANGLE2 )
	//		MSG_WriteUint8( to->angles[ 1 ] >> 8 );
	//	if ( bits & U_ANGLE3 )
	//		MSG_WriteUint8( to->angles[ 2 ] >> 8 );
	//}

	if ( bits & U_OLDORIGIN ) {
		MSG_WriteInt16( to->old_origin[ 0 ] );
		MSG_WriteInt16( to->old_origin[ 1 ] );
		MSG_WriteInt16( to->old_origin[ 2 ] );
	}

	if ( bits & U_SOUND8 )
		MSG_WriteUint8( to->sound );
	else if ( bits & U_SOUND16 )
		MSG_WriteUint16( to->sound );

	if ( bits & U_EVENT )
		MSG_WriteUint8( to->event );
	if ( bits & U_SOLID ) {
		// WID: upgr-solid: WriteLong by default.
		MSG_WriteInt32( to->solid.u );
	}
}