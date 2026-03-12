/**
*
*
*   Collision Model: Box Sweep and Tracing routines.
*
*
**/
#include "shared/shared.h"
#include "common/collisionmodel.h"

#include <mutex>
#include <unordered_set>

//! Uncomment for enabling second best hit plane tracing results.
//#define SECOND_PLANE_TRACE

//! Uncomment to debug box tracing.
//#define BOX_TRACE_DEBUG

#ifdef BOX_TRACE_DEBUG
    #define CM_DebugPrint( ... ) Com_DPrintf( __VA_ARGS__ )
#else
    #define CM_DebugPrint( ... )
#endif

#define LerpVectorDP(a,b,c,d) \
    ((d)[0]=( double )((a)[0])+(( double )(c))*(( double )((b)[0])-( double )((a)[0])), \
     (d)[1]=( double )((a)[1])+(( double )(c))*(( double )((b)[1])-( double )((a)[1])), \
     (d)[2]=( double )((a)[2])+(( double )(c))*(( double )((b)[2])-( double )((a)[2])))

#define DotProductDP(x,y)         (( double )((x)[0])*( double )((y)[0])+( double )((x)[1])*( double )((y)[1])+( double )((x)[2])*( double )((y)[2]))
#define PlaneDiffDP(v,p)   (DotProductDP(v,(p)->normal)-(double)((p)->dist))

/**
*
*
*   Box Sweep Tracing:
*
*
**/
// 1/32 epsilon to keep floating point happy
static constexpr double DIST_EPSILON = 0.03125;

/**
*	@brief	For thread-safety, we use a structure that is locally declared in a Trace function after which
*			they are copied out to the caller's cm_trace_t. This is because the tracing implementation below uses shared file-scope scratch state and is therefore not re-entrant.
**/
struct cm_trace_reantrant_state_t {
	// Actual trace state, this	copied out to the caller's cm_trace_t after the trace completes. (Making it re-entrant.)
	cm_trace_t trResult;

	//! If true, the trace is an optimized point trace.
	bool trIsPoint = false;

	//! The start and end points of the trace.
	Vector3  trStart = {}, trEnd = { };
	//! Offsets for the 8 corners of the box being traced. Used for passing box size information to the trace implementation.
	Vector3  trOffsets[ 8 ] = {};
	//!	The extents of the box being traced. This is set by the caller before the trace and is used for passing box size information to the trace implementation.
	Vector3  trExtents = {};

	//!	A bitfield of the contents flags of the surface that was hit by the trace. This is set by the trace implementation when a surface is hit, and is used for passing contents information back to the caller.
	cm_contents_t trContents = CONTENTS_NONE;

	/**
	*	Technically storing these pointers would be a non-reeantrant operation, but since the trace 
	*	implementation is single-threaded and these are only set by the trace implementation, when a
	*	surface is hit, this should be safe. These are used ONLY for passing material and surface 
	*	information back to the caller.
	**/
	//! A pointer to the material of the surface that was hit by the trace. This is set by the trace implementation when a surface is hit, and is used for passing material information back to the caller.
	cm_material_t *trMaterial = &cm_default_material;
	//! A copy of the surface that was hit by the trace. This is set by the trace implementation when a surface is hit, and is used for passing surface information back to the caller.
	cm_surface_t *trSurface = &nulltexinfo.c;
};

// Old non-reeantrant file-scope state used by the trace implementation. This is copied into a cm_trace_reeantrant
#if 0
static Vector3  trace_start, trace_end;
static Vector3  trace_offsets[ 8 ];
static Vector3  trace_extents;

static cm_trace_t *trace_trace;
static uint32_t trace_contents;
static cm_material_t *trace_material;
static cm_surface_t trace_surface;
static bool     trace_ispoint;      // optimized case
#endif

/**
*   @brief  Retrieve the current thread's reusable visited-brush table for trace deduplication.
*   @param  cm    Collision model whose BSP brush count is used to size the table conservatively.
*   @return Pointer to the cleared thread-local visited-brush table for the active trace.
*   @note   Keeping this container thread-local avoids cross-thread races while also reusing bucket capacity
*           across traces so we do not pay repeated heap growth costs every call.
**/
static std::unordered_set<const mbrush_t *> *CM_BeginVisitedBrushSet( const cm_t *cm ) {
    //! Thread-local visited-brush table reused across traces on this worker thread.
    static thread_local std::unordered_set<const mbrush_t *> visitedBrushes = {};

    /**
    *   Clear any brushes remembered from the previous trace on this thread before starting a new one.
    **/
    visitedBrushes.clear();

    /**
    *   Grow the retained bucket capacity only when this thread encounters a larger BSP than before.
    **/
    if ( cm && cm->cache && cm->cache->numbrushes > 0 ) {
        const size_t reserveCount = ( size_t )std::min( cm->cache->numbrushes, 512 );
        if ( visitedBrushes.bucket_count() < reserveCount ) {
            visitedBrushes.reserve( reserveCount );
        }
    }

    return &visitedBrushes;
}

/**
*   @brief  Try to mark a brush as visited for the active trace.
*   @param  visitedBrushes    Active thread-local visited-brush table for the current trace.
*   @param  brush             Brush about to be tested.
*   @return True when the brush was first seen by this trace, false when it was already processed from another leaf.
**/
static bool CM_TryVisitBrush( std::unordered_set<const mbrush_t *> *visitedBrushes, const mbrush_t *brush ) {
    /**
    *   Sanity checks: tolerate missing visited-table storage by falling back to always testing the brush.
    **/
    if ( !visitedBrushes || !brush ) {
        return true;
    }

    /**
    *   Insert the brush pointer into the per-trace visited set and report whether it was new.
    **/
    return visitedBrushes->insert( brush ).second;
}

/**
*   @brief
**/
static void CM_ClipBoxToBrush( const Vector3 &p1, const Vector3 &p2, cm_trace_reantrant_state_t *reantrantState, mbrush_t *brush ) {
    if ( !brush->numsides )
        return;


    // The actual plane.
    cm_plane_t *plane = nullptr;

    // lead side(s).
    mbrushside_t *leadside[ 2 ] = { nullptr, nullptr };
    // Clip plane.
    cm_plane_t *clipplane[ 2 ] = { nullptr, nullptr, };

    // Distances.
    double d1 = 0.;
    double d2 = 0.;
    double dist = 0.;

    // Fractions.
    double f = 0.; // Actual fraction.
    double enterfrac[ 2 ] = { -1., -1. };
    double leavefrac = 1.;

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

        // special case for axial planes (plane->type < 3) to avoid a full dot
        if ( plane->type < 3 ) {
            if ( !reantrantState->trIsPoint ) {
                // choose the offset coordinate corresponding to the plane axis and sign
                const double off = reantrantState->trOffsets[ plane->signbits ][ plane->type ];
                // axial normal has non-zero at plane->type only (usually ±1)
                dist = plane->dist - off * plane->normal[ plane->type ];
            } else {
                // point trace: no offset
                dist = plane->dist;
            }
        } else {
            // general (non-axial) case
            if ( !reantrantState->trIsPoint ) {
                // push the plane out apropriately for mins/maxs
                dist = DotProductDP( reantrantState->trOffsets[ plane->signbits ], plane->normal );
                dist = plane->dist - dist;
            } else {
                // special point case
                dist = plane->dist;
            }
        }

        d1 = DotProductDP( p1, plane->normal ) - dist;
        d2 = DotProductDP( p2, plane->normal ) - dist;

        if ( d2 > 0. ) {
            getout = true; // endpoint is not in solid
        }
        if ( d1 > 0. ) {
            startout = true;
        }

        // if completely in front of face, no intersection with the entire brush
        // Paril: Q3A fix
        if ( d1 > 0. && ( d2 >= DIST_EPSILON || d2 >= d1 ) ) {
            // Paril
            return;
        }

        // if it doesn't cross the plane, the plane isn't relevent
        if ( d1 <= 0. && d2 <= 0. ) {
            continue;
        }

        // Crosses face.
        if ( d1 > d2 ) { // Enter.
            f = ( d1 - DIST_EPSILON ) / ( d1 - d2 );
            if ( f < 0. ) {
                f = 0.;
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
            if ( f > 1. ) {
                f = 1.;
            }
            // Paril
            if ( f < leavefrac ) {
                leavefrac = f;
            }
        }
    }

    if ( !startout ) {
        // original point was inside brush
		reantrantState->trResult.startsolid = true;
        if ( !getout ) {
			reantrantState->trResult.allsolid = true;
            //if ( !map_allsolid_bug->integer ) {
                // original Q2 didn't set these
			reantrantState->trResult.fraction = 0;
			reantrantState->trResult.contents = static_cast<cm_contents_t>( brush->contents );
			reantrantState->trResult.material = nullptr;
            //}
        }
        return;
    }
    if ( enterfrac[ 0 ] < leavefrac ) {
        if ( enterfrac[ 0 ] > -1. && enterfrac[ 0 ] < reantrantState->trResult.fraction ) {
            if ( enterfrac[ 0 ] < 0 ) {
                enterfrac[ 0 ] = 0;
            }
			reantrantState->trResult.fraction = enterfrac[ 0 ];
			reantrantState->trResult.plane = *clipplane[ 0 ];
			reantrantState->trResult.surface = &( leadside[ 0 ]->texinfo->c );
			reantrantState->trResult.contents = static_cast<cm_contents_t>( brush->contents );
			reantrantState->trResult.material = reantrantState->trResult.surface->material;

            #ifdef SECOND_PLANE_TRACE
            if ( leadside[ 1 ] ) {
				reantrantState->trResult.plane2 = *clipplane[ 1 ];
				reantrantState->trResult.surface2 = &( leadside[ 1 ]->texinfo->c );
				reantrantState->trResult.material2 = reantrantState->trResult.surface2->material;
            }
            #else
			reantrantState->trResult.plane2 = *clipplane[ 0 ];
			reantrantState->trResult.surface2 = &( leadside[ 0 ]->texinfo->c );
			reantrantState->trResult.material2 = reantrantState->trResult.surface2->material;
            //trace->plane2 = {};
            //trace->surface2 = nullptr;
            #endif
        }
    }
}

/**
*   @brief
**/
static void CM_TestBoxInBrush( const Vector3 &p1, cm_trace_reantrant_state_t *reantrantState, mbrush_t *brush ) {
    int         i;
    cm_plane_t *plane;
    double       dist;
    double       d1;
    mbrushside_t *side;

    if ( !brush->numsides )
        return;

    side = brush->firstbrushside;
    for ( i = 0; i < brush->numsides; i++, side++ ) {
        plane = side->plane;

        // special case for axial planes to match CM_ClipBoxToBrush behavior
        if ( plane->type < 3 ) {
            if ( reantrantState->trIsPoint ) {
                // point: compare against plane dist directly
                dist = plane->dist;
            } else {
                double off = reantrantState->trOffsets[ plane->signbits ][ plane->type ];
                dist = plane->dist - off * plane->normal[ plane->type ];
            }
        } else {
            // general box case
            // push the plane out apropriately for mins/maxs
            if ( reantrantState->trIsPoint ) {
                dist = plane->dist;
            } else {
                dist = DotProductDP( reantrantState->trOffsets[ plane->signbits ], plane->normal );
                dist = plane->dist - dist;
            }
        }

        d1 = DotProductDP( p1, plane->normal ) - dist;

        // if completely in front of face, no intersection
        if ( d1 > 0. )
            return;
    }

    // inside this brush
	reantrantState->trResult.startsolid = reantrantState->trResult.allsolid = true;
	reantrantState->trResult.fraction = 0;
	reantrantState->trResult.contents = static_cast<cm_contents_t>( brush->contents );
    //if ( trace->material ) {
	reantrantState->trResult.material = nullptr;
    //}
}

/**
*   @brief
**/
static void CM_TraceToLeaf( cm_t *cm, cm_trace_reantrant_state_t *reantrantState, mleaf_t *leaf, std::unordered_set<const mbrush_t *> *visitedBrushes ) {
 /**
    *   Unused for now: leaf brush iteration only needs the trace state and the caller-supplied visited set.
    **/
    (void)cm;

    /**
    *   Skip this leaf entirely when its contents cannot satisfy the active trace mask.
    **/
	if ( !( leaf->contents & reantrantState->trContents ) ) {
		return;
	}

 /**
    *   Iterate each brush referenced by the leaf and test it at most once per trace.
    **/
    mbrush_t **leafbrush = leaf->firstleafbrush;
    for ( int32_t i = 0; i < leaf->numleafbrushes; i++, leafbrush++ ) {
        // Resolve the current leaf brush pointer.
        mbrush_t *b = *leafbrush;
        // Skip brushes that were already processed from another leaf during this trace.
     if ( !CM_TryVisitBrush( visitedBrushes, b ) ) {
            continue;
        }
		// Skip this brush if its contents don't match the trace's contents
		if ( !( b->contents & reantrantState->trContents ) ) {
			continue;
		}
		// Test the box against the brush. If the box is completely inside the brush, we can stop testing.
        CM_ClipBoxToBrush( reantrantState->trStart, reantrantState->trEnd, reantrantState, b );
		//if ( !trace_trace->fraction )
		if ( !reantrantState->trResult.fraction ) {
			return;
		}
    }
}

/**
*   @brief
**/
static void CM_TestInLeaf( cm_t *cm, cm_trace_reantrant_state_t *reantrantState, mleaf_t *leaf, std::unordered_set<const mbrush_t *> *visitedBrushes ) {
   /**
    *   Unused for now: leaf brush iteration only needs the trace state and the caller-supplied visited set.
    **/
    (void)cm;

    /**
    *   Skip this leaf entirely when its contents cannot satisfy the active trace mask.
    **/
	if ( !( leaf->contents & reantrantState->trContents ) ) {
		return;
	}

    /**
    *   Iterate each brush referenced by the leaf and test it at most once per trace.
    **/
	mbrush_t **leafbrush = leaf->firstleafbrush;
    for ( int32_t i = 0; i < leaf->numleafbrushes; i++, leafbrush++ ) {
        mbrush_t *b = *leafbrush;
        // Skip brushes that were already processed from another leaf during this trace.
        if ( !CM_TryVisitBrush( visitedBrushes, b ) ) {
            continue;
        }

		// Skip this brush if its contents don't match the trace's contents
		if ( !( b->contents & reantrantState->trContents ) ) {
			continue;
		}
		// Test the point against the brush. If the point is inside the brush, we can stop testing.
        CM_TestBoxInBrush( reantrantState->trStart, reantrantState, b );

        //if ( !trace_trace->fraction )
        if ( reantrantState->trResult.allsolid ) {
            return;
        }
    }
}

/**
*   @brief
**/
static void CM_RecursiveHullCheck( cm_t *cm, cm_trace_reantrant_state_t *reantrantState, mnode_t *node, double p1f, double p2f, const Vector3 &p1, const Vector3 &p2, std::unordered_set<const mbrush_t *> *visitedBrushes ) {
    cm_plane_t *plane;
    double   t1, t2, offset;
    double   frac, frac2;
    double   idist;
    vec3_t  mid;
    int     side;
    double   midf;

    /**
    *   Stop immediately when the caller provides no node or when an earlier hit is already closer.
    *   This keeps malformed child pointers or synthetic hull edge cases from dereferencing null nodes.
    **/
    if ( !node || reantrantState->trResult.fraction <= p1f )
        return;     // already hit something nearer

recheck:
    // if plane is NULL, we are in a leaf node
    plane = node->plane;
    if ( !plane ) {
        CM_TraceToLeaf( cm, reantrantState, (mleaf_t *)node, visitedBrushes );
        return;
    }

    //
    // find the point distances to the seperating plane
    // and the offset for the size of the box
    //
    if ( plane->type < 3 ) {
        t1 = p1[ plane->type ] - plane->dist;
        t2 = p2[ plane->type ] - plane->dist;
        offset = reantrantState->trExtents[ plane->type ];
    } else {
        t1 = PlaneDiffDP( p1, plane );
        t2 = PlaneDiffDP( p2, plane );
        if ( reantrantState->trIsPoint )
            offset = 0.;
        else
            offset = /*std::sqrt*/( std::fabs( (double)reantrantState->trExtents[ 0 ] * (double)plane->normal[ 0 ] ) +
                std::fabs( (double)reantrantState->trExtents[ 1 ] * (double)plane->normal[ 1 ] ) +
                std::fabs( (double)reantrantState->trExtents[ 2 ] * (double)plane->normal[ 2 ] ) );
    }

    // see which sides we need to consider
    if ( t1 >= offset + 1. && t2 >= offset + 1. ) {
        node = node->children[ 0 ];
        goto recheck;
    }
    if ( t1 < -offset - 1. && t2 < -offset -1. ) {
        node = node->children[ 1 ];
        goto recheck;
    }

    // put the crosspoint DIST_EPSILON pixels on the near side
    if ( t1 < t2 ) {
        idist = 1.0 / ( t1 - t2 );
        side = 1;
        frac2 = ( t1 + offset + DIST_EPSILON ) * idist;
        frac = ( t1 - offset + DIST_EPSILON ) * idist;
    } else if ( t1 > t2 ) {
        idist = 1.0 / ( t1 - t2 );
        side = 0;
        frac2 = ( t1 - offset - DIST_EPSILON ) * idist;
        frac = ( t1 + offset + DIST_EPSILON ) * idist;
    } else {
        side = 0;
        frac = 1;
        frac2 = 0;
    }

    // move up to the node
    midf = p1f + ( p2f - p1f ) * std::clamp( frac, 0., 1. );
    LerpVectorDP( p1, p2, frac, mid );

    CM_RecursiveHullCheck( cm, reantrantState, node->children[ side ], p1f, midf, p1, mid, visitedBrushes );

    // go past the node
    midf = p1f + ( p2f - p1f ) * std::clamp( frac2, 0., 1. );
    LerpVectorDP( p1, p2, frac2, mid );

    CM_RecursiveHullCheck( cm, reantrantState, node->children[ side ^ 1 ], midf, p2f, mid, p2, visitedBrushes );
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
const cm_trace_t CM_BoxTrace( cm_t *cm,
    const Vector3 &start, const Vector3 &end,
    const Vector3 *mins, const Vector3 *maxs,
    mnode_t *headnode, const cm_contents_t brushmask ) {
    /**
    *   Serialize trace execution because this routine relies on shared file-scope
    *   scratch state that can otherwise be clobbered by concurrent callers.
    **/
    //std::lock_guard<std::recursive_mutex> traceLock( cm_trace_mutex );

    // Validate mins and maxs, ohterwise assign them vec3_origin.
    if ( !mins ) {
        mins = &qm_vector3_null;
    }
    if ( !maxs ) {
        maxs = &qm_vector3_null;
    }

  const Vector3 bounds[ 2 ] = {
		( *mins ),
		( *maxs )
	};
   /**
    *   Reuse a thread-local visited-brush table so per-trace dedup stays local without re-growing buckets every call.
    **/
    std::unordered_set<const mbrush_t *> *visitedBrushes = CM_BeginVisitedBrushSet( cm );
    int32_t i, j;
  /**
    *   Fill in a default trace result and capture the immutable inputs for this re-entrant trace call.
    **/
	cm_trace_reantrant_state_t reantrantState = {
		.trStart = start,
		.trEnd = end,
      .trContents = brushmask
	};
    reantrantState.trResult = {
		.entityNumber = ENTITYNUM_NONE,
		.fraction = 1.0,
		.plane = {
			.normal = { 0.0f, 0.0f, 0.0f },
			.dist = 0.0f,
			.type = PLANE_NON_AXIAL,
			.signbits = 0,
		},

		.surface = &nulltexinfo.c,
		.material = &cm_default_material,

		.plane2 = {
			.normal = { 0.0f, 0.0f, 0.0f },
			.dist = 0.0f,
			.type = PLANE_NON_AXIAL,
			.signbits = 0,
		},
		.surface2 = &( nulltexinfo.c ),
		.material2 = &( cm_default_material ),
	};

  /**
    *   Bail out immediately when the caller did not provide a hull to trace against.
    **/
    if ( !headnode ) {
        return reantrantState.trResult;
    }

 /**
    *   Precompute the eight corner offsets used to expand brush planes for box traces.
    **/
    for ( i = 0; i < 8; i++ ) {
        for ( j = 0; j < 3; j++ ) {
			reantrantState.trOffsets[ i ][ j ] = ( bounds[ ( i >> j ) & 1 ] )[ j ];
        }
    }

    //
    // check for position test special case
    //
    if ( VectorCompare( start, end ) ) {
		mleaf_t *leafs[ 1024 ] = {};
		int32_t numleafs = 0;
        Vector3 c1 = start + *mins;
        Vector3 c2 = start + *maxs;

		// Slight epsilon to avoid numerical issues.
        for ( i = 0; i < 3; i++ ) {
            c1[ i ] -= 1;
            c2[ i ] += 1;
        }
		// CM_BoxLeafs_headnode is used instead of CM_PointLeafnum because we need to consider the entire box, not just the point.
        numleafs = CM_BoxLeafs_headnode( cm, c1, c2, leafs, q_countof( leafs ), headnode, NULL );
		// Test the point against all brushes in the leafs, since we don't have a ray to test against the planes.
        for ( i = 0; i < numleafs; i++ ) {
          // Test the point against all brushes in the current touched leaf.
            CM_TestInLeaf( cm, &reantrantState, leafs[ i ], visitedBrushes );
			// If the point is in solid, we can stop testing.
            if ( reantrantState.trResult.allsolid )
                break;
        }
        VectorCopy( start, reantrantState.trResult.endpos );
        return reantrantState.trResult;
    }

    //
    // check for point special case
    //
    if ( VectorEmpty( mins ) && VectorEmpty( maxs ) ) {
		reantrantState.trIsPoint = true;
		reantrantState.trExtents = {};
    } else {
		reantrantState.trIsPoint = false;
		reantrantState.trExtents[ 0 ] = std::max( -mins->x, maxs->x );
		reantrantState.trExtents[ 1 ] = std::max( -mins->y, maxs->y );
		reantrantState.trExtents[ 2 ] = std::max( -mins->z, maxs->z );
    }

    //
    // general sweeping through world
    //
	// CM_RecursiveHullCheck is where the actual tracing happens, and it will update reantrantState.trResult with the results of the trace.
    CM_RecursiveHullCheck( cm, &reantrantState, headnode, 0, 1, start, end, visitedBrushes );
	// Lerp the end position based on the fraction of the trace that was completed, so that the caller can know where the trace ended up in world space.
    LerpVectorDP( start, end, reantrantState.trResult.fraction, reantrantState.trResult.endpos );

	//
	// Return the results of the trace.
	//
	return reantrantState.trResult;
}
/**
*   @brief  Handles offseting and rotation of the end points for moving and
*           rotating entities.
**/
const cm_trace_t CM_TransformedBoxTrace( cm_t *cm,
    const Vector3 &start, const Vector3 &end,
    const Vector3 *mins, const Vector3 *maxs,
    mnode_t *headnode, const cm_contents_t brushmask,
    const vec3_t origin, const vec3_t angles ) {
	/**
    *   Serialize transformed traces for the same reason as CM_BoxTrace. A recursive
    *   mutex is used because this entry point delegates into CM_BoxTrace.
    **/
    //std::lock_guard<std::recursive_mutex> traceLock( cm_trace_mutex );

    vec3_t      start_l, end_l;
    vec3_t      axis[ 3 ];

    /**
    *   Classify synthetic hull headnodes through helper functions so thread-local hull storage is handled correctly.
    **/
    const bool isBoxHull = CM_IsBoundingBoxHullHeadnode( cm, headnode );
    const bool isOctagonHull = CM_IsOctagonBoxHullHeadnode( cm, headnode );
    const bool rotated = ( !isBoxHull && !isOctagonHull && !VectorEmpty( angles ) );

    if ( isOctagonHull ) {
        /**
        *   Resolve the synthetic octagon hull cylinder offset from the active headnode so concurrent traces never read shared mutable state.
        **/
        const vec3_t *cylinderOffset = CM_GetOctagonBoxHullCylinderOffset( cm, headnode );
        if ( cylinderOffset ) {
            VectorSubtract( start, *cylinderOffset, start_l );
            VectorSubtract( end, *cylinderOffset, end_l );
        } else {
            VectorCopy( start, start_l );
            VectorCopy( end, end_l );
        }
    } else {
        VectorCopy( start, start_l );
        VectorCopy( end, end_l );
    }

    // subtract origin offset
    VectorSubtract( start_l, origin, start_l );
    VectorSubtract( end_l, origin, end_l );

    // rotate start and end into the models frame of reference
    if ( rotated ) {
        AnglesToAxis( angles, axis );
        RotatePoint( start_l, axis );
        RotatePoint( end_l, axis );
    }

	cm_trace_reantrant_state_t reantrantState = {
		.trStart = start,
		.trEnd = end,
		.trContents = brushmask
	};
	reantrantState.trResult = {
		.entityNumber = ENTITYNUM_NONE,
		.fraction = 1.0,
		.plane = {
			.normal = { 0.0f, 0.0f, 0.0f },
			.dist = 0.0f,
			.type = PLANE_NON_AXIAL,
			.signbits = 0,
		},

		.surface = &nulltexinfo.c,
		.material = &cm_default_material,

		.plane2 = {
			.normal = { 0.0f, 0.0f, 0.0f },
			.dist = 0.0f,
			.type = PLANE_NON_AXIAL,
			.signbits = 0,
		},
		.surface2 = &( nulltexinfo.c ),
		.material2 = &( cm_default_material ),
	};

	if ( !headnode ) {
		return reantrantState.trResult;
	}

    // sweep the box through the model
    reantrantState.trResult = CM_BoxTrace( cm, start_l, end_l, mins, maxs, headnode, brushmask );

    // rotate plane normal into the worlds frame of reference
    if ( reantrantState.trResult.fraction < 1.0 ) {
        #if 1
        if ( rotated ) {
            TransposeAxis( axis );
            RotatePoint( reantrantState.trResult.plane.normal, axis );
        }
        // FIXME: offset plane distance?
        //trace->plane.dist += DotProduct( trace->plane.normal, origin );
        #else
        vec3_t a;
        VectorNegate( angles, a );
        AnglesToAxis( a, axis );
        RotatePoint( trace->plane.normal, axis );
        // FIXME: offset plane distance?
        //trace->plane.dist = DotProductDP( trace->plane.normal, origin );
        #endif
    }

    // <Q2RTXP>: Don't do this for non rotated planes.
    // FIXME: offset plane distance?
    //trace->plane.dist += DotProductDP( trace->plane.normal, origin );

    LerpVectorDP( start, end, reantrantState.trResult.fraction, reantrantState.trResult.endpos );

	// Return the result
	return reantrantState.trResult;
}


// <q2RTXP>: WID: Deprecated, may reappply for the future.
#if 0
/**
*   @brief
**/
void CM_ClipEntity( cm_t *cm, cm_trace_t *dst, const cm_trace_t *src, const int32_t entityNumber ) {
    dst->allsolid |= src->allsolid;
    dst->startsolid |= src->startsolid;
    if ( src->fraction < dst->fraction ) {
        dst->fraction = src->fraction;
        dst->entityNumber = entityNumber;
        dst->endpos = src->endpos;

        dst->plane = src->plane;
        dst->surface = src->surface;
        dst->material = src->material;
        dst->contents = ( dst->contents | src->contents );

        dst->plane2 = src->plane2;
        dst->surface2 = src->surface2;
        dst->material2 = src->material2;
    }

	// <Q2RTXP>: Fix for allsolid and startsolid traces losing their entity number.
    //if ( src->allsolid || src->startsolid ) {
    //    dst->entityNumber = entityNumber;
    //}
}//
#endif