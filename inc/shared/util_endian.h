/********************************************************************
*
*
*   Endianness Utilities:
*
*
********************************************************************/
#pragma once

static inline uint16_t ShortSwap( uint16_t s ) {
	s = ( s >> 8 ) | ( s << 8 );
	return s;
}

static inline uint32_t LongSwap( uint32_t l ) {
	l = ( ( l >> 8 ) & 0x00ff00ff ) | ( ( l << 8 ) & 0xff00ff00 );
	l = ( l >> 16 ) | ( l << 16 );
	return l;
}

static inline float FloatSwap( float f ) {
	union {
		float f;
		uint32_t l;
	} dat1, dat2;

	dat1.f = f;
	dat2.l = LongSwap( dat1.l );
	return dat2.f;
}

static inline float LongToFloat( uint32_t l ) {
	union {
		float f;
		uint32_t l;
	} dat;

	dat.l = l;
	return dat.f;
}

#if USE_LITTLE_ENDIAN
#define BigShort    ShortSwap
#define BigLong     LongSwap
#define BigFloat    FloatSwap
#define LittleShort(x)    ((uint16_t)(x))
#define LittleLong(x)     ((uint32_t)(x))
#define LittleLongLong(x) ((uint64_t)(x))
#define LittleFloat(x)    ((float)(x))
#define MakeRawLong(b1,b2,b3,b4) (((unsigned)(b4)<<24)|((b3)<<16)|((b2)<<8)|(b1))
#define MakeRawShort(b1,b2) (((b2)<<8)|(b1))
#elif USE_BIG_ENDIAN
#define BigShort(x)     ((uint16_t)(x))
#define BigLong(x)      ((uint32_t)(x))
#define BigLongLong(x)      ((uint32_t)(x))
#define BigFloat(x)     ((float)(x))
#define LittleShort ShortSwap
#define LittleLong  LongSwap
#define LittleFloat FloatSwap
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
