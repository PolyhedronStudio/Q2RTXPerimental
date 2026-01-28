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
    const Vector3 leaf_mins;
    const Vector3 leaf_maxs;

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
/**
*	@brief	CM_BoxLeafs_r - Recursive helper for CM_BoxLeafs functions.
*	@param	boxLeafTest	Accumulation structure for box leaf tests.
*	@param	node		Current BSP node to process.
**/
static void CM_BoxLeafs_r( box_leaf_test_t &boxLeafTest, mnode_t *node ) {
	/**
	*	Iterative BSP walk that matches our node layout:
	*		- A node with a null plane pointer represents a leaf.
	*		- Leaves are stored as mleaf_t and are reachable through mnode_t pointers.
	*
	*	This variant is hardened for runtime use:
	*		- Safely handles null node pointers when following children.
	*		- Preserves the existing "topnode" behavior for boxes spanning both sides.
	**/
	while ( true ) {
	/**
	*	Sanity: if the caller (or BSP) gives us a null node, there is nothing to accumulate.
	**/
	if ( !node ) {
		return;
	}

	/**
	*	Leaf node: accumulate contents and optionally append to the caller-provided list.
	**/
	if ( !node->plane ) {
		mleaf_t *leaf = (mleaf_t *)node;
		boxLeafTest.leafs_contents = ( boxLeafTest.leafs_contents | (cm_contents_t)leaf->contents );

		if ( boxLeafTest.leaf_list && boxLeafTest.leaf_count < boxLeafTest.leaf_max_count ) {
			boxLeafTest.leaf_list[ boxLeafTest.leaf_count++ ] = leaf;
		}
		return;
	}

	/**
	*	Internal node: classify the box relative to the split plane.
	**/
	const int32_t side = BoxOnPlaneSideFast( boxLeafTest.leaf_mins, boxLeafTest.leaf_maxs, node->plane );
	if ( side == BOX_INFRONT ) {
		// Box is entirely in front.
		node = node->children[ 0 ];
		continue;
	}
	if ( side == BOX_BEHIND ) {
		// Box is entirely behind.
		node = node->children[ 1 ];
		continue;
	}

	/**
	*	Box spans both sides: recurse into the front side, then continue iterating the back.
	**/
	if ( !boxLeafTest.leaf_topnode ) {
		boxLeafTest.leaf_topnode = node;
	}

	CM_BoxLeafs_r( boxLeafTest, node->children[ 0 ] );
	node = node->children[ 1 ];
}
}

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
const int32_t CM_BoxLeafs_headnode( cm_t *cm, const Vector3 &mins, const Vector3 &maxs, mleaf_t **list, const int32_t listsize, mnode_t *headnode, mnode_t **topnode ) {
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

	/**
	*	If no headnode was provided, default to the world's BSP root.
	*	This matches common engine usage where nullptr means "world".
	**/
	if ( !headnode && cm && cm->cache ) {
		headnode = cm->cache->nodes;
	}

    // Recursively.
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
const int32_t CM_BoxLeafs( cm_t *cm, const Vector3 &mins, const Vector3 &maxs, mleaf_t **list, const int32_t listsize, mnode_t **topnode ) {
    // Map not loaded.
    if ( !cm->cache ) {
        return 0;
    }

    return CM_BoxLeafs_headnode( cm, mins, maxs, list, listsize, cm->cache->nodes, topnode );
}



