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

#ifndef SIZEBUF_H
#define SIZEBUF_H

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
extern "C" {
#endif

	/**
	*	@brief	"SizeBuffer", used as a safe memory buffer(mainly for net-code).
	**/
	typedef struct sizebuf_s {
		//bool        allowoverflow;
		//bool        allowunderflow;
		//bool        overflowed;     // set to true if the buffer size failed
		//byte        *data;
		//size_t      maxsize;
		//size_t      cursize;
		//size_t      readcount;
		//uint32_t    bits_buf;
		//uint32_t    bits_left;
		//const char  *tag;			// for debugging
		qboolean	allowoverflow;	// if false, do a Com_Error
		qboolean	allowunderflow;	// This seems silly but still.
		byte		*data;
		qboolean	overflowed;		// set to true if the buffer size failed (with allowoverflow set)
		qboolean	oob;			// set to true if the buffer size failed (with allowoverflow set)
		int32_t		maxsize;
		int32_t		cursize;
		int32_t		readcount;
		int32_t		bit;				// for bitwise reads and writes
		const char *tag;				// for debugging
	} sizebuf_t;


	void SZ_Init( sizebuf_t *buf, void *data, const size_t size );
	void SZ_TagInit( sizebuf_t *buf, void *data, const size_t size, const char *tag );
	void *SZ_GetSpace( sizebuf_t *buf, size_t len );
	void SZ_Clear( sizebuf_t *buf );

	void SZ_WriteInt8( sizebuf_t *sb, const int32_t c );
	void SZ_WriteUint8( sizebuf_t *sb, const uint32_t c );
	void SZ_WriteInt16( sizebuf_t *sb, const int32_t c );
	void SZ_WriteUint16( sizebuf_t *sb, const uint32_t c );
	void SZ_WriteInt32( sizebuf_t *sb, const int32_t c );
	void SZ_WriteUint32( sizebuf_t *sb, const uint32_t c );
	void SZ_WriteInt64( sizebuf_t *sb, const int64_t c );
	void SZ_WriteUint64( sizebuf_t *sb, const uint64_t c );
	void SZ_WriteUintBase128( sizebuf_t *sb, uint64_t c );
	void SZ_WriteIntBase128( sizebuf_t *sb, const int64_t c );
	void SZ_WriteHalfFloat( sizebuf_t *sb, const float f );
	void SZ_WriteFloat( sizebuf_t *sb, const float f );
	void SZ_WriteString( sizebuf_t *sb, const char *s );

	static inline void *SZ_WriteData( sizebuf_t *buf, const void *data, const size_t len ) {
		return memcpy( SZ_GetSpace( buf, len ), data, len );
	}
	void *SZ_ReadData( sizebuf_t *buf, size_t len );

	void SZ_WriteBits( sizebuf_t *sb, int32_t value, int32_t bits );
	const int32_t SZ_ReadBits( sizebuf_t *sb, int32_t bits );

	const int32_t SZ_ReadInt8( sizebuf_t *sb );
	const int32_t SZ_ReadUint8( sizebuf_t *sb );
	const int32_t SZ_ReadInt16( sizebuf_t *sb );
	const int32_t SZ_ReadUint16( sizebuf_t *sb );
	const int32_t SZ_ReadInt32( sizebuf_t *sb );
	const int32_t SZ_ReadUint32( sizebuf_t *sb );
	const int64_t SZ_ReadInt64( sizebuf_t *sb );
	const int64_t SZ_ReadUint64( sizebuf_t *sb );
	const uint64_t SZ_ReadUintBase128( sizebuf_t *sb );
	const int64_t SZ_ReadIntBase128( sizebuf_t *sb );

	const float SZ_ReadHalfFloat( sizebuf_t *sb );
	const float SZ_ReadFloat( sizebuf_t *sb );

// WID: C++20: In case of C++ including this..
#ifdef __cplusplus
// We extern "C"
};
#endif
#endif // SIZEBUF_H
