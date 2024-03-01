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



//static cplane_t cm->hull_boundingbox.planes[ 12 ];
//static mnode_t  cm->hull_boundingbox.nodes[ 6 ];
//mnode_t *cm->hull_boundingbox.headnode;
//static mbrush_t cm->hull_boundingbox.brush;
//static mbrush_t *cm->hull_boundingbox.leafbrush;
//static mbrushside_t cm->hull_boundingbox.brushsides[ 6 ];
//static mleaf_t  cm->hull_boundingbox.leaf;
//static mleaf_t  cm->hull_boundingbox.emptyleaf;

/**
*   Set up the planes and nodes so that the six floats of a bounding box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitBoxHull( cm_t *cm ) {
    int         i;
    int         side;
    mnode_t *c;
    cplane_t *p;
    mbrushside_t *s;

    cm->hull_boundingbox.headnode = &cm->hull_boundingbox.nodes[ 0 ];

    cm->hull_boundingbox.brush.numsides = 6;
    cm->hull_boundingbox.brush.firstbrushside = &cm->hull_boundingbox.brushsides[ 0 ];
    cm->hull_boundingbox.brush.contents = CONTENTS_MONSTER;

    cm->hull_boundingbox.leaf.contents = CONTENTS_MONSTER;
    cm->hull_boundingbox.leaf.firstleafbrush = &cm->hull_boundingbox.leafbrush;
    cm->hull_boundingbox.leaf.numleafbrushes = 1;

    cm->hull_boundingbox.leafbrush = &cm->hull_boundingbox.brush;

    for ( i = 0; i < 6; i++ ) {
        side = i & 1;

        // brush sides
        s = &cm->hull_boundingbox.brushsides[ i ];
        s->plane = &cm->hull_boundingbox.planes[ i * 2 + side ];
        s->texinfo = &nulltexinfo;

        // nodes
        c = &cm->hull_boundingbox.nodes[ i ];
        c->plane = &cm->hull_boundingbox.planes[ i * 2 ];
        c->children[ side ] = (mnode_t *)&cm->hull_boundingbox.emptyleaf;
        if ( i != 5 )
            c->children[ side ^ 1 ] = &cm->hull_boundingbox.nodes[ i + 1 ];
        else
            c->children[ side ^ 1 ] = (mnode_t *)&cm->hull_boundingbox.leaf;

        // planes
        p = &cm->hull_boundingbox.planes[ i * 2 ];
        p->type = i >> 1;
        p->normal[ i >> 1 ] = 1;

        p = &cm->hull_boundingbox.planes[ i * 2 + 1 ];
        p->type = 3 + ( i >> 1 );
        p->signbits = 1 << ( i >> 1 );
        p->normal[ i >> 1 ] = -1;
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
    // Setup its bounding boxes.
    VectorCopy( mins, cm->hull_boundingbox.headnode->mins );
    VectorCopy( maxs, cm->hull_boundingbox.headnode->maxs );
    VectorCopy( mins, cm->hull_boundingbox.leaf.mins );
    VectorCopy( maxs, cm->hull_boundingbox.leaf.maxs );

    // Setup planes.
    cm->hull_boundingbox.planes[ 0 ].dist = maxs[ 0 ];
    cm->hull_boundingbox.planes[ 1 ].dist = -maxs[ 0 ];
    cm->hull_boundingbox.planes[ 2 ].dist = mins[ 0 ];
    cm->hull_boundingbox.planes[ 3 ].dist = -mins[ 0 ];
    cm->hull_boundingbox.planes[ 4 ].dist = maxs[ 1 ];
    cm->hull_boundingbox.planes[ 5 ].dist = -maxs[ 1 ];
    cm->hull_boundingbox.planes[ 6 ].dist = mins[ 1 ];
    cm->hull_boundingbox.planes[ 7 ].dist = -mins[ 1 ];
    cm->hull_boundingbox.planes[ 8 ].dist = maxs[ 2 ];
    cm->hull_boundingbox.planes[ 9 ].dist = -maxs[ 2 ];
    cm->hull_boundingbox.planes[ 10 ].dist = mins[ 2 ];
    cm->hull_boundingbox.planes[ 11 ].dist = -mins[ 2 ];

    // Setup to CONTENTS_SOLID in case of no contents being passed in.
    if ( !contents ) {
        cm->hull_boundingbox.leaf.contents = cm->hull_boundingbox.brush.contents = CONTENTS_MONSTER;
    } else {
        cm->hull_boundingbox.leaf.contents = cm->hull_boundingbox.brush.contents = contents;
    }

    return cm->hull_boundingbox.headnode;
}
