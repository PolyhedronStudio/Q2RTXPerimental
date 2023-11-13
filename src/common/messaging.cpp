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
const entity_packed_t   nullEntityState;
//! Baseline PlayerState.
const player_packed_t   nullPlayerState;
//! Baseline UserCommand.
const usercmd_t         nullUserCmd;


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
	SZ_Write( buf, msg_write.data, msg_write.cursize );
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



/**
*
*   Wire Types "Write" functionality.
*
**/
/**
*   @brief
**/
void MSG_WriteChar( int c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= -128 && c <= 127 );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 1 ) ); // WID: C++20: Added cast.
	buf[ 0 ] = c;
}

/**
*   @brief
**/
void MSG_WriteByte( int c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= 0 && c <= 255 );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 1 ) );
	buf[ 0 ] = c;
}

/**
*   @brief
**/
void MSG_WriteShort( int c ) {
	byte *buf;
	#ifdef PARANOID
	Q_assert( c >= -0x8000 && c <= 0x7fff );
	#endif
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 2 ) ); // WID: C++20: Added cast.
	WL16( buf, c );
}

/**
*   @brief
**/
void MSG_WriteLong( int c ) {
	byte *buf;
	buf = static_cast<byte *>( SZ_GetSpace( &msg_write, 4 ) ); // WID: C++20: Added cast.
	WL32( buf, c );
}

/**
*   @brief
**/
void MSG_WriteString( const char *string ) {
	SZ_WriteString( &msg_write, string );
}

/**
*   @brief
**/
static inline void MSG_WriteCoord( float f ) {
	MSG_WriteShort( COORD2SHORT( f ) );
}

/**
*   @brief
**/
void MSG_WritePos( const vec3_t pos ) {
	MSG_WriteCoord( pos[ 0 ] );
	MSG_WriteCoord( pos[ 1 ] );
	MSG_WriteCoord( pos[ 2 ] );
}

/**
*   @brief
**/
void MSG_WriteAngle8( float f ) {
	MSG_WriteByte( ANGLE2BYTE( f ) );
}

/**
*   @brief
**/
void MSG_WriteDir8( const vec3_t dir ) {
	int32_t best = DirToByte( dir );
	MSG_WriteByte( best );
}



/**
*
*   Wire Types "Read" functionality.
*
**/
/**
*   @brief	Returns -1 if no more characters are available
**/
int MSG_ReadChar( void ) {
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
*   @brief	Returns -1 if no more characters are available
**/
int MSG_ReadByte( void ) {
	byte *buf = MSG_ReadData( 1 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (uint8_t)buf[ 0 ];
	}

	return c;
}

int MSG_ReadShort( void ) {
	byte *buf = MSG_ReadData( 2 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (int16_t)RL16( buf );
	}

	return c;
}

int MSG_ReadWord( void ) {
	byte *buf = MSG_ReadData( 2 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (uint16_t)RL16( buf );
	}

	return c;
}

int MSG_ReadLong( void ) {
	byte *buf = MSG_ReadData( 4 );
	int c;

	if ( !buf ) {
		c = -1;
	} else {
		c = (int32_t)RL32( buf );
	}

	return c;
}

size_t MSG_ReadString( char *dest, size_t size ) {
	int     c;
	size_t  len = 0;

	while ( 1 ) {
		c = MSG_ReadByte( );
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

size_t MSG_ReadStringLine( char *dest, size_t size ) {
	int     c;
	size_t  len = 0;

	while ( 1 ) {
		c = MSG_ReadByte( );
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

const float MSG_ReadCoord( void ) {
	return SHORT2COORD( MSG_ReadShort( ) );
}

void MSG_ReadPos( vec3_t pos ) {
	pos[ 0 ] = MSG_ReadCoord( );
	pos[ 1 ] = MSG_ReadCoord( );
	pos[ 2 ] = MSG_ReadCoord( );
}

/**
*	@brief Reads a byte and decodes it to a float angle.
**/
const float MSG_ReadAngle8( void ) {
	return BYTE2ANGLE( MSG_ReadChar( ) );
}
/**
*	@brief Reads a short and decodes it to a float angle.
**/
const float MSG_ReadAngle16( void ) {
	return SHORT2ANGLE( MSG_ReadShort( ) );
}
/**
*	@brief	Reads a byte, and decodes it using it as an index into our directions table.
**/
void MSG_ReadDir8( vec3_t dir ) {
	int     b;

	b = MSG_ReadByte( );
	if ( b < 0 || b >= NUMVERTEXNORMALS )
		Com_Error( ERR_DROP, "MSG_ReadDir: out of range" );
	VectorCopy( bytedirs[ b ], dir );
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
void MSG_ShowDeltaPlayerstateBits( int flags ) {
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
void MSG_ShowDeltaEntityBits( int bits ) {
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
	S( SOUND, "sound" );
	S( EVENT, "event" );
	S( SOLID, "solid" );
	#undef S
}
const char *MSG_ServerCommandString( int cmd ) {
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

