/********************************************************************
*
*
*   Encode Utilities:
*
*
********************************************************************/
#pragma once

/**
*	Byte to Angles/Angles to Bytes.
**/
//! Used for decoding the 'wired' angle in a 'float'.
static inline const float BYTE2ANGLE( const int s ) {
	return ( ( s ) * ( 360.0f / 256 ) );
}
/**
*	Short to Angles/Angles to Shorts.
**/
//! Used for decoding the 'wired' angle in a 'float'.
static inline const float SHORT2ANGLE( const int s ) {
	return ( ( s ) * ( 360.0f / 65536 ) );
}
/**
*	Short to Origin/Origin to float.
**/
//! Used for decoding the 'wired' origin in a 'float'. with a ratio of 8
static inline const float SHORT2COORD( const int s ) {
	return ( ( s ) * ( 1.0f / 8 ) );
}

/**
*   @brief	Encodes the float to an int8_t, having a limit of: -32/+32
**/
static inline const int8_t OFFSET2CHAR( float f ) {
	clamp( f, -32, 127.0f / 4 );
	return (int8_t)( f * 4.f );
}
/**
*   @brief	Encodes the "blend" float to an uint8_t, range(0 = 0, 1 = 255).
**/
static inline const uint8_t BLEND2BYTE( float f ) {
	clamp( f, 0.f, 1.f );
	return (uint8_t)( f * 255.f );
}