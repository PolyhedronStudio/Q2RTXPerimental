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
#pragma once


// Protocol necessities.
#include "common/protocol.h"
// SizeBuffer.
#include "common/sizebuf.h"



/**
*
*   "Packing" POD types.
*
**/
// WID: upgr-solid: From Q2RE, similar it seems to how Q3 does it. (Did not check that).
/**
*	@brief	union struct, for packing the bounds as uint32_t.
**/
typedef union bounds_packed_u {
	//! Memory packed representation of the bounds.
	struct p {
		uint8_t x;
		uint8_t y;
		uint8_t zd; //! "z=down" always negative
		uint8_t zu; //! "z-up", encoded as + 32
	} p;

	//! Actual uint32_t value used for bounds.
	uint32_t u;
} bounds_packed_t;


/**
*	@brief	entity and player states are pre-quantized before sending to make delta
*			comparsion easier.
**/
typedef struct {
	uint16_t    number;
	uint8_t		entityType;
	uint16_t	otherEntityNumber;

	Vector3		origin;
	Vector3		angles;
	Vector3		old_origin;

	//! Solid for collision prediction.
	cm_solid_t solid;
	//! The bounding box for the solid's hull type, also needed for collision prediction.
	bounds_packed_t bounds;
	//! Clipmask for collision prediction.
	int32_t clipMask;
	//! Hull Contents for collision prediction.
	cm_contents_t hullContents;
	//! Entity which owns this entity, for collision prediction.
	int32_t ownerNumber;


	uint32_t	modelindex;
	uint32_t	modelindex2;
	uint32_t	modelindex3;
	uint32_t	modelindex4;

	uint32_t    skinnum;
	uint32_t    renderfx;
	uint32_t    effects;

	uint32_t    frame;
	uint32_t	old_frame;

	uint16_t    sound;

	int32_t		event;
	int32_t		eventParm0, eventParm1; // Belong to 'event'.

	// Spotlights
	Vector3 rgb;
	float intensity;
	float angle_width;
	float angle_falloff;
} entity_packed_t;

/**
*	@brief	entity and player states are pre-quantized before sending to make delta
*			comparsion easier.
**/
typedef struct {
	pmove_state_t   pmove;

	vec3_t			viewangles;

	int16_t			viewoffset[3];	// WID: new-pmove int8_t          viewoffset[3];
	int16_t			kick_angles[3];	// WID: new-pmove int8_t          kick_angles[3];

	// WID: Moved to CLGame.
	//int16_t		gunangles[3]; // WID: new-pmove //int8_t          gunangles[3];
	// WID: Moved to CLGame.
	//int16_t		gunoffset[3]; // WID: new-pmove //int8_t          gunoffset[3];

	uint32_t		gunModelIndex;
	uint8_t			gunAnimationID;
	//uint8_t         damage_blend[ 4 ];
	uint8_t         screen_blend[4];
	uint8_t         fov;
	int32_t			rdflags;
	int64_t         stats[MAX_STATS];

	uint8_t			eventSequence;
	uint8_t			events[ MAX_PS_EVENTS ];
	uint8_t			eventParms[ MAX_PS_EVENTS ];

	uint8_t			externalEvent;
	int32_t			externalEventParm0;
	int32_t			externalEventParm1;

	uint8_t			bobCycle;
} player_packed_t;

//! Extern access to the 'NULL Baseline' states of entity, player, and user commands.
extern const entity_packed_t    nullEntityState;
extern const player_packed_t    nullPlayerState;
extern const usercmd_t          nullUserCmd;

/**
*	@brief	Will encode/pack the mins/maxs bounds into the solid_packet_t uint32_t.
**/
static inline const bounds_packed_t MSG_PackBoundsUint32( const vec3_t mins, const vec3_t maxs ) {
	bounds_packed_t packedBounds;

	packedBounds.p.x = maxs[ 0 ];
	packedBounds.p.y = maxs[ 1 ];
	packedBounds.p.zd = -mins[ 2 ];
	packedBounds.p.zu = maxs[ 2 ] + 32;

	return packedBounds;
}
/**
*	@brief	Will decode/unpack the solid_packet_t uint32_t, into the pointers mins/maxs.
**/
static inline void MSG_UnpackBoundsUint32( const bounds_packed_t packedBounds, vec3_t mins, vec3_t maxs ) {
	mins[ 0 ] = -packedBounds.p.x;  maxs[ 0 ] = packedBounds.p.x;
	mins[ 1 ] = -packedBounds.p.y;  maxs[ 1 ] = packedBounds.p.y;
	mins[ 2 ] = -packedBounds.p.zd; maxs[ 2 ] = packedBounds.p.zu - 32;
}


/**
*
*   "Packing" POD types.
*
**/
/**
*	@brief	TODO: To be removed when removing more r1q2/q2pro protocol stuff.
*
*			CAN BE REMOVED NOW..
**/
//typedef enum {
//	MSG_PS_IGNORE_GUNINDEX      = (1 << 0),
//	MSG_PS_IGNORE_GUNFRAMES     = (1 << 1),
//	MSG_PS_IGNORE_BLEND         = (1 << 2),
//	MSG_PS_IGNORE_VIEWANGLES    = (1 << 3),
//	MSG_PS_IGNORE_DELTAANGLES   = (1 << 4),
//	MSG_PS_IGNORE_PREDICTION    = (1 << 5),      // mutually exclusive with IGNORE_VIEWANGLES
//	MSG_PS_FORCE                = (1 << 7),
//	MSG_PS_REMOVE               = (1 << 8)
//} msgPsFlags_t;

// Moved to shared.h since we need it in more places.
///**
//*	@brief	Entity State messaging properties.
//*/
//typedef enum {
//	MSG_ES_FORCE        = (1 << 0),
//	MSG_ES_NEWENTITY    = (1 << 1),
//	MSG_ES_FIRSTPERSON  = (1 << 2),
//	MSG_ES_UMASK        = (1 << 4),
//	MSG_ES_BEAMORIGIN   = (1 << 5),
//	MSG_ES_REMOVE       = (1 << 7)
//} msgEsFlags_t;


/**
*
*   Wire Delta Packing:
*
**/
/**
*   @brief	Pack a player state(in) encoding it into player_packed_t(out)
**/
void    MSG_PackPlayer( player_packed_t *out, const player_state_t *in );
/**
*   @brief	Pack an entity state(in) encoding it into entity_packed_t(out)
**/
void    MSG_PackEntity( entity_packed_t *out, const entity_state_t *in);



/**
*
*   Message Buffer functionality.
*
**/
//! Extern the access to the message "write" buffer.
extern sizebuf_t    msg_write;
extern byte         msg_write_buffer[ MAX_MSGLEN ];

//! Extern the access to the message "read" buffer.
extern sizebuf_t    msg_read;
extern byte         msg_read_buffer[ MAX_MSGLEN ];
	
/**
*   @details    Initialize default buffers, clearing allow overflow/underflow flags.
*
*               This is the only place where writing buffer is initialized. Writing buffer is
*               never allowed to overflow.
*
*               Reading buffer is reinitialized in many other places. Reinitializing will set
*               the allow underflow flag as appropriate.
**/
void MSG_Init( void );

/**
*   @brief	Resets the "Write" message buffer for a new write session.
**/
void MSG_BeginWriting( void );
/**
*   @brief	Resets the "Write" message buffer for a new write session.
**/
void MSG_BeginWritingOOB( void );
/**
*	@brief	Will copy(by appending) the 'data' memory into the "Write" message buffer.
*
*			Triggers Com_Errors in case of trouble such as 'Overflowing'.
**/
void *MSG_WriteData( const void *data, const size_t len );
/**
*	@brief	Will copy(by appending) the "Write" message buffer into the 'data' memory.
*
*			Triggers Com_Errors in case of trouble such as 'Overflowing'.
*
*			Will clear the "Write" message buffer after succesfully copying the data.
**/
void MSG_FlushTo( sizebuf_t *buf );
/**
*   @brief
**/
void MSG_BeginReading( void );
/**
*   @brief	OOB variant.
**/
void MSG_BeginReadingOOB( void );
/**
*   @brief
**/
byte *MSG_ReadData( const size_t len );


/**
*	@brief
**/
void MSG_WriteBits( const int64_t value, const int64_t bits );
/**
*	@brief
**/
const int64_t MSG_ReadBits( const int64_t bits );
/**
*	@brief
**/
void MSG_FlushBits( void );



/**
*
*   Wire Types "Write" functionality.
*
**/
/**
*   @brief Writes a signed 8 bit byte.
**/
void MSG_WriteInt8( const int32_t c);
/**
*   @brief Writes an unsigned 8 bit byte.
**/
void MSG_WriteUint8( const int32_t c );
/**
*   @brief Writes a signed 16 bit short.
**/
void MSG_WriteInt16( const int32_t c );
/**
*   @brief Writes an unsigned 16 bit short.
**/
void MSG_WriteUint16( const int32_t c );
/**
*   @brief Writes a 32 bit integer.
**/
void MSG_WriteInt32( const int32_t c );
/**
*   @brief Writes a 32 bit integer.
**/
void MSG_WriteUint32( const uint32_t c );
/**
*   @brief Writes a 64 bit integer.
**/
void MSG_WriteInt64( const int64_t c );
/**
*   @brief Writes a 64 bit integer.
**/
void MSG_WriteUint64( const uint64_t c );
/**
*   @brief Writes an unsigned LEB 128(base 128 encoded) integer.
**/
void MSG_WriteUintBase128( uint64_t c );
/**
*   @brief Writes a zic-zac encoded signed integer.
**/
void MSG_WriteIntBase128( const int64_t c );
/**
*   @brief Writes a full precision float. (Transfered over the wire as an int32_t).
**/
void MSG_WriteFloat( const float f );
/**
*   @return The first 13 bits of what was a full precision float. Hence, 'truncated' float.
**/
void MSG_WriteTruncatedFloat( const float f);
/**
*   @brief Writes a half float, lesser precision. (Transfered over the wire as an uint16_t)
**/
void MSG_WriteHalfFloat( const float f );
/**
*   @brief Writes a full precision double. (Transfered over the wire as an int64_t).
**/
void MSG_WriteDouble( const double d );

/**
*   @brief Writes a character string.
**/
void MSG_WriteString( const char *s );

/**
*	@brief	Writes an 8 bit byte encoded angle value of (float)f.
**/
void MSG_WriteAngle8( const float f );
/**
*	@brief	Writes a 16 bit short encoded angle value of (float)f.
**/
void MSG_WriteAngle16( const float f );
/**
*	@brief	Reads a short and decodes its 'half float' into a float angle.
**/
void MSG_WriteAngleHalfFloat( const float f );

/**
*   @brief Writes an optional 'short' encoded coordinate position vector.
**/
void MSG_WritePosition( const vec3_t pos, const msgPositionEncoding_t encoding );
/**
*	@brief	Writes an 8 bit byte, table index encoded direction vector.
**/
void MSG_WriteDir8( const vec3_t vector );

#if USE_CLIENT
/**
*   @brief Write a client's delta move command.
**/
const int64_t MSG_WriteDeltaUserCommand( const usercmd_t *from, const usercmd_t *cmd, const int32_t protocolVersion );
#endif
/**
*   @brief  Writes entity number, remove bit, and the byte mask to buffer.
**/
void MSG_WriteEntityNumber( const int32_t number, const bool remove, const uint64_t byteMask );
/**
*   @brief Writes the delta values of the entity state.
**/
void MSG_WriteDeltaEntity( const entity_packed_t *from, const entity_packed_t *to, msgEsFlags_t flags );
/**
*   @brief Writes the delta player state.
**/
void MSG_WriteDeltaPlayerstate( const player_packed_t *from, const player_packed_t *to );



/**
*
*   Wire Types "Read" functionality.
*
**/
/**
*   @return Signed 8 bit byte. Returns -1 if no more characters are available
**/
const int32_t MSG_ReadInt8( void );
/**
*   @return Unsigned 8 bit byte. Returns -1 if no more characters are available
**/
const int32_t MSG_ReadUint8( void );
/**
*   @return Signed 16 bit short.
**/
const int32_t MSG_ReadInt16( void );
/**
*   @return Unsigned 16 bit short.
**/
const int32_t MSG_ReadUint16( void );
/**
*   @return Signed 32 bit int.
**/
const int32_t MSG_ReadInt32( void );
/**
*   @return Unsigned 32 bit int.
**/
const int32_t MSG_ReadUint32( void );
/**
*   @return Signed 64 bit int.
**/
const int64_t MSG_ReadInt64( void );
/**
*   @return Unsigned 64 bit int.
**/
const uint64_t MSG_ReadUint64( void );
/**
*   @return Base 128 decoded unsigned integer.
**/
const uint64_t MSG_ReadUintBase128( void );
/**
*   @return Zig-Zac decoded signed integer.
**/
const int64_t MSG_ReadIntBase128( void );

/**
*   @return The full precision float.
**/
const float MSG_ReadFloat( void );
/**
*   @return The first 13 bits of what was a full precision float. Hence, 'truncated' float.
**/
const float MSG_ReadTruncatedFloat( void );
/**
*   @return A half float, converted to float, keep in mind that half floats have less precision.
**/
const float MSG_ReadHalfFloat( void );
/**
*   @return The full precision double.
**/
const double MSG_ReadDouble( void );



/**
*   @return The full string until its end.
**/
const size_t MSG_ReadString( char *dest, const size_t size );
/**
*   @return The part of the string data up till the first '\n'
**/
const size_t MSG_ReadStringLine( char *dest, const size_t size );

/**
*	@brief Reads a byte and decodes it to a float angle.
**/
const float MSG_ReadAngle8( void );
/**
*	@brief Reads a short and decodes it to a float angle.
**/
const float MSG_ReadAngle16( void );
/**
*	@brief	Reads a short and decodes its 'half float' into a float angle.
**/
const float MSG_ReadAngleHalfFloat( void );

/**
*	@brief	Reads a byte, and decodes it using it as an index into our directions table.
**/
void MSG_ReadDir8( vec3_t dir );
#if USE_CLIENT
	void    MSG_ReadDir8( vec3_t vector );

	/**
	*	@return The read positional coordinate. Optionally from 'short' to float. (Limiting in the range of -4096/+4096
	**/
	void    MSG_ReadPos( vec3_t pos, const msgPositionEncoding_t encoding );
#endif
/**
*   @brief Read a client's delta move command.
**/
void MSG_ParseDeltaUserCommand( const usercmd_t *from, usercmd_t *cmd );
/**
* @brief Reads out entity number of current message in the buffer.
*
* @param remove     Set to true in case a remove bit was send along the wire..
* @param byteMask   Mask containing all the bits of message data to read out.
*
* @return   The entity number.
**/
const int32_t MSG_ReadEntityNumber( bool *remove, uint64_t *byteMask );

/**
*   @brief Reads the delta entity state, can go from either a baseline or a previous packet Entity State.
**/
void MSG_ParseDeltaEntity( const entity_state_t *from, entity_state_t *to, int number, uint64_t bits, msgEsFlags_t flags );
#if USE_CLIENT
	/**
	*   @brief  Parses the delta packets of player states.
	*			Can go from either a baseline or a previous packet_entity
	**/
	void MSG_ParseDeltaPlayerstate( const player_state_t *from, player_state_t *to, uint64_t flags );
#endif



/**
*
*   Wire Message Debug Functionality.
*
**/
#if USE_DEBUG
	#if USE_CLIENT
		void    MSG_ShowDeltaEntityBits( const uint64_t bits );
		void    MSG_ShowDeltaPlayerstateBits( const uint64_t flags );
		void	MSG_ShowDeltaUserCommandBits( const int32_t bits );
		const char *MSG_ServerCommandString( const int32_t cmd );
		#define MSG_ShowSVC(cmd) \
			Com_LPrintf(PRINT_DEVELOPER, "%3zu:%s\n", msg_read.readcount - 1, \
				MSG_ServerCommandString(cmd))
	#endif // USE_CLIENT
#endif // USE_DEBUG
