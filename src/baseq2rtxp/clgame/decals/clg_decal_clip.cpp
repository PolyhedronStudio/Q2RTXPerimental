/********************************************************************
*
*
*    ClientGame: Decal Clip Helpers.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_world.h"
#include "clgame/decals/clg_decal_clip.h"

//! Minimum alignment required between decal projection normal and candidate surface normal.
static constexpr float CLG_DECAL_MIN_FACING_DOT = 0.50f;
//! Small depth slack to avoid precision edge rejection at clip volume boundaries.
static constexpr float CLG_DECAL_DEPTH_EPSILON = 0.25f;
//! Maximum projected-depth difference to treat two candidates as the same receiving plane.
static constexpr float CLG_DECAL_CANDIDATE_MERGE_DEPTH_EPSILON = 0.75f;
//! Minimum normal alignment to merge duplicate candidates.
static constexpr float CLG_DECAL_CANDIDATE_MERGE_NORMAL_DOT = 0.98f;
//! Epsilon used when intersecting the decal clip volume with a receiver plane.
static constexpr float CLG_DECAL_CLIP_PLANE_EPSILON = 0.05f;
//! Epsilon used when merging duplicate polygon vertices from edge intersections.
static constexpr float CLG_DECAL_CLIP_VERTEX_MERGE_EPSILON = 0.05f;
//! Broad-phase expansion around decal OBB bounds for robust leaf/node overlap queries.
static constexpr float CLG_DECAL_BOUNDS_QUERY_EXPANSION = 2.0f;
//! Radius used for impact-local leaf lookup fallback when broad-phase gather returns no faces.
static constexpr float CLG_DECAL_IMPACT_LOOKUP_RADIUS = 6.0f;
//! Edge slack used when deciding whether the snapped impact still belongs to a BSP face winding.
static constexpr float CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON = 0.5f;
//! Lift used when probing plane-only fallback corners back onto real collision.
static constexpr float CLG_DECAL_PLANE_FALLBACK_TRACE_LIFT = 0.5f;
//! Maximum acceptable offset from the intended receiver plane during plane-only support probes.
static constexpr float CLG_DECAL_PLANE_FALLBACK_SUPPORT_EPSILON = 1.0f;



/**
*    @brief  Builds an orthonormal decal basis from projected forward vector.
*    @param  forward Projected forward vector.
*    @param  outRight [out] Tangent axis.
*    @param  outUp [out] Bitangent axis.
*    @param  outForward [out] Normalized forward axis.
**/
static void CLG_DecalClip_BuildBasis( const vec3_t forward, vec3_t outRight, vec3_t outUp, vec3_t outForward );

/**
*    @brief  Builds a stable tangent basis on a receiver plane aligned to the decal projection.
*    @param  context Decal clip context.
*    @param  surfaceNormal Receiver plane normal.
*    @param  outTangent [out] Tangent axis projected onto receiver plane.
*    @param  outBitangent [out] Bitangent axis projected onto receiver plane.
**/
static void CLG_DecalClip_BuildSurfaceBasis( const clg_decal_clip_context_t &context, const vec3_t surfaceNormal, vec3_t outTangent, vec3_t outBitangent );

/**
*    @brief  Builds the eight world-space corners of the oriented decal clip box.
*    @param  context Decal clip context.
*    @param  outCorners [out] World-space clip box corners.
**/
static void CLG_DecalClip_BuildVolumeCorners( const clg_decal_clip_context_t &context, vec3_t outCorners[ 8 ] );

/**
*    @brief  Builds world-space AABB bounds enclosing the oriented decal volume.
*    @param  context Decal clip context.
*    @param  outMins [out] Minimum world-space bounds.
*    @param  outMaxs [out] Maximum world-space bounds.
**/
static void CLG_DecalClip_BuildVolumeBounds( const clg_decal_clip_context_t &context, vec3_t outMins, vec3_t outMaxs );

/**
*    @brief  Builds a simple 4-vertex quad on receiver plane aligned to surface basis.
*    @param  context Decal clip context with spawn/volume info.
*    @param  surface Surface with normal, tangent, and bitangent already computed.
*    @param  outPolygon [out] Quad polygon (4 vertices) aligned to surface axes.
*    @return True if quad was successfully created.
**/
static bool CLG_DecalClip_BuildPlaneQuad( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon );

/**
*    @brief  Computes signed distance between a point and a receiver plane.
*    @param  planePoint One point on the plane.
*    @param  planeNormal Plane normal.
*    @param  point Point to classify.
*    @return Signed distance from plane.
**/
static float CLG_DecalClip_ComputePlaneDistance( const vec3_t planePoint, const vec3_t planeNormal, const vec3_t point );

/**
*    @brief  Appends one unique polygon vertex if it is not already present.
*    @param  point Candidate vertex.
*    @param  outPolygon [out] Polygon receiving the unique point.
*    @return True when point was appended or already existed.
**/
static bool CLG_DecalClip_TryAppendUniqueVertex( const vec3_t point, clg_decal_clip_polygon_t *outPolygon );

/**
*    @brief  Computes decal UV coordinates for one world-space point.
*    @param  context Decal clip context.
*    @param  point World-space point on the clipped polygon.
*    @param  outUv [out] Decal UV inside 0..1 footprint.
**/
static void CLG_DecalClip_ProjectPointToUv( const clg_decal_clip_context_t &context, const vec3_t point, vec2_t outUv );

/**
*    @brief  Sorts a convex clipped polygon around its centroid for stable fan triangulation.
*    @param  surface Receiver surface holding tangent basis.
*    @param  inOutPolygon [in/out] Polygon to reorder in-place.
**/
static void CLG_DecalClip_SortPolygonVertices( const clg_world_surface_t *surface, clg_decal_clip_polygon_t *inOutPolygon );

/**
*    @brief  Returns the effective outward normal for one BSP face.
*    @param  face BSP face whose oriented normal should be returned.
*    @param  outNormal [out] Effective face normal with draw-side applied.
**/
static void CLG_DecalClip_GetBspFaceNormal( const mface_t *face, vec3_t outNormal );

/**
*    @brief  Aligns one receiver normal to the same hemisphere as the decal projection.
*    @param  context Decal clip context.
*    @param  inOutNormal [in/out] Receiver normal to orient consistently.
**/
static void CLG_DecalClip_AlignNormalToProjection( const clg_decal_clip_context_t &context, vec3_t inOutNormal );

/**
*    @brief  Appends one candidate surface after facing, depth, and duplicate checks.
*    @param  context Decal clip context.
*    @param  origin Representative point on the candidate receiver.
*    @param  normal Receiver normal.
*    @param  containsImpactPoint True when the original impact projects inside this receiver.
*    @param  bspFace Optional concrete BSP face for polygon clipping.
*    @param  entityNumber Receiver entity number for inline brush-model fallbacks.
*    @param  outSurfaces Destination candidate array.
*    @param  maxSurfaces Maximum writable candidates.
*    @param  inOutCount [in/out] Candidate count.
*    @return True when a new candidate was appended.
**/
static bool CLG_DecalClip_TryAddSurfaceCandidate( const clg_decal_clip_context_t &context, const vec3_t origin, const vec3_t normal, const bool containsImpactPoint, const mface_t *bspFace, const int32_t entityNumber, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount );

/**
*    @brief  Appends one BSP face as a candidate receiver when it passes broad-phase filters.
*    @param  context Decal clip context.
*    @param  face BSP face to consider.
*    @param  referencePoint Point used to project onto the face plane.
*    @param  outSurfaces Destination candidate array.
*    @param  maxSurfaces Maximum writable candidates.
*    @param  inOutCount [in/out] Candidate count.
*    @return True when a new candidate was appended.
**/
static bool CLG_DecalClip_TryAddBspFaceCandidate( const clg_decal_clip_context_t &context, const mface_t *face, const vec3_t referencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount );

/**
*    @brief  Returns true when [start, start + bytes) lies fully inside [base, base + spanBytes).
*    @param  start Address range start.
*    @param  bytes Address range length in bytes.
*    @param  base Span base address.
*    @param  spanBytes Span length in bytes.
*    @return True when range is fully contained and arithmetic is overflow-safe.
**/
static bool CLG_DecalClip_IsAddressRangeInsideSpan( const void *start, const size_t bytes, const void *base, const size_t spanBytes );

/**
*    @brief  Returns true when one BSP face pointer lies inside world BSP face storage.
*    @param  worldBsp World BSP cache.
*    @param  face Candidate face pointer.
*    @return True when the pointer belongs to the world BSP face array.
**/
static bool CLG_DecalClip_IsFacePointerInWorldBsp( const bsp_t *worldBsp, const mface_t *face );

/**
*    @brief  Returns true when one BSP face has safe surfedge/edge/vertex references.
*    @param  worldBsp World BSP cache.
*    @param  face Candidate face pointer.
*    @return True when all face geometry references are valid for traversal.
**/
static bool CLG_DecalClip_IsValidBspFaceGeometry( const bsp_t *worldBsp, const mface_t *face );

/**
*    @brief  Returns true when two world-space AABBs overlap.
*    @param  minsA Minimum bounds of AABB A.
*    @param  maxsA Maximum bounds of AABB A.
*    @param  minsB Minimum bounds of AABB B.
*    @param  maxsB Maximum bounds of AABB B.
*    @return True when bounds overlap on all axes.
**/
static bool CLG_DecalClip_DoBoundsOverlap( const vec3_t minsA, const vec3_t maxsA, const vec3_t minsB, const vec3_t maxsB );

/**
*    @brief  Gathers candidate BSP faces from leaves overlapped by the decal bounds.
*    @param  context Decal clip context.
*    @param  queryMins Minimum world-space bounds.
*    @param  queryMaxs Maximum world-space bounds.
*    @param  outSurfaces Destination candidate array.
*    @param  maxSurfaces Maximum writable candidates.
*    @param  inOutCount [in/out] Candidate count.
**/
static void CLG_DecalClip_GatherLeafFaceCandidates( const clg_decal_clip_context_t &context, const vec3_t queryMins, const vec3_t queryMaxs, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount );

/**
*    @brief  Recursively gathers candidate BSP faces from intersecting BSP nodes.
*    @param  context Decal clip context.
*    @param  node Current BSP node to test.
*    @param  queryMins Minimum world-space bounds.
*    @param  queryMaxs Maximum world-space bounds.
*    @param  outSurfaces Destination candidate array.
*    @param  maxSurfaces Maximum writable candidates.
*    @param  inOutCount [in/out] Candidate count.
**/
static void CLG_DecalClip_GatherNodeFaceCandidatesRecursive( const clg_decal_clip_context_t &context, const mnode_t *node, const vec3_t queryMins, const vec3_t queryMaxs, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount );

/**
*    @brief  Scores one nearby BSP face as a fallback receiver candidate near the impact point.
*    @param  context Decal clip context.
*    @param  face BSP face to evaluate.
*    @param  maxPlaneDistance Relaxed local plane-distance budget from impact point.
*    @param  inOutBestFace [in/out] Best scored face so far.
*    @param  inOutBestPlaneDistance [in/out] Best scored plane distance so far.
*    @param  inOutBestImpactInset [in/out] Best scored impact inset inside the face winding.
*    @param  inOutBestFacingAlignment [in/out] Best scored facing alignment so far.
**/
static void CLG_DecalClip_ConsiderImpactAnchoredFaceCandidate( const clg_decal_clip_context_t &context, const mface_t *face, const float maxPlaneDistance, const mface_t **inOutBestFace, float *inOutBestPlaneDistance, float *inOutBestImpactInset, float *inOutBestFacingAlignment );

/**
*    @brief  Recursively scans intersecting BSP nodes for the best impact-local fallback face.
*    @param  context Decal clip context.
*    @param  node Current BSP node to test.
*    @param  queryMins Minimum world-space bounds around the impact.
*    @param  queryMaxs Maximum world-space bounds around the impact.
*    @param  maxPlaneDistance Relaxed local plane-distance budget from impact point.
*    @param  inOutBestFace [in/out] Best scored face so far.
*    @param  inOutBestPlaneDistance [in/out] Best scored plane distance so far.
*    @param  inOutBestImpactInset [in/out] Best scored impact inset inside the face winding.
*    @param  inOutBestFacingAlignment [in/out] Best scored facing alignment so far.
**/
static void CLG_DecalClip_FindBestImpactAnchoredFaceRecursive( const clg_decal_clip_context_t &context, const mnode_t *node, const vec3_t queryMins, const vec3_t queryMaxs, const float maxPlaneDistance, const mface_t **inOutBestFace, float *inOutBestPlaneDistance, float *inOutBestImpactInset, float *inOutBestFacingAlignment );

/**
*    @brief  Appends one impact-anchored fallback face near decal origin when broad-phase gather misses.
*    @param  context Decal clip context.
*    @param  outSurfaces Destination candidate array.
*    @param  maxSurfaces Maximum writable candidates.
*    @param  inOutCount [in/out] Candidate count.
*    @param  outSelectedPlaneDistance [out] Selected fallback face plane distance from impact origin.
*    @return True when one fallback face candidate was appended.
**/
static bool CLG_DecalClip_GatherImpactAnchoredFallbackCandidate( const clg_decal_clip_context_t &context, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount, float *outSelectedPlaneDistance );

/**
*    @brief  Builds a world-space polygon from one BSP face.
*    @param  face BSP face whose winding should be expanded.
*    @param  outPolygon [out] Polygon populated from face surfedges.
*    @return True when the face produced a valid polygon.
**/
static bool CLG_DecalClip_BuildPolygonFromBspFace( const mface_t *face, clg_decal_clip_polygon_t *outPolygon );

/**
*    @brief  Projects one point directly onto a BSP face plane.
*    @param  face BSP face providing the plane.
*    @param  point World-space point to project.
*    @param  outProjectedPoint [out] Projected point on the face plane.
*    @return True when the face supplied a valid plane.
**/
static bool CLG_DecalClip_ProjectPointOntoBspFacePlane( const mface_t *face, const vec3_t point, vec3_t outProjectedPoint );

/**
*    @brief  Computes how far one projected point lies inside a convex BSP face winding.
*    @param  face BSP face whose winding should be tested.
*    @param  projectedPoint Point already projected onto the face plane.
*    @param  faceNormal Effective face normal with planeback applied.
*    @param  outInset [out] Minimum signed inset to the face edges in world units.
*    @return True when the face produced a usable convex winding for containment tests.
**/
static bool CLG_DecalClip_ComputeProjectedPointInsetToFace( const mface_t *face, const vec3_t projectedPoint, const vec3_t faceNormal, float *outInset );


/**
*    @brief  Clips a convex polygon against one half-space.
*    @param  inPolygon Source polygon.
*    @param  planePoint Any point on the clip plane.
*    @param  planeNormal Outward-facing clip plane normal.
*    @param  outPolygon [out] Destination polygon after clipping.
*    @return True when the clipped polygon still has vertices.
**/
static bool CLG_DecalClip_ClipPolygonAgainstPlane( const clg_decal_clip_polygon_t &inPolygon, const vec3_t planePoint, const vec3_t planeNormal, clg_decal_clip_polygon_t *outPolygon );

/**
*    @brief  Clips a BSP face polygon against the six planes of the oriented decal volume.
*    @param  context Decal clip context.
*    @param  surface Candidate surface carrying the matched BSP face.
*    @param  outPolygon [out] Clipped polygon.
*    @return True when a non-empty clipped polygon remains.
**/
static bool CLG_DecalClip_ClipBspFaceToDecalVolume( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon );



/**
*    @brief  Returns signed depth of a world point in decal projection space.
*    @param  context Decal clip context.
*    @param  worldPoint Point in world space.
**/
static float CLG_DecalClip_ComputeSignedDepth( const clg_decal_clip_context_t &context, const vec3_t worldPoint ) {
    vec3_t toPoint = {};
    VectorSubtract( worldPoint, context.spawn.origin, toPoint );
    return DotProduct( toPoint, context.basisForward );
}

static float CLG_DecalClip_ComputePlaneDistance( const vec3_t planePoint, const vec3_t planeNormal, const vec3_t point ) {
    vec3_t toPoint = {};
    VectorSubtract( point, planePoint, toPoint );
    return DotProduct( toPoint, planeNormal );
}

static void CLG_DecalClip_AlignNormalToProjection( const clg_decal_clip_context_t &context, vec3_t inOutNormal ) {
    if ( !inOutNormal ) {
        return;
    }

    if ( DotProduct( inOutNormal, context.basisForward ) < 0.0f ) {
        inOutNormal[ 0 ] = -inOutNormal[ 0 ];
        inOutNormal[ 1 ] = -inOutNormal[ 1 ];
        inOutNormal[ 2 ] = -inOutNormal[ 2 ];
    }
}

/**
*    @brief  Returns the live client brush-model entity to use for inline decal support.
*    @param  entityNumber Impacted entity number carried by the temp event.
*    @return Brush-model centity when inline clipping should target an entity, nullptr otherwise.
**/
static const centity_t *CLG_DecalClip_GetInlineBrushEntity( const int32_t entityNumber ) {
    if ( entityNumber <= ENTITYNUM_WORLD || entityNumber >= MAX_EDICTS ) {
        return nullptr;
    }

    if ( !clgi.GetEntityHullNode ) {
        return nullptr;
    }

    const centity_t *entity = &clg_entities[ entityNumber ];
    if ( entity->current.solid != BOUNDS_BRUSHMODEL ) {
        return nullptr;
    }

    if ( clgi.GetEntityHullNode( entity ) == nullptr ) {
        return nullptr;
    }

    return entity;
}

/**
*    @brief  Returns true when surface is front-facing enough for the decal projection.
*    @param  context Decal clip context.
*    @param  surfaceNormal Candidate surface normal.
**/
static bool CLG_DecalClip_IsSurfaceFacingProjection( const clg_decal_clip_context_t &context, const vec3_t surfaceNormal ) {
    // Event normals can arrive in either orientation depending on source payload,
    // so accept either sign while still requiring meaningful alignment.
    const float facingDot = DotProduct( surfaceNormal, context.basisForward );
    return ( fabsf( facingDot ) >= CLG_DECAL_MIN_FACING_DOT );
}

/**
*    @brief  Returns true when a signed depth lies inside decal clip volume bounds.
*    @param  context Decal clip context.
*    @param  signedDepth Signed depth in decal projection space.
**/
static bool CLG_DecalClip_IsDepthInsideVolume( const clg_decal_clip_context_t &context, const float signedDepth ) {
    return ( fabsf( signedDepth ) <= ( context.halfDepth + CLG_DECAL_DEPTH_EPSILON ) );
}

/**
*	@brief	Probe whether one plane-only fallback point is actually supported by collision.
*	@param	context		Decal clip context.
*	@param	surface		Plane-only fallback surface basis/normal.
*	@param	point		Candidate point on the receiver plane.
*	@param	outSupportedPoint	[out] Collision-supported point projected back from trace.
*	@return	True when the point is supported by a nearby surface aligned to the receiver plane.
**/
static bool CLG_DecalClip_SamplePlaneFallbackSupport( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, const vec3_t point, vec3_t outSupportedPoint ) {
    if ( !surface || !outSupportedPoint ) {
        return false;
    }

    const centity_t *clipEntity = CLG_DecalClip_GetInlineBrushEntity( surface->entityNumber );

    vec3_t traceStart = {};
    vec3_t traceEnd = {};
    VectorCopy( point, traceStart );
    VectorMA( traceStart, CLG_DECAL_PLANE_FALLBACK_TRACE_LIFT, surface->normal, traceStart );
    VectorCopy( point, traceEnd );
    VectorMA( traceEnd, -( context.halfDepth + CLG_DECAL_PLANE_FALLBACK_TRACE_LIFT + CLG_DECAL_DEPTH_EPSILON ), surface->normal, traceEnd );

    const cm_trace_t trace = CLG_Clip( traceStart, nullptr, nullptr, traceEnd, clipEntity, CM_CONTENTMASK_SOLID );
    if ( trace.allsolid || trace.startsolid || trace.fraction >= 1.0 ) {
        return false;
    }

    if ( fabsf( DotProduct( trace.plane.normal, surface->normal ) ) < CLG_DECAL_MIN_FACING_DOT ) {
        return false;
    }

    /**
    *    Convert the trace endpoint into the legacy vec3_t layout before feeding it to
    *    clip helpers that still accept raw float-array vectors.
    **/
    vec3_t traceEndPoint = {};
    VectorCopy( trace.endpos, traceEndPoint );

    const float planeOffset = fabsf( CLG_DecalClip_ComputePlaneDistance( point, surface->normal, traceEndPoint ) );
    if ( planeOffset > CLG_DECAL_PLANE_FALLBACK_SUPPORT_EPSILON ) {
        return false;
    }

    VectorCopy( trace.endpos, outSupportedPoint );
    return true;
}

/**
*	@brief	Bisect one fallback quad edge to find the last supported point before it leaves the receiver.
*	@param	context		Decal clip context.
*	@param	surface		Plane-only fallback surface basis/normal.
*	@param	supportedCorner	Known supported edge endpoint.
*	@param	unsupportedCorner	Known unsupported edge endpoint.
*	@param	outEdgePoint	[out] Approximated boundary point that still lies on supported collision.
*	@return	True when an edge boundary point was found.
**/
static bool CLG_DecalClip_FindPlaneFallbackEdgePoint( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, const vec3_t supportedCorner, const vec3_t unsupportedCorner, vec3_t outEdgePoint ) {
    if ( !surface || !outEdgePoint ) {
        return false;
    }

    vec3_t low = {};
    vec3_t high = {};
    vec3_t bestSupportedPoint = {};
    VectorCopy( supportedCorner, low );
    VectorCopy( unsupportedCorner, high );

    if ( !CLG_DecalClip_SamplePlaneFallbackSupport( context, surface, low, bestSupportedPoint ) ) {
        return false;
    }

    for ( int32_t iteration = 0; iteration < 6; iteration++ ) {
        vec3_t mid = {};
        vec3_t midSupportedPoint = {};
        for ( int32_t axis = 0; axis < 3; axis++ ) {
            mid[ axis ] = 0.5f * ( low[ axis ] + high[ axis ] );
        }

        if ( CLG_DecalClip_SamplePlaneFallbackSupport( context, surface, mid, midSupportedPoint ) ) {
            VectorCopy( mid, low );
            VectorCopy( midSupportedPoint, bestSupportedPoint );
        } else {
            VectorCopy( mid, high );
        }
    }

    VectorCopy( bestSupportedPoint, outEdgePoint );
    return true;
}

/**
*	@brief	Build a simple 4-vertex quad on the receiver plane aligned to surface basis axes.
*	@param	context		Decal clip context with spawn/volume info.
*	@param	surface		Surface with normal, tangent, and bitangent already computed.
*	@param	outPolygon	[out] Quad polygon (4 vertices) aligned to surface axes.
*	@return	True if quad was successfully created.
*	@note	For plane-only candidates (no BSP face), this creates a properly-oriented
*			decal footprint that avoids stretching/misalignment on sloped surfaces by
*			aligning the quad directly to the surface's tangent/bitangent basis.
**/
static bool CLG_DecalClip_BuildPlaneQuad( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !surface || !outPolygon ) {
        return false;
    }

    memset( outPolygon, 0, sizeof( *outPolygon ) );
    const float halfSize = context.halfSize;
    vec3_t corners[ 4 ] = {};
    vec3_t supportedPoints[ 4 ] = {};
    bool cornerSupported[ 4 ] = { false, false, false, false };

    // Corner 0: +tangent +bitangent
    VectorCopy( surface->origin, corners[ 0 ] );
    VectorMA( corners[ 0 ], halfSize, surface->tangent, corners[ 0 ] );
    VectorMA( corners[ 0 ], halfSize, surface->bitangent, corners[ 0 ] );

    // Corner 1: -tangent +bitangent
    VectorCopy( surface->origin, corners[ 1 ] );
    VectorMA( corners[ 1 ], -halfSize, surface->tangent, corners[ 1 ] );
    VectorMA( corners[ 1 ], halfSize, surface->bitangent, corners[ 1 ] );

    // Corner 2: -tangent -bitangent
    VectorCopy( surface->origin, corners[ 2 ] );
    VectorMA( corners[ 2 ], -halfSize, surface->tangent, corners[ 2 ] );
    VectorMA( corners[ 2 ], -halfSize, surface->bitangent, corners[ 2 ] );

    // Corner 3: +tangent -bitangent
    VectorCopy( surface->origin, corners[ 3 ] );
    VectorMA( corners[ 3 ], halfSize, surface->tangent, corners[ 3 ] );
    VectorMA( corners[ 3 ], -halfSize, surface->bitangent, corners[ 3 ] );

    /**
    *	Probe each intended corner back into collision so plane-only fallback quads do not
    *	hang off unsupported edges when the exact BSP face could not be resolved.
    **/
    for ( int32_t cornerIndex = 0; cornerIndex < 4; cornerIndex++ ) {
        cornerSupported[ cornerIndex ] = CLG_DecalClip_SamplePlaneFallbackSupport( context, surface, corners[ cornerIndex ], supportedPoints[ cornerIndex ] );
    }

    for ( int32_t edgeIndex = 0; edgeIndex < 4; edgeIndex++ ) {
        const int32_t nextIndex = ( edgeIndex + 1 ) % 4;
        if ( cornerSupported[ edgeIndex ] ) {
            if ( !CLG_DecalClip_TryAppendUniqueVertex( supportedPoints[ edgeIndex ], outPolygon ) ) {
                return false;
            }
        }

        if ( cornerSupported[ edgeIndex ] == cornerSupported[ nextIndex ] ) {
            continue;
        }

        const int32_t supportedIndex = cornerSupported[ edgeIndex ] ? edgeIndex : nextIndex;
        const int32_t unsupportedIndex = cornerSupported[ edgeIndex ] ? nextIndex : edgeIndex;
        vec3_t edgePoint = {};
        if ( CLG_DecalClip_FindPlaneFallbackEdgePoint( context, surface, corners[ supportedIndex ], corners[ unsupportedIndex ], edgePoint ) ) {
            if ( !CLG_DecalClip_TryAppendUniqueVertex( edgePoint, outPolygon ) ) {
                return false;
            }
        }
    }

    return ( outPolygon->vertexCount >= 3 );
}

static void CLG_DecalClip_BuildSurfaceBasis( const clg_decal_clip_context_t &context, const vec3_t surfaceNormal, vec3_t outTangent, vec3_t outBitangent ) {
    /**
    *    Project the rotated decal right axis onto the receiver plane so polygon winding
    *    stays stable and dynamic decal rotation affects the final mesh.
    **/
    const float rightDot = DotProduct( context.basisRight, surfaceNormal );
    outTangent[ 0 ] = context.basisRight[ 0 ] - ( surfaceNormal[ 0 ] * rightDot );
    outTangent[ 1 ] = context.basisRight[ 1 ] - ( surfaceNormal[ 1 ] * rightDot );
    outTangent[ 2 ] = context.basisRight[ 2 ] - ( surfaceNormal[ 2 ] * rightDot );

    /**
    *    If projection collapses near parallel, fall back to the rotated decal up axis.
    **/
    if ( VectorLength( outTangent ) <= 0.001f ) {
        const float upDot = DotProduct( context.basisUp, surfaceNormal );
        outTangent[ 0 ] = context.basisUp[ 0 ] - ( surfaceNormal[ 0 ] * upDot );
        outTangent[ 1 ] = context.basisUp[ 1 ] - ( surfaceNormal[ 1 ] * upDot );
        outTangent[ 2 ] = context.basisUp[ 2 ] - ( surfaceNormal[ 2 ] * upDot );
    }

    /**
    *    As a final fallback, build any valid plane basis from the surface normal.
    **/
    if ( VectorLength( outTangent ) <= 0.001f ) {
        vec3_t forward = {};
        CLG_DecalClip_BuildBasis( surfaceNormal, outTangent, outBitangent, forward );
        return;
    }

    VectorNormalize( outTangent );
    CrossProduct( surfaceNormal, outTangent, outBitangent );
    VectorNormalize( outBitangent );
}

static void CLG_DecalClip_BuildVolumeCorners( const clg_decal_clip_context_t &context, vec3_t outCorners[ 8 ] ) {
    for ( int32_t i = 0; i < 8; i++ ) {
        const float signRight = ( ( i & 1 ) != 0 ) ? 1.0f : -1.0f;
        const float signUp = ( ( i & 2 ) != 0 ) ? 1.0f : -1.0f;
        const float signForward = ( ( i & 4 ) != 0 ) ? 1.0f : -1.0f;

        VectorCopy( context.spawn.origin, outCorners[ i ] );
        VectorMA( outCorners[ i ], signRight * context.halfSize, context.basisRight, outCorners[ i ] );
        VectorMA( outCorners[ i ], signUp * context.halfSize, context.basisUp, outCorners[ i ] );
        VectorMA( outCorners[ i ], signForward * context.halfDepth, context.basisForward, outCorners[ i ] );
    }
}

static void CLG_DecalClip_BuildVolumeBounds( const clg_decal_clip_context_t &context, vec3_t outMins, vec3_t outMaxs ) {
    vec3_t corners[ 8 ] = {};
    CLG_DecalClip_BuildVolumeCorners( context, corners );

    VectorCopy( corners[ 0 ], outMins );
    VectorCopy( corners[ 0 ], outMaxs );
    for ( int32_t i = 1; i < 8; i++ ) {
        for ( int32_t axis = 0; axis < 3; axis++ ) {
            outMins[ axis ] = std::min( outMins[ axis ], corners[ i ][ axis ] );
            outMaxs[ axis ] = std::max( outMaxs[ axis ], corners[ i ][ axis ] );
        }
    }
}

static bool CLG_DecalClip_TryAppendUniqueVertex( const vec3_t point, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !outPolygon ) {
        return false;
    }

    /**
    *    Merge edge hits that land on the same corner so the polygon stays convex
    *    and within the fixed vertex budget.
    **/
    for ( int32_t i = 0; i < outPolygon->vertexCount; i++ ) {
        vec3_t delta = {};
        VectorSubtract( point, outPolygon->positions[ i ], delta );
        if ( VectorLengthSquared( delta ) <= ( CLG_DECAL_CLIP_VERTEX_MERGE_EPSILON * CLG_DECAL_CLIP_VERTEX_MERGE_EPSILON ) ) {
            return true;
        }
    }

    if ( outPolygon->vertexCount >= (int32_t)std::size( outPolygon->positions ) ) {
        return false;
    }

    VectorCopy( point, outPolygon->positions[ outPolygon->vertexCount++ ] );
    return true;
}

static void CLG_DecalClip_ProjectPointToUv( const clg_decal_clip_context_t &context, const vec3_t point, vec2_t outUv ) {
    vec3_t toPoint = {};
    VectorSubtract( point, context.spawn.origin, toPoint );

    float u = ( DotProduct( toPoint, context.basisRight ) / ( context.halfSize * 2.0f ) ) + 0.5f;
    float v = ( DotProduct( toPoint, context.basisUp ) / ( context.halfSize * 2.0f ) ) + 0.5f;

    if ( u < 0.0f ) {
        u = 0.0f;
    } else if ( u > 1.0f ) {
        u = 1.0f;
    }

    if ( v < 0.0f ) {
        v = 0.0f;
    } else if ( v > 1.0f ) {
        v = 1.0f;
    }

    outUv[ 0 ] = u;
    outUv[ 1 ] = v;
}

static void CLG_DecalClip_SortPolygonVertices( const clg_world_surface_t *surface, clg_decal_clip_polygon_t *inOutPolygon ) {
    if ( !surface || !inOutPolygon || inOutPolygon->vertexCount < 3 ) {
        return;
    }

    vec3_t centroid = {};
    for ( int32_t i = 0; i < inOutPolygon->vertexCount; i++ ) {
        centroid[ 0 ] += inOutPolygon->positions[ i ][ 0 ];
        centroid[ 1 ] += inOutPolygon->positions[ i ][ 1 ];
        centroid[ 2 ] += inOutPolygon->positions[ i ][ 2 ];
    }

    const float invCount = 1.0f / (float)inOutPolygon->vertexCount;
    centroid[ 0 ] *= invCount;
    centroid[ 1 ] *= invCount;
    centroid[ 2 ] *= invCount;

    float angles[ 32 ] = {};
    for ( int32_t i = 0; i < inOutPolygon->vertexCount; i++ ) {
        vec3_t toVertex = {};
        VectorSubtract( inOutPolygon->positions[ i ], centroid, toVertex );
        const float tangentComponent = DotProduct( toVertex, surface->tangent );
        const float bitangentComponent = DotProduct( toVertex, surface->bitangent );
        angles[ i ] = atan2f( bitangentComponent, tangentComponent );
    }

    /**
    *    Keep the convex polygon ordered around its centroid so downstream fan triangulation
    *    produces non-overlapping triangles.
    **/
    for ( int32_t i = 0; i < inOutPolygon->vertexCount - 1; i++ ) {
        int32_t bestIndex = i;
        for ( int32_t j = i + 1; j < inOutPolygon->vertexCount; j++ ) {
            if ( angles[ j ] < angles[ bestIndex ] ) {
                bestIndex = j;
            }
        }

        if ( bestIndex == i ) {
            continue;
        }

        vec3_t positionTmp = {};
        vec2_t uvTmp = {};
        VectorCopy( inOutPolygon->positions[ i ], positionTmp );
        uvTmp[ 0 ] = inOutPolygon->uv[ i ][ 0 ];
        uvTmp[ 1 ] = inOutPolygon->uv[ i ][ 1 ];
        VectorCopy( inOutPolygon->positions[ bestIndex ], inOutPolygon->positions[ i ] );
        inOutPolygon->uv[ i ][ 0 ] = inOutPolygon->uv[ bestIndex ][ 0 ];
        inOutPolygon->uv[ i ][ 1 ] = inOutPolygon->uv[ bestIndex ][ 1 ];
        VectorCopy( positionTmp, inOutPolygon->positions[ bestIndex ] );
        inOutPolygon->uv[ bestIndex ][ 0 ] = uvTmp[ 0 ];
        inOutPolygon->uv[ bestIndex ][ 1 ] = uvTmp[ 1 ];

        const float angleTmp = angles[ i ];
        angles[ i ] = angles[ bestIndex ];
        angles[ bestIndex ] = angleTmp;
    }
}

static void CLG_DecalClip_GetBspFaceNormal( const mface_t *face, vec3_t outNormal ) {
    if ( !outNormal ) {
        return;
    }

    if ( !face || !face->plane ) {
        VectorClear( outNormal );
        return;
    }

    VectorCopy( face->plane->normal, outNormal );
    if ( ( face->drawflags & DSURF_PLANEBACK ) != 0 ) {
        outNormal[ 0 ] = -outNormal[ 0 ];
        outNormal[ 1 ] = -outNormal[ 1 ];
        outNormal[ 2 ] = -outNormal[ 2 ];
    }
}

static bool CLG_DecalClip_TryAddSurfaceCandidate( const clg_decal_clip_context_t &context, const vec3_t origin, const vec3_t normal, const bool containsImpactPoint, const mface_t *bspFace, const int32_t entityNumber, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return false;
    }

    vec3_t orientedNormal = {};
    VectorCopy( normal, orientedNormal );
    CLG_DecalClip_AlignNormalToProjection( context, orientedNormal );

    if ( !CLG_DecalClip_IsSurfaceFacingProjection( context, orientedNormal ) ) {
        return false;
    }

    const float signedDepth = CLG_DecalClip_ComputeSignedDepth( context, origin );
    if ( !CLG_DecalClip_IsDepthInsideVolume( context, signedDepth ) ) {
        return false;
    }

    /**
    *    Keep one candidate per concrete BSP face, and only keep plane-only fallbacks
    *    when no resolved receiver on that plane already exists.
    **/
    for ( int32_t i = 0; i < *inOutCount; i++ ) {
        const clg_world_surface_t *existing = &outSurfaces[ i ];
        if ( bspFace && existing->bspFace == bspFace ) {
            return false;
        }

        const float normalDot = fabsf( DotProduct( orientedNormal, existing->normal ) );
        const float existingDepth = CLG_DecalClip_ComputeSignedDepth( context, existing->origin );
		if ( !bspFace && existing->entityNumber == entityNumber && normalDot >= CLG_DECAL_CANDIDATE_MERGE_NORMAL_DOT && fabsf( signedDepth - existingDepth ) <= CLG_DECAL_CANDIDATE_MERGE_DEPTH_EPSILON ) {
            return false;
        }
    }

    clg_world_surface_t *surface = &outSurfaces[ ( *inOutCount )++ ];
    VectorCopy( origin, surface->origin );
	VectorCopy( orientedNormal, surface->normal );
	surface->containsImpactPoint = containsImpactPoint;
	surface->entityNumber = entityNumber;
    surface->bspFace = bspFace;

    vec3_t tangent = {};
    vec3_t bitangent = {};
    CLG_DecalClip_BuildSurfaceBasis( context, surface->normal, tangent, bitangent );
    VectorCopy( tangent, surface->tangent );
    VectorCopy( bitangent, surface->bitangent );
    return true;
}

static bool CLG_DecalClip_TryAddBspFaceCandidate( const clg_decal_clip_context_t &context, const mface_t *face, const vec3_t referencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !face || !face->plane || !face->texinfo || face->numsurfedges < 3 ) {
        return false;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache ) {
        return false;
    }

    bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !CLG_DecalClip_IsFacePointerInWorldBsp( worldBsp, face ) || !CLG_DecalClip_IsValidBspFaceGeometry( worldBsp, face ) ) {
        return false;
    }

    if ( ( face->texinfo->c.flags & ( CM_SURFACE_FLAG_SKY | CM_SURFACE_NODRAW ) ) != 0 ) {
        return false;
    }

    vec3_t faceNormal = {};
    CLG_DecalClip_GetBspFaceNormal( face, faceNormal );

    vec3_t projectedOrigin = {};
    if ( !CLG_DecalClip_ProjectPointOntoBspFacePlane( face, referencePoint, projectedOrigin ) ) {
        return false;
    }

    float impactInset = -FLT_MAX;
    const bool containsImpactPoint = CLG_DecalClip_ComputeProjectedPointInsetToFace( face, projectedOrigin, faceNormal, &impactInset ) && impactInset >= -CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON;

    return CLG_DecalClip_TryAddSurfaceCandidate( context, projectedOrigin, faceNormal, containsImpactPoint, face, ENTITYNUM_WORLD, outSurfaces, maxSurfaces, inOutCount );
}

static void CLG_DecalClip_ConsiderImpactAnchoredFaceCandidate( const clg_decal_clip_context_t &context, const mface_t *face, const float maxPlaneDistance, const mface_t **inOutBestFace, float *inOutBestPlaneDistance, float *inOutBestImpactInset, float *inOutBestFacingAlignment ) {
    if ( !face || !inOutBestFace || !inOutBestPlaneDistance || !inOutBestImpactInset || !inOutBestFacingAlignment ) {
        return;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache ) {
        return;
    }

    bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !CLG_DecalClip_IsFacePointerInWorldBsp( worldBsp, face ) || !CLG_DecalClip_IsValidBspFaceGeometry( worldBsp, face ) ) {
        return;
    }

    if ( !face->plane || !face->texinfo ) {
        return;
    }

    if ( ( face->texinfo->c.flags & ( CM_SURFACE_FLAG_SKY | CM_SURFACE_NODRAW ) ) != 0 ) {
        return;
    }

    vec3_t faceNormal = {};
    CLG_DecalClip_GetBspFaceNormal( face, faceNormal );
    CLG_DecalClip_AlignNormalToProjection( context, faceNormal );
    if ( !CLG_DecalClip_IsSurfaceFacingProjection( context, faceNormal ) ) {
        return;
    }

    const float planeDistance = fabsf( PlaneDiff( context.spawn.origin, face->plane ) );
    if ( planeDistance > maxPlaneDistance ) {
        return;
    }

    vec3_t projectedImpactPoint = {};
    if ( !CLG_DecalClip_ProjectPointOntoBspFacePlane( face, context.spawn.origin, projectedImpactPoint ) ) {
        return;
    }

    float impactInset = -FLT_MAX;
    if ( !CLG_DecalClip_ComputeProjectedPointInsetToFace( face, projectedImpactPoint, faceNormal, &impactInset ) ) {
        return;
    }

    if ( impactInset < -CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON ) {
        return;
    }

    const float facingAlignment = DotProduct( faceNormal, context.basisForward );
    if ( planeDistance < *inOutBestPlaneDistance ||
        ( fabsf( planeDistance - *inOutBestPlaneDistance ) <= 0.001f && impactInset > *inOutBestImpactInset ) ||
        ( fabsf( planeDistance - *inOutBestPlaneDistance ) <= 0.001f && fabsf( impactInset - *inOutBestImpactInset ) <= 0.001f && facingAlignment > *inOutBestFacingAlignment ) ) {
        *inOutBestPlaneDistance = planeDistance;
        *inOutBestImpactInset = impactInset;
        *inOutBestFacingAlignment = facingAlignment;
        *inOutBestFace = face;
    }
}

static bool CLG_DecalClip_DoBoundsOverlap( const vec3_t minsA, const vec3_t maxsA, const vec3_t minsB, const vec3_t maxsB ) {
    for ( int32_t axis = 0; axis < 3; axis++ ) {
        if ( maxsA[ axis ] < minsB[ axis ] || minsA[ axis ] > maxsB[ axis ] ) {
            return false;
        }
    }

    return true;
}

static bool CLG_DecalClip_IsAddressRangeInsideSpan( const void *start, const size_t bytes, const void *base, const size_t spanBytes ) {
    if ( !start || !base || bytes == 0 || spanBytes == 0 ) {
        return false;
    }

    const uintptr_t startAddress = (uintptr_t)start;
    const uintptr_t baseAddress = (uintptr_t)base;
    if ( startAddress < baseAddress ) {
        return false;
    }

    const uintptr_t relativeOffset = startAddress - baseAddress;
    if ( relativeOffset > spanBytes ) {
        return false;
    }

    if ( bytes > ( spanBytes - relativeOffset ) ) {
        return false;
    }

    return true;
}

static bool CLG_DecalClip_IsFacePointerInWorldBsp( const bsp_t *worldBsp, const mface_t *face ) {
    if ( !worldBsp || !face || !worldBsp->faces || worldBsp->numfaces <= 0 ) {
        return false;
    }

    return CLG_DecalClip_IsAddressRangeInsideSpan( face, sizeof( mface_t ), worldBsp->faces, (size_t)worldBsp->numfaces * sizeof( mface_t ) );
}

static bool CLG_DecalClip_IsValidBspFaceGeometry( const bsp_t *worldBsp, const mface_t *face ) {
    if ( !worldBsp || !face || !face->firstsurfedge || face->numsurfedges < 3 ) {
        return false;
    }

    if ( !worldBsp->surfedges || worldBsp->numsurfedges <= 0 || !worldBsp->edges || worldBsp->numedges <= 0 || !worldBsp->vertices || worldBsp->numvertices <= 0 ) {
        return false;
    }

    if ( !CLG_DecalClip_IsAddressRangeInsideSpan( face->firstsurfedge, (size_t)face->numsurfedges * sizeof( msurfedge_t ), worldBsp->surfedges, (size_t)worldBsp->numsurfedges * sizeof( msurfedge_t ) ) ) {
        return false;
    }

    for ( int32_t i = 0; i < face->numsurfedges; i++ ) {
        const msurfedge_t *surfedge = face->firstsurfedge + i;
        if ( !surfedge || !surfedge->edge ) {
            return false;
        }

        if ( !CLG_DecalClip_IsAddressRangeInsideSpan( surfedge->edge, sizeof( medge_t ), worldBsp->edges, (size_t)worldBsp->numedges * sizeof( medge_t ) ) ) {
            return false;
        }

        if ( surfedge->vert < 0 || surfedge->vert > 1 ) {
            return false;
        }

        const mvertex_t *vertex = surfedge->edge->v[ surfedge->vert ];
        if ( !vertex ) {
            return false;
        }

        if ( !CLG_DecalClip_IsAddressRangeInsideSpan( vertex, sizeof( mvertex_t ), worldBsp->vertices, (size_t)worldBsp->numvertices * sizeof( mvertex_t ) ) ) {
            return false;
        }
    }

    return true;
}

static void CLG_DecalClip_GatherLeafFaceCandidates( const clg_decal_clip_context_t &context, const vec3_t queryMins, const vec3_t queryMaxs, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache ) {
        return;
    }

    bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp->faces || worldBsp->numfaces <= 0 ) {
        return;
    }

    mleaf_t *leafs[ 128 ] = {};
    mnode_t *topnode = nullptr;
    const int32_t leafCount = clgi.CM_BoxLeafs( queryMins, queryMaxs, leafs, (int32_t)std::size( leafs ), &topnode );
    (void)topnode;

    for ( int32_t leafIndex = 0; leafIndex < leafCount && *inOutCount < maxSurfaces; leafIndex++ ) {
        const mleaf_t *leaf = leafs[ leafIndex ];
        if ( !leaf || !leaf->firstleafface ) {
            continue;
        }

        for ( int32_t faceIndex = 0; faceIndex < leaf->numleaffaces && *inOutCount < maxSurfaces; faceIndex++ ) {
            const mface_t *face = leaf->firstleafface[ faceIndex ];
            if ( !CLG_DecalClip_IsFacePointerInWorldBsp( worldBsp, face ) ) {
                continue;
            }

            (void)CLG_DecalClip_TryAddBspFaceCandidate( context, face, context.spawn.origin, outSurfaces, maxSurfaces, inOutCount );
        }
    }
}

static bool CLG_DecalClip_GatherImpactAnchoredFallbackCandidate( const clg_decal_clip_context_t &context, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount, float *outSelectedPlaneDistance ) {
    if ( outSelectedPlaneDistance ) {
        *outSelectedPlaneDistance = -1.0f;
    }

    if ( !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return false;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache ) {
        return false;
    }

    const centity_t *inlineBrushEntity = CLG_DecalClip_GetInlineBrushEntity( context.spawn.hitEntityNumber );
    if ( inlineBrushEntity ) {
        /**
        *    Inline brush-model impacts already know the exact trace plane and entity, so
        *    bypass world BSP fallback lookup and let entity-targeted support traces trim
        *    the plane quad against the mover's collision instead.
        **/
        if ( CLG_DecalClip_TryAddSurfaceCandidate( context, context.spawn.origin, context.spawn.normal, true, nullptr, inlineBrushEntity->current.number, outSurfaces, maxSurfaces, inOutCount ) ) {
            if ( outSelectedPlaneDistance ) {
                *outSelectedPlaneDistance = 0.0f;
            }

            return true;
        }

        return false;
    }

    bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp->faces || worldBsp->numfaces <= 0 ) {
        return false;
    }

    vec3_t queryMins = {};
    vec3_t queryMaxs = {};
    const float impactLookupRadius = std::max( CLG_DECAL_IMPACT_LOOKUP_RADIUS, context.halfSize + context.halfDepth + 2.0f );
    for ( int32_t axis = 0; axis < 3; axis++ ) {
        queryMins[ axis ] = context.spawn.origin[ axis ] - impactLookupRadius;
        queryMaxs[ axis ] = context.spawn.origin[ axis ] + impactLookupRadius;
    }

    mleaf_t *leafs[ 64 ] = {};
    mnode_t *topnode = nullptr;
    const int32_t leafCount = clgi.CM_BoxLeafs( queryMins, queryMaxs, leafs, (int32_t)std::size( leafs ), &topnode );

    const mface_t *bestFace = nullptr;
    float bestPlaneDistance = FLT_MAX;
    float bestImpactInset = -FLT_MAX;
    float bestFacingAlignment = -1.0f;
    const float maxPlaneDistance = std::max( 2.0f, context.halfSize * 0.5f );

    for ( int32_t leafIndex = 0; leafIndex < leafCount; leafIndex++ ) {
        const mleaf_t *leaf = leafs[ leafIndex ];
        if ( !leaf || !leaf->firstleafface ) {
            continue;
        }

        for ( int32_t faceIndex = 0; faceIndex < leaf->numleaffaces; faceIndex++ ) {
            const mface_t *face = leaf->firstleafface[ faceIndex ];
            CLG_DecalClip_ConsiderImpactAnchoredFaceCandidate( context, face, maxPlaneDistance, &bestFace, &bestPlaneDistance, &bestImpactInset, &bestFacingAlignment );
        }
    }

    /**
    *    Nearby world surfaces can live on node-owned faces instead of leaf-owned face lists.
    *    Re-scan the intersecting BSP nodes with the same local scoring before giving up and
    *    falling back to the raw trace plane on stairs, bevels, and sloped brush seams.
    **/
    CLG_DecalClip_FindBestImpactAnchoredFaceRecursive(
        context,
        topnode ? topnode : worldBsp->nodes,
        queryMins,
        queryMaxs,
        maxPlaneDistance,
        &bestFace,
        &bestPlaneDistance,
        &bestImpactInset,
        &bestFacingAlignment );

    if ( bestFace && CLG_DecalClip_TryAddBspFaceCandidate( context, bestFace, context.spawn.origin, outSurfaces, maxSurfaces, inOutCount ) ) {
        if ( outSelectedPlaneDistance ) {
            *outSelectedPlaneDistance = bestPlaneDistance;
        }

        return true;
    }

    /**
    *    If we resolved a valid BSP face but it was already present in the candidate set,
    *    do not fall through to the raw trace-normal plane fallback. That plane-only quad
    *    can be slightly tilted relative to the real brush plane on slopes, which produces
    *    skewed receiver quads that intersect the surface.
    **/
    if ( bestFace ) {
        return false;
    }

    /**
    *    Last resort: use the original impact endpoint and trace normal as an infinite
    *    receiver plane. This is intentionally simple so dead-zone areas still behave
    *    like normal impact surfaces even when BSP face lookup misses or returns only
    *    troublesome neighboring candidates.
    **/
    if ( CLG_DecalClip_TryAddSurfaceCandidate( context, context.spawn.origin, context.spawn.normal, true, nullptr, ENTITYNUM_WORLD, outSurfaces, maxSurfaces, inOutCount ) ) {
        if ( outSelectedPlaneDistance ) {
            *outSelectedPlaneDistance = 0.0f;
        }

        return true;
    }

    return false;
}

static void CLG_DecalClip_FindBestImpactAnchoredFaceRecursive( const clg_decal_clip_context_t &context, const mnode_t *node, const vec3_t queryMins, const vec3_t queryMaxs, const float maxPlaneDistance, const mface_t **inOutBestFace, float *inOutBestPlaneDistance, float *inOutBestImpactInset, float *inOutBestFacingAlignment ) {
    if ( !node || !node->plane || !inOutBestFace || !inOutBestPlaneDistance || !inOutBestImpactInset || !inOutBestFacingAlignment ) {
        return;
    }

    if ( !CLG_DecalClip_DoBoundsOverlap( queryMins, queryMaxs, node->mins, node->maxs ) ) {
        return;
    }

    if ( node->numfaces > 0 && !node->firstface ) {
        return;
    }

    for ( int32_t faceIndex = 0; faceIndex < node->numfaces; faceIndex++ ) {
        const mface_t *face = node->firstface + faceIndex;
        CLG_DecalClip_ConsiderImpactAnchoredFaceCandidate( context, face, maxPlaneDistance, inOutBestFace, inOutBestPlaneDistance, inOutBestImpactInset, inOutBestFacingAlignment );
    }

    CLG_DecalClip_FindBestImpactAnchoredFaceRecursive( context, node->children[ 0 ], queryMins, queryMaxs, maxPlaneDistance, inOutBestFace, inOutBestPlaneDistance, inOutBestImpactInset, inOutBestFacingAlignment );
    CLG_DecalClip_FindBestImpactAnchoredFaceRecursive( context, node->children[ 1 ], queryMins, queryMaxs, maxPlaneDistance, inOutBestFace, inOutBestPlaneDistance, inOutBestImpactInset, inOutBestFacingAlignment );
}

static void CLG_DecalClip_GatherNodeFaceCandidatesRecursive( const clg_decal_clip_context_t &context, const mnode_t *node, const vec3_t queryMins, const vec3_t queryMaxs, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !node || !node->plane || !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return;
    }

    if ( !CLG_DecalClip_DoBoundsOverlap( queryMins, queryMaxs, node->mins, node->maxs ) ) {
        return;
    }

    bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp || !worldBsp->faces || worldBsp->numfaces <= 0 ) {
        return;
    }

    if ( node->numfaces > 0 && !node->firstface ) {
        return;
    }

    for ( int32_t faceIndex = 0; faceIndex < node->numfaces && *inOutCount < maxSurfaces; faceIndex++ ) {
        const mface_t *face = node->firstface + faceIndex;
        if ( !CLG_DecalClip_IsFacePointerInWorldBsp( worldBsp, face ) ) {
            continue;
        }

        (void)CLG_DecalClip_TryAddBspFaceCandidate( context, face, context.spawn.origin, outSurfaces, maxSurfaces, inOutCount );
    }

    CLG_DecalClip_GatherNodeFaceCandidatesRecursive( context, node->children[ 0 ], queryMins, queryMaxs, outSurfaces, maxSurfaces, inOutCount );
    CLG_DecalClip_GatherNodeFaceCandidatesRecursive( context, node->children[ 1 ], queryMins, queryMaxs, outSurfaces, maxSurfaces, inOutCount );
}

static bool CLG_DecalClip_BuildPolygonFromBspFace( const mface_t *face, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !face || !outPolygon || face->numsurfedges < 3 || face->numsurfedges > (int32_t)std::size( outPolygon->positions ) ) {
        return false;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache ) {
        return false;
    }

    bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !CLG_DecalClip_IsFacePointerInWorldBsp( worldBsp, face ) || !CLG_DecalClip_IsValidBspFaceGeometry( worldBsp, face ) ) {
        return false;
    }

    memset( outPolygon, 0, sizeof( *outPolygon ) );

    /**
    *    Expand the BSP face winding directly from its surfedge list so we can clip
    *    the actual world polygon rather than an infinite receiver plane.
    **/
    for ( int32_t i = 0; i < face->numsurfedges; i++ ) {
        const msurfedge_t *surfedge = face->firstsurfedge + i;
        const medge_t *edge = surfedge->edge;
        const mvertex_t *vertex = edge->v[ surfedge->vert ];
        VectorCopy( vertex->point, outPolygon->positions[ outPolygon->vertexCount++ ] );
    }

    return ( outPolygon->vertexCount >= 3 );
}

static bool CLG_DecalClip_ProjectPointOntoBspFacePlane( const mface_t *face, const vec3_t point, vec3_t outProjectedPoint ) {
    if ( !face || !face->plane || !outProjectedPoint ) {
        return false;
    }

    VectorCopy( point, outProjectedPoint );
    const float planeDistance = PlaneDiff( point, face->plane );
    VectorMA( outProjectedPoint, -planeDistance, face->plane->normal, outProjectedPoint );
    return true;
}

static bool CLG_DecalClip_ComputeProjectedPointInsetToFace( const mface_t *face, const vec3_t projectedPoint, const vec3_t faceNormal, float *outInset ) {
    if ( !outInset ) {
        return false;
    }

    clg_decal_clip_polygon_t facePolygon = {};
    if ( !CLG_DecalClip_BuildPolygonFromBspFace( face, &facePolygon ) ) {
        return false;
    }

    vec3_t polygonNormal = {};

    /**
    *    Accumulate a stable polygon normal so edge tests can respect the stored winding.
    **/
    for ( int32_t i = 0; i < facePolygon.vertexCount; i++ ) {
        const int32_t nextIndex = ( i + 1 ) % facePolygon.vertexCount;
        vec3_t edgeCross = {};
        CrossProduct( facePolygon.positions[ i ], facePolygon.positions[ nextIndex ], edgeCross );
        VectorAdd( polygonNormal, edgeCross, polygonNormal );
    }

    const float windingDot = DotProduct( polygonNormal, faceNormal );
    if ( fabsf( windingDot ) <= 0.001f ) {
        return false;
    }

    const float windingSign = ( windingDot >= 0.0f ) ? 1.0f : -1.0f;
    float minInset = FLT_MAX;

    /**
    *    Measure the signed perpendicular distance from the projected impact point to each
    *    face edge. Positive values are inside the winding, negative values lie outside.
    **/
    for ( int32_t i = 0; i < facePolygon.vertexCount; i++ ) {
        const int32_t nextIndex = ( i + 1 ) % facePolygon.vertexCount;
        vec3_t edge = {};
        VectorSubtract( facePolygon.positions[ nextIndex ], facePolygon.positions[ i ], edge );
        const float edgeLength = VectorLength( edge );
        if ( edgeLength <= 0.001f ) {
            continue;
        }

        vec3_t toPoint = {};
        VectorSubtract( projectedPoint, facePolygon.positions[ i ], toPoint );

        vec3_t edgeCrossPoint = {};
        CrossProduct( edge, toPoint, edgeCrossPoint );

        const float signedInset = ( DotProduct( edgeCrossPoint, faceNormal ) * windingSign ) / edgeLength;
        if ( signedInset < minInset ) {
            minInset = signedInset;
        }
    }

    if ( minInset == FLT_MAX ) {
        return false;
    }

    *outInset = minInset;
    return true;
}


static bool CLG_DecalClip_ClipPolygonAgainstPlane( const clg_decal_clip_polygon_t &inPolygon, const vec3_t planePoint, const vec3_t planeNormal, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !outPolygon || inPolygon.vertexCount < 3 ) {
        return false;
    }

    memset( outPolygon, 0, sizeof( *outPolygon ) );

    for ( int32_t i = 0; i < inPolygon.vertexCount; i++ ) {
        const int32_t nextIndex = ( i + 1 ) % inPolygon.vertexCount;
        const vec3_t &current = inPolygon.positions[ i ];
        const vec3_t &next = inPolygon.positions[ nextIndex ];
        const float currentDistance = CLG_DecalClip_ComputePlaneDistance( planePoint, planeNormal, current );
        const float nextDistance = CLG_DecalClip_ComputePlaneDistance( planePoint, planeNormal, next );
        const bool currentInside = ( currentDistance <= CLG_DECAL_CLIP_PLANE_EPSILON );
        const bool nextInside = ( nextDistance <= CLG_DECAL_CLIP_PLANE_EPSILON );

        if ( currentInside ) {
            if ( !CLG_DecalClip_TryAppendUniqueVertex( current, outPolygon ) ) {
                return false;
            }
        }

        if ( currentInside != nextInside ) {
            vec3_t intersection = {};
            const float fraction = currentDistance / ( currentDistance - nextDistance );
            for ( int32_t axis = 0; axis < 3; axis++ ) {
                intersection[ axis ] = current[ axis ] + ( ( next[ axis ] - current[ axis ] ) * fraction );
            }

            if ( !CLG_DecalClip_TryAppendUniqueVertex( intersection, outPolygon ) ) {
                return false;
            }
        }
    }

    return ( outPolygon->vertexCount >= 3 );
}

static bool CLG_DecalClip_ClipBspFaceToDecalVolume( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !surface || !surface->bspFace || !outPolygon ) {
        return false;
    }

    clg_decal_clip_polygon_t workingPolygon = {};
    clg_decal_clip_polygon_t scratchPolygon = {};
    if ( !CLG_DecalClip_BuildPolygonFromBspFace( surface->bspFace, &workingPolygon ) ) {
        return false;
    }

    const vec3_t planeNormals[ 6 ] = {
        { context.basisRight[ 0 ], context.basisRight[ 1 ], context.basisRight[ 2 ] },
        { -context.basisRight[ 0 ], -context.basisRight[ 1 ], -context.basisRight[ 2 ] },
        { context.basisUp[ 0 ], context.basisUp[ 1 ], context.basisUp[ 2 ] },
        { -context.basisUp[ 0 ], -context.basisUp[ 1 ], -context.basisUp[ 2 ] },
        { context.basisForward[ 0 ], context.basisForward[ 1 ], context.basisForward[ 2 ] },
        { -context.basisForward[ 0 ], -context.basisForward[ 1 ], -context.basisForward[ 2 ] },
    };
    vec3_t planePoints[ 6 ] = {};

    VectorCopy( context.spawn.origin, planePoints[ 0 ] );
    VectorMA( planePoints[ 0 ], context.halfSize, context.basisRight, planePoints[ 0 ] );
    VectorCopy( context.spawn.origin, planePoints[ 1 ] );
    VectorMA( planePoints[ 1 ], -context.halfSize, context.basisRight, planePoints[ 1 ] );
    VectorCopy( context.spawn.origin, planePoints[ 2 ] );
    VectorMA( planePoints[ 2 ], context.halfSize, context.basisUp, planePoints[ 2 ] );
    VectorCopy( context.spawn.origin, planePoints[ 3 ] );
    VectorMA( planePoints[ 3 ], -context.halfSize, context.basisUp, planePoints[ 3 ] );
    VectorCopy( context.spawn.origin, planePoints[ 4 ] );
    VectorMA( planePoints[ 4 ], context.halfDepth, context.basisForward, planePoints[ 4 ] );
    VectorCopy( context.spawn.origin, planePoints[ 5 ] );
    VectorMA( planePoints[ 5 ], -context.halfDepth, context.basisForward, planePoints[ 5 ] );

    /**
    *    Clip the actual face winding against each side of the oriented decal box.
    **/
    for ( int32_t planeIndex = 0; planeIndex < 6; planeIndex++ ) {
        if ( !CLG_DecalClip_ClipPolygonAgainstPlane( workingPolygon, planePoints[ planeIndex ], planeNormals[ planeIndex ], &scratchPolygon ) ) {
            return false;
        }

        workingPolygon = scratchPolygon;
    }

    *outPolygon = workingPolygon;
    return ( outPolygon->vertexCount >= 3 );
}

/**
*    @brief  Builds an orthonormal decal basis from projected forward vector.
*    @param  forward Projected forward vector.
*    @param  outRight [out] Tangent axis.
*    @param  outUp [out] Bitangent axis.
*    @param  outForward [out] Normalized forward axis.
**/
static void CLG_DecalClip_BuildBasis( const vec3_t forward, vec3_t outRight, vec3_t outUp, vec3_t outForward ) {
    VectorCopy( forward, outForward );
    if ( VectorLength( outForward ) <= 0.001f ) {
        VectorSet( outForward, 0.0f, 0.0f, 1.0f );
    }
    VectorNormalize( outForward );

    vec3_t referenceUp = { 0.0f, 0.0f, 1.0f };
    if ( fabsf( DotProduct( outForward, referenceUp ) ) > 0.95f ) {
        VectorSet( referenceUp, 0.0f, 1.0f, 0.0f );
    }

    CrossProduct( referenceUp, outForward, outRight );
    if ( VectorLength( outRight ) <= 0.001f ) {
        VectorSet( outRight, 1.0f, 0.0f, 0.0f );
    }
    VectorNormalize( outRight );

    CrossProduct( outForward, outRight, outUp );
    VectorNormalize( outUp );
}

const bool CLG_DecalClip_BuildContext( const sg_decal_spawn_params_t &spawn, clg_decal_clip_context_t *outContext ) {
    if ( !outContext ) {
        return false;
    }

    if ( spawn.radius <= 0.0f || spawn.depth <= 0.0f ) {
        return false;
    }

    memset( outContext, 0, sizeof( *outContext ) );
    outContext->spawn = spawn;
    outContext->halfSize = spawn.radius;
    outContext->halfDepth = spawn.depth * 0.5f;

    CLG_DecalClip_BuildBasis( spawn.normal, outContext->basisRight, outContext->basisUp, outContext->basisForward );

    /**
    *    Rotate the decal footprint around its projection axis so dynamic decals can use
    *    their randomized spawn rotation while keeping depth tests in the same local space.
    **/
    if ( fabsf( spawn.rotationRadians ) > 0.001f ) {
        vec3_t rotatedRight = {};
        vec3_t rotatedUp = {};
        const float cosAngle = cosf( spawn.rotationRadians );
        const float sinAngle = sinf( spawn.rotationRadians );

        for ( int32_t i = 0; i < 3; i++ ) {
            rotatedRight[ i ] = ( outContext->basisRight[ i ] * cosAngle ) + ( outContext->basisUp[ i ] * sinAngle );
            rotatedUp[ i ] = ( outContext->basisUp[ i ] * cosAngle ) - ( outContext->basisRight[ i ] * sinAngle );
        }

        VectorCopy( rotatedRight, outContext->basisRight );
        VectorCopy( rotatedUp, outContext->basisUp );
    }

    return true;
}

int32_t CLG_DecalClip_GatherCandidateSurfaces( const clg_decal_clip_context_t &context, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces ) {
    if ( !outSurfaces || maxSurfaces <= 0 ) {
        return 0;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache || !clgi.client->collisionModel.cache->nodes ) {
        return 0;
    }

    int32_t outCount = 0;
    vec3_t queryMins = {};
    vec3_t queryMaxs = {};
    CLG_DecalClip_BuildVolumeBounds( context, queryMins, queryMaxs );
    const centity_t *inlineBrushEntity = CLG_DecalClip_GetInlineBrushEntity( context.spawn.hitEntityNumber );

    /**
    *    Expand broad-phase bounds slightly to avoid precision misses near leaf splits
    *    and large brush boundaries where decals previously had dead zones.
    **/
    const float queryExpansion = std::max( CLG_DECAL_BOUNDS_QUERY_EXPANSION, context.halfSize + 1.0f );
    for ( int32_t axis = 0; axis < 3; axis++ ) {
        queryMins[ axis ] -= queryExpansion;
        queryMaxs[ axis ] += queryExpansion;
    }

    /**
    *    Gather from overlapping leaves first, then from intersecting BSP nodes so large
    *    faces referenced above leaf level are still captured without scanning all faces.
    **/
    if ( !inlineBrushEntity ) {
        CLG_DecalClip_GatherLeafFaceCandidates( context, queryMins, queryMaxs, outSurfaces, maxSurfaces, &outCount );
        CLG_DecalClip_GatherNodeFaceCandidatesRecursive( context, clgi.client->collisionModel.cache->nodes, queryMins, queryMaxs, outSurfaces, maxSurfaces, &outCount );
    }
    const int32_t broadPhaseCandidateCount = outCount;

    /**
    *    Always let the impact-local receiver participate in the candidate set. Broad-phase
    *    can find nearby split faces while still missing the exact surface hit by the trace,
    *    so using this only when the broad phase is empty leaves dead-zones on large floors.
    **/
    bool fallbackUsed = false;
    float fallbackSelectedPlaneDistance = -1.0f;
    fallbackUsed = CLG_DecalClip_GatherImpactAnchoredFallbackCandidate( context, outSurfaces, maxSurfaces, &outCount, &fallbackSelectedPlaneDistance );

	clgi.Print( PRINT_DEVELOPER, "[CLG Decals][ClipDbg] hit-entity:%d broad-phase candidates:%d fallback-used:%s fallback-selected-plane-distance:%.3f\n",
		context.spawn.hitEntityNumber,
        broadPhaseCandidateCount,
        fallbackUsed ? "yes" : "no",
        fallbackSelectedPlaneDistance );

    return outCount;
}

const bool CLG_DecalClip_ClipSurfaceToDecal( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !surface || !outPolygon ) {
        return false;
    }

    // Clip-stage facing rejection to guard against stale or externally sourced candidates.
    if ( !CLG_DecalClip_IsSurfaceFacingProjection( context, surface->normal ) ) {
        return false;
    }

    const float centerDepth = CLG_DecalClip_ComputeSignedDepth( context, surface->origin );
    // Clip-stage depth rejection to avoid projecting through nearby opposite walls.
    if ( !CLG_DecalClip_IsDepthInsideVolume( context, centerDepth ) ) {
        return false;
    }

    memset( outPolygon, 0, sizeof( *outPolygon ) );

    /**
    *    Prefer clipping the real BSP face polygon against the decal OBB. If no concrete
    *    face was resolved for this trace, fall back to the receiver-plane intersection path.
    **/
    if ( surface->bspFace ) {
        if ( !CLG_DecalClip_ClipBspFaceToDecalVolume( context, surface, outPolygon ) ) {
            return false;
        }

        goto finalize_polygon;
    }

    {
        /**
		*	For plane-only candidates (no BSP face), build a simple quad aligned to the
		*	surface's tangent/bitangent basis. This ensures proper angular alignment to the
		*	surface normal and avoids stretching/misalignment issues on sloped surfaces.
		**/
		if ( !CLG_DecalClip_BuildPlaneQuad( context, surface, outPolygon ) ) {
			return false;
		}
    }

finalize_polygon:
    /**
    *    Project UVs from decal-local axes and sort the convex polygon before triangulation.
    **/
    for ( int32_t i = 0; i < outPolygon->vertexCount; i++ ) {
        CLG_DecalClip_ProjectPointToUv( context, outPolygon->positions[ i ], outPolygon->uv[ i ] );
    }

    /**
    *    Concrete BSP faces already preserve cyclic winding through face expansion and
    *    Sutherland-Hodgman clipping. Re-sorting those small beveled polygons can perturb
    *    a valid winding into a bad fan, so only plane-only fallback polygons are re-ordered.
    **/
    if ( !surface->bspFace ) {
        CLG_DecalClip_SortPolygonVertices( surface, outPolygon );
    }

    return true;
}
