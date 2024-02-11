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
