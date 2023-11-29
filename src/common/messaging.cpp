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
*
*   "The Wire", these are the main write/read message buffers.
*
*	Handles byte ordering and avoids alignment errors
*
**/
//! The one and single only write buffer. Only initialized just once.
sizebuf_t   msg_write;
byte        msg_write_buffer[ MAX_MSGLEN ];

//! The one and single only read buffer. Reinitialized in various places.
sizebuf_t   msg_read;
byte        msg_read_buffer[ MAX_MSGLEN ];

//! Baseline EntityState.
const entity_packed_t   nullEntityState = {};
//! Baseline PlayerState.
const player_packed_t   nullPlayerState = {};
//! Baseline UserCommand.
const usercmd_t         nullUserCmd = {};


/**
*	@brief	Initialize default buffers, clearing allow overflow/underflow flags.
*
*	This is the only place where writing buffer is initialized. Writing buffer is
*	never allowed to overflow.
*
*	Reading buffer is reinitialized in many other places. Reinitializing will set
*	the allow underflow flag as appropriate.
**/
void MSG_Init( void ) {
	SZ_TagInit( &msg_read, msg_read_buffer, MAX_MSGLEN, "msg_read" );
	SZ_TagInit( &msg_write, msg_write_buffer, MAX_MSGLEN, "msg_write" );
}

/**
*   @brief	Resets the "Write" message buffer for a new write session.
**/
void MSG_BeginWriting( void ) {
	msg_write.cursize = 0;
	msg_write.bits_buf = 0;
	msg_write.bits_left = 32;
	msg_write.overflowed = false;
}
/**
*	@brief	Will copy(by appending) the 'data' memory into the "Write" message buffer.
*
*			Triggers Com_Errors in case of trouble such as 'Overflowing'.
**/
void *MSG_WriteData( const void *data, const size_t len ) {
	return memcpy( SZ_GetSpace( &msg_write, len ), data, len );
}
/**
*	@brief	Will copy(by appending) the "Write" message buffer into the 'data' memory.
*
*			Triggers Com_Errors in case of trouble such as 'Overflowing'.
*
*			Will clear the "Write" message buffer after succesfully copying the data.
**/
void MSG_FlushTo( sizebuf_t *buf ) {
	if ( buf != nullptr ) {
		SZ_Write( buf, msg_write.data, msg_write.cursize );
	} else {
		#ifdef PARANOID
		Q_assert( buf == nullptr );
		#endif
	}

	SZ_Clear( &msg_write );
}

/**
*   @brief
**/
void MSG_BeginReading( void ) {
	msg_read.readcount = 0;
	msg_read.bits_buf = 0;
	msg_read.bits_left = 0;
}
/**
*   @brief
**/
byte *MSG_ReadData( const size_t len ) {
	return static_cast<byte *>( SZ_ReadData( &msg_read, len ) ); // WID: C++20: Added cast.
}




///**
//*	@brief
//**/
//void MSG_WriteBits( const int32_t value, int32_t bits ) {
//	if ( bits == 0 || bits < -31 || bits > 31 ) {
//		Com_Error( ERR_FATAL, "MSG_WriteBits: bad bits: %d", bits );
//	}
//
//	if ( bits < 0 ) {
//		bits = -bits;
//	}
//
//	uint32_t bits_buf = msg_write.bits_buf;
//	uint32_t bits_left = msg_write.bits_left;
//	uint32_t v = value & ( ( 1U << bits ) - 1 );
//
//	bits_buf |= v << ( 32 - bits_left );
//	if ( bits >= bits_left ) {
//		MSG_WriteInt32( bits_buf );
//		bits_buf = v >> bits_left;
//		bits_left += 32;
//	}
//	bits_left -= bits;
//
//	msg_write.bits_buf = bits_buf;
//	msg_write.bits_left = bits_left;
//}
//
///**
//*	@brief
//**/
//const int32_t MSG_ReadBits( int32_t bits ) {
//	bool sgn = false;
//
//	if ( bits == 0 || bits < -25 || bits > 25 ) {
//		Com_Error( ERR_FATAL, "MSG_ReadBits: bad bits: %d", bits );
//	}
//
//	if ( bits < 0 ) {
//		bits = -bits;
//		sgn = true;
//	}
//
//	uint32_t bits_buf = msg_read.bits_buf;
//	uint32_t bits_left = msg_read.bits_left;
//
//	while ( bits > bits_left ) {
//		bits_buf |= (uint32_t)MSG_ReadUint8( ) << bits_left;
//		bits_left += 8;
//	}
//
//	uint32_t value = bits_buf & ( ( 1U << bits ) - 1 );
//
//	msg_read.bits_buf = bits_buf >> bits;
//	msg_read.bits_left = bits_left - bits;
//
//	if ( sgn ) {
//		return (int32_t)( value << ( 32 - bits ) ) >> ( 32 - bits );
//	}
//
//	return value;
//}
//
/**
*	@brief	
**/
void MSG_FlushBits( void ) {
	uint32_t bits_buf = msg_write.bits_buf;
	uint32_t bits_left = msg_write.bits_left;

	while ( bits_left < 32 ) {
		MSG_WriteUint8( bits_buf & 255 );
		bits_buf >>= 8;
		bits_left += 8;
	}

	msg_write.bits_buf = 0;
	msg_write.bits_left = 32;
}



/**
*
*   Wire Types "Write" functionality.
*
**/
/**
*   @brief Writes a signed 8 bit byte.
**/
void MSG_WriteInt8( const int32_t c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= -128 && c <= 127 );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 1 ) ); // WID: C++20: Added cast.
	buf[ 0 ] = c;
}
/**
*   @brief Writes an unsigned 8 bit byte.
**/
void MSG_WriteUint8( const int32_t c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= 0 && c <= 255 );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 1 ) );
	buf[ 0 ] = c;
}
/**
*   @brief Writes a signed 16 bit short.
**/
void MSG_WriteInt16( const int32_t c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= -0x8000 && c <= 0x7fff );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 2 ) ); // WID: C++20: Added cast.
	WL16( buf, c );
}
/**
*   @brief Writes an unsigned 16 bit short.
**/
void MSG_WriteUint16( const int32_t c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= 0 && c <= 65535 );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 2 ) ); // WID: C++20: Added cast.
	WL16( buf, c );
}
/**
*   @brief Writes a 32 bit integer.
**/
void MSG_WriteInt32( const int32_t c ) {
	byte *buf;
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 4 ) ); // WID: C++20: Added cast.
	WL32( buf, c );
}
/**
*   @brief Writes a 64 bit integer.
**/
void MSG_WriteInt64( const int64_t c ) {
	byte *buf;
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 8 ) ); // WID: C++20: Added cast.
	WL64( buf, c );
}
/**
*   @brief Writes an unsigned LEB 128(base 128 encoded) integer.
**/
void MSG_WriteUintBase128( uint64_t c ) {
	uint8_t buf[ 10 ];
	size_t len = 0;

	do {
		buf[ len ] = c & 0x7fU;
		if ( c >>= 7 ) {
			buf[ len ] |= 0x80U;
		}
		len++;
	} while ( c );

	MSG_WriteData( buf, len );
}
/**
*   @brief Writes a zic-zac encoded signed integer.
**/
void MSG_WriteIntBase128( const int64_t c ) {
	// use Zig-zag encoding for signed integers for more efficient storage
	const uint64_t cc = (uint64_t)( c << 1 ) ^ (uint64_t)( c >> 63 );
	MSG_WriteUintBase128( cc );
}
/**
*   @brief Writes a full precision float. (Transfered over the wire as an int32_t).
**/
void MSG_WriteFloat( const float f ) {
	union {
		float f;
		int32_t l;
	} dat;

	dat.f = f;
	MSG_WriteInt32( dat.l );
}

/**
*   @brief Writes a character string.
**/
void MSG_WriteString( const char *string ) {
	SZ_WriteString( &msg_write, string );
}

/**
*	@brief	Writes an 8 bit byte encoded angle value of (float)f.
**/
void MSG_WriteAngle8( const float f ) {
	MSG_WriteUint8( ANGLE2BYTE( f ) );
}
/**
*	@brief	Writes a 16 bit short encoded angle value of (float)f.
**/
void MSG_WriteAngle16( const float f ) {
	MSG_WriteInt16( ANGLE2SHORT( f ) );
}

/**
*   @brief Writes out the position/coordinate, optionally 'short' encoded. (Limits us between range -4096/4096+)
**/
void MSG_WritePos( const vec3_t pos, const bool encodeAsShort = false ) {
	if ( encodeAsShort ) {
		MSG_WriteInt16( COORD2SHORT( pos[ 0 ] ) );
		MSG_WriteInt16( COORD2SHORT( pos[ 1 ] ) );
		MSG_WriteInt16( COORD2SHORT( pos[ 2 ] ) );
	} else {
		MSG_WriteFloat( pos[ 0 ] );
		MSG_WriteFloat( pos[ 1 ] );
		MSG_WriteFloat( pos[ 2 ] );
	}
}
/**
*	@brief	Writes an 8 bit byte, table index encoded direction vector.
**/
void MSG_WriteDir8( const vec3_t dir ) {
	int32_t best = DirToByte( dir );
	MSG_WriteUint8( best );
}


/**
*
*   Wire Types "Read" functionality.
*
**/
/**
*   @return Signed 8 bit byte. Returns -1 if no more characters are available
**/
const int32_t MSG_ReadInt8( void ) {
	byte *buf = MSG_ReadData( 1 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (int8_t)buf[ 0 ];
	}

	return c;
}
/**
*   @return Unsigned 8 bit byte. Returns -1 if no more characters are available
**/
const int32_t MSG_ReadUint8( void ) {
	byte *buf = MSG_ReadData( 1 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (uint8_t)buf[ 0 ];
	}

	return c;
}
/**
*   @return Signed 16 bit short.
**/
const int32_t MSG_ReadInt16( void ) {
	byte *buf = MSG_ReadData( 2 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (int16_t)RL16( buf );
	}

	return c;
}
/**
*   @return Unsigned 16 bit short.
**/
const int32_t MSG_ReadUint16( void ) {
	byte *buf = MSG_ReadData( 2 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (uint16_t)RL16( buf );
	}

	return c;
}
/**
*   @return Signed 32 bit int.
**/
const int32_t MSG_ReadInt32( void ) {
	byte *buf = MSG_ReadData( 4 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (int32_t)RL32( buf );
	}

	return c;
}
/**
*   @return Signed 32 bit int.
**/
const int64_t MSG_ReadInt64( void ) {
	byte *buf = MSG_ReadData( 8 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (int64_t)RL64( buf );
	}

	return c;
}

/**
*   @return Base 128 decoded unsigned integer.
**/
const uint64_t MSG_ReadUintBase128( ) {
	size_t   len = 0;
	uint64_t i = 0;

	while ( len < 10 ) {
		const uint8_t c = MSG_ReadUint8( );
		i |= ( c & 0x7fLL ) << ( 7 * len );
		len++;

		if ( !( c & 0x80 ) ) {
			break;
		}
	}

	return i;
}
/**
*   @return Zig-Zac decoded signed integer.
**/
const int64_t MSG_ReadIntBase128( ) {
	// un-Zig-Zag our value back to a signed integer
	const uint64_t c = MSG_ReadUintBase128( );
	return (int64_t)( c >> 1 ) ^ ( -(int64_t)( c & 1 ) );
}
/**
*   @return The full precision float.
**/
const float MSG_ReadFloat( ) {
	union {
		float f;
		int32_t   l;
	} dat;

	dat.l = MSG_ReadInt32( );
	if ( msg_read.readcount > msg_read.cursize) {
		dat.f = -1;
	}
	return dat.f;
}

/**
*   @return The full string until its end.
**/
const size_t MSG_ReadString( char *dest, const size_t size ) {
	int     c;
	size_t  len = 0;

	while ( 1 ) {
		c = MSG_ReadUint8( );
		if ( c == -1 || c == 0 ) {
			break;
		}
		if ( len + 1 < size ) {
			*dest++ = c;
		}
		len++;
	}
	if ( size ) {
		*dest = 0;
	}

	return len;
}
/**
*   @return The part of the string data up till the first '\n'
**/
const size_t MSG_ReadStringLine( char *dest, const size_t size ) {
	int     c;
	size_t  len = 0;

	while ( 1 ) {
		c = MSG_ReadUint8( );
		if ( c == -1 || c == 0 || c == '\n' ) {
			break;
		}
		if ( len + 1 < size ) {
			*dest++ = c;
		}
		len++;
	}
	if ( size ) {
		*dest = 0;
	}

	return len;
}

/**
*	@brief Reads a byte and decodes it to a float angle.
**/
const float MSG_ReadAngle8( void ) {
	return BYTE2ANGLE( MSG_ReadInt8( ) );
}
/**
*	@brief Reads a short and decodes it to a float angle.
**/
const float MSG_ReadAngle16( void ) {
	return SHORT2ANGLE( MSG_ReadInt16( ) );
}

/**
*	@brief	Reads a byte, and decodes it using it as an index into our directions table.
**/
void MSG_ReadDir8( vec3_t dir ) {
	int     b;

	b = MSG_ReadUint8( );
	if ( b < 0 || b >= NUMVERTEXNORMALS )
		Com_Error( ERR_DROP, "MSG_ReadDir: out of range" );
	VectorCopy( bytedirs[ b ], dir );
}
/**
*	@return The read positional coordinate. Optionally from 'short' to float. (Limiting in the range of -4096/+4096
**/
void MSG_ReadPos( vec3_t pos, const bool decodeFromShort = false ) {
	if ( decodeFromShort ) {
		pos[ 0 ] = SHORT2COORD( MSG_ReadInt16( ) );
		pos[ 1 ] = SHORT2COORD( MSG_ReadInt16( ) );
		pos[ 2 ] = SHORT2COORD( MSG_ReadInt16( ) );
	} else {
		pos[ 0 ] = MSG_ReadFloat( );
		pos[ 1 ] = MSG_ReadFloat( );
		pos[ 2 ] = MSG_ReadFloat( );
	}
}



/*****************************************************************************
*
*	Message Debugging:
*
*****************************************************************************/
#if USE_DEBUG

//#define SHOWBITS(x) Com_LPrintf(PRINT_DEVELOPER, x " ")
static inline void SHOWBITS( const char *x ) {
	Com_LPrintf( PRINT_DEVELOPER, "%s ", x );
}
#if USE_CLIENT
void MSG_ShowDeltaEntityBits( const uint64_t bits ) {
	#define S(b,s) if(bits&U_##b) SHOWBITS(s)
	S( MODEL, "modelindex" );
	S( MODEL2, "modelindex2" );
	S( MODEL3, "modelindex3" );
	S( MODEL4, "modelindex4" );

	if ( bits & U_FRAME8 )
		SHOWBITS( "frame8" );
	if ( bits & U_FRAME16 )
		SHOWBITS( "frame16" );

	if ( ( bits & ( U_SKIN8 | U_SKIN16 ) ) == ( U_SKIN8 | U_SKIN16 ) )
		SHOWBITS( "skinnum32" );
	else if ( bits & U_SKIN8 )
		SHOWBITS( "skinnum8" );
	else if ( bits & U_SKIN16 )
		SHOWBITS( "skinnum16" );

	if ( ( bits & ( U_EFFECTS8 | U_EFFECTS16 ) ) == ( U_EFFECTS8 | U_EFFECTS16 ) )
		SHOWBITS( "effects32" );
	else if ( bits & U_EFFECTS8 )
		SHOWBITS( "effects8" );
	else if ( bits & U_EFFECTS16 )
		SHOWBITS( "effects16" );

	if ( ( bits & ( U_RENDERFX8 | U_RENDERFX16 ) ) == ( U_RENDERFX8 | U_RENDERFX16 ) )
		SHOWBITS( "renderfx32" );
	else if ( bits & U_RENDERFX8 )
		SHOWBITS( "renderfx8" );
	else if ( bits & U_RENDERFX16 )
		SHOWBITS( "renderfx16" );

	S( ORIGIN1, "origin[0]" );
	S( ORIGIN2, "origin[1]" );
	S( ORIGIN3, "origin[2]" );
	S( ANGLE1, "angles[0]" );
	S( ANGLE2, "angles[1]" );
	S( ANGLE3, "angles[2]" );
	S( OLDORIGIN, "old_origin" );
	else if ( bits & U_SOUND8 )
		SHOWBITS( "sound8" );
	else if ( bits & U_SOUND16 )
		SHOWBITS( "sound16" );
	S( EVENT, "event" );
	S( SOLID, "solid" );
	#undef S
}
void MSG_ShowDeltaPlayerstateBits( const uint64_t flags ) {
	#define S(b,s) if(flags&PS_##b) SHOWBITS(s)
	S( M_TYPE, "pmove.pm_type" );
	S( M_ORIGIN, "pmove.origin" );
	S( M_VELOCITY, "pmove.velocity" );
	S( M_TIME, "pmove.pm_time" );
	S( M_FLAGS, "pmove.pm_flags" );
	S( M_GRAVITY, "pmove.gravity" );
	S( M_DELTA_ANGLES, "pmove.delta_angles" );
	S( VIEWOFFSET, "viewoffset" );
	S( VIEWANGLES, "viewangles" );
	S( KICKANGLES, "kick_angles" );
	S( WEAPONINDEX, "gunindex" );
	S( WEAPONFRAME, "gunframe" );
	S( WEAPONRATE, "gunrate" );
	S( BLEND, "blend" );
	S( FOV, "fov" );
	S( RDFLAGS, "rdflags" );
	#undef S
}
void MSG_ShowDeltaUserCommandBits( const int32_t bits ) {
	if ( !bits ) {
		SHOWBITS( "<none>" );
		return;
	}

	#define S(b,s) if(bits&CM_##b) SHOWBITS(s)
	S( ANGLE1, "angle1" );
	S( ANGLE2, "angle2" );
	S( ANGLE3, "angle3" );
	S( FORWARD, "forward" );
	S( SIDE, "side" );
	S( UP, "up" );
	S( BUTTONS, "buttons" );
	S( IMPULSE, "msec" );
	#undef S
}
const char *MSG_ServerCommandString( const int32_t cmd ) {
	switch ( cmd ) {
		case -1: return "END OF MESSAGE";
		default: return "UNKNOWN COMMAND";
			#define S(x) case svc_##x: return "svc_" #x;
			S( bad )
				S( muzzleflash )
				S( muzzleflash2 )
				S( temp_entity )
				S( layout )
				S( inventory )
				S( nop )
				S( disconnect )
				S( reconnect )
				S( sound )
				S( print )
				S( stufftext )
				S( serverdata )
				S( configstring )
				S( spawnbaseline )
				S( centerprint )
				S( download )
				S( playerinfo )
				S( packetentities )
				S( deltapacketentities )
				S( frame )
				S( zpacket )
				S( zdownload )
				S( gamestate )
				#undef S
	}
}

#endif // USE_CLIENT

#endif // USE_DEBUG

