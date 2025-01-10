#pragma once

// Extern C
QEXTERN_C_OPEN

typedef float vec_t;
typedef vec_t vec2_t[ 2 ];
typedef vec_t vec3_t[ 3 ];
typedef vec_t vec4_t[ 4 ];
typedef vec_t vec5_t[ 5 ];

typedef vec_t quat_t[ 4 ];

typedef float mat4_t[ 16 ];

typedef union {
	uint32_t u32;
	uint8_t u8[ 4 ];
} color_t;

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;

#ifndef M_PI
#define M_PI        3.14159265358979323846  // matches value in gcc v2 math.h
#endif

struct cplane_s;

extern const vec3_t vec3_origin;

typedef struct vrect_s {
	int             x, y, width, height;
} vrect_t;

#define DEG2RAD(a)      ((a) * (M_PI / 180))
#define RAD2DEG(a)      ((a) * (180 / M_PI))

#define ALIGN(x, a)     (((x) + (a) - 1) & ~((a) - 1))

#define SWAP(type, a, b) \
    do { type SWAP_tmp = a; a = b; b = SWAP_tmp; } while (0)

#define DotProduct(x,y)         ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define CrossProduct(v1,v2,cross) \
        ((cross)[0]=(v1)[1]*(v2)[2]-(v1)[2]*(v2)[1], \
         (cross)[1]=(v1)[2]*(v2)[0]-(v1)[0]*(v2)[2], \
         (cross)[2]=(v1)[0]*(v2)[1]-(v1)[1]*(v2)[0])
#define VectorSubtract(a,b,c) \
        ((c)[0]=(a)[0]-(b)[0], \
         (c)[1]=(a)[1]-(b)[1], \
         (c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c) \
        ((c)[0]=(a)[0]+(b)[0], \
         (c)[1]=(a)[1]+(b)[1], \
         (c)[2]=(a)[2]+(b)[2])
#define VectorAdd3(a,b,c,d) \
        ((d)[0]=(a)[0]+(b)[0]+(c)[0], \
         (d)[1]=(a)[1]+(b)[1]+(c)[1], \
         (d)[2]=(a)[2]+(b)[2]+(c)[2])
#define VectorCopy(a,b)     ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorClear(a)      ((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)   ((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorInverse(a)    ((a)[0]=-(a)[0],(a)[1]=-(a)[1],(a)[2]=-(a)[2])
#define VectorSet(v, x, y, z)   ((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define VectorAvg(a,b,c) \
        ((c)[0]=((a)[0]+(b)[0])*0.5f, \
         (c)[1]=((a)[1]+(b)[1])*0.5f, \
         (c)[2]=((a)[2]+(b)[2])*0.5f)
#define VectorMA(a,b,c,d) \
        ((d)[0]=(a)[0]+(b)*(c)[0], \
         (d)[1]=(a)[1]+(b)*(c)[1], \
         (d)[2]=(a)[2]+(b)*(c)[2])
#define VectorVectorMA(a,b,c,d) \
        ((d)[0]=(a)[0]+(b)[0]*(c)[0], \
         (d)[1]=(a)[1]+(b)[1]*(c)[1], \
         (d)[2]=(a)[2]+(b)[2]*(c)[2])
#define VectorEmpty(v) ((v)[0]==0&&(v)[1]==0&&(v)[2]==0)
#define VectorCompare(v1,v2)    ((v1)[0]==(v2)[0]&&(v1)[1]==(v2)[1]&&(v1)[2]==(v2)[2])
#define VectorLength(v)     (sqrtf(DotProduct((v),(v))))
#define VectorLengthSquared(v)      (DotProduct((v),(v)))
#define VectorScale(in,scale,out) \
        ((out)[0]=(in)[0]*(scale), \
         (out)[1]=(in)[1]*(scale), \
         (out)[2]=(in)[2]*(scale))
#define VectorVectorScale(in,scale,out) \
        ((out)[0]=(in)[0]*(scale)[0], \
         (out)[1]=(in)[1]*(scale)[1], \
         (out)[2]=(in)[2]*(scale)[2])
#define DistanceSquared(v1,v2) \
        (((v1)[0]-(v2)[0])*((v1)[0]-(v2)[0])+ \
        ((v1)[1]-(v2)[1])*((v1)[1]-(v2)[1])+ \
        ((v1)[2]-(v2)[2])*((v1)[2]-(v2)[2]))
#define Distance(v1,v2) (sqrtf(DistanceSquared(v1,v2)))
#define LerpAngles(a,b,c,d) \
        ((d)[0]=LerpAngle((a)[0],(b)[0],c), \
         (d)[1]=LerpAngle((a)[1],(b)[1],c), \
         (d)[2]=LerpAngle((a)[2],(b)[2],c))
#define LerpVector(a,b,c,d) \
    ((d)[0]=(a)[0]+(c)*((b)[0]-(a)[0]), \
     (d)[1]=(a)[1]+(c)*((b)[1]-(a)[1]), \
     (d)[2]=(a)[2]+(c)*((b)[2]-(a)[2]))
#define LerpVector2(a,b,c,d,e) \
    ((e)[0]=(a)[0]*(c)+(b)[0]*(d), \
     (e)[1]=(a)[1]*(c)+(b)[1]*(d), \
     (e)[2]=(a)[2]*(c)+(b)[2]*(d))
#define PlaneDiff(v,p)   (DotProduct(v,(p)->normal)-(p)->dist)

#define Vector2Subtract(a,b,c)  ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1])
#define Vector2Add(a,b,c)       ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1])

#define Vector4Subtract(a,b,c)  ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#define Vector4Add(a,b,c)       ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2],(c)[3]=(a)[3]+(b)[3])
#define Vector4Copy(a,b)        ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define Vector4Clear(a)         ((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#define Vector4Negate(a,b)      ((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2],(b)[3]=-(a)[3])
#define Vector4Set(v, a, b, c, d)   ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3]=(d))
#define Vector4Compare(v1,v2)    ((v1)[0]==(v2)[0]&&(v1)[1]==(v2)[1]&&(v1)[2]==(v2)[2]&&(v1)[3]==(v2)[3])
#define Vector4MA(a,b,c,d) \
        ((d)[0]=(a)[0]+(b)*(c)[0], \
         (d)[1]=(a)[1]+(b)*(c)[1], \
         (d)[2]=(a)[2]+(b)*(c)[2], \
         (d)[3]=(a)[3]+(b)*(c)[3])
#define Dot4Product(x, y)           ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]+(x)[3]*(y)[3])
#define Vector4Lerp(a,b,c,d) \
    ((d)[0]=(a)[0]+(c)*((b)[0]-(a)[0]), \
     (d)[1]=(a)[1]+(c)*((b)[1]-(a)[1]), \
     (d)[2]=(a)[2]+(c)*((b)[2]-(a)[2]), \
     (d)[3]=(a)[3]+(c)*((b)[3]-(a)[3]))

#define QuatCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
vec_t VectorNormalize( vec3_t v );        // returns vector length
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void ClearBounds( vec3_t mins, vec3_t maxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
vec_t RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void UnionBounds( const vec3_t a[ 2 ], const vec3_t b[ 2 ], vec3_t c[ 2 ] );

static inline void AnglesToAxis( const vec3_t angles, vec3_t axis[ 3 ] ) {
	AngleVectors( angles, axis[ 0 ], axis[ 1 ], axis[ 2 ] );
	VectorInverse( axis[ 1 ] );
}

static inline void TransposeAxis( vec3_t axis[ 3 ] ) {
	SWAP( vec_t, axis[ 0 ][ 1 ], axis[ 1 ][ 0 ] );
	SWAP( vec_t, axis[ 0 ][ 2 ], axis[ 2 ][ 0 ] );
	SWAP( vec_t, axis[ 1 ][ 2 ], axis[ 2 ][ 1 ] );
}

static inline void RotatePoint( vec3_t point, const vec3_t axis[ 3 ] ) {
	vec3_t temp;

	VectorCopy( point, temp );
	point[ 0 ] = DotProduct( temp, axis[ 0 ] );
	point[ 1 ] = DotProduct( temp, axis[ 1 ] );
	point[ 2 ] = DotProduct( temp, axis[ 2 ] );
}

static inline unsigned npot32( unsigned k ) {
	if ( k == 0 )
		return 1;

	k--;
	k = k | ( k >> 1 );
	k = k | ( k >> 2 );
	k = k | ( k >> 4 );
	k = k | ( k >> 8 );
	k = k | ( k >> 16 );

	return k + 1;
}

static inline float LerpAngle( float a2, float a1, float frac ) {
	if ( a1 - a2 > 180 )
		a1 -= 360;
	if ( a1 - a2 < -180 )
		a1 += 360;
	return a2 + frac * ( a1 - a2 );
}

static inline const float AngleMod( float a ) {
// Float based method:
	#if 1
    // Float based method:
    const float v = fmod( a, 360.0f );

    if ( v < 0 ) {
        return 360.f + v;
    }

    return v;
	#endif
	// Failed attempt:
	#if 0
	return ( 360.0 / UINT64_MAX ) * ( (int64_t)( a * ( UINT64_MAX / 360.0 ) ) & UINT64_MAX );
	#endif
	// Old 'vanilla' method:
	#if 0
	return ( 360.0 / 65536 ) * ( (int32_t)( a * ( 65536 / 360.0 ) ) & 65535 );
	#endif
}

static inline const int64_t Q_align( const int64_t value, const int64_t align ) {
    int64_t mod = value % align;
	return mod ? value + align - mod : value;
}

void Q_srand( uint32_t seed );
uint32_t Q_rand( void );
uint32_t Q_rand_uniform( uint32_t n );

#define constclamp(a,b,c)   ((a)<(b)?(b):(a)>(c)?(c):(a))
#define clamp(a,b,c)    ((a)<(b)?(a)=(b):(a)>(c)?(a)=(c):(a))
#define cclamp(a,b,c)   ((b)>(c)?clamp(a,c,b):clamp(a,b,c))

// WID: C++20:
//#ifdef __cplusplus
//#define max std::max
//#define min std::min
//#else 
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
//#endif

#define frand()     ((int32_t)Q_rand() * 0x1p-32f + 0.5f)
#define crand()     ((int32_t)Q_rand() * 0x1p-31f)

#define Q_rint(x)   ((x) < 0 ? ((int)((x) - 0.5f)) : ((int)((x) + 0.5f)))

#define NUMVERTEXNORMALS    162

void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);

extern const vec3_t bytedirs[NUMVERTEXNORMALS];

const int32_t DirToByte( const vec3_t dir );
void ByteToDir(const int32_t index, vec3_t dir );

// Extern C
QEXTERN_C_CLOSE