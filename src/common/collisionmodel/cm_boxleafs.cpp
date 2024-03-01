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
*   @brief  Fills in a list of all the leafs touched
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
*   @brief
**/
const int32_t CM_BoxLeafs_headnode( cm_t *cm, const vec3_t mins, const vec3_t maxs,
    mleaf_t **list, int listsize,
    mnode_t *headnode, mnode_t **topnode ) {
    leaf_list = list;
    leaf_count = 0;
    leaf_maxcount = listsize;
    leaf_mins = mins;
    leaf_maxs = maxs;

    leaf_topnode = NULL;

    CM_BoxLeafs_r( cm, headnode );

    if ( topnode )
        *topnode = leaf_topnode;

    return leaf_count;
}

/**
*   @brief  Call with topnode set to the headnode, returns with topnode
*           set to the first node that splits the box
**/
const int32_t CM_BoxLeafs( cm_t *cm, const vec3_t mins, const vec3_t maxs,
    mleaf_t **list, const int32_t listsize, mnode_t **topnode ) {
    // Map not loaded.
    if ( !cm->cache ) {
        return 0;
    }

    return CM_BoxLeafs_headnode( cm, mins, maxs, list, listsize, cm->cache->nodes, topnode );
}
