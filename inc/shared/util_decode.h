/********************************************************************
*
*
*   Decode Utilities:
*
*
********************************************************************/
#pragma once

/**
*	Byte to Angles/Angles to Bytes.
**/
//! Used for 'wiring' angles, encoded in a 'byte/int8_t'.
static inline const uint8_t ANGLE2BYTE( const float coord ) {
	return ( (int)( ( coord ) * 256.0f / 360 ) & 255 );
}

/**
*	Short to Angles/Angles to Shorts.
**/
//! Used for 'wiring' angles encoded in a 'short/int16_t'.
static inline const int16_t ANGLE2SHORT( const float coord ) {
	return ( (int)( ( coord ) * 65536 / 360 ) & 65535 );
}
/**
*	Short to Origin/Origin to float.
**/
//! Used for 'wiring' origins encoded in a 'short/int16_t'.. with a ratio of 8
static inline const int16_t COORD2SHORT( const float coord ) {
	return ( (int)( ( coord ) * 8.0f ) );
}


/**
*	@brief	Decodes the byte back into a float, limit is: -32.f/+32.f
**/
static inline const float CHAR2OFFSET( const int8_t b ) {
	return (float)( b ) / 255.f;
}
/**
*	@brief	Decodes the byte back into a "blend" float, range(0 = 0, 255 = 1)
**/
static inline const float BYTE2BLEND( const uint8_t b ) {
	return ( 1.0 / 255 ) * b;
	//return (float)( b ) * 0.25f;
	//return (float)( b ) / 255.f;
}