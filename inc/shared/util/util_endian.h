/********************************************************************
*
*
*   Endianness Utilities:
*
*
********************************************************************/
#pragma once

//
// WID: Taken from, https://stackoverflow.com/questions/41770887/cross-platform-definition-of-byteswap-uint64-and-byteswap-ulong
//
#ifdef _MSC_VER
	#include <stdlib.h>
	#define bswap_16(x) _byteswap_ushort(x)
	#define bswap_32(x) _byteswap_ulong(x)
	#define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
	// Mac OS X / Darwin features
	#include <libkern/OSByteOrder.h>
	#define bswap_16(x) OSSwapInt16(x)
	#define bswap_32(x) OSSwapInt32(x)
	#define bswap_64(x) OSSwapInt64(x)
#elif defined(__sun) || defined(sun)
	#include <sys/byteorder.h>
	#define bswap_16(x) BSWAP_16(x)
	#define bswap_32(x) BSWAP_32(x)
	#define bswap_64(x) BSWAP_64(x)
#elif defined(__FreeBSD__)
	#include <sys/endian.h>
	#define bswap_16(x) bswap16(x)	
	#define bswap_32(x) bswap32(x)
	#define bswap_64(x) bswap64(x)
#elif defined(__OpenBSD__)
	#include <sys/types.h>
	#define bswap_16(x) swap16(x)
	#define bswap_32(x) swap32(x)
	#define bswap_64(x) swap64(x)
#elif defined(__NetBSD__)
	#include <sys/types.h>
	#include <machine/bswap.h>
	#if defined(__BSWAP_RENAME) && !defined(__bswap_32)
		#define bswap_16(x) bswap16(x)	
		#define bswap_32(x) bswap32(x)
		#define bswap_64(x) bswap64(x)
	#endif
#else
	#include <byteswap.h>
#endif

/**
*	@brief 
**/
static inline const uint16_t ShortSwap( uint16_t s ) {
	//s = ( s >> 8 ) | ( s << 8 );
	//return s;
	return bswap_16( s );
}
/**
*	@brief
**/
static inline const uint32_t LongSwap( uint32_t l ) {
	//l = ( ( l >> 8 ) & 0x00ff00ff ) | ( ( l << 8 ) & 0xff00ff00 );
	//l = ( l >> 16 ) | ( l << 16 );
	//return l;
	return bswap_32( l );
}
/**
*	@brief
**/
static inline const uint64_t LongLongSwap( uint64_t ll ) {
	//ll = ( ll & 0x00000000FFFFFFFF ) << 32 | ( ll & 0xFFFFFFFF00000000 ) >> 32;
	//ll = ( ll & 0x0000FFFF0000FFFF ) << 16 | ( ll & 0xFFFF0000FFFF0000 ) >> 16;
	//ll = ( ll & 0x00FF00FF00FF00FF ) << 8 | ( ll & 0xFF00FF00FF00FF00 ) >> 8;
	//return ll;
	return bswap_64( ll );
}
/**
*	@brief
**/
static inline float FloatSwap( float f ) {
	union {
		float f;
		uint32_t l;
	} dat1, dat2;

	dat1.f = f;
	dat2.l = LongSwap( dat1.l );
	return dat2.f;
}
/**
*	@brief
**/
static inline double DoubleSwap( double d ) {
	union {
		double d;
		uint64_t l;
	} dat1, dat2;
	
	dat1.d = d;
	dat2.l = LongLongSwap( dat1.l );
	return dat2.d;
}
/**
*	@brief
**/
static inline const float LongToFloat( uint32_t l ) {
	union {
		float f;
		uint32_t l;
	} dat;

	dat.l = l;
	return dat.f;
}
/**
*	@brief
**/
static inline const double LongLongToDouble( uint64_t ll ) {
	union {
		double d;
		uint64_t ll;
	} dat;

	dat.ll = ll;
	return dat.d;
}

#if USE_LITTLE_ENDIAN
#define BigShort    ShortSwap
#define BigLong     LongSwap
#define BigFloat    FloatSwap
#define LittleShort(x)    ((uint16_t)(x))
#define LittleLong(x)     ((uint32_t)(x))
#define LittleLongLong(x) ((uint64_t)(x))
#define LittleFloat(x)    ((float)(x))
#define LittleDouble(x)    ((double)(x))
#define MakeRawLong(b1,b2,b3,b4) (((unsigned)(b4)<<24)|((b3)<<16)|((b2)<<8)|(b1))
#define MakeRawShort(b1,b2) (((b2)<<8)|(b1))
#elif USE_BIG_ENDIAN
#define BigShort(x)     ((uint16_t)(x))
#define BigLong(x)      ((uint32_t)(x))
#define BigLongLong(x)      ((uint64_t)(x))
#define BigFloat(x)     ((float)(x))
#define BigDouble(x)     ((double)(x))
#define LittleShort ShortSwap
#define LittleLong  LongSwap
#define LittleFloat FloatSwap
#define LittleDouble DoubleSwap
#define MakeRawLong(b1,b2,b3,b4) (((unsigned)(b1)<<24)|((b2)<<16)|((b3)<<8)|(b4))
#define MakeRawShort(b1,b2) (((b1)<<8)|(b2))
#else
#error Unknown byte order
#endif

#define MakeLittleLong(b1,b2,b3,b4) (((unsigned)(b4)<<24)|((b3)<<16)|((b2)<<8)|(b1))

#define LittleVector(a,b) \
    ((b)[0]=LittleFloat((a)[0]),\
     (b)[1]=LittleFloat((a)[1]),\
     (b)[2]=LittleFloat((a)[2]))

static inline void LittleBlock( void *out, const void *in, size_t size ) {
	memcpy( out, in, size );
	#if USE_BIG_ENDIAN
	for ( int i = 0; i < size / 4; i++ )
		( (uint32_t *)out )[ i ] = LittleLong( ( (uint32_t *)out )[ i ] );
	#endif
}

#if USE_BGRA
#define MakeColor(r, g, b, a)   MakeRawLong(b, g, r, a)
#else
#define MakeColor(r, g, b, a)   MakeRawLong(r, g, b, a)
#endif
