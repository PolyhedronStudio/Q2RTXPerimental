/**
*
*
*   Collision Model: Point Contents
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
*   @return Pointer to the leaf matching coordinate 'p'.
**/
mleaf_t *CM_PointLeaf( cm_t *cm, const vec3_t p ) {
    if ( !cm->cache ) {
        return &cm->nullLeaf;       // server may call this without map loaded
    }
    return BSP_PointLeaf( cm->cache->nodes, p );
}

/**
*   @return An ORed contents mask
**/
const cm_contents_t CM_PointContents( cm_t *cm, const vec3_t p, mnode_t *headnode ) {
    if ( !headnode ) {
        return CONTENTS_NONE;
    }
    return static_cast<cm_contents_t>( BSP_PointLeaf( headnode, p )->contents );
}

/**
*   @brief  Handles offseting and rotation of the end points for moving and
*           rotating entities
**/
const cm_contents_t  CM_TransformedPointContents( cm_t *cm, const vec3_t p, mnode_t *headnode, const vec3_t origin, const vec3_t angles ) {
    vec3_t      p_l;
    vec3_t      axis[ 3 ];

    if ( !headnode ) {
        return CONTENTS_NONE;
    }

    bool isBoxHull = ( headnode == cm->hull_boundingbox->headnode );
    bool isOctagonHull = ( headnode == cm->hull_octagonbox->headnode );
    bool rotated = ( ( headnode != cm->hull_boundingbox->headnode ) &&
        ( headnode != cm->hull_octagonbox->headnode ) &&
        !VectorEmpty( angles )
    );

    // subtract origin offset
    VectorSubtract( p, origin, p_l );

    // rotate start and end into the models frame of reference
    if ( rotated ) {
        AnglesToAxis( angles, axis );
        RotatePoint( p_l, axis );
    }

    return static_cast<cm_contents_t>( BSP_PointLeaf( headnode, p_l )->contents );
}