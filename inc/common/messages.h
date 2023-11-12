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

#ifndef MSG_H
#define MSG_H

#ifdef __cplusplus
extern "C" {
#endif

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
	typedef union solid_packed_u {
		struct p {
			uint8_t x;
			uint8_t y;
			uint8_t zd; // always negative
			uint8_t zu; // encoded as + 32
		} p;

		uint32_t u;
	} solid_packed_t;

	/**
	*	@brief	entity and player states are pre-quantized before sending to make delta
	*			comparsion easier.
	**/
	typedef struct {
		uint16_t    number;
		int16_t     origin[3];
		int16_t     angles[3];
		int16_t     old_origin[3];
		uint8_t     modelindex;
		uint8_t     modelindex2;
		uint8_t     modelindex3;
		uint8_t     modelindex4;
		uint32_t    skinnum;
		uint32_t    effects;
		uint32_t    renderfx;
		solid_packed_t solid;
		uint16_t    frame;
		uint8_t     sound;
		uint8_t     event;
	} entity_packed_t;

	/**
	*	@brief	entity and player states are pre-quantized before sending to make delta
	*			comparsion easier.
	**/
	typedef struct {
		pmove_state_t   pmove;
		int16_t         viewangles[3];
		int8_t          viewoffset[3];
		int8_t          kick_angles[3];
		int8_t          gunangles[3];
		int8_t          gunoffset[3];
		uint8_t         gunindex;
		uint8_t         gunframe;
		int8_t			gunrate;
		uint8_t         blend[4];
		uint8_t         fov;
		uint8_t         rdflags;
		int16_t         stats[MAX_STATS];
	} player_packed_t;

	//! Extern access to the 'NULL Baseline' states of entity, player, and user commands.
	extern const entity_packed_t    nullEntityState;
	extern const player_packed_t    nullPlayerState;
	extern const usercmd_t          nullUserCmd;

	/**
	*	@brief	Will encode/pack the mins/maxs bounds into the solid_packet_t uint32_t.
	**/
	static inline solid_packed_t MSG_PackSolidUint32( const vec3_t mins, const vec3_t maxs ) {
		solid_packed_t packedSolid;

		packedSolid.p.x = maxs[ 0 ];
		packedSolid.p.y = maxs[ 1 ];
		packedSolid.p.zd = -mins[ 2 ];
		packedSolid.p.zu = maxs[ 2 ] + 32;

		return packedSolid;
	}
	/**
	*	@brief	Will decode/unpack the solid_packet_t uint32_t, into the pointers mins/maxs.
	**/
	static inline void MSG_UnpackSolidUint32( uint32_t solid, vec3_t mins, vec3_t maxs ) {
		solid_packed_t packedSolid;
		packedSolid.u = solid;

		//packed.u = state->solid;
		mins[ 0 ] = -packedSolid.p.x;  maxs[ 0 ] = packedSolid.p.x;
		mins[ 1 ] = -packedSolid.p.y;  maxs[ 1 ] = packedSolid.p.y;
		mins[ 2 ] = -packedSolid.p.zd; maxs[ 2 ] = packedSolid.p.zu - 32;
	}



	/**
	*
	*   "Packing" POD types.
	*
	**/
	/**
	*	@brief	TODO: To be removed when removing more r1q2/q2pro protocol stuff.
	**/
	typedef enum {
		MSG_PS_IGNORE_GUNINDEX      = (1 << 0),
		MSG_PS_IGNORE_GUNFRAMES     = (1 << 1),
		MSG_PS_IGNORE_BLEND         = (1 << 2),
		MSG_PS_IGNORE_VIEWANGLES    = (1 << 3),
		MSG_PS_IGNORE_DELTAANGLES   = (1 << 4),
		MSG_PS_IGNORE_PREDICTION    = (1 << 5),      // mutually exclusive with IGNORE_VIEWANGLES
		MSG_PS_FORCE                = (1 << 7),
		MSG_PS_REMOVE               = (1 << 8)
	} msgPsFlags_t;

	/**
	*	@brief	Entity State messaging properties.
	*/
	typedef enum {
		MSG_ES_FORCE        = (1 << 0),
		MSG_ES_NEWENTITY    = (1 << 1),
		MSG_ES_FIRSTPERSON  = (1 << 2),
		//MSG_ES_LONGSOLID    = (1 << 3), // WID: upgr-solid: Depracated, we now use the Q2RE Approach.
		MSG_ES_UMASK        = (1 << 4),
		MSG_ES_BEAMORIGIN   = (1 << 5),
		MSG_ES_SHORTANGLES  = (1 << 6),
		MSG_ES_REMOVE       = (1 << 7)
	} msgEsFlags_t;



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
	*   @brief
	**/
	byte *MSG_ReadData( const size_t len );



	/**
	*
	*   Wire Types "Write" functionality.
	*
	**/
	void    MSG_WriteChar(int c);
	void    MSG_WriteByte(int c);
	void    MSG_WriteShort(int c);
	void    MSG_WriteLong(int c);
	void    MSG_WriteString(const char *s);
	void    MSG_WritePos(const vec3_t pos);
	void    MSG_WriteAngle(float f);
	#if USE_CLIENT
	void    MSG_FlushBits(void);
	void    MSG_WriteBits(int value, int bits);
	int     MSG_WriteDeltaUsercmd(const usercmd_t *from, const usercmd_t *cmd, int version);
	#endif
	void    MSG_WriteDir(const vec3_t vector);
	void    MSG_PackEntity(entity_packed_t *out, const entity_state_t *in, bool short_angles);
	void    MSG_WriteDeltaEntity(const entity_packed_t *from, const entity_packed_t *to, msgEsFlags_t flags);
	void    MSG_PackPlayer(player_packed_t *out, const player_state_t *in);
	void    MSG_WriteDeltaPlayerstate( const player_packed_t *from, const player_packed_t *to );


	/**
	*
	*   Wire Types "Read" functionality.
	*
	**/
	/**
	*   @brief
	**/
	void    MSG_BeginReading(void);
	/**
	*   @brief
	**/
	byte    *MSG_ReadData(size_t len);
	/**
	*   @brief	Returns -1 if no more characters are available
	**/
	int     MSG_ReadChar(void);
	/**
	*   @brief	Returns -1 if no more characters are available
	**/
	int     MSG_ReadByte(void);
	int     MSG_ReadShort(void);
	int     MSG_ReadWord(void);
	int     MSG_ReadLong(void);
	size_t  MSG_ReadString(char *dest, size_t size);
	size_t  MSG_ReadStringLine(char *dest, size_t size);
	#if USE_CLIENT
	void    MSG_ReadPos(vec3_t pos);
	void    MSG_ReadDir(vec3_t vector);
	#endif
	int     MSG_ReadBits(int bits);
	void    MSG_ReadDeltaUsercmd(const usercmd_t *from, usercmd_t *cmd);
	int     MSG_ParseEntityBits(int *bits);
	void    MSG_ParseDeltaEntity(const entity_state_t *from, entity_state_t *to, int number, int bits, msgEsFlags_t flags);
	#if USE_CLIENT
	void    MSG_ParseDeltaPlayerstate( const player_state_t *from, player_state_t *to, int flags );
	#endif


	/**
	*
	*   Wire Message Debug Functionality.
	*
	**/
	#if USE_DEBUG
	#if USE_CLIENT
	void    MSG_ShowDeltaPlayerstateBits( int flags );
	#endif
	#if USE_CLIENT
	void    MSG_ShowDeltaEntityBits(int bits);
	const char *MSG_ServerCommandString(int cmd);
	#define MSG_ShowSVC(cmd) \
		Com_LPrintf(PRINT_DEVELOPER, "%3zu:%s\n", msg_read.readcount - 1, \
			MSG_ServerCommandString(cmd))
	#endif // USE_CLIENT
	#endif // USE_DEBUG

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
};
#endif // #ifdef __cplusplus
#endif // MSG_H
