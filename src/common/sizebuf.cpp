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
#include "common/protocol.h"
#include "common/sizebuf.h"
#include "common/intreadwrite.h"
#include "common/halffloat.h"
#include "common/huffman.h"



/////////////////////////////////////////////////////////////////////////
// TODO: Clean this? GetHuffMessageTree func?
extern huffman_t		msgHuff;
extern int32_t oldsize;
/////////////////////////////////////////////////////////////////////////

static constexpr int32_t FLOAT_INT_BITS = 13;
static constexpr int32_t FLOAT_INT_BIAS = ( 1 << ( FLOAT_INT_BITS - 1 ) );


/**
*
*
*	Initializing/Clearing:
*
*
**/
/**
*	@brief
**/
void SZ_TagInit( sizebuf_t *buf, void *data, const size_t size, const char *tag ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = static_cast<byte *>( data ); // WID: C++20: Added cast.
	buf->maxsize = size;
	buf->oob = false;
	buf->tag = tag;
}
/**
*	@brief
**/
void SZ_TagInitOOB( sizebuf_t *buf, void *data, const size_t size, const char *tag ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = static_cast<byte *>( data ); // WID: C++20: Added cast.
	buf->maxsize = size;
	buf->oob = true;
	buf->tag = tag;
}
/**
*	@brief
**/
void SZ_Init( sizebuf_t *buf, void *data, const size_t size ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = static_cast<byte *>( data );
	buf->maxsize = size;
	buf->allowoverflow = true;
	buf->allowunderflow = true;
	buf->oob = false;
	buf->tag = "none";
}
/**
*	@brief
**/
void SZ_InitOOB( sizebuf_t *buf, void *data, const size_t size ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = static_cast<byte *>( data );
	buf->maxsize = size;
	buf->allowoverflow = true;
	buf->allowunderflow = true;
	buf->oob = true;
	buf->tag = "none";
}
/**
*	@brief	Gets a pointer to the buffer's data for writing, increases its curernt size by len.
**/
void *SZ_GetSpace( sizebuf_t *buf, const size_t len ) {
	void *data;

	if ( buf->cursize > buf->maxsize ) {
		Com_Error( ERR_FATAL,
			"%s: %s: already overflowed",
			__func__, buf->tag );
	}

	if ( len > buf->maxsize - buf->cursize ) {
		if ( len > buf->maxsize ) {
			Com_Error( ERR_FATAL,
				"%s: %s: %zu is > full buffer size %zu",
				__func__, buf->tag, len, buf->maxsize );
		}

		if ( !buf->allowoverflow ) {
			Com_Error( ERR_FATAL,
				"%s: %s: overflow without allowoverflow set",
				__func__, buf->tag );
		}

		//Com_DPrintf("%s: %s: overflow\n", __func__, buf->tag);
		SZ_Clear( buf );
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += len;
	buf->bit = buf->cursize * 8;
	//buf->bit += len * 8;

	return data;
}
/**
*	@brief	Clears the sizebuffer state as in a reset for read/write.
**/
void SZ_Clear( sizebuf_t *buf ) {
	buf->cursize = 0;
	buf->readcount = 0;
	buf->overflowed = false;

	buf->bit = 0;					//<- in bits
}



/**
*
*
*	Writing generic types as bytes, using SZ_GetSpace:
*
*
**/
/**
*	@brief	
**/
void SZ_WriteInt8( sizebuf_t *sb, const int32_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 1 ) ); // WID: C++20: Added cast.
	buf[ 0 ] = c;
}
/**
*	@brief
**/
void SZ_WriteUint8( sizebuf_t *sb, const uint32_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 1 ) ); // WID: C++20: Added cast.
	buf[ 0 ] = c;
}

/**
*	@brief
**/
void SZ_WriteInt16( sizebuf_t *sb, const int32_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 2 ) ); // WID: C++20: Added cast.
	WL16( buf, c );
}
/**
*	@brief
**/
void SZ_WriteUint16( sizebuf_t *sb, const uint32_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 2 ) ); // WID: C++20: Added cast.
	WL16( buf, c );
}

/**
*	@brief
**/
void SZ_WriteInt32( sizebuf_t *sb, const int32_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 4 ) );
	WL32( buf, c );
}
/**
*	@brief
**/
void SZ_WriteUint32( sizebuf_t *sb, const uint32_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 4 ) );
	WL32( buf, c );
}

/**
*	@brief
**/
void SZ_WriteInt64( sizebuf_t *sb, const int64_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 8 ) );
	WL64( buf, c );
}
/**
*	@brief
**/
void SZ_WriteUint64( sizebuf_t *sb, const uint64_t c ) {
	byte *buf = static_cast<byte *>( SZ_GetSpace( sb, 8 ) );
	WL64( buf, c );
}

/**
*   @brief Writes an unsigned LEB 128(base 128 encoded) integer.
**/
void SZ_WriteUintBase128( sizebuf_t *sb, uint64_t c ) {
	uint8_t buf[ 10 ];
	size_t len = 0;

	do {
		buf[ len ] = c & 0x7fU;
		if ( c >>= 7 ) {
			buf[ len ] |= 0x80U;
		}
		len++;
	} while ( c );

	SZ_WriteData( sb, buf, len );
}
/**
*   @brief Writes a zic-zac encoded signed integer.
**/
void SZ_WriteIntBase128( sizebuf_t *sb, const int64_t c ) {
	// use Zig-zag encoding for signed integers for more efficient storage
	const uint64_t cc = (uint64_t)( c << 1 ) ^ (uint64_t)( c >> 63 );
	SZ_WriteUintBase128( sb, cc );
}

/**
*	@brief
**/
void SZ_WriteHalfFloat( sizebuf_t *sb, const float f ) {
	SZ_WriteUint16( sb, float_to_half( f ) );
}
/**
*	@brief
**/
void SZ_WriteFloat( sizebuf_t *sb, const float f ) {
	// Conversion trick:
	union {
		float f;
		int32_t l;
	} dat;
	dat.f = f;
	// Wire as int32_t.
	SZ_WriteInt32( sb, dat.l );

	// TODO: See what is up with gcc/g++ and this.
	// Incompatible so far with Linux.
	//SZ_WriteInt32( sb, std::bit_cast<std::int32_t>( f ) );
}
/**
*	@brief	Uses the first 13 bits.
**/
void SZ_WriteTruncatedFloat( sizebuf_t *sb, const float f ) {
	// Conversion trick:
	union {
		float f;
		int32_t l;
	} dat;
	dat.f = (int32_t)f; // Truncate it. First 13 bits are float bits.
	// Allows sending it as a small integer.
	dat.l += FLOAT_INT_BIAS;
	// Wire as int32_t.
	SZ_WriteIntBase128( sb, dat.l );
}

/**
*	@brief
**/
void SZ_WriteString( sizebuf_t *sb, const char *s ) {
	size_t len;

	if ( !s ) {
		SZ_WriteUint8( sb, 0 );
		return;
	}

	len = strlen( s );
	if ( len >= MAX_NET_STRING ) {
		Com_WPrintf( "%s: overflow: %zu chars", __func__, len );
		SZ_WriteUint8( sb, 0 );
		return;
	}

	SZ_WriteData( sb, s, len + 1 );
}



/**
*
*
*	Reading Int/Uint 8, 16, 32, 64, Base128, HalfFloat, Float, Str, StrLine:
* 
* 
**/
/**
*	@brief
**/
const int32_t SZ_ReadInt8( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 1 ) ); // WID: C++20: Added cast.
	return buf ? (int8_t)( *buf ) : -1;
}
/**
*	@brief
**/
const int32_t SZ_ReadUint8( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 1 ) ); // WID: C++20: Added cast.
	return buf ? (uint8_t)( *buf ) : -1;
}


/**
*	@brief
**/
const int32_t SZ_ReadInt16( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 2 ) ); // WID: C++20: Added cast.
	return buf ? (int16_t)RL16( buf ) : -1;
}
/**
*	@brief
**/
const int32_t SZ_ReadUint16( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 2 ) ); // WID: C++20: Added cast.
	return buf ? (uint16_t)RL16( buf ) : -1;
}


/**
*	@brief
**/
const int32_t SZ_ReadInt32( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 4 ) ); // WID: C++20: Added cast.
	return buf ? (int32_t)RL32( buf ) : -1;
}
/**
*	@brief
**/
const int32_t SZ_ReadUint32( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 4 ) ); // WID: C++20: Added cast.
	return buf ? (uint32_t)RL32( buf ) : -1;
}


/**
*	@brief
**/
const int64_t SZ_ReadInt64( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 8 ) ); // WID: C++20: Added cast.
	return buf ? (int64_t)RL64( buf ) : -1;
}
/**
*	@brief	
**/
const int64_t SZ_ReadUint64( sizebuf_t *sb ) {
	byte *buf = static_cast<byte *>( SZ_ReadData( sb, 8 ) ); // WID: C++20: Added cast.
	return buf ? (uint64_t)RL64( buf ) : -1;
}

/**
*   @return Base 128 decoded unsigned integer.
**/
const uint64_t SZ_ReadUintBase128( sizebuf_t *sb ) {
	size_t   len = 0;
	uint64_t i = 0;

	while ( len < 10 ) {
		const uint8_t c = SZ_ReadUint8( sb );
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
const int64_t SZ_ReadIntBase128( sizebuf_t *sb ) {
	// un-Zig-Zag our value back to a signed integer
	const uint64_t c = SZ_ReadUintBase128( sb );
	return (int64_t)( c >> 1 ) ^ ( -(int64_t)( c & 1 ) );
}

/**
*	@brief	 
**/
const float SZ_ReadHalfFloat( sizebuf_t *sb ) {
	const uint16_t half = SZ_ReadUint16( sb );
	return half_to_float( half );
}
/**
*	@brief
**/
const float SZ_ReadFloat( sizebuf_t *sb ) {
	// Conversion trick.
	union {
		float f;
		int32_t l;
	} dat;

	dat.l = SZ_ReadInt32( sb );
	return dat.f;
}
/**
*	@brief	Uses the first 13 bits.
**/
const float SZ_ReadTruncatedFloat( sizebuf_t *sb ) {
	// Conversion trick.
	union {
		float f;
		int32_t l;
	} dat;

	dat.l = SZ_ReadIntBase128( sb );
	// bias to allow equal parts positive and negative
	dat.l -= FLOAT_INT_BIAS;
	return dat.f;
}











/**
*
*
*	Byte based ReadData/WriteData:
*
*
**/
void *SZ_ReadData( sizebuf_t *buf, const size_t len ) {
	void *data = nullptr;

	if ( buf->readcount > buf->cursize || len > buf->cursize - buf->readcount ) {
		if ( !buf->allowunderflow ) {
			Com_Error( ERR_DROP, "%s: read past end of message", __func__ );
		}
		buf->readcount = buf->cursize + 1;
		return nullptr;
	}

	data = buf->data + buf->readcount;
	buf->readcount += len;
	buf->bit = buf->readcount * 8;
	//buf->bit += len * 8;
	return data;
}


/**
*
*
*	BitStream Read/Write:
*
*
**/
/**
*	@brief	Will write the value bits into the size buffer's data pointer + current offset.
**/
void SZ_WriteBits( sizebuf_t *sb, int32_t value, int32_t bits ) {
	int	i;

	// TODO: Just apply msg_write but testing this for now.
	sizebuf_t *msg = sb;

	oldsize += bits;

	if ( msg->overflowed ) {
		return;
	}

	if ( bits == 0 || bits < -31 || bits > 32 ) {
		Com_Error( ERR_DROP, "%s: bad bits %i", __func__, bits );
	}

	if ( bits < 0 ) {
		bits = -bits;
	}

	if ( msg->oob ) {
		if ( msg->cursize + ( bits >> 3 ) > msg->maxsize ) {
			msg->overflowed = qtrue;
			return;
		}

		if ( bits == 8 ) {
			msg->data[ msg->cursize ] = value;
			msg->cursize += 1;
			msg->bit += 8;
		} else if ( bits == 16 ) {
			short temp = value;

			//CopyLittleShort( &msg->data[ msg->cursize ], &temp );
			LittleBlock( &msg->data[ msg->cursize ], &temp, 2 );
			msg->cursize += 2;
			msg->bit += 16;
		} else if ( bits == 32 ) {
			//CopyLittleLong( &msg->data[ msg->cursize ], &value );
			LittleBlock( &msg->data[ msg->cursize ], &value, 4 );

			msg->cursize += 4;
			msg->bit += 32;
		} else {
			Com_Error( ERR_DROP, "%s: can't write %d bits", __func__, bits );
		}
	} else {
		value &= ( 0xffffffff >> ( 32 - bits ) );
		if ( bits & 7 ) {
			int nbits;
			nbits = bits & 7;
			if ( msg->bit + nbits > msg->maxsize << 3 ) {
				msg->overflowed = qtrue;
				return;
			}
			for ( i = 0; i < nbits; i++ ) {
				Huff_putBit( ( value & 1 ), msg->data, &msg->bit );
				value = ( value >> 1 );
			}
			bits = bits - nbits;
		}
		if ( bits ) {
			for ( i = 0; i < bits; i += 8 ) {
				Huff_offsetTransmit( &msgHuff.compressor, ( value & 0xff ), msg->data, &msg->bit, msg->maxsize << 3 );
				value = ( value >> 8 );

				if ( msg->bit > msg->maxsize << 3 ) {
					msg->overflowed = qtrue;
					return;
				}
			}
		}
		msg->cursize = ( msg->bit >> 3 ) + 1;
	}
}
/**
*	@brief	Will read the requested amount of bits from the sizebuffer's data pointer + offset.
**/
const int32_t SZ_ReadBits( sizebuf_t *sb, int32_t bits ) {
	int			value;
	int			get;
	qboolean	sgn;
	int			i, nbits;
	//	FILE*	fp;

	// TODO: Just apply msg_write but testing this for now.
	sizebuf_t *msg = sb;

	if ( msg->readcount > msg->cursize ) {
		return 0;
	}

	value = 0;

	if ( bits < 0 ) {
		bits = -bits;
		sgn = qtrue;
	} else {
		sgn = qfalse;
	}

	if ( msg->oob ) {
		if ( msg->readcount + ( bits >> 3 ) > msg->cursize ) {
			msg->readcount = msg->cursize + 1;
			return 0;
		}

		if ( bits == 8 ) {
			value = msg->data[ msg->readcount ];
			msg->readcount += 1;
			msg->bit += 8;
		} else if ( bits == 16 ) {
			short temp;

			//CopyLittleShort( &temp, &msg->data[ msg->readcount ] );
			LittleBlock( &temp, &msg->data[ msg->readcount ], 2 );
			value = temp;
			msg->readcount += 2;
			msg->bit += 16;
		} else if ( bits == 32 ) {
			//CopyLittleLong( &value, &msg->data[ msg->readcount ] );
			LittleBlock( &value, &msg->data[ msg->readcount ], 4 );
			msg->readcount += 4;
			msg->bit += 32;
		} else
			Com_Error( ERR_DROP, "can't read %d bits", bits );
	} else {
		nbits = 0;
		if ( bits & 7 ) {
			nbits = bits & 7;
			if ( msg->bit + nbits > msg->cursize << 3 ) {
				msg->readcount = msg->cursize + 1;
				return 0;
			}
			for ( i = 0; i < nbits; i++ ) {
				value |= ( Huff_getBit( msg->data, &msg->bit ) << i );
			}
			bits = bits - nbits;
		}
		if ( bits ) {
			//			fp = fopen("c:\\netchan.bin", "a");
			for ( i = 0; i < bits; i += 8 ) {
				Huff_offsetReceive( msgHuff.decompressor.tree, &get, msg->data, &msg->bit, msg->cursize << 3 );
				//				fwrite(&get, 1, 1, fp);
				value = (unsigned int)value | ( (unsigned int)get << ( i + nbits ) );

				if ( msg->bit > msg->cursize << 3 ) {
					msg->readcount = msg->cursize + 1;
					return 0;
				}
			}
			//			fclose(fp);
		}
		msg->readcount = ( msg->bit >> 3 ) + 1;
	}
	if ( sgn && bits > 0 && bits < 32 ) {
		if ( value & ( 1 << ( bits - 1 ) ) ) {
			value |= -1 ^ ( ( 1 << bits ) - 1 );
		}
	}

	return value;
}