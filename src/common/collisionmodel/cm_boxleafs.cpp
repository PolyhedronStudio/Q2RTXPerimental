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

/**
*   @brief  Struct containing data for box leaf tests.
**/
typedef struct box_leaf_test_s {
    //! [In]: The collision model we're operating on.
    cm_t *cm;

    //! [In]: The bounds for the box that is testing leafs.
    const vec_t *leaf_mins;
    const vec_t *leaf_maxs;

    //! [In]: List holding all leafs to test against.
    mleaf_t **leaf_list;
    //! [In]: Accumulated matching leaf count.
    int32_t leaf_count;
    //! [In]: Maximum amount of leafs to accumulate.
    int32_t leaf_max_count;

    //! [Out]: Top node, if any.
    mnode_t *leaf_topnode;
    //! [Out]: BoxLeafContents only: Holds the specified contents flags that are set in all the various accumulated leafs.
    cm_contents_t leafs_contents;
} box_leaf_test_t;

//static int32_t leaf_count, leaf_maxcount;
//static mleaf_t **leaf_list;
//static const vec_t *leaf_mins, *leaf_maxs;
//static mnode_t *leaf_topnode;

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
//static void CM_BoxLeafs_r( cm_t *cm, mnode_t *node ) {
//    int     s;
//
//    while ( node->plane ) {
//        s = BoxOnPlaneSideFast( leaf_mins, leaf_maxs, node->plane );
//        if ( s == BOX_INFRONT ) {
//            node = node->children[ 0 ];
//        } else if ( s == BOX_BEHIND ) {
//            node = node->children[ 1 ];
//        } else {
//            // go down both
//            if ( !leaf_topnode ) {
//                leaf_topnode = node;
//            }
//            CM_BoxLeafs_r( cm, node->children[ 0 ] );
//            node = node->children[ 1 ];
//        }
//    }
//
//    if ( leaf_count < leaf_maxcount ) {
//        leaf_list[ leaf_count++ ] = (mleaf_t *)node;
//    }
//}

// WORKS:
//static void CM_BoxLeafs_r( box_leaf_test_t &boxLeafTest, mnode_t *node ) {
//    while ( node->plane ) {
//        s = BoxOnPlaneSideFast( boxLeafTest.leaf_mins, boxLeafTest.leaf_maxs, node->plane );
//        if ( s == BOX_INFRONT ) {
//            node = node->children[ 0 ];
//        } else if ( s == BOX_BEHIND ) {
//            node = node->children[ 1 ];
//        } else {
//            // go down both
//            if ( !boxLeafTest.leaf_topnode ) {
//                boxLeafTest.leaf_topnode = node;
//            }
//            CM_BoxLeafs_r( boxLeafTest, node->children[ 0 ] );
//            node = node->children[ 1 ];
//        }
//    }
//
//    if ( boxLeafTest.leaf_count < boxLeafTest.leaf_max_count ) {
//        boxLeafTest.leaf_list[ boxLeafTest.leaf_count++ ] = (mleaf_t *)node;
//    }
//}
static void CM_BoxLeafs_r( box_leaf_test_t &boxLeafTest, mnode_t *node ) {
    // Old:
    //int     s;
    //while ( true /*node->plane */) {
    //    if ( !node->plane ) {
    //        //    // Get leaf.
    //        mleaf_t *leaf = (mleaf_t*)( node );
    //        // Accumulate contents flags.
    //        boxLeafTest.leafs_contents = ( boxLeafTest.leafs_contents | leaf->contents );
    //        // Add to list if room left.
    //        if ( boxLeafTest.leaf_count < boxLeafTest.leaf_max_count ) {
    //            boxLeafTest.leaf_list[ boxLeafTest.leaf_count++ ] = leaf;
    //        }
    //        return;
    //    }
    //    s = BoxOnPlaneSideFast( boxLeafTest.leaf_mins, boxLeafTest.leaf_maxs, node->plane );
    //    if ( s == BOX_INFRONT ) {
    //        node = node->children[ 0 ];
    //    } else if ( s == BOX_BEHIND ) {
    //        node = node->children[ 1 ];
    //    } else {
    //        // Go down both.
    //        if ( !boxLeafTest.leaf_topnode ) {
    //            boxLeafTest.leaf_topnode = node;
    //        }
    //        CM_BoxLeafs_r( boxLeafTest, node->children[ 0 ] );
    //        node = node->children[ 1 ];
    //    }
    //}

    while ( node->plane ) {
        const int32_t boxOnPlaneSide = BoxOnPlaneSideFast( boxLeafTest.leaf_mins, boxLeafTest.leaf_maxs, node->plane );
        if ( boxOnPlaneSide == BOX_INFRONT ) {
            node = node->children[ 0 ];
        } else if ( boxOnPlaneSide == BOX_BEHIND ) {
            node = node->children[ 1 ];
        } else {
            // go down both
            if ( !boxLeafTest.leaf_topnode ) {
                boxLeafTest.leaf_topnode = node;
            }
            CM_BoxLeafs_r( boxLeafTest, node->children[ 0 ] );
            node = node->children[ 1 ];
        }
    }
    
    // >Q2RTXP>: Do we need this here?
    #if 0
    // Accumulate contents flags.
    mleaf_t *leaf = (mleaf_t *)( node );
    boxLeafTest.leafs_contents = cm_contents_t( boxLeafTest.leafs_contents | leaf->contents );
    #endif

    if ( boxLeafTest.leaf_count < boxLeafTest.leaf_max_count ) {
        boxLeafTest.leaf_list[ boxLeafTest.leaf_count++ ] = (mleaf_t *)node;
    }
}

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
const int32_t CM_BoxLeafs_headnode( cm_t *cm, const vec3_t mins, const vec3_t maxs, mleaf_t **list, int listsize, mnode_t *headnode, mnode_t **topnode ) {
    box_leaf_test_t boxLeafTest = {
        // [In]:
        .cm = cm,

        .leaf_mins = mins,
        .leaf_maxs = maxs,

        .leaf_list = list,
        .leaf_count = 0,
        .leaf_max_count = listsize,

        // [Out]:
        .leaf_topnode = nullptr,
        .leafs_contents = CONTENTS_NONE
    };

    // Recursively 
    CM_BoxLeafs_r( boxLeafTest, headnode );

    // Copy over the leaf_topnode value into the topnode pointer value if it was set.
    if ( topnode ) {
        *topnode = boxLeafTest.leaf_topnode;
    }

    return boxLeafTest.leaf_count;
}

/**
*   @brief  Populates the list of leafs which the specified absolute bounds box touches. If top_node is not 
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

/**
*   @return The contents mask of all leafs within the absolute bounds.
**/
const cm_contents_t CM_BoxContents( cm_t *cm, const vec3_t mins, const vec3_t maxs, mnode_t *headnode ) {
    box_leaf_test_t boxLeafTest = {
        // [In]:
        .cm = cm,

        .leaf_mins = mins,
        .leaf_maxs = maxs,

        .leaf_list = nullptr,
        .leaf_count = 0,
        .leaf_max_count = 0,

        // [Out]:
        .leaf_topnode = nullptr,
        .leafs_contents = CONTENTS_NONE
    };

    // Recursively 
    CM_BoxLeafs_r( boxLeafTest, headnode );

    // Return contents found in all leafs touching the bounds.
    return boxLeafTest.leafs_contents;
}