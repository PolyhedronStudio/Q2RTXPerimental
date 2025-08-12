/**
*
*
*   Collision Model: Box Sweep and Tracing routines.
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

//! Needed here.
extern cvar_t *map_allsolid_bug;

//! Uncomment for enabling second best hit plane tracing results.
#define SECOND_PLANE_TRACEXX

/**
*
*
*   Box Sweep Tracing:
*
*
**/
// 1/32 epsilon to keep floating point happy
static constexpr float DIST_EPSILON = 0.03125f;

static vec3_t   trace_start, trace_end;
static vec3_t   trace_offsets[ 8 ];
static vec3_t   trace_extents;

static cm_trace_t *trace_trace;
static uint32_t trace_contents;
static cm_material_t *trace_material;
static cm_surface_t trace_surface;
static bool     trace_ispoint;      // optimized case

/**
*   @brief
**/
static void CM_ClipBoxToBrush( const vec3_t p1, const vec3_t p2, cm_trace_t *trace, mbrush_t *brush ) {
    if ( !brush->numsides )
        return;


    // The actual plane.
    cm_plane_t *plane = nullptr;

    // lead side(s).
    mbrushside_t *leadside[ 2 ] = { nullptr, nullptr };
    // Clip plane.
    cm_plane_t *clipplane[ 2 ] = { nullptr, nullptr, };

    // Distances.
    float d1 = 0.f;
    float d2 = 0.f;
    float dist = 0.f;

    // Fractions.
    float f = 0.f; // Actual fraction.
    float enterfrac[ 2 ] = { -1, -1 };
    float leavefrac = 1;

    // Trace escaped the brush.
    bool getout = false;
	// Trace started outside the brush.
    bool startout = false;

    //
    // Compares the trace against all planes of the brush,
    // Seeks the latest time the trace crosses a plane towards the Interior,
    // and the earliest time the trace crosses a plane towards the Exterior.
    //
    mbrushside_t *side = brush->firstbrushside;
    mbrushside_t *endside = side + brush->numsides;
    int32_t i = 0;
    for ( i = 0, side = side; side < endside; i++, side++ ) {
        plane = side->plane;

        // FIXME: special case for axial
        if ( !trace_ispoint ) {
            // general box case
            // push the plane out apropriately for mins/maxs
            dist = DotProduct( trace_offsets[ plane->signbits ], plane->normal );
            dist = plane->dist - dist;
        } else {
            // special point case
            dist = plane->dist;
        }

        d1 = DotProduct( p1, plane->normal ) - dist;
        d2 = DotProduct( p2, plane->normal ) - dist;

        if ( d2 > 0 ) {
            getout = true; // endpoint is not in solid
        }
        if ( d1 > 0 ) {
            startout = true;
        }

        // if completely in front of face, no intersection with the entire brush
        // Paril: Q3A fix
        if ( d1 > 0 && ( d2 >= DIST_EPSILON || d2 >= d1 ) ) {
            // Paril
            return;
        }

        // if it doesn't cross the plane, the plane isn't relevent
        if ( d1 <= 0 && d2 <= 0 ) {
            continue;
        }

        // Crosses face.
        if ( d1 > d2 ) { // Enter.
            f = ( d1 - DIST_EPSILON ) / ( d1 - d2 );
            if ( f < 0 ) {
                f = 0;
			}
            if ( f > enterfrac[ 0 ] ) {
                enterfrac[ 0 ] = f;
                clipplane[ 0 ] = plane;
                leadside[ 0 ] = side;
            #ifdef SECOND_PLANE_TRACE
            } else if ( f > enterfrac[ 1 ] ) {
                enterfrac[ 1 ] = f;
                clipplane[ 1 ] = plane;
                leadside[ 1 ] = side;
            #endif
            }
        // KEX
        } else { // Leave.
            f = ( d1 + DIST_EPSILON ) / ( d1 - d2 );
            if ( f > 1 ) {
                f = 1;
			}
            // Paril
            if ( f < leavefrac ) {
                leavefrac = f;
            }
        }
    }

    if ( !startout ) {
        // original point was inside brush
        trace->startsolid = true;
        if ( !getout ) {
            trace->allsolid = true;
            //if ( !map_allsolid_bug->integer ) {
                // original Q2 didn't set these
                trace->fraction = 0;
                trace->contents = static_cast<cm_contents_t>( brush->contents );
                trace->material = nullptr;
            //}
        }
        return;
    }
    if ( enterfrac[ 0 ] < leavefrac ) {
        if ( enterfrac[ 0 ] > -1 && enterfrac[ 0 ] < trace->fraction ) {
            if ( enterfrac[ 0 ] < 0 ) {
                enterfrac[ 0 ] = 0;
			}
            trace->fraction = enterfrac[ 0 ];
            trace->plane = *clipplane[ 0 ];
            trace->surface = &( leadside[ 0 ]->texinfo->c );
            trace->contents = static_cast<cm_contents_t>( brush->contents );
            trace->material = trace->surface->material;
            #ifdef SECOND_PLANE_TRACE
            if ( leadside[ 1 ] ) {
                trace->plane2 = *clipplane[ 1 ];
                trace->surface2 = &( leadside[ 1 ]->texinfo->c );
                trace->material2 = trace->surface2->material;
            }
            #else
            trace->plane2 = *clipplane[ 0 ];
            trace->surface2 = &( leadside[ 0 ]->texinfo->c );
            trace->material2 = trace->surface->material;
            //trace->plane2 = {};
            //trace->surface2 = nullptr;
            #endif
        }
    }
}

/**
*   @brief
**/
static void CM_TestBoxInBrush( const vec3_t p1, cm_trace_t *trace, mbrush_t *brush ) {
    int         i;
    cm_plane_t *plane;
    float       dist;
    float       d1;
    mbrushside_t *side;

    if ( !brush->numsides )
        return;

    side = brush->firstbrushside;
    for ( i = 0; i < brush->numsides; i++, side++ ) {
        plane = side->plane;

        // FIXME: special case for axial
        // general box case
        // push the plane out apropriately for mins/maxs
        dist = DotProduct( trace_offsets[ plane->signbits ], plane->normal );
        dist = plane->dist - dist;

        d1 = DotProduct( p1, plane->normal ) - dist;

        // if completely in front of face, no intersection
        if ( d1 > 0 )
            return;
    }

    // inside this brush
    trace->startsolid = trace->allsolid = true;
    trace->fraction = 0;
    trace->contents = static_cast<cm_contents_t>( brush->contents );
    //if ( trace->material ) {
    trace->material = nullptr;
    //}
}

/**
*   @brief
**/
static void CM_TraceToLeaf( cm_t *cm, mleaf_t *leaf ) {
    int         k;
    mbrush_t *b, **leafbrush;

    if ( !( leaf->contents & trace_contents ) )
        return;
    // trace line against all brushes in the leaf
    leafbrush = leaf->firstleafbrush;
    for ( k = 0; k < leaf->numleafbrushes; k++, leafbrush++ ) {
        b = *leafbrush;
        if ( b->checkcount == cm->checkCount )
            continue;   // already checked this brush in another leaf
        b->checkcount = cm->checkCount;

        if ( !( b->contents & trace_contents ) )
            continue;
        CM_ClipBoxToBrush( trace_start, trace_end, trace_trace, b );
        if ( !trace_trace->fraction )
            return;
    }
}

/**
*   @brief
**/
static void CM_TestInLeaf( cm_t *cm, mleaf_t *leaf ) {
    int         k;
    mbrush_t *b, **leafbrush;

    if ( !( leaf->contents & trace_contents ) )
        return;
    // trace line against all brushes in the leaf
    leafbrush = leaf->firstleafbrush;
    for ( k = 0; k < leaf->numleafbrushes; k++, leafbrush++ ) {
        b = *leafbrush;
        if ( b->checkcount == cm->checkCount )
            continue;   // already checked this brush in another leaf
        b->checkcount = cm->checkCount;

        if ( !( b->contents & trace_contents ) )
            continue;
        CM_TestBoxInBrush( trace_start, trace_trace, b );
        if ( !trace_trace->fraction )
            return;
    }
}

/**
*   @brief
**/
static void CM_RecursiveHullCheck( cm_t *cm, mnode_t *node, float p1f, float p2f, const vec3_t p1, const vec3_t p2 ) {
    cm_plane_t *plane;
    float   t1, t2, offset;
    float   frac, frac2;
    float   idist;
    vec3_t  mid;
    int     side;
    float   midf;

    if ( trace_trace->fraction <= p1f )
        return;     // already hit something nearer

recheck:
    // if plane is NULL, we are in a leaf node
    plane = node->plane;
    if ( !plane ) {
        CM_TraceToLeaf( cm, (mleaf_t *)node );
        return;
    }

    //
    // find the point distances to the seperating plane
    // and the offset for the size of the box
    //
    if ( plane->type < 3 ) {
        t1 = p1[ plane->type ] - plane->dist;
        t2 = p2[ plane->type ] - plane->dist;
        offset = trace_extents[ plane->type ];
    } else {
        t1 = PlaneDiff( p1, plane );
        t2 = PlaneDiff( p2, plane );
        if ( trace_ispoint )
            offset = 0;
        else
            offset = fabsf( trace_extents[ 0 ] * plane->normal[ 0 ] ) +
            fabsf( trace_extents[ 1 ] * plane->normal[ 1 ] ) +
            fabsf( trace_extents[ 2 ] * plane->normal[ 2 ] );
    }

    // see which sides we need to consider
    if ( t1 >= offset && t2 >= offset ) {
        node = node->children[ 0 ];
        goto recheck;
    }
    if ( t1 < -offset && t2 < -offset ) {
        node = node->children[ 1 ];
        goto recheck;
    }

    // put the crosspoint DIST_EPSILON pixels on the near side
    if ( t1 < t2 ) {
        idist = 1.0f / ( t1 - t2 );
        side = 1;
        frac2 = ( t1 + offset + DIST_EPSILON ) * idist;
        frac = ( t1 - offset + DIST_EPSILON ) * idist;
    } else if ( t1 > t2 ) {
        idist = 1.0f / ( t1 - t2 );
        side = 0;
        frac2 = ( t1 - offset - DIST_EPSILON ) * idist;
        frac = ( t1 + offset + DIST_EPSILON ) * idist;
    } else {
        side = 0;
        frac = 1;
        frac2 = 0;
    }

    // move up to the node
    midf = p1f + ( p2f - p1f ) * std::clamp( frac, 0.f, 1.f );
    LerpVector( p1, p2, frac, mid );

    CM_RecursiveHullCheck( cm, node->children[ side ], p1f, midf, p1, mid );

    // go past the node
    midf = p1f + ( p2f - p1f ) * std::clamp( frac2, 0.f, 1.f );
    LerpVector( p1, p2, frac2, mid );

    CM_RecursiveHullCheck( cm, node->children[ side ^ 1 ], midf, p2f, mid, p2 );
}



/**
*
*
*   BoundingBox Shape Tracing:
*
*
**/
/**
*   @brief
**/
void CM_BoxTrace( cm_t *cm, cm_trace_t *trace,
    const vec3_t start, const vec3_t end,
    const vec3_t mins, const vec3_t maxs,
    mnode_t *headnode, const cm_contents_t brushmask ) {

    // Validate mins and maxs, ohterwise assign them vec3_origin.
    if ( !mins ) {
        mins = vec3_origin;
    }
    if ( !maxs ) {
        maxs = vec3_origin;
    }

    const vec_t *bounds[ 2 ] = { mins, maxs };
    int i, j;

    cm->checkCount++;       // for multi-check avoidance

    // fill in a default trace
    trace_trace = trace;
    //trace_trace = {};
    memset( trace_trace, 0, sizeof( *trace_trace ) );
    trace_trace->fraction = 1;
    trace_trace->surface = &nulltexinfo.c;
    trace_trace->material = &cm_default_material;

    if ( !headnode ) {
        return;
    }

    trace_contents = brushmask;
    VectorCopy( start, trace_start );
    VectorCopy( end, trace_end );
    for ( i = 0; i < 8; i++ )
        for ( j = 0; j < 3; j++ )
            trace_offsets[ i ][ j ] = bounds[ ( i >> j ) & 1 ][ j ];

    //
    // check for position test special case
    //
    if ( VectorCompare( start, end ) ) {
        mleaf_t *leafs[ 1024 ];
        int         numleafs;
        vec3_t      c1, c2;

        VectorAdd( start, mins, c1 );
        VectorAdd( start, maxs, c2 );
        for ( i = 0; i < 3; i++ ) {
            c1[ i ] -= 1;
            c2[ i ] += 1;
        }

        numleafs = CM_BoxLeafs_headnode( cm, c1, c2, leafs, q_countof( leafs ), headnode, NULL );
        for ( i = 0; i < numleafs; i++ ) {
            CM_TestInLeaf( cm, leafs[ i ] );
            if ( trace_trace->allsolid )
                break;
        }
        VectorCopy( start, trace_trace->endpos );
        return;
    }

    //
    // check for point special case
    //
    if ( VectorEmpty( mins ) && VectorEmpty( maxs ) ) {
        trace_ispoint = true;
        VectorClear( trace_extents );
    } else {
        trace_ispoint = false;
        trace_extents[ 0 ] = std::max( -mins[ 0 ], maxs[ 0 ] );
        trace_extents[ 1 ] = std::max( -mins[ 1 ], maxs[ 1 ] );
        trace_extents[ 2 ] = std::max( -mins[ 2 ], maxs[ 2 ] );
    }

    //
    // general sweeping through world
    //
    CM_RecursiveHullCheck( cm, headnode, 0, 1, start, end );

    if ( trace_trace->fraction == 1 )
        VectorCopy( end, trace_trace->endpos );
    else
        LerpVector( start, end, trace_trace->fraction, trace_trace->endpos );
}
/**
*   @brief  Handles offseting and rotation of the end points for moving and
*           rotating entities.
**/
void CM_TransformedBoxTrace( cm_t *cm, cm_trace_t *trace,
    const vec3_t start, const vec3_t end,
    const vec3_t mins, const vec3_t maxs,
    mnode_t *headnode, const cm_contents_t brushmask,
    const vec3_t origin, const vec3_t angles ) {
    vec3_t      start_l, end_l;
    vec3_t      axis[ 3 ];

    bool isBoxHull = ( headnode == cm->hull_boundingbox->headnode );
    bool isOctagonHull = ( headnode == cm->hull_octagonbox->headnode );
    bool rotated = ( ( headnode != cm->hull_boundingbox->headnode ) &&
        ( headnode != cm->hull_octagonbox->headnode ) &&
        !VectorEmpty( angles )
        );

    //if ( isOctagonHull ) {
    //    // cylinder offset
    //    VectorSubtract( start, cm->hull_octagonbox->cylinder_offset, start_l );
    //    VectorSubtract( end, cm->hull_octagonbox->cylinder_offset, end_l );
    //} else {
    VectorCopy( start, start_l );
    VectorCopy( end, end_l );
    //}

    // subtract origin offset
    VectorSubtract( start_l, origin, start_l );
    VectorSubtract( end_l, origin, end_l );

    // rotate start and end into the models frame of reference
    if ( rotated ) {
        AnglesToAxis( angles, axis );
        RotatePoint( start_l, axis );
        RotatePoint( end_l, axis );
    }

    // sweep the box through the model
    CM_BoxTrace( cm, trace, start_l, end_l, mins, maxs, headnode, brushmask );

    // rotate plane normal into the worlds frame of reference
    if ( rotated && trace->fraction != 1.0f ) {
        TransposeAxis( axis );
        RotatePoint( trace->plane.normal, axis );

        // FIXME: offset plane distance?
        trace->plane.dist += DotProduct( trace->plane.normal, origin );
    }

    LerpVector( start, end, trace->fraction, trace->endpos );
}
/**
*   @brief
**/
void CM_ClipEntity( cm_t *cm, cm_trace_t *dst, const cm_trace_t *src, const int32_t entityNumber ) {
    dst->allsolid |= src->allsolid;
    dst->startsolid |= src->startsolid;
    if ( src->fraction < dst->fraction ) {
        dst->fraction = src->fraction;
        dst->entityNumber = entityNumber;
        VectorCopy( src->endpos, dst->endpos );

        dst->plane = src->plane;
        dst->surface = src->surface;
        dst->material = src->material;
        dst->contents = ( dst->contents | src->contents );

        dst->plane2 = src->plane2;
        dst->surface2 = src->surface2;
        dst->material2 = src->material2;
    }
}
