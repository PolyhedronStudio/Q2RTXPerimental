/**
*
*
*   CollisionModel: Bounding Box BSP Hull
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



namespace {
    /**
    *   @brief  Initialize the structural topology of a synthetic bounding-box hull.
    *   @param  hull    Hull storage to initialize.
    *   @note   This only wires up planes, nodes, and leaf/brush ownership. Per-trace mins/maxs and plane distances are assigned later.
    **/
    static void CM_InitBoxHullState( hull_boundingbox_t *hull ) {
        /**
        *   Sanity check: require destination storage before wiring the synthetic hull.
        **/
        if ( !hull ) {
            return;
        }

        /**
        *   Reset the hull storage so every plane, node, and leaf starts from a deterministic baseline.
        **/
        *hull = {};

        /**
        *   Initialize the root node, brush, and leaf ownership for the synthetic hull tree.
        **/
        hull->headnode = &hull->nodes[ 0 ];
        hull->brush.numsides = 6;
        hull->brush.firstbrushside = &hull->brushsides[ 0 ];
        hull->brush.contents = CONTENTS_MONSTER;
        hull->leaf.firstleafbrush = &hull->leafbrush;
        hull->leaf.numleafbrushes = 1;
        hull->leaf.contents = CONTENTS_MONSTER;
        hull->leafbrush = &hull->brush;

        /**
        *   Wire the six axial brush sides and their clip nodes into one small BSP chain.
        **/
        for ( int32_t i = 0; i < 6; i++ ) {
            // Determine which side of the split points at the empty leaf.
            const int32_t side = i & 1;

            // Bind the brush side to the corresponding plane and null texture info.
            mbrushside_t *brushSide = &hull->brushsides[ i ];
            brushSide->plane = &hull->planes[ i * 2 + side ];
            brushSide->texinfo = &nulltexinfo;

            // Chain clip nodes together until the final solid leaf is reached.
            mnode_t *clipNode = &hull->nodes[ i ];
            clipNode->plane = &hull->planes[ i * 2 ];
            clipNode->children[ side ] = ( mnode_t * )&hull->emptyleaf;
            if ( i != 5 ) {
                clipNode->children[ side ^ 1 ] = &hull->nodes[ i + 1 ];
            } else {
                clipNode->children[ side ^ 1 ] = ( mnode_t * )&hull->leaf;
            }

            // Initialize the positive-facing axial plane for this axis.
            cm_plane_t *plane = &hull->planes[ i * 2 ];
            plane->normal[ 0 ] = 0.0f;
            plane->normal[ 1 ] = 0.0f;
            plane->normal[ 2 ] = 0.0f;
            plane->normal[ i >> 1 ] = 1.0f;
            SetPlaneType( plane );
            SetPlaneSignbits( plane );

            // Initialize the negative-facing companion axial plane for this axis.
            plane = &hull->planes[ i * 2 + 1 ];
            plane->normal[ 0 ] = 0.0f;
            plane->normal[ 1 ] = 0.0f;
            plane->normal[ 2 ] = 0.0f;
            plane->normal[ i >> 1 ] = -1.0f;
            SetPlaneType( plane );
            SetPlaneSignbits( plane );
        }
    }

    /**
    *   @brief  Retrieve the current thread's synthetic bounding-box hull storage.
    *   @return Pointer to reusable thread-local hull storage.
    *   @note   This avoids cross-thread mutation of shared synthetic hull state while also avoiding per-trace heap allocations.
    **/
    static hull_boundingbox_t *CM_GetThreadLocalBoxHull( void ) {
        //! Thread-local synthetic hull reused by box traces on this worker thread.
        static thread_local hull_boundingbox_t threadLocalHull = {};
        //! Tracks whether the thread-local synthetic hull topology was already initialized.
        static thread_local bool threadLocalHullInitialized = false;

        /**
        *   Lazily wire the synthetic hull topology the first time this thread needs it.
        **/
        if ( !threadLocalHullInitialized ) {
            CM_InitBoxHullState( &threadLocalHull );
            threadLocalHullInitialized = true;
        }

        return &threadLocalHull;
    }
}



/**
*   Set up the planes and nodes so that the six floats of a bounding box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitBoxHull( cm_t *cm ) {
    /**
    *   Sanity check: require collision-model storage before allocating the shared compatibility hull.
    **/
    if ( !cm ) {
        return;
    }

    // Moved hull_boundingbox from stack memory to heap allocated memory, 
    // this so that we can properly call initialize only during CM_LoadMap.
    // 
    // The server copies over the local function stack mapcmd_t cm into sv.cm 
    // which causes the hull_boundingbox.headnode pointer to an invalid nonexistent(old) memory address.
	
    // Bounding box hulls are used in CM_HeadnodeForBox() which is called by the server
    cm->hull_boundingbox = static_cast<hull_boundingbox_t *>( Z_TagMallocz( sizeof( hull_boundingbox_t ), TAG_CMODEL ) );

    /**
    *   Initialize the shared compatibility hull topology once during collision-model load.
    **/
    CM_InitBoxHullState( cm->hull_boundingbox );
}

/**
*   @brief  To keep everything totally uniform, bounding boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
mnode_t *CM_HeadnodeForBox( cm_t *cm, const vec3_t mins, const vec3_t maxs, const cm_contents_t contents ) {
    /**
    *   Use thread-local synthetic hull storage so concurrent traces never race on the shared collision-model hull template.
    **/
    hull_boundingbox_t *hull = CM_GetThreadLocalBoxHull();

    // Setup to CONTENTS_MONSTER in case of no contents being passed in.
    if ( contents == CONTENTS_NONE ) {
        hull->leaf.contents = hull->brush.contents = CONTENTS_MONSTER;
    } else {
        hull->leaf.contents = hull->brush.contents = contents;
    }

    // Setup its bounding boxes.
    VectorCopy( mins, hull->headnode->mins );
    VectorCopy( maxs, hull->headnode->maxs );
    VectorCopy( mins, hull->leaf.mins );
    VectorCopy( maxs, hull->leaf.maxs );

    // Setup planes.
    hull->planes[ 0 ].dist = maxs[ 0 ];
    hull->planes[ 1 ].dist = -maxs[ 0 ];
    hull->planes[ 2 ].dist = mins[ 0 ];
    hull->planes[ 3 ].dist = -mins[ 0 ];
    hull->planes[ 4 ].dist = maxs[ 1 ];
    hull->planes[ 5 ].dist = -maxs[ 1 ];
    hull->planes[ 6 ].dist = mins[ 1 ];
    hull->planes[ 7 ].dist = -mins[ 1 ];
    hull->planes[ 8 ].dist = maxs[ 2 ];
    hull->planes[ 9 ].dist = -maxs[ 2 ];
    hull->planes[ 10 ].dist = mins[ 2 ];
    hull->planes[ 11 ].dist = -mins[ 2 ];

    // Return boundingbox' headnode pointer.
    return hull->headnode;
}

/**
*   @brief  Determine whether the specified headnode belongs to a synthetic bounding-box hull.
*   @param  cm          Collision model owning the shared compatibility template.
*   @param  headnode    Headnode to classify.
*   @return True when the headnode belongs to either the shared compatibility hull or the current thread-local hull.
**/
const bool CM_IsBoundingBoxHullHeadnode( const cm_t *cm, const mnode_t *headnode ) {
    /**
    *   Reject null headnodes before touching any compatibility or thread-local hull storage.
    **/
    if ( !headnode ) {
        return false;
    }

    /**
    *   Accept the shared compatibility hull used during collision-model initialization.
    **/
    if ( cm && cm->hull_boundingbox && headnode == cm->hull_boundingbox->headnode ) {
        return true;
    }

    /**
    *   Accept the current thread's live synthetic hull as well.
    **/
    return headnode == CM_GetThreadLocalBoxHull()->headnode;
}
