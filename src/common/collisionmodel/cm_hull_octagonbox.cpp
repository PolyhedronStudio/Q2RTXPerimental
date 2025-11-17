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



/**
*   Set up the planes and nodes so that the ten floats of a Bounding 'Octagon' Box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitOctagonHull( cm_t *cm ) {
    // Moved hull_octagonbox from heap memory to stack allocated memory, 
    // this so that we can properly call initialize only during CM_LoadMap.
    // 
    // The server copies over the local function heap mapcmd_t cm into sv.cm 
    // which causes the hull_octagonbox.headnode pointer to an invalid nonexistent(old) memory address.
    cm->hull_octagonbox = static_cast<hull_octagonbox_t *>( Z_TagMallocz( sizeof( hull_octagonbox_t ), TAG_CMODEL ) );

    cm->hull_octagonbox->headnode = &cm->hull_octagonbox->nodes[ 0 ];

    cm->hull_octagonbox->brush.numsides = 10;
    cm->hull_octagonbox->brush.firstbrushside = &cm->hull_octagonbox->brushsides[ 0 ];
    cm->hull_octagonbox->brush.contents = CONTENTS_MONSTER;

    cm->hull_octagonbox->leaf.contents = CONTENTS_MONSTER;
    cm->hull_octagonbox->leaf.firstleafbrush = &cm->hull_octagonbox->leafbrush;
    cm->hull_octagonbox->leaf.numleafbrushes = 1;

    cm->hull_octagonbox->leafbrush = &cm->hull_octagonbox->brush;

    // First the actual bounding box planes.
    for ( int32_t i = 0; i < 6; i++ ) {
        // Determine side.
        const int32_t side = i & 1;

        // Brush Sides:
        mbrushside_t *brushSide = &cm->hull_octagonbox->brushsides[ i ];
        brushSide->plane = &cm->hull_octagonbox->planes[ i * 2 + side ];
        brushSide->texinfo = &nulltexinfo;

        // Clipping Nodes:
        mnode_t *clipNode = &cm->hull_octagonbox->nodes[ i ];
        clipNode->plane = &cm->hull_octagonbox->planes[ i * 2 ];
        clipNode->children[ side ] = (mnode_t *)&cm->hull_octagonbox->emptyleaf;
        if ( i != 5 ) {
            clipNode->children[ side ^ 1 ] = &cm->hull_octagonbox->nodes[ i + 1 ];
        } else {
            clipNode->children[ side ^ 1 ] = (mnode_t *)&cm->hull_octagonbox->leaf;
        }
        // planes - initialize normals fully, then set type & signbits
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ i * 2 ];
        plane->normal[0] = plane->normal[1] = plane->normal[2] = 0.0f;
        plane->normal[ i >> 1 ] = 1.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );

        plane = &cm->hull_octagonbox->planes[ i * 2 + 1 ];
        plane->normal[0] = plane->normal[1] = plane->normal[2] = 0.0f;
        plane->normal[ i >> 1 ] = -1.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
    }

    // Now the octogonal planes.
    const vec3_t oct_dirs[ 4 ] = { { 1, 1, 0 }, { -1, 1, 0 }, { -1, -1, 0 }, { 1, -1, 0 } };

    for ( int32_t i = 6; i < 10; i++ ) {
        // Determine Side Index:
        const int32_t side = i & 1;

        // Brush Sides:
        mbrushside_t *brushSide = &cm->hull_octagonbox->brushsides[ i ];
        brushSide->plane = &cm->hull_octagonbox->planes[ i * 2 + side ];
        brushSide->texinfo = &nulltexinfo;

        // Clipping Nodes:
        mnode_t *clipNode = &cm->hull_octagonbox->nodes[ i ];
        clipNode->plane = &cm->hull_octagonbox->planes[ i * 2 ];
        clipNode->children[ side ] = (mnode_t *)&cm->hull_octagonbox->emptyleaf;
        if ( i != 9 ) {
            clipNode->children[ side ^ 1 ] = &cm->hull_octagonbox->nodes[ i + 1 ];
        } else {
            clipNode->children[ side ^ 1 ] = (mnode_t *)&cm->hull_octagonbox->leaf;
        }

        // Planes - set normals (negated for the first of the pair), set type & signbits
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ i * 2 ];
        plane->normal[0] = oct_dirs[ i - 6 ][0] * -1.0f;
        plane->normal[1] = oct_dirs[ i - 6 ][1] * -1.0f;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );

        plane = &cm->hull_octagonbox->planes[ i * 2 + 1 ];
        plane->normal[0] = oct_dirs[ i - 6 ][0];
        plane->normal[1] = oct_dirs[ i - 6 ][1];
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
    }
}

/**
*   @brief  Utility function to complement CM_HeadnodeForOctagon with.
**/
static inline const float CalculateOctagonPlaneDist( cm_plane_t &plane, const Vector3 &mins, const Vector3 &maxs, const bool negate = false ) {
    if ( negate == true ) {
        return QM_Vector3DotProduct( plane.normal, { ( plane.signbits & 1 ) ? -mins[ 0 ] : -maxs[ 0 ], ( plane.signbits & 2 ) ? -mins[ 1 ] : -maxs[ 1 ], ( plane.signbits & 4 ) ? -mins[ 2 ] : -maxs[ 2 ] } );
    } else {
        return QM_Vector3DotProduct( plane.normal, { ( plane.signbits & 1 ) ? mins[ 0 ] : maxs[ 0 ], ( plane.signbits & 2 ) ? mins[ 1 ] : maxs[ 1 ], ( plane.signbits & 4 ) ? mins[ 2 ] : maxs[ 2 ] } );
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
    // Setup to CONTENTS_MONSTER in case of no contents being passed in.
    if ( contents == CONTENTS_NONE ) {
        cm->hull_octagonbox->leaf.contents = cm->hull_octagonbox->brush.contents = CONTENTS_MONSTER;
    } else {
        cm->hull_octagonbox->leaf.contents = cm->hull_octagonbox->brush.contents = contents;
    }

    // Setup its bounding boxes.
    VectorCopy( mins, cm->hull_octagonbox->headnode->mins );
    VectorCopy( maxs, cm->hull_octagonbox->headnode->maxs );
    VectorCopy( mins, cm->hull_octagonbox->leaf.mins );
    VectorCopy( maxs, cm->hull_octagonbox->leaf.maxs );

    // Setup planes.
    // Fix: compute axis-aligned plane distances from plane normals and the actual box corners.
    // Use the helper to pick the correct corner for each plane based on signbits.
    for ( int i = 0; i < 12; ++i ) {
        cm->hull_octagonbox->planes[ i ].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[ i ], mins, maxs );
    }

    // Cylindrical offset.
    for ( int32_t i = 0; i < 3; i++ ) {
        cm->hull_octagonbox->cylinder_offset[ i ] = ( mins[ i ] + maxs[ i ] ) * 0.5;
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
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 12 ];
        plane->normal[0] = cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[12].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[12], mins, maxs );
    }
    // Plane 13: inner negated (same normal, use negate flag)
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 13 ];
        plane->normal[0] = cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[13].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[13], mins, maxs, true);
    }
    // Plane 14: outer (-cosa, sina, 0) (negated axis-x)
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 14 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[14].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[14], mins, maxs, true);
    }
    // Plane 15: inner (-cosa, sina, 0)
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 15 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[15].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[15], mins, maxs);
    }
    // Plane 16: outer (-cosa, -sina, 0)
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 16 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[16].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[16], mins, maxs);
    }
    // Plane 17: inner (-cosa, -sina, 0) negated
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 17 ];
        plane->normal[0] = -cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[17].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[17], mins, maxs, true);
    }
    // Plane 18: outer (cosa, -sina, 0)
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 18 ];
        plane->normal[0] = cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[18].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[18], mins, maxs, true);
    }
    // Plane 19: inner (cosa, -sina, 0)
    {
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ 19 ];
        plane->normal[0] = cosa;
        plane->normal[1] = -sina;
        plane->normal[2] = 0.0f;
        SetPlaneType( plane );
        SetPlaneSignbits( plane );
        cm->hull_octagonbox->planes[19].dist = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[19], mins, maxs);
    }

    // Return octagonbox' headnode pointer.
    return cm->hull_octagonbox->headnode;
}
