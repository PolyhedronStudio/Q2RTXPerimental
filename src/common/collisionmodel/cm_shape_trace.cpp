/**
/*	@file
/*	@brief	Collision model shape sweep and tracing helpers.
/*	@note	Implements swept point, box, sphere, capsule, and cylinder tracing.
**/
#include "shared/shared.h"
#include "common/collisionmodel.h"

#include "common/collisionmodel/cm_shape_trace_sweep.h"

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
//! 1/32 epsilon to keep floating point stable at brush boundaries.
static constexpr double DIST_EPSILON = 0.03125;

/**
*	@brief	For thread-safety, we use a structure that is locally declared in a Trace function after which
*			they are copied out to the caller's cm_trace_t. This is because the tracing implementation below uses shared file-scope scratch state and is therefore not re-entrant.
**/
struct cm_trace_reantrant_state_t {
	// Actual trace state, this	copied out to the caller's cm_trace_t after the trace completes. (Making it re-entrant.)
	cm_trace_t trResult;

	//! Shape of the trace (Point, AABB, Sphere, Capsule, Cylinder)
	cm_trace_shape_t trShape;

	//! The start and end points of the trace.
	Vector3  trStart = {}, trEnd = { };
	//! Offsets for the 8 corners of the box being traced. Used ONLY when trShape.type == SHAPE_AABB.
	Vector3  trOffsets[ 8 ] = {};
	//! Extents of the shape for early AABB rejection in BSP tree.
	Vector3  trExtents = {};
	//! Optimization flag for point traces.
	bool trIsPoint = false;

	//! Flag indicating whether the trace model is rotated in world space.
	bool isRotated = false;
	//! Transformation axis from local model space to world space (transpose of AnglesToAxis).
	Vector3 localToWorldAxis[ 3 ] = {};

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
/*	@brief	Retrieve the current thread's reusable visited-brush table for trace deduplication.
/*	@param	cm	Collision model whose brush count is used to size the table conservatively.
/*	@return	Pointer to the cleared thread-local visited-brush table for the active trace.
/*	@note	Keeping this container thread-local avoids cross-thread races while also reusing bucket
/*		capacity across traces so we do not pay repeated heap growth costs every call.
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
/*	@brief	Try to mark a brush as visited for the active trace.
/*	@param	visitedBrushes	Active thread-local visited-brush table for the current trace.
/*	@param	brush	Brush about to be tested.
/*	@return	True when the brush was first seen by this trace, false when it was already processed
/*		from another leaf.
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
/*	@brief	Clip a swept shape against a brush and record the earliest blocking plane.
/*	@param	p1	Trace start point in world space.
/*	@param	p2	Trace end point in world space.
/*	@param	reantrantState	Mutable re-entrant trace state shared across brush tests.
/*	@param	brush	Brush to clip against.
/*	@note	Expands each brush plane to match the active trace shape before testing.
**/
static void CM_ClipShapeToBrush( const Vector3 &p1, const Vector3 &p2, cm_trace_reantrant_state_t *reantrantState, mbrush_t *brush ) {
	/**
	/*	Bail out immediately when the brush has no sides.
	/*	Without any planes there is nothing meaningful to clip against.
	**/
	if ( !brush->numsides )
		return;

	/**
	/*	Attempt analytic continuous collision detection for shapes sweeping against axial boxes.
	/*	This avoids snagging caused by bevel approximations when brushing over stair corners.
	**/
	if ( reantrantState->trShape.type == SHAPE_SPHERE ||
		reantrantState->trShape.type == SHAPE_CAPSULE ||
		reantrantState->trShape.type == SHAPE_CYLINDER ) {
		Vector3 boxMins, boxMaxs;
		if ( reantrantState->trShape.type != SHAPE_AABB && CM_IsBrushAxialBox( brush, boxMins, boxMaxs ) ) {
			float tHit = 1.0f;
			Vector3 nHit;
			bool startsolid = false;

			if ( CM_SweepShapeVsAxialBox( p1, p2, reantrantState->trShape.radius, reantrantState->trShape.halfHeight,
				reantrantState->trShape.type, boxMins, boxMaxs, tHit, nHit, startsolid ) ) {

				if ( startsolid ) {
					reantrantState->trResult.startsolid = true;
					if ( tHit == 0.0f ) {
						reantrantState->trResult.allsolid = true;
						reantrantState->trResult.fraction = 0.0f;
						reantrantState->trResult.contents = static_cast< cm_contents_t >( brush->contents );
						reantrantState->trResult.brushID = brush->brushID;
						reantrantState->trResult.material = nullptr;
					}
				} else {
					if ( tHit > -1.0f && tHit < 1.0f ) {
						Vector3 V = QM_Vector3Subtract( p2, p1 );
						float dot = QM_Vector3DotProduct( V, nHit );
						float t_adjusted = tHit;
						if ( dot < 0.0f ) {
							// Use the larger separation only for lateral contacts; floor and ceiling traces must not bounce.
							const float separation = ( std::fabs( nHit.z ) < 0.5f )
								? SWEEP_ROUNDED_SHAPE_EPSILON
								: SWEEP_DIST_EPSILON;
							float dist_back = separation / -dot;
							t_adjusted -= dist_back;
						}
						if ( t_adjusted < 0.0f ) {
							t_adjusted = 0.0f;
						}

						if ( t_adjusted < reantrantState->trResult.fraction ) {
							reantrantState->trResult.fraction = t_adjusted;
							reantrantState->trResult.plane.normal[ 0 ] = nHit.x;
							reantrantState->trResult.plane.normal[ 1 ] = nHit.y;
							reantrantState->trResult.plane.normal[ 2 ] = nHit.z;
							reantrantState->trResult.plane.dist = QM_Vector3DotProduct( p1 + ( p2 - p1 ) * tHit, nHit );
							reantrantState->trResult.plane.type = 3;
							reantrantState->trResult.surface = &( brush->firstbrushside[ 0 ].texinfo->c );
							reantrantState->trResult.contents = static_cast< cm_contents_t >( brush->contents );
							reantrantState->trResult.brushID = brush->brushID;
						}
					}
				}
			}
			return; // Handled analytically, no need for the generic loop.
		}
	}

	/**
	/*	Working storage reused while comparing the segment against each brush plane.
	**/
	cm_plane_t *plane = nullptr;

	//! Lead side(s) for the plane(s) that produce the best entering fraction.
	mbrushside_t *leadside[ 2 ] = { nullptr, nullptr };
	//! Clip plane(s) associated with the best entering fraction(s).
	cm_plane_t *clipplane[ 2 ] = { nullptr, nullptr, };

	//! Distances from both endpoints to the current plane.
	double d1 = 0.;
	double d2 = 0.;
	double dist = 0.;

	//! Current crossing fraction for the active plane.
	double f = 0.;
	//! Best entering fraction(s) seen so far.
	double enterfrac[ 2 ] = { -1., -1. };
	//! Earliest leaving fraction seen so far.
	double leavefrac = 1.;

	//! True when the end point is outside the brush volume.
	bool getout = false;
	//! True when the start point begins outside the brush volume.
	bool startout = false;

	/**
	/*	Compare the trace segment against every plane in the brush.
	/*	Track the latest entry fraction and earliest exit fraction so the final result reflects
	/*	the first meaningful collision.
	**/
	mbrushside_t *side = brush->firstbrushside;
	mbrushside_t *endside = side + brush->numsides;
	int32_t i = 0;

	float max_d1 = -99999.0f;
	cm_plane_t *best_plane = nullptr;
	mbrushside_t *best_side = nullptr;

	for ( i = 0, side = side; side < endside; i++, side++ ) {
		plane = side->plane;

		/**
		/*	Expand the plane by the active trace shape before measuring endpoint distances.
		/*	This keeps point, box, sphere, cylinder, and capsule tests consistent.
		**/
		Vector3 worldNormal = plane->normal;
		if ( reantrantState->isRotated ) {
			worldNormal.x = ( float )DotProductDP( reantrantState->localToWorldAxis[ 0 ], plane->normal );
			worldNormal.y = ( float )DotProductDP( reantrantState->localToWorldAxis[ 1 ], plane->normal );
			worldNormal.z = ( float )DotProductDP( reantrantState->localToWorldAxis[ 2 ], plane->normal );
		}

		float expand = 0.0f;
		switch ( reantrantState->trShape.type ) {
		case SHAPE_POINT:
			expand = 0.0f;
			break;
		case SHAPE_AABB:
			if ( !reantrantState->isRotated && plane->type < 3 ) {
				const double off = reantrantState->trOffsets[ plane->signbits ][ plane->type ];
				expand = ( float )( off * plane->normal[ plane->type ] );
			} else if ( !reantrantState->isRotated ) {
				expand = ( float )DotProductDP( reantrantState->trOffsets[ plane->signbits ], plane->normal );
			} else {
				expand = -( ( reantrantState->trShape.extents.x * std::abs( worldNormal.x ) ) +
							( reantrantState->trShape.extents.y * std::abs( worldNormal.y ) ) +
							( reantrantState->trShape.extents.z * std::abs( worldNormal.z ) ) );
			}
			break;
		case SHAPE_SPHERE:
			// Use the exact sphere radius so floor contact matches the true geometric bounds.
			expand = -reantrantState->trShape.radius;
			break;
		case SHAPE_CYLINDER:
			expand = -( ( reantrantState->trShape.radius * std::sqrt( worldNormal.x * worldNormal.x + worldNormal.y * worldNormal.y ) ) +
				( reantrantState->trShape.halfHeight * std::abs( worldNormal.z ) ) );
			break;
		case SHAPE_CAPSULE:
			// Keep capsule expansion exact so the trace does not hover above or sink into floors.
			expand = -( reantrantState->trShape.radius + ( reantrantState->trShape.halfHeight * std::abs( worldNormal.z ) ) );
			break;
		}
		dist = plane->dist - expand;

		d1 = DotProductDP( p1, plane->normal ) - dist;
		d2 = DotProductDP( p2, plane->normal ) - dist;

		if ( d1 > max_d1 ) {
			max_d1 = (float)d1;
			best_plane = plane;
			best_side = side;
		}

		if ( d2 > 0. ) {
			getout = true;
		}
		if ( d1 > 0. ) {
			startout = true;
		}

		/**
		/*	Reject the whole brush when the segment remains in front of a plane.
		/*	This is a fast early-out for traces that never enter the hull.
		**/
		if ( d1 > 0. && ( d2 >= DIST_EPSILON || d2 >= d1 ) ) {
			return;
		}

		/**
		/*	If both endpoints are behind this plane, the plane cannot clip the trace.
		**/
		if ( d1 <= 0. && d2 <= 0. ) {
			continue;
		}

		/**
		/*	Record the best enter/leave fraction for the current plane crossing.
		**/
		if ( d1 > d2 ) {
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
		} else {
			f = ( d1 + DIST_EPSILON ) / ( d1 - d2 );
			if ( f > 1. ) {
				f = 1.;
			}
			if ( f < leavefrac ) {
				leavefrac = f;
			}
		}
	}

	if ( !startout ) {
		/**
		/*	Barely inside the expanded brush (happens with cylinders on concave curves).
		/*	Do not trap the player; instead, return a collision against the closest plane at fraction 0.
		**/
		if ( max_d1 >= -0.125f && best_plane != nullptr ) {
			if ( reantrantState->trResult.fraction > 0.0 ) {
				reantrantState->trResult.fraction = 0.0;
				reantrantState->trResult.plane = *best_plane;
				reantrantState->trResult.contents = static_cast< cm_contents_t >( brush->contents );
				reantrantState->trResult.brushID = brush->brushID;
				reantrantState->trResult.surface = &( best_side->texinfo->c );
				reantrantState->trResult.material = nullptr;
			}
			return;
		}

		/**
		/*	The trace starts inside the brush, so mark the result as solid at fraction 0.
		**/
		reantrantState->trResult.startsolid = true;
		if ( !getout ) {
			reantrantState->trResult.allsolid = true;
			reantrantState->trResult.fraction = 0;
			reantrantState->trResult.contents = static_cast< cm_contents_t >( brush->contents );
			reantrantState->trResult.brushID = brush->brushID;
			reantrantState->trResult.material = nullptr;
		}
		return;
	}

	/**
	/*	Commit the first valid entering plane when it occurs before the leaving plane.
	/*	This updates the trace result with brush and surface data for the hit.
	**/
	if ( enterfrac[ 0 ] < leavefrac ) {
		if ( enterfrac[ 0 ] > -1. && enterfrac[ 0 ] < reantrantState->trResult.fraction ) {
			if ( enterfrac[ 0 ] < 0 ) {
				enterfrac[ 0 ] = 0;
			}
			reantrantState->trResult.fraction = enterfrac[ 0 ];
			reantrantState->trResult.plane = *clipplane[ 0 ];
			reantrantState->trResult.surface = &( leadside[ 0 ]->texinfo->c );
			reantrantState->trResult.contents = static_cast< cm_contents_t >( brush->contents );
			reantrantState->trResult.brushID = brush->brushID;
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
/*	@brief	Test whether a shape's start point lies inside a brush.
/*	@param	p1	Start point to classify.
/*	@param	reantrantState	Mutable re-entrant trace state shared across brush tests.
/*	@param	brush	Brush to test against.
/*	@note	Sets `startsolid` and `allsolid` when the point is fully contained.
**/
static void CM_TestShapeInBrush( const Vector3 &p1, cm_trace_reantrant_state_t *reantrantState, mbrush_t *brush ) {
	/**
	/*	Bail out immediately when the brush has no sides.
	/*	Without any planes there is nothing meaningful to test against.
	**/
	int         i;
	cm_plane_t *plane;
	double       dist;
	double       d1;
	mbrushside_t *side;

	if ( !brush->numsides )
		return;

	/**
	/*	Walk each plane and ensure the point remains behind it after expansion.
	**/
	side = brush->firstbrushside;
	for ( i = 0; i < brush->numsides; i++, side++ ) {
		plane = side->plane;

		Vector3 worldNormal = plane->normal;
		if ( reantrantState->isRotated ) {
			worldNormal.x = ( float )DotProductDP( reantrantState->localToWorldAxis[ 0 ], plane->normal );
			worldNormal.y = ( float )DotProductDP( reantrantState->localToWorldAxis[ 1 ], plane->normal );
			worldNormal.z = ( float )DotProductDP( reantrantState->localToWorldAxis[ 2 ], plane->normal );
		}

		float expand = 0.0f;
		switch ( reantrantState->trShape.type ) {
		case SHAPE_POINT:
			expand = 0.0f;
			break;
		case SHAPE_AABB:
			if ( !reantrantState->isRotated && plane->type < 3 ) {
				const double off = reantrantState->trOffsets[ plane->signbits ][ plane->type ];
				expand = ( float )( off * plane->normal[ plane->type ] );
			} else if ( !reantrantState->isRotated ) {
				expand = ( float )DotProductDP( reantrantState->trOffsets[ plane->signbits ], plane->normal );
			} else {
				expand = -( ( reantrantState->trShape.extents.x * std::abs( worldNormal.x ) ) +
							( reantrantState->trShape.extents.y * std::abs( worldNormal.y ) ) +
							( reantrantState->trShape.extents.z * std::abs( worldNormal.z ) ) );
			}
			break;
		case SHAPE_SPHERE:
			expand = -( reantrantState->trShape.radius );
			break;
		case SHAPE_CYLINDER:
			expand = -( ( reantrantState->trShape.radius * std::sqrt( worldNormal.x * worldNormal.x + worldNormal.y * worldNormal.y ) ) +
				( reantrantState->trShape.halfHeight * std::abs( worldNormal.z ) ) );
			break;
		case SHAPE_CAPSULE:
			expand = -( reantrantState->trShape.radius + ( reantrantState->trShape.halfHeight * std::abs( worldNormal.z ) ) );
			break;
		}
		dist = plane->dist - expand;

		d1 = DotProductDP( p1, plane->normal ) - dist;

		/**
		/*	A point in front of any plane cannot be inside the brush.
		**/
		if ( d1 > 0. )
			return;
	}

	/**
	/*	The point is inside every brush plane, so mark the trace as solid.
	**/
	reantrantState->trResult.startsolid = reantrantState->trResult.allsolid = true;
	reantrantState->trResult.fraction = 0;
	reantrantState->trResult.contents = static_cast< cm_contents_t >( brush->contents );
	reantrantState->trResult.material = nullptr;
}

/**
/*	@brief	Trace a swept shape against every unique brush referenced by a leaf.
/*	@param	cm	Collision model context.
/*	@param	reantrantState	Active trace state and destination result.
/*	@param	leaf	Leaf whose brushes should be tested.
/*	@param	visitedBrushes	Deduplication set so shared brushes are tested once per trace.
/*	@note	The collision-model pointer is currently unused here; leaf-local data is sufficient.
**/
static void CM_TraceToLeaf( cm_t *cm, cm_trace_reantrant_state_t *reantrantState, mleaf_t *leaf, std::unordered_set<const mbrush_t *> *visitedBrushes ) {
	/**
	/*	The collision-model pointer is unused for now because leaf-local brush iteration only needs
	/*	the trace state and the caller-supplied visited set.
	**/
	( void )cm;

	/**
	/*	Skip this leaf entirely when its contents cannot satisfy the active trace mask.
	**/
	if ( !( leaf->contents & reantrantState->trContents ) ) {
		return;
	}

	/**
	/*	Iterate each brush referenced by the leaf and test it at most once per trace.
	**/
	mbrush_t **leafbrush = leaf->firstleafbrush;
	for ( int32_t i = 0; i < leaf->numleafbrushes; i++, leafbrush++ ) {
		/**
		/*	Resolve the current leaf brush pointer and skip brushes already processed.
		**/
		mbrush_t *b = *leafbrush;
		if ( !CM_TryVisitBrush( visitedBrushes, b ) ) {
			continue;
		}

		/**
		/*	Skip this brush if its contents do not match the active trace mask.
		**/
		if ( !( b->contents & reantrantState->trContents ) ) {
			continue;
		}

		/**
		/*	Clip the swept shape against the brush and stop early when the trace is already solid.
		**/
		CM_ClipShapeToBrush( reantrantState->trStart, reantrantState->trEnd, reantrantState, b );
		if ( !reantrantState->trResult.fraction ) {
			return;
		}
	}
}

/**
/*	@brief	Test a point trace against every unique brush referenced by a leaf.
/*	@param	cm	Collision model context.
/*	@param	reantrantState	Active trace state and destination result.
/*	@param	leaf	Leaf whose brushes should be tested.
/*	@param	visitedBrushes	Deduplication set so shared brushes are tested once per trace.
/*	@note	Stops as soon as the point is confirmed solid.
**/
static void CM_TestInLeaf( cm_t *cm, cm_trace_reantrant_state_t *reantrantState, mleaf_t *leaf, std::unordered_set<const mbrush_t *> *visitedBrushes ) {
	/**
	/*	The collision-model pointer is unused for now because leaf-local brush iteration only needs
	/*	the trace state and the caller-supplied visited set.
	**/
	( void )cm;

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

	/**
	/*	Skip this brush if its contents do not match the active trace mask.
	**/
		if ( !( b->contents & reantrantState->trContents ) ) {
			continue;
		}

		/**
		/*	Test the point against the brush and stop immediately when it is solid.
		**/
		CM_TestShapeInBrush( reantrantState->trStart, reantrantState, b );

		if ( reantrantState->trResult.allsolid ) {
			return;
		}
	}
}

/**
/*	@brief	Recursively traverse the hull tree and clip the current sweep against it.
/*	@param	cm	Collision model context.
/*	@param	reantrantState	Active trace state and destination result.
/*	@param	node	Current node or leaf being traversed.
/*	@param	p1f	Normalized start fraction for the current segment.
/*	@param	p2f	Normalized end fraction for the current segment.
/*	@param	p1	Start point of the current segment.
/*	@param	p2	End point of the current segment.
/*	@param	visitedBrushes	Deduplication set so shared brushes are tested once per trace.
/*	@note	Uses plane splitting to recurse front/back while preserving the closest hit.
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
	/*	Stop immediately when the caller provides no node or when an earlier hit is already closer.
	/*	This keeps malformed child pointers or synthetic hull edge cases from dereferencing null nodes.
	**/
	if ( !node || reantrantState->trResult.fraction <= p1f )
		return;

recheck:
	/**
	/*	When the node has no plane, it is a leaf and brush testing can happen immediately.
	**/
	plane = node->plane;
	if ( !plane ) {
		CM_TraceToLeaf( cm, reantrantState, ( mleaf_t * )node, visitedBrushes );
		return;
	}

	/**
	/*	Measure both endpoints against the split plane and expand by the active trace extents.
	/*	This determines which side of the hull can be rejected and where the segment crosses.
	**/
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
			offset = /*std::sqrt*/( std::fabs( ( double )reantrantState->trExtents[ 0 ] * ( double )plane->normal[ 0 ] ) +
				std::fabs( ( double )reantrantState->trExtents[ 1 ] * ( double )plane->normal[ 1 ] ) +
				std::fabs( ( double )reantrantState->trExtents[ 2 ] * ( double )plane->normal[ 2 ] ) );
	}

	/**
	/*	Reject the whole segment when both endpoints stay on the same side of the split plane.
	**/
	if ( t1 >= offset + 1. && t2 >= offset + 1. ) {
		node = node->children[ 0 ];
		goto recheck;
	}
	if ( t1 < -offset - 1. && t2 < -offset - 1. ) {
		node = node->children[ 1 ];
		goto recheck;
	}

	/**
	/*	Compute the front/back fractions while biasing the split point toward the near side.
	**/
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

	/**
	/*	Recurse into the near side first so the closest hit can terminate traversal early.
	**/
	midf = p1f + ( p2f - p1f ) * std::clamp( frac, 0., 1. );
	LerpVectorDP( p1, p2, frac, mid );

	CM_RecursiveHullCheck( cm, reantrantState, node->children[ side ], p1f, midf, p1, mid, visitedBrushes );

	/**
	/*	Then recurse into the far side if the trace still continues past the split.
	**/
	midf = p1f + ( p2f - p1f ) * std::clamp( frac2, 0., 1. );
	LerpVectorDP( p1, p2, frac2, mid );

	CM_RecursiveHullCheck( cm, reantrantState, node->children[ side ^ 1 ], midf, p2f, mid, p2, visitedBrushes );
}



/**
/*	@brief	Sweep a shape through the collision model and return the trace result.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	shape	Shape definition to sweep.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@return	Final trace result for the sweep.
/*	@note	Initializes shape extents, handles point traces as a special case, then performs the recursive hull check.
**/
const cm_trace_t CM_ShapeTraceEx( cm_t *cm,
	const Vector3 &start, const Vector3 &end,
	const cm_trace_shape_t &shape,
	mnode_t *headnode, const cm_contents_t brushmask,
	bool isRotated, const vec3_t *localToWorldAxis );

const cm_trace_t CM_ShapeTrace( cm_t *cm,
	const Vector3 &start, const Vector3 &end,
	const cm_trace_shape_t &shape,
	mnode_t *headnode, const cm_contents_t brushmask ) {
	return CM_ShapeTraceEx( cm, start, end, shape, headnode, brushmask, false, nullptr );
}

const cm_trace_t CM_ShapeTraceEx( cm_t *cm,
	const Vector3 &start, const Vector3 &end,
	const cm_trace_shape_t &shape,
	mnode_t *headnode, const cm_contents_t brushmask,
	bool isRotated, const vec3_t *localToWorldAxis ) {
	/**
	/*	Serialize trace execution because this routine relies on shared file-scope scratch state
	/*	that can otherwise be clobbered by concurrent callers.
	**/
	//std::lock_guard<std::recursive_mutex> traceLock( cm_trace_mutex );

	/**
	/*	Reuse a thread-local visited-brush table so per-trace dedup stays local without re-growing
	/*	buckets every call.
	**/
	std::unordered_set<const mbrush_t *> *visitedBrushes = CM_BeginVisitedBrushSet( cm );
	int32_t i, j;
	/**
	/*	Fill in a default trace result and capture the immutable inputs for this re-entrant trace call.
	**/
	cm_trace_reantrant_state_t reantrantState = {};
	reantrantState.trShape = shape;
	reantrantState.trStart = start;
	reantrantState.trEnd = end;
	reantrantState.trContents = brushmask;
	reantrantState.isRotated = isRotated;
	if ( isRotated && localToWorldAxis ) {
		reantrantState.localToWorldAxis[ 0 ] = localToWorldAxis[ 0 ];
		reantrantState.localToWorldAxis[ 1 ] = localToWorldAxis[ 1 ];
		reantrantState.localToWorldAxis[ 2 ] = localToWorldAxis[ 2 ];
	}
	/**
	/*	Derive conservative extents for the current trace shape.
	/*	These values are used for early hull rejection and point-trace special cases.
	**/
	if ( shape.type == SHAPE_SPHERE ) {
		reantrantState.trExtents.x = shape.radius;
		reantrantState.trExtents.y = shape.radius;
		reantrantState.trExtents.z = shape.radius;
	} else if ( shape.type == SHAPE_CYLINDER ) {
		reantrantState.trExtents.x = shape.radius;
		reantrantState.trExtents.y = shape.radius;
		reantrantState.trExtents.z = shape.halfHeight;
	} else if ( shape.type == SHAPE_CAPSULE ) {
		reantrantState.trExtents.x = shape.radius;
		reantrantState.trExtents.y = shape.radius;
		reantrantState.trExtents.z = shape.radius + shape.halfHeight;
	} else if ( shape.type == SHAPE_AABB ) {
		reantrantState.trExtents = shape.extents;
	} else {
		reantrantState.trExtents = {};
	}
	reantrantState.trIsPoint = ( reantrantState.trExtents.x == 0.0f && reantrantState.trExtents.y == 0.0f && reantrantState.trExtents.z == 0.0f );


	reantrantState.trResult = {
		.entityNumber = ENTITYNUM_NONE,
		.brushID = BRUSHID_NONE,
		.hitBodyID = HITBODYID_NONE,

		.fraction = 1.0,
		.endpos = end,

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
	/*	Bail out immediately when the caller did not provide a hull to trace against.
	**/
	if ( !headnode ) {
		return reantrantState.trResult;
	}

	/**
	/*	Precompute the eight corner offsets used to expand brush planes for swept volume tests.
	/*	This covers both box-like shapes and the swept capsule path used by this model.
	**/
	const Vector3 bounds[ 2 ] = {
		( -reantrantState.trExtents ),
		( reantrantState.trExtents )
	};
	for ( i = 0; i < 8; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			reantrantState.trOffsets[ i ][ j ] = ( bounds[ ( i >> j ) & 1 ] )[ j ];
		}
	}

	/**
	/*	Handle the position-test special case where the start and end are identical.
	/*	In this case we need a box query through the leaf list rather than a sweep.
	**/
	if ( VectorCompare( start, end ) ) {
		mleaf_t *leafs[ 1024 ] = {};
		int32_t numleafs = 0;
		Vector3 c1 = start;
		Vector3 c2 = start;

		float extentX = shape.extents.x;
		float extentY = shape.extents.y;
		float extentZ = shape.extents.z;
		if ( shape.type == SHAPE_SPHERE || shape.type == SHAPE_CYLINDER || shape.type == SHAPE_CAPSULE ) {
			extentX = shape.radius;
			extentY = shape.radius;
			extentZ = shape.type == SHAPE_SPHERE ? shape.radius : shape.radius + shape.halfHeight;
		}

		c1.x -= extentX; c1.y -= extentY; c1.z -= extentZ;
		c2.x += extentX; c2.y += extentY; c2.z += extentZ;

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
const cm_trace_t CM_TraceSphere( cm_t *cm, const Vector3 &start, const Vector3 &end, float radius, mnode_t *headnode, const cm_contents_t brushmask ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_SPHERE;
	shape.radius = radius;
	return CM_ShapeTrace( cm, start, end, shape, headnode, brushmask );
}

/**
/*	@brief	Trace a capsule through the collision model.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	radius	Capsule radius.
/*	@param	halfHeight	Half-height of the capsule's cylindrical section.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@return	Final trace result for the sweep.
**/
const cm_trace_t CM_TraceCapsule( cm_t *cm, const Vector3 &start, const Vector3 &end, float radius, float halfHeight, mnode_t *headnode, const cm_contents_t brushmask ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CAPSULE;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return CM_ShapeTrace( cm, start, end, shape, headnode, brushmask );
}

/**
/*	@brief	Trace a cylinder through the collision model.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	radius	Cylinder radius.
/*	@param	halfHeight	Half-height of the cylinder.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@return	Final trace result for the sweep.
**/
const cm_trace_t CM_TraceCylinder( cm_t *cm, const Vector3 &start, const Vector3 &end, float radius, float halfHeight, mnode_t *headnode, const cm_contents_t brushmask ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CYLINDER;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return CM_ShapeTrace( cm, start, end, shape, headnode, brushmask );
}

/**
/*	@brief	Sweep a shape through a transformed model space and return the world-space trace result.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	shape	Shape definition to sweep.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@param	origin	Translation applied before tracing in model space.
/*	@param	angles	Rotation applied before tracing in model space.
/*	@return	Final trace result transformed back into world space.
/*	@note	Converts the sweep into local model space, runs `CM_ShapeTrace`, then rotates the hit plane back into world space if necessary.
**/
const cm_trace_t CM_TransformedShapeTrace( cm_t *cm,
	const Vector3 &start, const Vector3 &end,
	const cm_trace_shape_t &shape,
	mnode_t *headnode, const cm_contents_t brushmask,
	const vec3_t origin, const vec3_t angles ) {

	vec3_t      start_l, end_l;
	vec3_t      axis[ 3 ];

	const bool isBoxHull = CM_IsBoundingBoxHullHeadnode( cm, headnode );
	const bool isOctagonHull = CM_IsOctagonBoxHullHeadnode( cm, headnode );
	const bool rotated = ( !isBoxHull && !isOctagonHull && !VectorEmpty( angles ) );

	/**
	/*	Apply hull-specific local offsets before tracing in model space.
	/*	Octagon boxes use an additional cylinder offset when available.
	**/
	if ( isOctagonHull ) {
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

	/**
	/*	Translate the sweep into the model's local origin before tracing.
	**/
	VectorSubtract( start_l, origin, start_l );
	VectorSubtract( end_l, origin, end_l );

	/**
	/*	Rotate the sweep into the model's frame of reference when the trace is not axis-aligned.
	**/
	if ( rotated ) {
		AnglesToAxis( angles, axis );
		RotatePoint( start_l, axis );
		RotatePoint( end_l, axis );
		TransposeAxis( axis ); // Convert model space axis to localToWorldAxis
	}

	cm_trace_t traceResult = {
		.contents = CONTENTS_NONE,
		.entityNumber = ENTITYNUM_NONE,
		.brushID = BRUSHID_NONE,
		.hitBodyID = HITBODYID_NONE,
		.fraction = 1.0,
		.endpos = end,
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
		return traceResult;
	}

	// sweep the shape through the model using world-normal expanded plane tests
	traceResult = CM_ShapeTraceEx( cm, start_l, end_l, shape, headnode, brushmask, rotated, axis );

	// rotate plane normal into the worlds frame of reference
	if ( traceResult.fraction < 1.0 ) {
		if ( rotated ) {
			RotatePoint( traceResult.plane.normal, axis );
		}
	}

	LerpVectorDP( start, end, traceResult.fraction, traceResult.endpos );

	return traceResult;
}

/**
/*	@brief	Trace a sphere through a transformed model space.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	radius	Sphere radius.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@param	origin	Translation applied before tracing in model space.
/*	@param	angles	Rotation applied before tracing in model space.
/*	@return	Final trace result transformed back into world space.
**/
const cm_trace_t CM_TransformedTraceSphere( cm_t *cm, const Vector3 &start, const Vector3 &end, float radius, mnode_t *headnode, const cm_contents_t brushmask, const vec3_t origin, const vec3_t angles ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_SPHERE;
	shape.radius = radius;
	return CM_TransformedShapeTrace( cm, start, end, shape, headnode, brushmask, origin, angles );
}

/**
/*	@brief	Trace a capsule through a transformed model space.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	radius	Capsule radius.
/*	@param	halfHeight	Half-height of the capsule's cylindrical section.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@param	origin	Translation applied before tracing in model space.
/*	@param	angles	Rotation applied before tracing in model space.
/*	@return	Final trace result transformed back into world space.
**/
const cm_trace_t CM_TransformedTraceCapsule( cm_t *cm, const Vector3 &start, const Vector3 &end, float radius, float halfHeight, mnode_t *headnode, const cm_contents_t brushmask, const vec3_t origin, const vec3_t angles ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CAPSULE;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return CM_TransformedShapeTrace( cm, start, end, shape, headnode, brushmask, origin, angles );
}

/**
/*	@brief	Trace a cylinder through a transformed model space.
/*	@param	cm	Collision model context.
/*	@param	start	Trace start point in world space.
/*	@param	end	Trace end point in world space.
/*	@param	radius	Cylinder radius.
/*	@param	halfHeight	Half-height of the cylinder.
/*	@param	headnode	Root node of the hull to trace against.
/*	@param	brushmask	Brush contents mask used to filter candidate brushes.
/*	@param	origin	Translation applied before tracing in model space.
/*	@param	angles	Rotation applied before tracing in model space.
/*	@return	Final trace result transformed back into world space.
**/
const cm_trace_t CM_TransformedTraceCylinder( cm_t *cm, const Vector3 &start, const Vector3 &end, float radius, float halfHeight, mnode_t *headnode, const cm_contents_t brushmask, const vec3_t origin, const vec3_t angles ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CYLINDER;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return CM_TransformedShapeTrace( cm, start, end, shape, headnode, brushmask, origin, angles );
}

/**
/*	@brief	Analytically sweep one axial shape against another using a capsule-style Minkowski sum.
/*	@param	start	Initial center of the moving shape.
/*	@param	shapeA	Moving shape definition.
/*	@param	end	End point of the moving shape sweep.
/*	@param	centerB	Static shape center in the same space as the sweep.
/*	@param	shapeB	Static shape definition.
/*	@return	Trace result describing the earliest intersection, if any.
/*	@note	Uses a small set of analytic ray and sphere-style tests against the combined radius and half-height.
**/
const cm_trace_t CM_AnalyticalShapeSweep(
	const Vector3 &start, const cm_trace_shape_t &shapeA, const Vector3 &end,
	const Vector3 &centerB, const cm_trace_shape_t &shapeB ) {
	/**
	*	Initialize the trace result to a no-hit state.
	**/
	cm_trace_t trace = {};
	trace.fraction = 1.0f;
	trace.allsolid = false;
	trace.startsolid = false;

	bool pureCylinder = ( shapeA.type == SHAPE_CYLINDER && shapeB.type == SHAPE_CYLINDER );

	/**
	/*	Reduce both shapes into a combined volume using Minkowski sum extents.
	**/
	float rA = shapeA.radius;
	float hA = shapeA.halfHeight;
	if ( shapeA.type == SHAPE_SPHERE ) {
		hA = 0.0f;
	}

	float rB = shapeB.radius;
	float hB = shapeB.halfHeight;
	if ( shapeB.type == SHAPE_SPHERE ) {
		hB = 0.0f;
	}

	float R = rA + rB;
	float H = hA + hB;

	/**
	/*	Express the sweep ray in the stationary shape's local space.
	**/
	Vector3 V = end - start;
	Vector3 P0_local = start - centerB;

	/**
	/*	Detect whether the sweep starts already inside the combined volume.
	**/
	float R_shrunk = std::max( 0.0f, R - ( float )DIST_EPSILON );
	float H_shrunk = std::max( 0.0f, H - ( float )DIST_EPSILON );

	auto IsInside = [&]( const Vector3 &point ) {
		Vector3 local = point - centerB;
		float distSqXY = local.x * local.x + local.y * local.y;
		if ( distSqXY <= R_shrunk * R_shrunk && local.z >= -H_shrunk && local.z <= H_shrunk ) {
			return true;
		}
		if ( local.z > H_shrunk ) {
			float dz = local.z - H_shrunk;
			if ( pureCylinder ) {
				if ( distSqXY <= R_shrunk * R_shrunk && dz <= 0.0f ) return true;
			} else {
				if ( distSqXY + dz * dz <= R_shrunk * R_shrunk ) return true;
			}
		}
		if ( local.z < -H_shrunk ) {
			float dz = local.z - ( -H_shrunk );
			if ( pureCylinder ) {
				if ( distSqXY <= R_shrunk * R_shrunk && dz >= 0.0f ) return true;
			} else {
				if ( distSqXY + dz * dz <= R_shrunk * R_shrunk ) return true;
			}
		}
		return false;
	};

	if ( IsInside( start ) ) {
		if ( IsInside( end ) ) {
			trace.allsolid = true;
			trace.startsolid = true;
			trace.fraction = 0.0f;
			return trace;
		}
		trace.startsolid = true;
		trace.fraction = 1.0f;
		return trace;
	}

	float tHit = 1.0f;
	bool hit = false;
	Vector3 normal = { 0, 0, 0 };

	/**
	/*	Test the cylindrical body first so side impacts can be resolved early.
	**/
	float a = V.x * V.x + V.y * V.y;
	float b = 2.0f * ( P0_local.x * V.x + P0_local.y * V.y );
	float c = P0_local.x * P0_local.x + P0_local.y * P0_local.y - R * R;

	float r_shrunk = std::max( 0.0f, R - ( float )DIST_EPSILON );
	float c_tol = r_shrunk * r_shrunk - R * R;

	if ( a > 0.0001f && !( c >= c_tol && b >= 0.0f ) ) {
		float discriminant = b * b - 4.0f * a * c;
		if ( discriminant >= 0.0f ) {
			float sqrtD = std::sqrt( discriminant );
			float t1 = ( -b - sqrtD ) / ( 2.0f * a );

			if ( t1 >= 0.0f && t1 < tHit ) {
				float zHit = P0_local.z + V.z * t1;
				if ( zHit >= -H && zHit <= H ) {
					tHit = t1;
					hit = true;
					normal = Vector3{ P0_local.x + V.x * t1, P0_local.y + V.y * t1, 0.0f };
					float nLen = std::sqrt( normal.x * normal.x + normal.y * normal.y );
					if ( nLen > 0.0001f ) {
						normal.x /= nLen;
						normal.y /= nLen;
					}
				}
			}
		}
	}

	if ( pureCylinder ) {
		/**
		/*	Test the top flat face of the cylinder.
		**/
		if ( V.z < 0.0f && P0_local.z > H ) {
			float t1 = ( H - P0_local.z ) / V.z;
			if ( t1 >= 0.0f && t1 < tHit ) {
				float xHit = P0_local.x + V.x * t1;
				float yHit = P0_local.y + V.y * t1;
				if ( xHit * xHit + yHit * yHit <= R * R ) {
					tHit = t1;
					hit = true;
					normal = Vector3{ 0.0f, 0.0f, 1.0f };
				}
			}
		}
	} else {
		/**
		/*	Test the top hemisphere when the cylinder did not already produce the closest hit.
		**/
		Vector3 topCenter{ 0.0f, 0.0f, H };
		Vector3 P0_top = P0_local - topCenter;
		float a_sph = V.x * V.x + V.y * V.y + V.z * V.z;
		float b_sph = 2.0f * ( P0_top.x * V.x + P0_top.y * V.y + P0_top.z * V.z );
		float c_sph = P0_top.x * P0_top.x + P0_top.y * P0_top.y + P0_top.z * P0_top.z - R * R;

		float r_shrunk = std::max( 0.0f, R - ( float )DIST_EPSILON );
		float c_tol = r_shrunk * r_shrunk - R * R;

		if ( a_sph > 0.0001f && !( c_sph >= c_tol && b_sph >= 0.0f ) ) {
			float disc = b_sph * b_sph - 4.0f * a_sph * c_sph;
			if ( disc >= 0.0f ) {
				float sqrtD = std::sqrt( disc );
				float t1 = ( -b_sph - sqrtD ) / ( 2.0f * a_sph );
				if ( t1 >= 0.0f && t1 < tHit ) {
					float zHitLocal = P0_top.z + V.z * t1;
					if ( zHitLocal >= 0.0f ) {
						tHit = t1;
						hit = true;
						normal = P0_top + V * t1;
						float nLen = std::sqrt( normal.x * normal.x + normal.y * normal.y + normal.z * normal.z );
						if ( nLen > 0.0001f ) {
							normal.x /= nLen;
							normal.y /= nLen;
							normal.z /= nLen;
						}
					}
				}
			}
		}
	}

	if ( pureCylinder ) {
		/**
		/*	Test the bottom flat face of the cylinder.
		**/
		if ( V.z > 0.0f && P0_local.z < -H ) {
			float t1 = ( -H - P0_local.z ) / V.z;
			if ( t1 >= 0.0f && t1 < tHit ) {
				float xHit = P0_local.x + V.x * t1;
				float yHit = P0_local.y + V.y * t1;
				if ( xHit * xHit + yHit * yHit <= R * R ) {
					tHit = t1;
					hit = true;
					normal = Vector3{ 0.0f, 0.0f, -1.0f };
				}
			}
		}
	} else {
		/**
		/*	Test the bottom hemisphere using the same local-space sphere intersection logic.
		**/
		Vector3 botCenter{ 0.0f, 0.0f, -H };
		Vector3 P0_bot = P0_local - botCenter;
		float a_sph = V.x * V.x + V.y * V.y + V.z * V.z;
		float b_sph = 2.0f * ( P0_bot.x * V.x + P0_bot.y * V.y + P0_bot.z * V.z );
		float c_sph = P0_bot.x * P0_bot.x + P0_bot.y * P0_bot.y + P0_bot.z * P0_bot.z - R * R;

		float r_shrunk = std::max( 0.0f, R - ( float )DIST_EPSILON );
		float c_tol = r_shrunk * r_shrunk - R * R;

		if ( a_sph > 0.0001f && !( c_sph >= c_tol && b_sph >= 0.0f ) ) {
			float disc = b_sph * b_sph - 4.0f * a_sph * c_sph;
			if ( disc >= 0.0f ) {
				float sqrtD = std::sqrt( disc );
				float t1 = ( -b_sph - sqrtD ) / ( 2.0f * a_sph );
				if ( t1 >= 0.0f && t1 < tHit ) {
					float zHitLocal = P0_bot.z + V.z * t1;
					if ( zHitLocal <= 0.0f ) {
						tHit = t1;
						hit = true;
						normal = P0_bot + V * t1;
						float nLen = std::sqrt( normal.x * normal.x + normal.y * normal.y + normal.z * normal.z );
						if ( nLen > 0.0001f ) {
							normal.x /= nLen;
							normal.y /= nLen;
							normal.z /= nLen;
						}
					}
				}
			}
		}
	}

	if ( hit ) {
		/**
		/*	Back the hit fraction off slightly so the trace stays numerically stable.
		**/
		float dot = V.x * normal.x + V.y * normal.y + V.z * normal.z;
		if ( dot < -0.0001f ) {
			tHit -= ( float )DIST_EPSILON / -dot;
		}
		if ( tHit < 0.0f ) {
			tHit = 0.0f;
		}

		trace.fraction = tHit;
		trace.plane.normal[ 0 ] = normal.x;
		trace.plane.normal[ 1 ] = normal.y;
		trace.plane.normal[ 2 ] = normal.z;
	}

	return trace;
}
