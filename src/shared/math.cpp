// WID: It makes sense to have the 'implementation' reside here.
//#define RAYMATH_IMPLEMENTATION
#include "shared/shared.h"

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up ) {
	float        angle;
	float        sr, sp, sy, cr, cp, cy;

	angle = DEG2RAD( angles[ YAW ] );
	sy = sin( angle );
	cy = cos( angle );
	angle = DEG2RAD( angles[ PITCH ] );
	sp = sin( angle );
	cp = cos( angle );
	angle = DEG2RAD( angles[ ROLL ] );
	sr = sin( angle );
	cr = cos( angle );

	if ( forward ) {
		forward[ 0 ] = cp * cy;
		forward[ 1 ] = cp * sy;
		forward[ 2 ] = -sp;
	}
	if ( right ) {
		right[ 0 ] = ( -1 * sr * sp * cy + -1 * cr * -sy );
		right[ 1 ] = ( -1 * sr * sp * sy + -1 * cr * cy );
		right[ 2 ] = -1 * sr * cp;
	}
	if ( up ) {
		up[ 0 ] = ( cr * sp * cy + -sr * -sy );
		up[ 1 ] = ( cr * sp * sy + -sr * cy );
		up[ 2 ] = cr * cp;
	}
}

vec_t VectorNormalize( vec3_t v ) {
	float    length, ilength;

	length = VectorLength( v );

	if ( length ) {
		ilength = 1 / length;
		v[ 0 ] *= ilength;
		v[ 1 ] *= ilength;
		v[ 2 ] *= ilength;
	}

	return length;

}

vec_t VectorNormalize2( const vec3_t v, vec3_t out ) {
	float    length, ilength;

	length = VectorLength( v );

	if ( length ) {
		ilength = 1 / length;
		out[ 0 ] = v[ 0 ] * ilength;
		out[ 1 ] = v[ 1 ] * ilength;
		out[ 2 ] = v[ 2 ] * ilength;
	}

	return length;

}

void ClearBounds( vec3_t mins, vec3_t maxs ) {
	mins[ 0 ] = mins[ 1 ] = mins[ 2 ] = 99999;
	maxs[ 0 ] = maxs[ 1 ] = maxs[ 2 ] = -99999;
}

void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	int        i;
	vec_t    val;

	for ( i = 0; i < 3; i++ ) {
		val = v[ i ];
		mins[ i ] = min( mins[ i ], val );
		maxs[ i ] = max( maxs[ i ], val );
	}
}

void UnionBounds( const vec3_t a[ 2 ], const vec3_t b[ 2 ], vec3_t c[ 2 ] ) {
	int        i;

	for ( i = 0; i < 3; i++ ) {
		c[ 0 ][ i ] = min( a[ 0 ][ i ], b[ 0 ][ i ] );
		c[ 1 ][ i ] = max( a[ 1 ][ i ], b[ 1 ][ i ] );
	}
}

/*
=================
RadiusFromBounds
=================
*/
vec_t RadiusFromBounds( const vec3_t mins, const vec3_t maxs ) {
	int     i;
	vec3_t  corner;
	vec_t   a, b;

	for ( i = 0; i < 3; i++ ) {
		a = fabsf( mins[ i ] );
		b = fabsf( maxs[ i ] );
		corner[ i ] = max( a, b );
	}

	return VectorLength( corner );
}

/*
=====================================================================

  MT19337 PRNG

=====================================================================
*/

#define N 624
#define M 397

static uint32_t mt_state[ N ];
static uint32_t mt_index;

/*
==================
Q_srand

Seed PRNG with initial value
==================
*/
void Q_srand( uint32_t seed ) {
	mt_index = N;
	mt_state[ 0 ] = seed;
	for ( int i = 1; i < N; i++ )
		mt_state[ i ] = seed = 1812433253 * ( seed ^ seed >> 30 ) + i;
}

/*
==================
Q_rand

Generate random integer in range [0, 2^32)
==================
*/
uint32_t Q_rand( void ) {
	uint32_t x, y;
	int i;

	if ( mt_index >= N ) {
		mt_index = 0;

		#define STEP(j, k) do {                 \
        x  = mt_state[i] & 0x80000000;  \
        x |= mt_state[j] & 0x7FFFFFFF;  \
        y  = x >> 1;                    \
        y ^= 0x9908B0DF & (uint32_t)(-(int)(x & 1)); \
        mt_state[i] = mt_state[k] ^ y;  \
    } while (0)

		for ( i = 0; i < N - M; i++ )
			STEP( i + 1, i + M );
		for ( ; i < N - 1; i++ )
			STEP( i + 1, i - N + M );
		STEP( 0, M - 1 );
	}

	y = mt_state[ mt_index++ ];
	y ^= y >> 11;
	y ^= y << 7 & 0x9D2C5680;
	y ^= y << 15 & 0xEFC60000;
	y ^= y >> 18;

	return y;
}

/*
==================
Q_rand_uniform

Generate random integer in range [0, n) avoiding modulo bias
==================
*/
uint32_t Q_rand_uniform( uint32_t n ) {
	uint32_t r, m;

	if ( n < 2 )
		return 0;

	m = (uint32_t)( -(int)n ) % n; // m = 2^32 mod n
	do {
		r = Q_rand( );
	} while ( r < m );

	return r % n;
}