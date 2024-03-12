/**
*
*
*   CollisionModel: BoxLeafs, getting all leafs that reside inside the bounding box:
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



static int32_t leaf_count, leaf_maxcount;
static mleaf_t **leaf_list;
static const vec_t *leaf_mins, *leaf_maxs;
static mnode_t *leaf_topnode;

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
static void CM_BoxLeafs_r( cm_t *cm, mnode_t *node ) {
    int     s;

    while ( node->plane ) {
        s = BoxOnPlaneSideFast( leaf_mins, leaf_maxs, node->plane );
        if ( s == BOX_INFRONT ) {
            node = node->children[ 0 ];
        } else if ( s == BOX_BEHIND ) {
            node = node->children[ 1 ];
        } else {
            // go down both
            if ( !leaf_topnode ) {
                leaf_topnode = node;
            }
            CM_BoxLeafs_r( cm, node->children[ 0 ] );
            node = node->children[ 1 ];
        }
    }

    if ( leaf_count < leaf_maxcount ) {
        leaf_list[ leaf_count++ ] = (mleaf_t *)node;
    }
}

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
const int32_t CM_BoxLeafs_headnode( cm_t *cm, const vec3_t mins, const vec3_t maxs, mleaf_t **list, int listsize, mnode_t *headnode, mnode_t **topnode ) {
    leaf_list = list;
    leaf_count = 0;
    leaf_maxcount = listsize;
    leaf_mins = mins;
    leaf_maxs = maxs;

    // Ensure we start with a null leaf_topnode.
    leaf_topnode = NULL;

    // Recursively 
    CM_BoxLeafs_r( cm, headnode );

    // Copy over the leaf_topnode value into the topnode pointer value if it was set.
    if ( topnode ) {
        *topnode = leaf_topnode;
    }

    return leaf_count;
}

/**
*   @brief  Populates the list of leafs which the specified bounding box touches. If top_node is not 
*           set to NULL, it will contain a value copy of the the top node of the BSP tree that fully 
*           contains the box.
**/
const int32_t CM_BoxLeafs( cm_t *cm, const vec3_t mins, const vec3_t maxs, mleaf_t **list, const int32_t listsize, mnode_t **topnode ) {
    // Map not loaded.
    if ( !cm->cache ) {
        return 0;
    }

    return CM_BoxLeafs_headnode( cm, mins, maxs, list, listsize, cm->cache->nodes, topnode );
}

//int32_t Cm_BoxContents( const box3_t bounds, int32_t head_node ) {
//    cm_box_leafnum_data data = {
//        .bounds = bounds,
//        .list = NULL,
//        .length = 0,
//        .top_node = -1
//    };
//
//    Cm_BoxLeafnums_r( &data, head_node );
//
//    return data.contents;
//}