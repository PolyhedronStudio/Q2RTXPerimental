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
        //if ( i != 5 ) {
            clipNode->children[ side ^ 1 ] = &cm->hull_octagonbox->nodes[ i + 1 ];
        //} else {
        //    clipNode->children[ side ^ 1 ] = (mnode_t *)&cm->hull_octagonbox->leaf;
        //}

        // planes
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ i * 2 ];
        plane->type = i >> 1;
        plane->normal[ i >> 1 ] = 1;
        //SetPlaneType( plane );
        //SetPlaneSignbits( plane );

        plane = &cm->hull_octagonbox->planes[ i * 2 + 1 ];
        plane->type = 3 + ( i >> 1 );
        plane->signbits = 1 << ( i >> 1 );
        plane->normal[ i >> 1 ] = -1;
        //SetPlaneType( plane );
        //SetPlaneSignbits( plane );
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

        // Planes
        cm_plane_t *plane = &cm->hull_octagonbox->planes[ i * 2 ];
        plane->type = 3;
        VectorCopy( oct_dirs[ i - 6 ], plane->normal );
        //SetPlaneType( plane );
        SetPlaneSignbits( plane );

        plane = &cm->hull_octagonbox->planes[ i * 2 + 1 ];
        plane->type = 3 + ( i >> 1 );
        Vector3 negatedDir = QM_Vector3Negate( oct_dirs[ i - 6 ] );
        VectorCopy( negatedDir, plane->normal );//VectorCopy( oct_dirs[ i - 6 ], plane->normal );
        //SetPlaneType( plane );
        SetPlaneSignbits( plane );
    }
}

/**
*   @brief  Utility function to complement CM_HeadnodeForOctagon with.
**/
static inline const float CalculateOctagonPlaneDist( cm_plane_t &plane, const Vector3 &mins, const Vector3 &maxs, const bool negate = false ) {
    if ( negate == true ) {
        return QM_Vector3DotProduct( plane.normal, { ( plane.signbits & 1 ) ? -mins[ 0 ] : -maxs[ 0 ], ( plane.signbits & 2 ) ? -mins[ 1 ] : -maxs[ 1 ], ( plane.signbits & 4 ) ? -mins[ 2 ] : -maxs[ 2 ] } );//-d;//d;
    } else {
        return QM_Vector3DotProduct( plane.normal, { ( plane.signbits & 1 ) ? mins[ 0 ] : maxs[ 0 ], ( plane.signbits & 2 ) ? mins[ 1 ] : maxs[ 1 ], ( plane.signbits & 4 ) ? mins[ 2 ] : maxs[ 2 ] } );//-d;//d;
    }
}

/**
*   @brief  To keep everything totally uniform, Bounding 'Octagon' Boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' octagon box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
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
    cm->hull_octagonbox->planes[ 0 ].dist = maxs[ 0 ];
    cm->hull_octagonbox->planes[ 1 ].dist = -maxs[ 0 ];
    cm->hull_octagonbox->planes[ 2 ].dist = mins[ 0 ];
    cm->hull_octagonbox->planes[ 3 ].dist = -mins[ 0 ];
    cm->hull_octagonbox->planes[ 4 ].dist = maxs[ 1 ];
    cm->hull_octagonbox->planes[ 5 ].dist = -maxs[ 1 ];
    cm->hull_octagonbox->planes[ 6 ].dist = mins[ 1 ];
    cm->hull_octagonbox->planes[ 7 ].dist = -mins[ 1 ];
    cm->hull_octagonbox->planes[ 8 ].dist = maxs[ 2 ];
    cm->hull_octagonbox->planes[ 9 ].dist = -maxs[ 2 ];
    cm->hull_octagonbox->planes[ 10 ].dist = mins[ 2 ];
    cm->hull_octagonbox->planes[ 11 ].dist = -mins[ 2 ];

    // Cylindrical offset.
    for ( int32_t i = 0; i < 3; i++ ) {
        cm->hull_octagonbox->cylinder_offset[ i ] = ( mins[ i ] + maxs[ i ] ) * 0.5;
    }

    // Determine half size.
    Vector3 halfSize[ 2 ] = {
        QM_Vector3Scale( mins, 0.5f ),
        QM_Vector3Scale( maxs, 0.5f ),
    };


    //// Calculate half size for mins and maxs.
    //const Vector3 offset = QM_Vector3Scale( Vector3( mins ) +  Vector3( maxs ), 0.5f );

    //const Vector3 /*centerSize*/halfSize[ 2 ] = {
    //    mins - offset, // Not sure why but this --> mins - offset, // was somehow not working well.
    //    maxs - offset, // Not sure why but this --> maxs - offset, // was somehow not working well.
    //};

    // Calculate actual up to scale normals for the non axial planes.
    const float a = maxs[ 0 ];//halfSize[ 1 ].x; // Half-X
    const float b = maxs[ 1 ];//halfSize[ 1 ].y; // Half-Y

    float dist = sqrtf( a * a + b * b ); // Hypothenuse

    float cosa = a / dist;
    float sina = b / dist;

    // Calculate and set distances for each non axial plane.
    // Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 12 ].normal, cosa, sina, 0.f );
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 12 ] );
    cm->hull_octagonbox->planes[ 12 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 12 ], halfSize[ 0 ], halfSize[ 1 ] );
    //cm->hull_octagonbox->planes[12].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[12], mins, maxs );
    // (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 13 ].normal, cosa, sina, 0.f );
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 13 ] );
    cm->hull_octagonbox->planes[ 13 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 13 ], halfSize[ 0 ], halfSize[ 1 ], true );//-CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 13 ], halfSize[ 0 ], halfSize[ 1 ], true );
    //cm->hull_octagonbox->planes[13].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[13], mins, maxs, true);

    // Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 14 ].normal, -cosa, sina, 0.f );
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 14 ] );
    cm->hull_octagonbox->planes[ 14 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 14 ], halfSize[ 0 ], halfSize[ 1 ], true );//-CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 14 ], halfSize[ 0 ], halfSize[ 1 ], true );
    //cm->hull_octagonbox->planes[14].dist     = -CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[14], mins, maxs, true);
    // (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 15 ].normal, -cosa, sina, 0.f );
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 15 ] );
    cm->hull_octagonbox->planes[ 15 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 15 ], halfSize[ 0 ], halfSize[ 1 ] );
    //cm->hull_octagonbox->planes[15].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[15], mins, maxs);

    // Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 16 ].normal, -cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 16 ].normal = vec3_t{ -cosa, -sina, 0.f };
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 16 ] );
    cm->hull_octagonbox->planes[ 16 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 16 ], halfSize[ 0 ], halfSize[ 1 ] );
    //cm->hull_octagonbox->planes[16].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[16], mins, maxs);
    // (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 17 ].normal, -cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 17 ].normal = vec3_t{ -cosa, -sina, 0.f };
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 17 ] );
    cm->hull_octagonbox->planes[ 17 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 17 ], halfSize[ 0 ], halfSize[ 1 ], true );//-CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 17 ], halfSize[ 0 ], halfSize[ 1 ], true );
    //cm->hull_octagonbox->planes[17].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[17], mins, maxs, true);

    // Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 18 ].normal, cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 18 ].normal = vec3_t{ cosa, -sina, 0.f };
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 18 ] );
    cm->hull_octagonbox->planes[ 18 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 18 ], halfSize[ 0 ], halfSize[ 1 ], true );//-CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 18 ], halfSize[ 0 ], halfSize[ 1 ], true );
    //cm->hull_octagonbox->planes[18].dist     = -CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[18], mins, maxs, true);
    // (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 19 ].normal, cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 19 ].normal = vec3_t{ cosa, -sina, 0.f };
    SetPlaneSignbits( &cm->hull_octagonbox->planes[ 19 ] );
    cm->hull_octagonbox->planes[ 19 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 19 ], halfSize[ 0 ], halfSize[ 1 ] );
    //cm->hull_octagonbox->planes[19].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[19], mins, maxs);

    // Works except for 2 side planes it seems??
    //// Calculate and set distances for each non axial plane.
    //// Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 12 ].normal, cosa, sina, 0.f );
    //cm->hull_octagonbox->planes[ 12 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 12 ], halfSize[ 0 ], halfSize[ 1 ] );
    ////cm->hull_octagonbox->planes[12].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[12], mins, maxs );
    //// (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 13 ].normal, cosa, sina, 0.f );
    //cm->hull_octagonbox->planes[ 13 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 13 ], halfSize[ 0 ], halfSize[ 1 ], true );
    ////cm->hull_octagonbox->planes[13].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[13], mins, maxs, true);

    //// Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 14 ].normal, -cosa, sina, 0.f );
    //cm->hull_octagonbox->planes[ 14 ].dist = -CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 14 ], halfSize[ 0 ], halfSize[ 1 ], true );
    ////cm->hull_octagonbox->planes[14].dist     = -CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[14], mins, maxs, true);
    //// (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 15 ].normal, -cosa, sina, 0.f );
    //cm->hull_octagonbox->planes[ 15 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 15 ], halfSize[ 0 ], halfSize[ 1 ] );
    ////cm->hull_octagonbox->planes[15].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[15], mins, maxs);

    //// Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 16 ].normal, -cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 16 ].normal = vec3_t{ -cosa, -sina, 0.f };
    //cm->hull_octagonbox->planes[ 16 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 16 ], halfSize[ 0 ], halfSize[ 1 ] );
    ////cm->hull_octagonbox->planes[16].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[16], mins, maxs);
    //// (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 17 ].normal, -cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 17 ].normal = vec3_t{ -cosa, -sina, 0.f };
    //cm->hull_octagonbox->planes[ 17 ].dist = -CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 17 ], halfSize[ 0 ], halfSize[ 1 ], true );
    ////cm->hull_octagonbox->planes[17].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[17], mins, maxs, true);

    //// Outer Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 18 ].normal, cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 18 ].normal = vec3_t{ cosa, -sina, 0.f };
    //cm->hull_octagonbox->planes[ 18 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 18 ], halfSize[ 0 ], halfSize[ 1 ], true );
    ////cm->hull_octagonbox->planes[18].dist     = -CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[18], mins, maxs, true);
    //// (Inner-)Negated Plane:
    //VectorSet( cm->hull_octagonbox->planes[ 19 ].normal, cosa, -sina, 0.f ); //cm->hull_octagonbox->planes[ 19 ].normal = vec3_t{ cosa, -sina, 0.f };
    //cm->hull_octagonbox->planes[ 19 ].dist = CalculateOctagonPlaneDist( cm->hull_octagonbox->planes[ 19 ], halfSize[ 0 ], halfSize[ 1 ] );
    ////cm->hull_octagonbox->planes[19].dist     = CalculateOctagonPlaneDist(cm->hull_octagonbox->planes[19], mins, maxs);


    // Return octagonbox' headnode pointer.
    return cm->hull_octagonbox->headnode;
}
