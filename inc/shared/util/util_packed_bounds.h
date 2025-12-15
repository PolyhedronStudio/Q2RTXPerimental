/********************************************************************
*
*
*   Bounds Encoding/Packing:
*
*
********************************************************************/
#pragma once



/**
*	@brief	union struct, for packing the bounds as uint32_t.
**/
typedef union bounds_packed_u {
	//! Memory packed representation of the bounds.
	struct p {
		uint8_t x;
		uint8_t y;
		uint8_t zd; //! "z=down" always negative
		uint8_t zu; //! "z-up", encoded as + 32
	} p;

	//! Actual uint32_t value used for bounds.
	uint32_t u;
} bounds_packed_t;

// We don't need these in VKPT(C code).
#ifdef __cplusplus
/**
*	@brief	Will encode/pack the mins/maxs bounds into the solid_packet_t uint32_t.
**/
static inline const bounds_packed_t MSG_PackBoundsUint32( const Vector3 &mins, const Vector3 &maxs ) {
	bounds_packed_t packedBounds;

	packedBounds.p.x = maxs[ 0 ];
	packedBounds.p.y = maxs[ 1 ];
	packedBounds.p.zd = -mins[ 2 ];
	packedBounds.p.zu = maxs[ 2 ] + 32;

	return packedBounds;
}
static inline const bounds_packed_t MSG_PackBoundsUint32( const BBox3 &bbox ) {
	return MSG_PackBoundsUint32( bbox.mins, bbox.maxs );
}
/**
*	@brief	Will decode/unpack the solid_packet_t uint32_t, into the pointers mins/maxs.
**/
static inline void MSG_UnpackBoundsUint32( const bounds_packed_t packedBounds, Vector3 &mins, Vector3 &maxs ) {
	mins[ 0 ] = -packedBounds.p.x;  maxs[ 0 ] = packedBounds.p.x;
	mins[ 1 ] = -packedBounds.p.y;  maxs[ 1 ] = packedBounds.p.y;
	mins[ 2 ] = -packedBounds.p.zd; maxs[ 2 ] = packedBounds.p.zu - 32;
}
static inline void MSG_UnpackBoundsUint32( const bounds_packed_t packedBounds, BBox3 &bbox ) {
	MSG_UnpackBoundsUint32( packedBounds, bbox.mins, bbox.maxs );
}
#endif // __cplusplus