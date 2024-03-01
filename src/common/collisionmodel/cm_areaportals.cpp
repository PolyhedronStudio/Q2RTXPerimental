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
        if ( cm->floodnums[ number ] == floodnum )
            return;
        Com_Error( ERR_DROP, "FloodArea_r: reflooded" );
    }

    cm->floodnums[ number ] = floodnum;
    area->floodvalid = cm->floodValid;
    p = area->firstareaportal;
    for ( i = 0; i < area->numareaportals; i++, p++ ) {
        if ( cm->portalopen[ p->portalnum ] )
            FloodArea_r( cm, p->otherarea, floodnum );
    }
}
/**
*   @brief
**/
void FloodAreaConnections( cm_t *cm ) {
    int     i;
    marea_t *area;
    int     floodnum;

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
*   @brief
**/
void CM_SetAreaPortalState( cm_t *cm, const int32_t portalnum, const bool open ) {
    if ( !cm->cache ) {
        return;
    }

    if ( portalnum < 0 || portalnum >= cm->cache->numportals ) {
        Com_DPrintf( "%s: portalnum %d is out of range\n", __func__, portalnum );
        return;
    }

    cm->portalopen[ portalnum ] = open;
    FloodAreaConnections( cm );
}
/**
*   @brief
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
const int32_t  CM_WriteAreaBits( cm_t *cm, byte *buffer, const int32_t  area ) {
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
        memset( buffer, 255, bytes );
    } else {
        memset( buffer, 0, bytes );

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

    numportals = min( cm->cache->numportals, MAX_MAP_PORTAL_BYTES << 3 );

    bytes = ( numportals + 7 ) >> 3;
    memset( buffer, 0, bytes );
    for ( i = 0; i < numportals; i++ ) {
        if ( cm->portalopen[ i ] ) {
            Q_SetBit( buffer, i );
        }
    }

    return bytes;
}
/**
*   @brief
**/
void CM_SetPortalStates( cm_t *cm, byte *buffer, const int32_t bytes ) {
    int     i, numportals;

    if ( !cm->cache ) {
        return;
    }

    numportals = min( cm->cache->numportals, bytes << 3 );
    for ( i = 0; i < numportals; i++ ) {
        cm->portalopen[ i ] = Q_IsBitSet( buffer, i );
    }
    for ( ; i < cm->cache->numportals; i++ ) {
        cm->portalopen[ i ] = true;
    }

    FloodAreaConnections( cm );
}
/**
*   @Return True if any leaf under headnode has a cluster that
*           is potentially visible
**/
const bool CM_HeadnodeVisible( mnode_t *node, byte *visbits ) {
    mleaf_t *leaf;
    int     cluster;

    while ( node->plane ) {
        if ( CM_HeadnodeVisible( node->children[ 0 ], visbits ) )
            return true;
        node = node->children[ 1 ];
    }

    leaf = (mleaf_t *)node;
    cluster = leaf->cluster;
    if ( cluster == -1 )
        return false;
    if ( Q_IsBitSet( visbits, cluster ) )
        return true;
    return false;
}
/**
*   @brief  The client will interpolate the view position,
*           so we can't use a single PVS point
**/
byte *CM_FatPVS( cm_t *cm, byte *mask, const vec3_t org, const int32_t vis ) {
    byte    temp[ VIS_MAX_BYTES ];
    mleaf_t *leafs[ 64 ];
    int     clusters[ 64 ];
    int     i, j, count, longs;
    size_t *src, *dst;
    vec3_t  mins, maxs;

    if ( !cm->cache ) {   // map not loaded
        return static_cast<byte *>( memset( mask, 0, VIS_MAX_BYTES ) ); // WID: C++20: Added cast.
    }
    if ( !cm->cache->vis ) {
        return static_cast<byte *>( memset( mask, 0xff, VIS_MAX_BYTES ) ); // WID: C++20: Added cast.
    }

    for ( i = 0; i < 3; i++ ) {
        mins[ i ] = org[ i ] - 8;
        maxs[ i ] = org[ i ] + 8;
    }

    count = CM_BoxLeafs( cm, mins, maxs, leafs, q_countof( leafs ), NULL );
    if ( count < 1 )
        Com_Error( ERR_DROP, "CM_FatPVS: leaf count < 1" );

    // convert leafs to clusters
    for ( i = 0; i < count; i++ ) {
        clusters[ i ] = leafs[ i ]->cluster;
    }

    BSP_ClusterVis( cm->cache, mask, clusters[ 0 ], vis );
    longs = VIS_FAST_LONGS( cm->cache );

    // or in all the other leaf bits
    for ( i = 1; i < count; i++ ) {
        for ( j = 0; j < i; j++ ) {
            if ( clusters[ i ] == clusters[ j ] ) {
                goto nextleaf; // already have the cluster we want
            }
        }
        src = (size_t *)BSP_ClusterVis( cm->cache, temp, clusters[ i ], vis );
        dst = (size_t *)mask;
        for ( j = 0; j < longs; j++ ) {
            *dst++ |= *src++;
        }

    nextleaf:;
    }

    return mask;
}
