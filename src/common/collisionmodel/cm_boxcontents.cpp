/**
*
*
*   CollisionModel: BoxContents, getting all leafs that reside inside the bounding box:
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
*
*
*
*	BoxContents
*
*
*
**/
/**
*	@brief	Utility structure for box contents accumulation.
**/
struct cm_box_contents_test_t {
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
};

/**
*	@brief	CM_BoxContents_r - Recursive helper for CM_BoxContents functions.
*	@param	boxContentsTest	Accumulation structure for box leaf tests.
*	@param	node		Current BSP node to process.
**/
static void CM_BoxContents_r( cm_box_contents_test_t &boxContentsTest, cm_contents_t *contents, mnode_t *node ) {
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
			mleaf_t *leaf = ( mleaf_t * )node;
			*contents = ( cm_contents_t )( ( cm_contents_t )boxContentsTest.leafs_contents | ( cm_contents_t )leaf->contents );

			if ( boxContentsTest.leaf_list && boxContentsTest.leaf_count < boxContentsTest.leaf_max_count ) {
				boxContentsTest.leaf_list[ boxContentsTest.leaf_count++ ] = leaf;
			}
			return;
		}

		/**
		*	Internal node: classify the box relative to the split plane.
		**/
		const int32_t side = BoxOnPlaneSideFast( boxContentsTest.leaf_mins, boxContentsTest.leaf_maxs, node->plane );
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
		if ( !boxContentsTest.leaf_topnode ) {
			boxContentsTest.leaf_topnode = node;
		}

		CM_BoxContents_r( boxContentsTest, contents, node->children[ 0 ] );
		node = node->children[ 1 ];
	}
}

/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
const int32_t CM_BoxContents_headnode( cm_t *cm, const Vector3 &mins, const Vector3 &maxs, cm_contents_t *contents, mleaf_t **list, const int32_t listsize, mnode_t *headnode, mnode_t **topnode ) {
	cm_box_contents_test_t boxContentsTest = {
		// [In]:
		.cm = cm,

		.leaf_mins = mins,
		.leaf_maxs = maxs,

		.leaf_list = list,
		.leaf_count = 0,
		.leaf_max_count = listsize,

		// [Out]:
		.leaf_topnode = nullptr,
	};

	*contents = CONTENTS_NONE;

	/**
	*	If no headnode was provided, default to the world's BSP root.
	*	This matches common engine usage where nullptr means "world".
	**/
	if ( !headnode && cm && cm->cache ) {
		headnode = cm->cache->nodes;
	}

	// Recursively.
	CM_BoxContents_r( boxContentsTest, contents, headnode );

	// Copy over the leaf_topnode value into the topnode pointer value if it was set.
	if ( topnode ) {
		*topnode = boxContentsTest.leaf_topnode;
	}

	return boxContentsTest.leaf_count;
}

/**
*   @brief  Populates the list of leafs which the specified absolute bounds box touches, as well as
*			a convenient contentsMask consisting of all CONTENTS_ found in the leafs.
*			If top_node is not set to NULL, it will contain a value copy of the the top 
*			node of the BSP tree that fully contains the box.
**/
const int32_t CM_BoxContents( cm_t *cm, const Vector3 &mins, const Vector3 &maxs, cm_contents_t *contents, mleaf_t **list, const int32_t listsize, mnode_t **topnode ) {
	// Map not loaded.
	if ( !cm->cache ) {
		return 0;
	}

	return CM_BoxContents_headnode( cm, mins, maxs, contents, list, listsize, cm->cache->nodes, topnode );
}

#if 0
/**
*   @return The contents mask of all leafs within the absolute bounds.
**/
const cm_boxcontents_t CM_BoxContents( cm_t *cm, const Vector3 &mins, const Vector3 &maxs, mnode_t *headnode ) {
	cm_box_contents_test_t boxContentsTest = {
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
	CM_BoxContents_r( boxContentsTest, headnode );

	// Return contents found in all leafs touching the bounds.
	return boxContentsTest.leafs_contents;
}
#endif