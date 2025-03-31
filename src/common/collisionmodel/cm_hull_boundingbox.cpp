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
*   Set up the planes and nodes so that the six floats of a bounding box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitBoxHull( cm_t *cm ) {
    // Moved hull_boundingbox from heap memory to stack allocated memory, 
    // this so that we can properly call initialize only during CM_LoadMap.
    // 
    // The server copies over the local function heap mapcmd_t cm into sv.cm 
    // which causes the hull_boundingbox.headnode pointer to an invalid nonexistent(old) memory address.
    cm->hull_boundingbox = static_cast<hull_boundingbox_t *>( Z_TagMallocz( sizeof( hull_boundingbox_t ), TAG_CMODEL ) );

    cm->hull_boundingbox->headnode = &cm->hull_boundingbox->nodes[ 0 ];

    cm->hull_boundingbox->brush.numsides = 6;
    cm->hull_boundingbox->brush.firstbrushside = &cm->hull_boundingbox->brushsides[ 0 ];
    cm->hull_boundingbox->brush.contents = CONTENTS_MONSTER;

    cm->hull_boundingbox->leaf.contents = CONTENTS_MONSTER;
    cm->hull_boundingbox->leaf.firstleafbrush = &cm->hull_boundingbox->leafbrush;
    cm->hull_boundingbox->leaf.numleafbrushes = 1;

    cm->hull_boundingbox->leafbrush = &cm->hull_boundingbox->brush;

    for ( int32_t i = 0; i < 6; i++ ) {
        // Brush Side Index:
        const int32_t side = i & 1;

        // Brush Sides:
        mbrushside_t *brushSide = &cm->hull_boundingbox->brushsides[ i ];
        brushSide->plane = &cm->hull_boundingbox->planes[ i * 2 + side ];
        brushSide->texinfo = &nulltexinfo;

        // Clipping Nodes:
        mnode_t *clipNode = &cm->hull_boundingbox->nodes[ i ];
        clipNode->plane = &cm->hull_boundingbox->planes[ i * 2 ];
        clipNode->children[ side ] = (mnode_t *)&cm->hull_boundingbox->emptyleaf;
        if ( i != 5 ) {
            clipNode->children[ side ^ 1 ] = &cm->hull_boundingbox->nodes[ i + 1 ];
        } else {
            clipNode->children[ side ^ 1 ] = (mnode_t *)&cm->hull_boundingbox->leaf;
        }

        // Planes:
        cm_plane_t *plane = &cm->hull_boundingbox->planes[ i * 2 ];
        plane->type = i >> 1;
        plane->normal[ i >> 1 ] = 1;

        plane = &cm->hull_boundingbox->planes[ i * 2 + 1 ];
        plane->type = 3 + ( i >> 1 );
        plane->signbits = 1 << ( i >> 1 );
        plane->normal[ i >> 1 ] = -1;
    }
}

/**
*   @brief  To keep everything totally uniform, bounding boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
mnode_t *CM_HeadnodeForBox( cm_t *cm, const vec3_t mins, const vec3_t maxs, const contents_t contents ) {
    // Setup to CONTENTS_MONSTER in case of no contents being passed in.
    if ( contents == CONTENTS_NONE ) {
        cm->hull_boundingbox->leaf.contents = cm->hull_boundingbox->brush.contents = CONTENTS_MONSTER;
    } else {
        cm->hull_boundingbox->leaf.contents = cm->hull_boundingbox->brush.contents = contents;
    }

    // Setup its bounding boxes.
    VectorCopy( mins, cm->hull_boundingbox->headnode->mins );
    VectorCopy( maxs, cm->hull_boundingbox->headnode->maxs );
    VectorCopy( mins, cm->hull_boundingbox->leaf.mins );
    VectorCopy( maxs, cm->hull_boundingbox->leaf.maxs );

    // Setup planes.
    cm->hull_boundingbox->planes[ 0 ].dist = maxs[ 0 ];
    cm->hull_boundingbox->planes[ 1 ].dist = -maxs[ 0 ];
    cm->hull_boundingbox->planes[ 2 ].dist = mins[ 0 ];
    cm->hull_boundingbox->planes[ 3 ].dist = -mins[ 0 ];
    cm->hull_boundingbox->planes[ 4 ].dist = maxs[ 1 ];
    cm->hull_boundingbox->planes[ 5 ].dist = -maxs[ 1 ];
    cm->hull_boundingbox->planes[ 6 ].dist = mins[ 1 ];
    cm->hull_boundingbox->planes[ 7 ].dist = -mins[ 1 ];
    cm->hull_boundingbox->planes[ 8 ].dist = maxs[ 2 ];
    cm->hull_boundingbox->planes[ 9 ].dist = -maxs[ 2 ];
    cm->hull_boundingbox->planes[ 10 ].dist = mins[ 2 ];
    cm->hull_boundingbox->planes[ 11 ].dist = -mins[ 2 ];

    // Return boundingbox' headnode pointer.
    return cm->hull_boundingbox->headnode;
}
