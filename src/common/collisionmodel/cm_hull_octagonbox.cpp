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
    *   @brief  Initialize the structural topology of a synthetic octagon hull.
    *   @param  hull    Hull storage to initialize.
    *   @note   This only wires up planes, nodes, and leaf/brush ownership. Per-trace distances and offsets are assigned later.
    **/
    static void CM_InitOctagonHullState( hull_octagonbox_t *hull ) {
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
        hull->brush.numsides = 10;
        hull->brush.firstbrushside = &hull->brushsides[ 0 ];
        hull->brush.contents = CONTENTS_MONSTER;
        hull->leaf.contents = CONTENTS_MONSTER;
        hull->leaf.firstleafbrush = &hull->leafbrush;
        hull->leaf.numleafbrushes = 1;
        hull->leafbrush = &hull->brush;

        /**
        *   Wire the six axial planes that form the inner bounding box shell.
        **/
        for ( int32_t i = 0; i < 6; i++ ) {
            // Determine which child points at the empty leaf for this split.
            const int32_t side = i & 1;

            // Bind the brush side to the corresponding plane and null texture info.
            mbrushside_t *brushSide = &hull->brushsides[ i ];
            brushSide->plane = &hull->planes[ i * 2 + side ];
            brushSide->texinfo = &nulltexinfo;

            // Chain clip nodes together until the solid leaf is reached.
            mnode_t *clipNode = &hull->nodes[ i ];
            clipNode->plane = &hull->planes[ i * 2 ];
            clipNode->children[ side ] = ( mnode_t * )&hull->emptyleaf;
            if ( i != 5 ) {
                clipNode->children[ side ^ 1 ] = &hull->nodes[ i + 1 ];
            } else {
                clipNode->children[ side ^ 1 ] = ( mnode_t * )&hull->leaf;
            }

            // Initialize the outward-facing axial plane for this axis.
            cm_plane_t *plane = &hull->planes[ i * 2 ];
            plane->normal[ 0 ] = 0.0f;
            plane->normal[ 1 ] = 0.0f;
            plane->normal[ 2 ] = 0.0f;
            plane->normal[ i >> 1 ] = 1.0f;
            SetPlaneType( plane );
            SetPlaneSignbits( plane );

            // Initialize the inward-facing axial companion plane for this axis.
            plane = &hull->planes[ i * 2 + 1 ];
            plane->normal[ 0 ] = 0.0f;
            plane->normal[ 1 ] = 0.0f;
            plane->normal[ 2 ] = 0.0f;
            plane->normal[ i >> 1 ] = -1.0f;
            SetPlaneType( plane );
            SetPlaneSignbits( plane );
        }

        /**
        *   Wire the four diagonal planes that complete the octagon shell.
        **/
        const vec3_t oct_dirs[ 4 ] = { { 1, 1, 0 }, { -1, 1, 0 }, { -1, -1, 0 }, { 1, -1, 0 } };
        for ( int32_t i = 6; i < 10; i++ ) {
            // Determine which child points at the empty leaf for this split.
            const int32_t side = i & 1;

            // Bind the brush side to the corresponding plane and null texture info.
            mbrushside_t *brushSide = &hull->brushsides[ i ];
            brushSide->plane = &hull->planes[ i * 2 + side ];
            brushSide->texinfo = &nulltexinfo;

            // Chain clip nodes together until the final solid leaf is reached.
            mnode_t *clipNode = &hull->nodes[ i ];
            clipNode->plane = &hull->planes[ i * 2 ];
            clipNode->children[ side ] = ( mnode_t * )&hull->emptyleaf;
            if ( i != 9 ) {
                clipNode->children[ side ^ 1 ] = &hull->nodes[ i + 1 ];
            } else {
                clipNode->children[ side ^ 1 ] = ( mnode_t * )&hull->leaf;
            }

            // Initialize the first diagonal plane of the pair.
            cm_plane_t *plane = &hull->planes[ i * 2 ];
            plane->normal[ 0 ] = oct_dirs[ i - 6 ][ 0 ] * -1.0f;
            plane->normal[ 1 ] = oct_dirs[ i - 6 ][ 1 ] * -1.0f;
            plane->normal[ 2 ] = 0.0f;
            SetPlaneType( plane );
            SetPlaneSignbits( plane );

            // Initialize the second diagonal plane of the pair.
            plane = &hull->planes[ i * 2 + 1 ];
            plane->normal[ 0 ] = oct_dirs[ i - 6 ][ 0 ];
            plane->normal[ 1 ] = oct_dirs[ i - 6 ][ 1 ];
            plane->normal[ 2 ] = 0.0f;
            SetPlaneType( plane );
            SetPlaneSignbits( plane );
        }
    }

    /**
    *   @brief  Retrieve the current thread's synthetic octagon hull storage.
    *   @return Pointer to reusable thread-local hull storage.
    *   @note   This avoids cross-thread mutation of shared synthetic hull state while also avoiding per-trace heap allocations.
    **/
    static hull_octagonbox_t *CM_GetThreadLocalOctagonHull( void ) {
        //! Thread-local synthetic hull reused by octagon traces on this worker thread.
        static thread_local hull_octagonbox_t threadLocalHull = {};
        //! Tracks whether the thread-local octagon hull topology was already initialized.
        static thread_local bool threadLocalHullInitialized = false;

        /**
        *   Lazily wire the synthetic hull topology the first time this thread needs it.
        **/
        if ( !threadLocalHullInitialized ) {
            CM_InitOctagonHullState( &threadLocalHull );
            threadLocalHullInitialized = true;
        }

        return &threadLocalHull;
    }
}



/**
*   Set up the planes and nodes so that the ten floats of a Bounding 'Octagon' Box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitOctagonHull( cm_t *cm ) {
    /**
    *   Sanity check: require collision-model storage before allocating the shared compatibility hull.
    **/
    if ( !cm ) {
        return;
    }

    // Moved hull_octagonbox from heap memory to stack allocated memory, 
    // this so that we can properly call initialize only during CM_LoadMap.
    // 
    // The server copies over the local function heap mapcmd_t cm into sv.cm 
    // which causes the hull_octagonbox.headnode pointer to an invalid nonexistent(old) memory address.
    cm->hull_octagonbox = static_cast<hull_octagonbox_t *>( Z_TagMallocz( sizeof( hull_octagonbox_t ), TAG_CMODEL ) );

    /**
    *   Initialize the shared compatibility hull topology once during collision-model load.
    **/
    CM_InitOctagonHullState( cm->hull_octagonbox );
}

/**
*   @brief  Utility function to complement CM_HeadnodeForOctagon with.
**/
static inline const float CalculateOctagonPlaneDist( cm_plane_t &plane, const Vector3 &mins, const Vector3 &maxs, const bool negate = false ) {
    const Vector3 planeNormal = { plane.normal[ 0 ], plane.normal[ 1 ], plane.normal[ 2 ] };
    if ( negate == true ) {
        return QM_Vector3DotProduct( planeNormal, Vector3{ ( plane.signbits & 1 ) ? -mins[ 0 ] : -maxs[ 0 ], ( plane.signbits & 2 ) ? -mins[ 1 ] : -maxs[ 1 ], ( plane.signbits & 4 ) ? -mins[ 2 ] : -maxs[ 2 ] } );
    } else {
        return QM_Vector3DotProduct( planeNormal, Vector3{ ( plane.signbits & 1 ) ? mins[ 0 ] : maxs[ 0 ], ( plane.signbits & 2 ) ? mins[ 1 ] : maxs[ 1 ], ( plane.signbits & 4 ) ? mins[ 2 ] : maxs[ 2 ] } );
    }
}

/**
*   @brief  To keep everything totally uniform, Bounding 'Octagon' Boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' octagon box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
//mnode_t *CM_HeadnodeForOctagon( cm_t *cm, const vec3_t mins, const vec3_t maxs, const cm_contents_t contents ) {
mnode_t *CM_HeadnodeForOctagon( cm_t *cm, const vec3_t mins, const vec3_t maxs, const cm_contents_t contents ) {
    /**
    *   Use thread-local synthetic hull storage so concurrent traces never race on the shared collision-model hull template.
    **/
    hull_octagonbox_t *hull = CM_GetThreadLocalOctagonHull();

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
    // Fix: compute axis-aligned plane distances from plane normals and the actual box corners.
    // Use the helper to pick the correct corner for each plane based on signbits.
    for ( int i = 0; i < 12; ++i ) {
        hull->planes[ i ].dist = CalculateOctagonPlaneDist( hull->planes[ i ], mins, maxs );
    }

    // Cylindrical offset.
    for ( int32_t i = 0; i < 3; i++ ) {
        hull->cylinder_offset[ i ] = ( mins[ i ] + maxs[ i ] ) * 0.5;
    }

    // Calculate actual up to scale normals for the non axial planes.
    // Fix: use half-sizes (extents) instead of raw maxs which assumed symmetric bounds.
    const float a = ( maxs[ 0 ] - mins[ 0 ] ) * 0.5f;
    const float b = ( maxs[ 1 ] - mins[ 1 ] ) * 0.5f;

    float dist = sqrtf( a * a + b * b ); // Hypothenuse

    float cosa = a / dist;
    float sina = b / dist;

    // Assign normalized normals for octagon planes, then set signbits and distances.
    // Plane 12: outer (cosa, sina, 0)
    {
        cm_plane_t *plane = &hull->planes[ 12 ];
        plane->normal[0] = cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[12].dist = CalculateOctagonPlaneDist( hull->planes[12], mins, maxs );
    }
    // Plane 13: inner negated (same normal, use negate flag)
    {
        cm_plane_t *plane = &hull->planes[ 13 ];
        plane->normal[0] = cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[13].dist = CalculateOctagonPlaneDist( hull->planes[13], mins, maxs, true );
    }
    // Plane 14: outer (-cosa, sina, 0) (negated axis-x)
    {
        cm_plane_t *plane = &hull->planes[ 14 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[14].dist = CalculateOctagonPlaneDist( hull->planes[14], mins, maxs, true );
    }
    // Plane 15: inner (-cosa, sina, 0)
    {
        cm_plane_t *plane = &hull->planes[ 15 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[15].dist = CalculateOctagonPlaneDist( hull->planes[15], mins, maxs );
    }
    // Plane 16: outer (-cosa, -sina, 0)
    {
        cm_plane_t *plane = &hull->planes[ 16 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[16].dist = CalculateOctagonPlaneDist( hull->planes[16], mins, maxs );
    }
    // Plane 17: inner (-cosa, -sina, 0) negated
    {
        cm_plane_t *plane = &hull->planes[ 17 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[17].dist = CalculateOctagonPlaneDist( hull->planes[17], mins, maxs, true );
    }
    // Plane 18: outer (cosa, -sina, 0)
    {
        cm_plane_t *plane = &hull->planes[ 18 ];
        plane->normal[0] = cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[18].dist = CalculateOctagonPlaneDist( hull->planes[18], mins, maxs, true );
    }
    // Plane 19: inner (cosa, -sina, 0)
    {
        cm_plane_t *plane = &hull->planes[ 19 ];
        plane->normal[0] = cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        hull->planes[19].dist = CalculateOctagonPlaneDist( hull->planes[19], mins, maxs );
    }

    // Return octagonbox' headnode pointer.
    return hull->headnode;
}

/**
*   @brief  Determine whether the specified headnode belongs to a synthetic octagon hull.
*   @param  cm          Collision model owning the shared compatibility template.
*   @param  headnode    Headnode to classify.
*   @return True when the headnode belongs to either the shared compatibility hull or the current thread-local hull.
**/
const bool CM_IsOctagonBoxHullHeadnode( const cm_t *cm, const mnode_t *headnode ) {
    /**
    *   Reject null headnodes before touching any compatibility or thread-local hull storage.
    **/
    if ( !headnode ) {
        return false;
    }

    /**
    *   Accept the shared compatibility hull used during collision-model initialization.
    **/
    if ( cm && cm->hull_octagonbox && headnode == cm->hull_octagonbox->headnode ) {
        return true;
    }

    /**
    *   Accept the current thread's live synthetic hull as well.
    **/
    return headnode == CM_GetThreadLocalOctagonHull()->headnode;
}

/**
*   @brief  Retrieve the cylinder offset for a synthetic octagon hull headnode.
*   @param  cm          Collision model owning the shared compatibility template.
*   @param  headnode    Headnode whose cylinder offset should be resolved.
*   @return Pointer to the cylinder offset for the matching synthetic octagon hull, or nullptr when the headnode is unrelated.
**/
const vec3_t *CM_GetOctagonBoxHullCylinderOffset( const cm_t *cm, const mnode_t *headnode ) {
    /**
    *   Reject null headnodes before resolving compatibility or thread-local hull storage.
    **/
    if ( !headnode ) {
        return nullptr;
    }

    /**
    *   Resolve the shared compatibility hull offset when callers still refer to that template.
    **/
    if ( cm && cm->hull_octagonbox && headnode == cm->hull_octagonbox->headnode ) {
        return &cm->hull_octagonbox->cylinder_offset;
    }

    /**
    *   Resolve the current thread's live synthetic octagon hull offset.
    **/
    hull_octagonbox_t *threadLocalHull = CM_GetThreadLocalOctagonHull();
    if ( headnode == threadLocalHull->headnode ) {
        return &threadLocalHull->cylinder_offset;
    }

    return nullptr;
}
