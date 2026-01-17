/**
*
*
*   CollisionModel: AreaPortals
*
*
**/
#include "shared/shared.h"
#include "common/bsp.h"
#include "common/cmd.h"
#include "common/collisionmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/sizebuf.h"
#include "common/zone.h"
#include "system/hunk.h"


extern cvar_t *map_noareas;

/**
*   @brief
**/
static void FloodArea_r( cm_t *cm, const int32_t number, const int32_t floodnum ) {
    int i;
    mareaportal_t *p;
    marea_t *area;

    area = &cm->cache->areas[ number ];
    if ( area->floodvalid == cm->floodValid ) {
		if ( cm->floodnums[ number ] == floodnum ) {
			return;
		}
        Com_Error( ERR_DROP, "FloodArea_r: reflooded" );
    }

    cm->floodnums[ number ] = floodnum;
    area->floodvalid = cm->floodValid;
    p = area->firstareaportal;
    for ( i = 0; i < area->numareaportals; i++, p++ ) {
        if ( cm->portalopen[ p->portalnum ] > 0 ) {
            FloodArea_r( cm, p->otherarea, floodnum );
		}
    }
}
/**
*   @brief
**/
void CM_FloodAreaConnections( cm_t *cm ) {
    int     i;
    marea_t *area;
    int     floodnum;

    if ( !cm->cache ) {
        return;
    }

    // All current floods are now invalid.
    cm->floodValid++;
    floodnum = 0;

    // Area 0 is not used:
    for ( i = 1; i < cm->cache->numareas; i++ ) {
        area = &cm->cache->areas[ i ];
        if ( area->floodvalid == cm->floodValid ) {
            continue;       // already flooded into
        }
        floodnum++;
        FloodArea_r( cm, i, floodnum );
    }
}
/**
*   @brief  Set the portal nums matching portal to open/closed state.
**/
void CM_SetAreaPortalState( cm_t *cm, const int32_t portalnum, const int32_t open ) {
    if ( !cm->cache ) {
        return;
    }

    if ( portalnum < 0 || portalnum >= cm->cache->numportals ) {
        Com_DPrintf( "%s: portalnum %d is out of range\n", __func__, portalnum );
        return;
    }

	if ( open < 0 ) {
		cm->portalopen[ portalnum ] = 0;
	} else {
		cm->portalopen[ portalnum ] = open;
	}
	CM_FloodAreaConnections( cm );
}
/**
*   @return False(0) if the portal nums matching portal is closed, true(1) otherwise.
**/
const int32_t CM_GetAreaPortalState( cm_t *cm, const int32_t portalnum ) {
    if ( !cm->cache ) {
        return 0;
    }

    if ( portalnum < 0 || portalnum >= cm->cache->numportals ) {
        Com_DPrintf( "%s: portalnum %d is out of range\n", __func__, portalnum );
        return 0;
    }

    return cm->portalopen[ portalnum ] > 0;
}
/**
*   @return True if the two areas are connected, false if not(or possibly blocked by a door for example.)
**/
const bool CM_AreasConnected( cm_t *cm, const int32_t area1, const int32_t area2 ) {
    bsp_t *cache = cm->cache;

    if ( !cache ) {
        return false;
    }
    if ( map_noareas->integer ) {
        return true;
    }
    if ( area1 < 1 || area2 < 1 ) {
        return false;
    }
    if ( area1 >= cache->numareas || area2 >= cache->numareas ) {
        Com_EPrintf( "%s: area > numareas\n", __func__ );
        return false;
    }
    if ( cm->floodnums[ area1 ] == cm->floodnums[ area2 ] ) {
        return true;
    }

    return false;
}
/**
*   @brief  Writes a length byte followed by a bit vector of all the areas
*           that area in the same flood as the area parameter
*
*           This is used by the client refreshes to cull visibility
**/
const int32_t  CM_WriteAreaBits( cm_t *cm, byte *buffer, const int32_t area ) {
    bsp_t *cache = cm->cache;
    int     i;
    int     floodnum;
    int     bytes;

    if ( !cache ) {
        return 0;
    }

    bytes = ( cache->numareas + 7 ) >> 3;
    Q_assert( bytes <= MAX_MAP_AREA_BYTES );

    if ( map_noareas->integer || !area ) {
        // for debugging, send everything
        std::memset( buffer, 255, bytes );
    } else {
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
*   @brief
**/
const int32_t CM_WritePortalBits( cm_t *cm, byte *buffer ) {
    int     i, bytes, numportals;

    if ( !cm->cache ) {
        return 0;
    }

    numportals = std::min( cm->cache->numportals, MAX_MAP_PORTAL_BYTES << 3 );

    bytes = ( numportals + 7 ) >> 3;
	std::memset( buffer, 0, bytes );
    for ( i = 0; i < numportals; i++ ) {
        if ( cm->portalopen[ i ] > 0 ) {
            Q_SetBit( buffer, i );
        }
    }

    return bytes;
}
/**
*   @brief
**/
void CM_SetPortalStates( cm_t *cm, byte *buffer, const int32_t bytes ) {
    int32_t i = 0;

    if ( !cm->cache ) {
        return;
    }
	// We only have enough bits in the buffer to set part of the portals,
	// the rest are all open.
    const int32_t numportals = std::min( cm->cache->numportals, bytes << 3 );
	// Set the specified portals
    for ( i = 0; i < numportals; i++ ) {
        cm->portalopen[ i ] = Q_IsBitSet( buffer, i );
    }
	// Set the rest to open
    for ( ; i < cm->cache->numportals; i++ ) {
        cm->portalopen[ i ] = true;
    }
	// Re-flood the area connections.
    CM_FloodAreaConnections( cm );
}
/**
*   @Return True if any leaf under headnode has a cluster that
*           is potentially visible
**/
const bool CM_HeadnodeVisible( mnode_t *node, byte *visbits ) {
    mleaf_t *leaf;
    int     cluster;

    while ( node->plane ) {
		if ( CM_HeadnodeVisible( node->children[ 0 ], visbits ) ) {
			return true;
		}
        node = node->children[ 1 ];
    }

    leaf = (mleaf_t *)node;
    cluster = leaf->cluster;
	if ( cluster == -1 ) {
		return false;
	}
	if ( Q_IsBitSet( visbits, cluster ) ) {
		return true;
	}
    return false;
}
/**
*   @brief  The client will interpolate the view position,
*           so we can't use a single PVS point
**/
byte *CM_FatPVS( cm_t *cm, byte *mask, const vec3_t org, const int32_t vis ) {
	byte    temp[ VIS_MAX_BYTES ] = {};
	mleaf_t *leafs[ 64 ] = {};
	int32_t	clusters[ 64 ] = {};
    int32_t	i = 0, j = 0, count = 0, longs = 0;
    size_t	*src = nullptr, *dst = nullptr;

	// Map not loaded.
    if ( !cm->cache ) {
        return static_cast<byte *>( std::memset( mask, 0, VIS_MAX_BYTES ) ); // WID: C++20: Added cast.
    }
	// Map was loaded but lacks VIS data.
    if ( !cm->cache->vis ) {
        return static_cast<byte *>( std::memset( mask, 0xff, VIS_MAX_BYTES ) ); // WID: C++20: Added cast.
    }

	// Expand the point a bit to get a 'fat' PVS.
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
	// Get the visibility cluster for the first leaf.
    BSP_ClusterVis( cm->cache, mask, clusters[ 0 ], vis );
	// Get the number of longs in the visibility bitmask.
    longs = VIS_FAST_LONGS( cm->cache );
    // or in all the other leaf bits
    for ( i = 1; i < count; i++ ) {
        for ( j = 0; j < i; j++ ) {
            if ( clusters[ i ] == clusters[ j ] ) {
				// Already have the cluster we want.
                goto nextleaf;
            }
        }
		// Accumulate the cluster visibility bitmask.
        src = (size_t *)BSP_ClusterVis( cm->cache, temp, clusters[ i ], vis );
		// Write the accumulated bits back to the output mask.
        dst = (size_t *)mask;
		// OR the bits together.
        for ( j = 0; j < longs; j++ ) {
            *dst++ |= *src++;
        }
	// Next leaf.
    nextleaf:;
    }
	// Return the fat PVS mask.
    return mask;
}
