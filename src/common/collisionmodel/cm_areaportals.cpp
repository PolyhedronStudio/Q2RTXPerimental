/**
*
*
*CollisionModel: AreaPortals
*
*
**/
#include "shared/shared.h"

#include "common/bsp.h"
#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/sizebuf.h"
#include "common/zone.h"
#include "system/hunk.h"

#include "common/collisionmodel.h"

extern cvar_t * map_noareas;

/**
*	@brief	Recursively flood-fills connected areas through open portals.
*	@note	This is used by `CM_FloodAreaConnections()` to assign `cm->floodnums[]`.
*	@param	cm			Collision model instance containing BSP area/portal state.
*	@param	number		Area index to flood from.
*	@param	floodnum	Connectivity group id to assign.
**/
static void FloodArea_r( cm_t *cm, const int32_t number, const int32_t floodnum ) {
	int i;
	mareaportal_t *p;
	marea_t *area;

	// Acquire the area record for this index.
	area = &cm->cache->areas[ number ];

	// If we already visited this area on the current flood pass, we're done.
	if ( area->floodvalid == cm->floodValid ) {
		// If it's already assigned to the same flood group, we can early-out.
		if ( cm->floodnums[ number ] == floodnum ) {
			return;
		}
		// Otherwise this implies area connectivity got re-walked inconsistently this pass.
		Com_Error( ERR_DROP, "FloodArea_r: reflooded" );
	}

	// Assign the flood group id to this area for the current pass.
	cm->floodnums[ number ] = floodnum;
	// Mark the area record as visited for the current pass so we don't revisit it.
	area->floodvalid = cm->floodValid;

	// Walk all portals that belong to this area and recursively traverse open ones.
	p = area->firstareaportal;
	for ( i = 0; i < area->numareaportals; i++, p++ ) {
		// Only traverse portals that are open.
		if ( cm->portalopen[ p->portalnum ] > 0 ) {
			FloodArea_r( cm, p->otherarea, floodnum );
		}
	}
}

/**
*	@brief	Recomputes area connectivity groups (flood numbers) based on portal open/closed state.
*	@note	This must be called after portal states change so `CM_AreasConnected()` is correct.
*	@param	cm	Collision model instance to update.
**/
void CM_FloodAreaConnections( cm_t *cm ) {
	int     i;
	marea_t *area;
	int     floodnum;

	// Nothing to do if no BSP is loaded.
	if ( !cm->cache ) {
		return;
	}

	// All current floods are now invalid.
	// This increments the "pass id" so we don't need to clear `area->floodvalid` for all areas.
	cm->floodValid++;
	floodnum = 0;

	// Area 0 is not used:
	// Flood all remaining areas into connected groups.
	for ( i = 1; i < cm->cache->numareas; i++ ) {
		area = &cm->cache->areas[ i ];

		// Skip if this area was already visited during this pass.
		if ( area->floodvalid == cm->floodValid ) {
			continue;       // already flooded into
		}

		// Start a new connectivity group and flood-fill from this area.
		floodnum++;
		FloodArea_r( cm, i, floodnum );
	}
}

/**
*	@brief	Set the portal number's open/closed state and reflood area connections.
*	@param	cm			Collision model instance containing BSP area/portal state.
*	@param	portalnum	Portal index to update.
*	@param	open		If < 0 the portal is forced closed, otherwise it's considered open.
**/
void CM_SetAreaPortalState( cm_t *cm, const int32_t portalnum, const int32_t open ) {
	// Nothing to do if no BSP is loaded.
	if ( !cm->cache ) {
		return;
	}

	// Validate portal index.
	if ( portalnum < 0 || portalnum >= cm->cache->numportals ) {
		Com_DPrintf( "%s: portalnum %d is out of range\n", __func__, portalnum );
		return;
	}

	// Apply the open/closed state.
	if ( open < 0 ) {
		cm->portalopen[ portalnum ] = 0;
	} else {
		cm->portalopen[ portalnum ] = open;
	}

	// Portal changes affect connectivity, so recompute flood groups.
	CM_FloodAreaConnections( cm );
}

/**
*	@brief	Query whether a portal is open.
*	@param	cm			Collision model instance containing BSP area/portal state.
*	@param	portalnum	Portal index to query.
*	@return	True if open, false if closed or invalid.
**/
const int32_t CM_GetAreaPortalState( cm_t *cm, const int32_t portalnum ) {
	// Nothing to do if no BSP is loaded.
	if ( !cm->cache ) {
		return 0;
	}

	// Validate portal index.
	if ( portalnum < 0 || portalnum >= cm->cache->numportals ) {
		Com_DPrintf( "%s: portalnum %d is out of range\n", __func__, portalnum );
		return 0;
	}

	// The engine convention is "open if > 0".
	return cm->portalopen[ portalnum ] > 0;
}

/**
*	@brief	Checks whether two BSP areas are connected through currently open portals.
*	@param	cm		Collision model instance containing BSP area/portal state.
*	@param	area1	First area index.
*	@param	area2	Second area index.
*	@return	True if connected, false otherwise.
**/
const bool CM_AreasConnected( cm_t *cm, const int32_t area1, const int32_t area2 ) {
	bsp_t *cache = cm->cache;

	// Nothing to do if no BSP is loaded.
	if ( !cache ) {
		return false;
	}

	// If the map disables areas, treat everything as connected.
	if ( map_noareas->integer ) {
		return true;
	}

	// Area 0 is not used.
	if ( area1 < 1 || area2 < 1 ) {
		return false;
	}

	// Validate area indices.
	if ( area1 >= cache->numareas || area2 >= cache->numareas ) {
		Com_EPrintf( "%s: area > numareas\n", __func__ );
		return false;
	}

	// Areas are connected if they share the same flood group id for the current portal state.
	if ( cm->floodnums[ area1 ] == cm->floodnums[ area2 ] ) {
		return true;
	}

	return false;
}

/**
*	@brief	Writes a bitmask of all areas connected to the given area.
*	@details	This is used by the client refresh code to cull visibility based on doors/portals.
*	@param	cm		Collision model instance containing BSP area/portal state.
*	@param	buffer	Output buffer which receives the bitmask.
*	@param	area	Area index used as the starting connectivity group.
*	@return	Number of bytes written to `buffer`.
**/
const int32_t  CM_WriteAreaBits( cm_t *cm, byte *buffer, const int32_t area ) {
	bsp_t *cache = cm->cache;
	int     i;
	int     floodnum;
	int     bytes;

	// Nothing to do if no BSP is loaded.
	if ( !cache ) {
		return 0;
	}

	// Compute required byte count for one bit per area.
	bytes = ( cache->numareas + 7 ) >> 3;
	Q_assert( bytes <= MAX_MAP_AREA_BYTES );

	// If areas are disabled (or no source area is provided), send everything as visible.
	if ( map_noareas->integer || !area ) {
		// For debugging, send everything.
		std::memset( buffer, 255, bytes );
	} else {
		// Clear output mask, then set bits for all areas in the same flood group as `area`.
		std::memset( buffer, 0, bytes );

		floodnum = cm->floodnums[ area ];
		for ( i = 0; i < cache->numareas; i++ ) {
			if ( cm->floodnums[ i ] == floodnum ) {
				Q_SetBit( buffer, i );
			}
		}
	}

	return bytes;
}

/**
*	@brief	Writes a bitmask of which portals are open.
*	@param	cm		Collision model instance containing BSP portal state.
*	@param	buffer	Output buffer which receives the portal bitmask.
*	@return	Number of bytes written to `buffer`.
**/
const int32_t CM_WritePortalBits( cm_t *cm, byte *buffer ) {
	int     i, bytes, numportals;

	// Nothing to do if no BSP is loaded.
	if ( !cm->cache ) {
		return 0;
	}

	// Clamp to the maximum size we can represent in the protocol buffer.
	numportals = std::min( cm->cache->numportals, MAX_MAP_PORTAL_BYTES << 3 );

	// Compute bytes required and clear output.
	bytes = ( numportals + 7 ) >> 3;
	std::memset( buffer, 0, bytes );

	// Set bits for open portals.
	for ( i = 0; i < numportals; i++ ) {
		if ( cm->portalopen[ i ] > 0 ) {
			Q_SetBit( buffer, i );
		}
	}

	return bytes;
}

/**
*	@brief	Applies portal states from a serialized bitmask and refloods area connectivity.
*	@note	If the buffer can't represent all portals, unspecified portals are treated as open.
*	@param	cm		Collision model instance containing BSP portal state.
*	@param	buffer	Input buffer containing portal bits.
*	@param	bytes	Size of `buffer` in bytes.
**/
void CM_SetPortalStates( cm_t *cm, byte *buffer, const int32_t bytes ) {
	int32_t i = 0;

	// Nothing to do if no BSP is loaded.
	if ( !cm->cache ) {
		return;
	}

	// We only have enough bits in the buffer to set part of the portals,
	// the rest are all open.
	const int32_t numportals = std::min( cm->cache->numportals, bytes << 3 );

	// Apply portal states from the bitmask.
	for ( i = 0; i < numportals; i++ ) {
		cm->portalopen[ i ] = Q_IsBitSet( buffer, i );
	}

	// Any portals not covered by the buffer are treated as open.
	for ( ; i < cm->cache->numportals; i++ ) {
		cm->portalopen[ i ] = true;
	}

	// Portal changes affect connectivity, so recompute flood groups.
	CM_FloodAreaConnections( cm );
}

/**
*	@brief	Checks if any leaf under a headnode is potentially visible.
*	@param	node	Root node to test.
*	@param	visbits	Cluster visibility bitmask.
*	@return	True if any leaf's cluster is visible, false otherwise.
**/
const bool CM_HeadnodeVisible( mnode_t *node, byte *visbits ) {
	mleaf_t *leaf;
	int     cluster;

	// Traverse down the node tree; recurse to test both sides.
	while ( node->plane ) {
		// If anything on the front side is visible, early-out.
		if ( CM_HeadnodeVisible( node->children[ 0 ], visbits ) ) {
			return true;
		}
		// Otherwise continue with the back side.
		node = node->children[ 1 ];
	}

	// Leaf nodes have no plane; their `cluster` drives PVS tests.
	leaf = ( mleaf_t * )node;
	cluster = leaf->cluster;

	// Cluster -1 indicates "not in PVS", treat as invisible.
	if ( cluster == -1 ) {
		return false;
	}

	// Visible if the cluster bit is set in the visibility bitmask.
	if ( Q_IsBitSet( visbits, cluster ) ) {
		return true;
	}

	return false;
}

/**
*	@brief	Builds a “fat” PVS mask by unioning visibility for clusters touched by a small AABB.
*	@note	The client interpolates the view position, so we can't use a single PVS point.
*	@param	cm		Collision model instance containing BSP visibility state.
*	@param	mask	Output visibility mask (`VIS_MAX_BYTES` bytes).
*	@param	org		Center point used for the fat PVS box.
*	@param	vis		Visibility mode forwarded to `BSP_ClusterVis`.
*	@return	Pointer to `mask`.
**/
byte *CM_FatPVS( cm_t *cm, byte *mask, const vec3_t org, const int32_t vis ) {
	byte    temp[ VIS_MAX_BYTES ] = {};
	mleaf_t *leafs[ 64 ] = {};
	int32_t	clusters[ 64 ] = {};
	int32_t	i = 0, j = 0, count = 0, longs = 0;
	size_t *src = nullptr, *dst = nullptr;

	// Map not loaded.
	if ( !cm->cache ) {
		return static_cast< byte * >( std::memset( mask, 0, VIS_MAX_BYTES ) ); // WID: C++20: Added cast.
	}

	// Map was loaded but lacks VIS data (treat everything as visible).
	if ( !cm->cache->vis ) {
		return static_cast< byte * >( std::memset( mask, 0xff, VIS_MAX_BYTES ) ); // WID: C++20: Added cast.
	}

	// Expand the point a bit to get a 'fat' PVS.
	// This reduces pop-in when the view origin moves small distances.
	vec3_t  mins = { }, maxs = { };
	for ( i = 0; i < 3; i++ ) {
		mins[ i ] = org[ i ] - 8.f;
		maxs[ i ] = org[ i ] + 8.f;
	}

	// Test the box against the BSP tree to find all the leafs it touches.
	count = CM_BoxLeafs( cm, mins, maxs, leafs, q_countof( leafs ), NULL );
	if ( count < 1 ) {
		Com_Error( ERR_DROP, "CM_FatPVS: leaf count < 1" );
	}

	// Convert leafs to clusters.
	for ( i = 0; i < count; i++ ) {
		clusters[ i ] = leafs[ i ]->cluster;
	}

	// Seed the output mask with the first cluster's visibility.
	BSP_ClusterVis( cm->cache, mask, clusters[ 0 ], vis );

	// Determine how many machine words we can safely OR.
	longs = VIS_FAST_LONGS( cm->cache );

	// OR in all the other unique cluster visibilities.
	for ( i = 1; i < count; i++ ) {
		// Skip duplicates; cluster lists can contain repeats.
		for ( j = 0; j < i; j++ ) {
			if ( clusters[ i ] == clusters[ j ] ) {
				// Already have the cluster we want.
				goto nextleaf;
			}
		}

		// Fetch cluster visibility into `temp`, then OR it into the output `mask`.
		src = ( size_t * )BSP_ClusterVis( cm->cache, temp, clusters[ i ], vis );
		dst = ( size_t * )mask;

		for ( j = 0; j < longs; j++ ) {
			*dst++ |= *src++;
		}
	// Next leaf.
	nextleaf:;
	}

	// Return the fat PVS mask.
	return mask;
}