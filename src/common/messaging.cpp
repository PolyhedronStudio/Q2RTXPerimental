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
#include "common/halffloat.h"
#include "common/messaging.h"
#include "common/protocol.h"
#include "common/sizebuf.h"
#include "common/huffman.h"
#include "common/math.h"
#include "common/intreadwrite.h"

//////////////////////////////////////////// HUFFMAN /////////////////////////////////////////////
huffman_t		msgHuff;
static qboolean			msgInit = qfalse;
int32_t pcount[ 256 ];
int32_t oldsize = 0;
static void MSG_initHuffman( void );
//////////////////////////////////////////////////////////////////////////////////////////////////

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
	// First initialize Huffman if we haven't already.
	if ( !msgInit ) {
		MSG_initHuffman();
	}

	// Initialize read buffer.
	SZ_TagInit( &msg_read, msg_read_buffer, MAX_MSGLEN, "msg_read" );
	// Initialize write buffer.
	SZ_TagInit( &msg_write, msg_write_buffer, MAX_MSGLEN, "msg_write" );
}

/**
*   @brief	Resets the "Write" message buffer for a new write session.
**/
void MSG_BeginWriting( void ) {
	// First initialize Huffman if we haven't already.
	if ( !msgInit ) {
		MSG_initHuffman();
	}

	msg_write.cursize = 0;
	msg_write.bit = 0;
	msg_write.oob = false;
	msg_write.overflowed = false;
}
/**
*   @brief	Resets the "Write" message buffer for a new write session.
**/
void MSG_BeginWritingOOB( void ) {
	// First initialize Huffman if we haven't already.
	if ( !msgInit ) {
		MSG_initHuffman();
	}

	msg_write.cursize = 0;
	msg_write.bit = 0;
	msg_write.oob = true;
	msg_write.overflowed = false;
}

/**
*	@brief	Will copy(by appending) the 'data' memory into the "Write" message buffer.
*
*			Triggers Com_Errors in case of trouble such as 'Overflowing'.
**/
void *MSG_WriteData( const void *data, const size_t len ) {
	return SZ_Write( &msg_write, data, len );
	
	//return memcpy( SZ_GetSpace( &msg_write, len ), data, len );
	
	//int i;
	//for ( i = 0; i < len; i++ ) {
	//	MSG_WriteUint8( ( (byte *)data )[ i ] );
	//}
	//return const_cast<void*>(data);
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
void MSG_ClearReadBuffer() {
	SZ_Clear( &msg_read );
}
void MSG_ClearWriteBuffer() {
	SZ_Clear( &msg_write );
}
/**
*   @brief
**/
void MSG_BeginReading( void ) {
	// First initialize Huffman if we haven't already.
	if ( !msgInit ) {
		MSG_initHuffman();
	}

	msg_read.readcount = 0;
	msg_read.bit = 0;
	msg_read.oob = false;
}
/**
*   @brief
**/
void MSG_BeginReadingOOB( void ) {
	// First initialize Huffman if we haven't already.
	if ( !msgInit ) {
		MSG_initHuffman();
	}

	msg_read.readcount = 0;
	msg_read.bit = 0;
	msg_read.oob = true;
}

/**
*   @brief
**/
byte* MSG_ReadData( const size_t len ) {
	//int		i;

	//for ( i = 0; i < len; i++ ) {
	//	( (byte *)data )[ i ] = MSG_ReadUint8( );
	//}
	return static_cast<byte *>( SZ_ReadData( &msg_read, len ) ); // WID: C++20: Added cast.
}




/**
*	@brief
**/
void MSG_WriteBits( int32_t value, int32_t bits ) {
	SZ_WriteBits( &msg_write, value, bits );
}

/**
*	@brief
**/
const int32_t MSG_ReadBits( int32_t bits ) {
	return SZ_ReadBits( &msg_read, bits );
}

/**
*	@brief	
**/
void MSG_FlushBits( void ) {
	//uint32_t bits_buf = msg_write.bits_buf;
	//uint32_t bits_left = msg_write.bits_left;

	//while ( bits_left < 32 ) {
	//	MSG_WriteUint8( bits_buf & 255 );
	//	bits_buf >>= 8;
	//	bits_left += 8;
	//}

	//msg_write.bits_buf = 0;
	//msg_write.bits_left = 32;
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
	#ifdef PARANOID
	Q_assert( c >= -128 && c <= 127 );
	#endif
	SZ_WriteInt8( &msg_write, c );
}
/**
*   @brief Writes an unsigned 8 bit byte.
**/
void MSG_WriteUint8( const int32_t c ) {
	#ifdef PARANOID
	Q_assert( c >= 0 && c <= 255 );
	#endif
	SZ_WriteUint8( &msg_write, c );
}
/**
*   @brief Writes a signed 16 bit short.
**/
void MSG_WriteInt16( const int32_t c ) {
	#ifdef PARANOID
	Q_assert( c >= -0x8000 && c <= 0x7fff );
	#endif
	SZ_WriteInt16( &msg_write, c );
}
/**
*   @brief Writes an unsigned 16 bit short.
**/
void MSG_WriteUint16( const int32_t c ) {
	#ifdef PARANOID
	Q_assert( c >= 0 && c <= 65535 );
	#endif
	SZ_WriteUint16( &msg_write, c );
}
/**
*   @brief Writes a signed 32 bit integer.
**/
void MSG_WriteInt32( const int32_t c ) {
	SZ_WriteInt32( &msg_write, c );
}
/**
*   @brief Writes an unsigned 32 bit integer.
**/
void MSG_WriteUint32( const uint32_t c ) {
	SZ_WriteUint32( &msg_write, c );
}
/**
*   @brief Writes a 64 bit integer.
**/
void MSG_WriteInt64( const int64_t c ) {
	SZ_WriteInt64( &msg_write, c );
}
/**
*   @brief Writes a 64 bit integer.
**/
void MSG_WriteUint64( const uint64_t c ) {
	SZ_WriteUint64( &msg_write, c );
}

/**
*   @brief Writes an unsigned LEB 128(base 128 encoded) integer.
**/
void MSG_WriteUintBase128( uint64_t c ) {
	SZ_WriteUintBase128( &msg_write, c );
}
/**
*   @brief Writes a zic-zac encoded signed integer.
**/
void MSG_WriteIntBase128( const int64_t c ) {
	return SZ_WriteIntBase128( &msg_write, c );
}

/**
*   @brief Writes a half float, lesser precision. (Transfered over the wire as an uint16_t)
**/
void MSG_WriteHalfFloat( const float f ) {
	SZ_WriteHalfFloat( &msg_write, f );
}
/**
*   @brief Writes a full precision float. (Transfered over the wire as an int32_t).
**/
void MSG_WriteFloat( const float f ) {
	SZ_WriteFloat( &msg_write, f );
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
*	@brief	Reads a short and decodes its 'half float' into a float angle.
**/
void MSG_WriteAngleHalfFloat( const vec_t f ) {
	MSG_WriteHalfFloat( f );
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
	return SZ_ReadInt8( &msg_read );
}
/**
*   @return Unsigned 8 bit byte. Returns -1 if no more characters are available
**/
const int32_t MSG_ReadUint8( void ) {
	return SZ_ReadUint8( &msg_read );
}

/**
*	@brief	... TODO ...
**/
int MSG_LookaheadByte( ) {
	const int bloc = Huff_getBloc();
	const int readcount = msg_read.readcount;
	const int bit = msg_read.bit;
	int c = MSG_ReadUint8( );
	Huff_setBloc( bloc );
	msg_read.readcount = readcount;
	msg_read.bit = bit;
	return c;
}
/**
*   @return Signed 16 bit short.
**/
const int32_t MSG_ReadInt16( void ) {
	return SZ_ReadInt16( &msg_read );
}
/**
*   @return Unsigned 16 bit short.
**/
const int32_t MSG_ReadUint16( void ) {
	return SZ_ReadUint16( &msg_read );
}
/**
*   @return Signed 32 bit int.
**/
const int32_t MSG_ReadInt32( void ) {
	return SZ_ReadInt32( &msg_read );
}
/**
*   @return Unsigned 32 bit int.
**/
const int32_t MSG_ReadUint32( void ) {
	return SZ_ReadUint32( &msg_read );
}
/**
*   @return Signed 64 bit int.
**/
const int64_t MSG_ReadInt64( void ) {
	return SZ_ReadInt64( &msg_read );
}
/**
*   @return Unsigned 64 bit int.
**/
const uint64_t MSG_ReadUint64( void ) {
	return SZ_ReadUint64( &msg_read );
}

/**
*   @return Base 128 decoded unsigned integer.
**/
const uint64_t MSG_ReadUintBase128( ) {
	return SZ_ReadUintBase128( &msg_read );
}
/**
*   @return Zig-Zac decoded signed integer.
**/
const int64_t MSG_ReadIntBase128( ) {
	return SZ_ReadIntBase128( &msg_read );
}

/**
*   @return A half float, converted to float, keep in mind that half floats have less precision.
**/
const float MSG_ReadHalfFloat() {
	return SZ_ReadHalfFloat( &msg_read );
}
/**
*   @return The full precision float.
**/
const float MSG_ReadFloat( ) {
	return SZ_ReadFloat( &msg_read );
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
*	@brief	Reads a short and decodes its 'half float' into a float angle.
**/
const float MSG_ReadAngleHalfFloat( void ) {
	return MSG_ReadHalfFloat();
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
void MSG_ReadPos( vec3_t pos, const qboolean decodeFromShort = false ) {
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

	if ( bits & U_FRAME )
		SHOWBITS( "frame" );
	if ( bits & U_SKIN )
		SHOWBITS( "skinnum" );
	if ( bits & U_EFFECTS )
		SHOWBITS( "effects" );
	if ( bits & U_RENDERFX )
		SHOWBITS( "renderfx" );

	S( ORIGIN1, "origin[0]" );
	S( ORIGIN2, "origin[1]" );
	S( ORIGIN3, "origin[2]" );
	S( ANGLE1, "angles[0]" );
	S( ANGLE2, "angles[1]" );
	S( ANGLE3, "angles[2]" );
	S( OLDORIGIN, "old_origin" );
	if ( bits & U_SOUND )
		SHOWBITS( "sound" );
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
	S( BLEND, "screen_blend" );
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


/**
*
*
*	Huffman Algorithm
*
*
**/
int msg_hData[ 256 ] = {
250315,			// 0
41193,			// 1
6292,			// 2
7106,			// 3
3730,			// 4
3750,			// 5
6110,			// 6
23283,			// 7
33317,			// 8
6950,			// 9
7838,			// 10
9714,			// 11
9257,			// 12
17259,			// 13
3949,			// 14
1778,			// 15
8288,			// 16
1604,			// 17
1590,			// 18
1663,			// 19
1100,			// 20
1213,			// 21
1238,			// 22
1134,			// 23
1749,			// 24
1059,			// 25
1246,			// 26
1149,			// 27
1273,			// 28
4486,			// 29
2805,			// 30
3472,			// 31
21819,			// 32
1159,			// 33
1670,			// 34
1066,			// 35
1043,			// 36
1012,			// 37
1053,			// 38
1070,			// 39
1726,			// 40
888,			// 41
1180,			// 42
850,			// 43
960,			// 44
780,			// 45
1752,			// 46
3296,			// 47
10630,			// 48
4514,			// 49
5881,			// 50
2685,			// 51
4650,			// 52
3837,			// 53
2093,			// 54
1867,			// 55
2584,			// 56
1949,			// 57
1972,			// 58
940,			// 59
1134,			// 60
1788,			// 61
1670,			// 62
1206,			// 63
5719,			// 64
6128,			// 65
7222,			// 66
6654,			// 67
3710,			// 68
3795,			// 69
1492,			// 70
1524,			// 71
2215,			// 72
1140,			// 73
1355,			// 74
971,			// 75
2180,			// 76
1248,			// 77
1328,			// 78
1195,			// 79
1770,			// 80
1078,			// 81
1264,			// 82
1266,			// 83
1168,			// 84
965,			// 85
1155,			// 86
1186,			// 87
1347,			// 88
1228,			// 89
1529,			// 90
1600,			// 91
2617,			// 92
2048,			// 93
2546,			// 94
3275,			// 95
2410,			// 96
3585,			// 97
2504,			// 98
2800,			// 99
2675,			// 100
6146,			// 101
3663,			// 102
2840,			// 103
14253,			// 104
3164,			// 105
2221,			// 106
1687,			// 107
3208,			// 108
2739,			// 109
3512,			// 110
4796,			// 111
4091,			// 112
3515,			// 113
5288,			// 114
4016,			// 115
7937,			// 116
6031,			// 117
5360,			// 118
3924,			// 119
4892,			// 120
3743,			// 121
4566,			// 122
4807,			// 123
5852,			// 124
6400,			// 125
6225,			// 126
8291,			// 127
23243,			// 128
7838,			// 129
7073,			// 130
8935,			// 131
5437,			// 132
4483,			// 133
3641,			// 134
5256,			// 135
5312,			// 136
5328,			// 137
5370,			// 138
3492,			// 139
2458,			// 140
1694,			// 141
1821,			// 142
2121,			// 143
1916,			// 144
1149,			// 145
1516,			// 146
1367,			// 147
1236,			// 148
1029,			// 149
1258,			// 150
1104,			// 151
1245,			// 152
1006,			// 153
1149,			// 154
1025,			// 155
1241,			// 156
952,			// 157
1287,			// 158
997,			// 159
1713,			// 160
1009,			// 161
1187,			// 162
879,			// 163
1099,			// 164
929,			// 165
1078,			// 166
951,			// 167
1656,			// 168
930,			// 169
1153,			// 170
1030,			// 171
1262,			// 172
1062,			// 173
1214,			// 174
1060,			// 175
1621,			// 176
930,			// 177
1106,			// 178
912,			// 179
1034,			// 180
892,			// 181
1158,			// 182
990,			// 183
1175,			// 184
850,			// 185
1121,			// 186
903,			// 187
1087,			// 188
920,			// 189
1144,			// 190
1056,			// 191
3462,			// 192
2240,			// 193
4397,			// 194
12136,			// 195
7758,			// 196
1345,			// 197
1307,			// 198
3278,			// 199
1950,			// 200
886,			// 201
1023,			// 202
1112,			// 203
1077,			// 204
1042,			// 205
1061,			// 206
1071,			// 207
1484,			// 208
1001,			// 209
1096,			// 210
915,			// 211
1052,			// 212
995,			// 213
1070,			// 214
876,			// 215
1111,			// 216
851,			// 217
1059,			// 218
805,			// 219
1112,			// 220
923,			// 221
1103,			// 222
817,			// 223
1899,			// 224
1872,			// 225
976,			// 226
841,			// 227
1127,			// 228
956,			// 229
1159,			// 230
950,			// 231
7791,			// 232
954,			// 233
1289,			// 234
933,			// 235
1127,			// 236
3207,			// 237
1020,			// 238
927,			// 239
1355,			// 240
768,			// 241
1040,			// 242
745,			// 243
952,			// 244
805,			// 245
1073,			// 246
740,			// 247
1013,			// 248
805,			// 249
1008,			// 250
796,			// 251
996,			// 252
1057,			// 253
11457,			// 254
13504,			// 255
};

static void MSG_initHuffman( void ) {
	int i, j;

	msgInit = qtrue;
	Huff_Init( &msgHuff );
	for ( i = 0; i < 256; i++ ) {
		for ( j = 0; j < msg_hData[ i ]; j++ ) {
			Huff_addRef( &msgHuff.compressor, (byte)i );			// Do update
			Huff_addRef( &msgHuff.decompressor, (byte)i );			// Do update
		}
	}
}
