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
//! Keep a small positive threshold to reject pathological near-perpendicular receivers.
static constexpr float CLG_DECAL_MIN_FACING_DOT = 0.10f;
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
//! Seed extent used to recover one collision brush-side polygon by clipping against the owning brush.
static constexpr float CLG_DECAL_COLLISION_FACE_SEED_EXTENT = 8192.0f;
//! Edge slack used when deciding whether the snapped impact still belongs to a BSP face winding.
//! Increased tolerance to absorb quantized impact origin jitter on detail brush seams.
static constexpr float CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON = 4.0f;

/**
*    @brief  Returns true when clip-debug logging is enabled at or above one level.
*    @param  level Required debug level.
**/
static bool CLG_DecalClip_IsDebugLevel( const int32_t level ) {
    static cvar_t *clg_decals_debug = nullptr;
    if ( !clg_decals_debug ) {
        clg_decals_debug = clgi.CVar_Get( "clg_decals_debug", "0", CVAR_ARCHIVE );
    }

    return ( clg_decals_debug && clg_decals_debug->integer >= level );
}

/**
*    @brief  Returns true when one BSP face has safe surfedge/edge/vertex references.
*    @param  worldBsp World BSP cache.
*    @param  face Candidate face pointer.
*    @return True when all face geometry references are valid for traversal.
**/
static bool CLG_DecalClip_IsValidBspFaceGeometry( const bsp_t *worldBsp, const mface_t *face );
static const mmodel_t *CLG_DecalClip_FindInlineBrushModel( const centity_t *inlineBrushEntity );
static bool CLG_DecalClip_IsFacePointerInWorldBsp( const bsp_t *worldBsp, const mface_t *face );

/**
*    @brief  Builds an orthonormal decal basis from projected forward vector.
*    @param  forward Projected forward vector.
*    @param  outRight [out] Tangent axis.
*    @param  outUp [out] Bitangent axis.
*    @param  outForward [out] Normalized forward axis.
**/
static void CLG_DecalClip_BuildBasis( const vec3_t forward, vec3_t outRight, vec3_t outUp, vec3_t outForward );

/**
*    @brief  Computes signed depth of one world point inside decal projection space.
*    @param  context Decal clip context.
*    @param  worldPoint Point in world space.
*    @return Signed depth along projection forward axis.
**/
static float CLG_DecalClip_ComputeSignedDepth( const clg_decal_clip_context_t &context, const vec3_t worldPoint );

/**
*    @brief  Computes signed point distance from one plane.
*    @param  planePoint Any point on the plane.
*    @param  planeNormal Plane normal.
*    @param  point Point to classify.
*    @return Signed distance from plane.
**/
static float CLG_DecalClip_ComputePlaneDistance( const vec3_t planePoint, const vec3_t planeNormal, const vec3_t point );

/**
*    @brief  Flips one normal to face the same hemisphere as the decal projection.
*    @param  context Decal clip context.
*    @param  inOutNormal [in/out] Normal to orient.
**/
static void CLG_DecalClip_AlignNormalToProjection( const clg_decal_clip_context_t &context, vec3_t inOutNormal );

/**
*    @brief  Returns one inline brush entity used for local-space clipping.
*    @param  entityNumber Candidate entity number.
*    @return Valid inline entity when a hull node exists, nullptr otherwise.
**/
static const centity_t *CLG_DecalClip_GetInlineBrushEntity( const int32_t entityNumber );

/**
*    @brief  Returns true when a surface normal is sufficiently aligned with decal projection.
*    @param  context Decal clip context.
*    @param  surfaceNormal Candidate normal.
*    @return True when alignment passes facing threshold.
**/
static bool CLG_DecalClip_IsSurfaceFacingProjection( const clg_decal_clip_context_t &context, const vec3_t surfaceNormal );

/**
*    @brief  Returns true when one signed depth lies inside decal depth bounds.
*    @param  context Decal clip context.
*    @param  signedDepth Signed depth value in decal space.
*    @return True when depth is inside clip slab.
**/
static bool CLG_DecalClip_IsDepthInsideVolume( const clg_decal_clip_context_t &context, const float signedDepth );

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
static void CLG_DecalClip_GatherInlineModelFaceCandidates( const clg_decal_clip_context_t &context, const centity_t *inlineBrushEntity, const mmodel_t *inlineModel, const vec3_t localQueryMins, const vec3_t localQueryMaxs, const vec3_t localReferencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount );
static void CLG_DecalClip_GatherInlineNodeFaceCandidatesRecursive( const clg_decal_clip_context_t &context, const centity_t *inlineBrushEntity, const mnode_t *node, const vec3_t localQueryMins, const vec3_t localQueryMaxs, const vec3_t localReferencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount );

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
static bool CLG_DecalClip_BuildPolygonFromCollisionBrushSide( const mbrush_t *brush, const mbrushside_t *brushSide, const vec3_t referencePoint, clg_decal_clip_polygon_t *outPolygon );
static bool CLG_DecalClip_ProjectPointOntoPlane( const cm_plane_t *plane, const vec3_t point, vec3_t outProjectedPoint );
static bool CLG_DecalClip_ProjectPointOntoBspFacePlane( const mface_t *face, const vec3_t point, vec3_t outProjectedPoint );
static bool CLG_DecalClip_ProjectPointOntoCollisionBrushSidePlane( const mbrushside_t *brushSide, const vec3_t point, vec3_t outProjectedPoint );
static bool CLG_DecalClip_ComputeProjectedPointInsetToPolygon( const clg_decal_clip_polygon_t &polygon, const vec3_t projectedPoint, const vec3_t faceNormal, float *outInset );

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
 *    @brief  Finds the exact world BSP face matching one traced world brush-side handle.
 *    @param  context Decal clip context holding the impact origin.
 *    @param  surfaceHandle Stable handle captured from the impact trace brush side.
*    @return Matching world BSP face, or nullptr when the handle could not be resolved.
**/
static const mface_t *CLG_DecalClip_FindWorldFaceBySurfaceHandle( const clg_decal_clip_context_t &context, const uintptr_t surfaceHandle );

/**
*    @brief  Builds orthonormal basis vectors for one inline brush-model entity.
*    @param  entity Inline brush-model entity.
*    @param  outForward [out] Local forward axis in world space.
*    @param  outRight [out] Local right axis in world space.
*    @param  outUp [out] Local up axis in world space.
**/
static bool CLG_DecalClip_BuildInlineEntityBasis( const centity_t *entity, vec3_t outForward, vec3_t outRight, vec3_t outUp );

/**
*    @brief  Converts one world-space point into inline-model local space.
*    @param  entity Inline brush-model entity.
*    @param  worldPoint World-space point.
*    @param  outLocalPoint [out] Point in inline-model local space.
**/
static bool CLG_DecalClip_WorldPointToInlineLocal( const centity_t *entity, const vec3_t worldPoint, vec3_t outLocalPoint );

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
    /**
    *    Accept any entity that provides a valid inline hull node. This keeps mover-domain
    *    decal clipping robust when network snapshots expose transitional solid encodings.
    **/
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

static bool CLG_DecalClip_BuildInlineEntityBasis( const centity_t *entity, vec3_t outForward, vec3_t outRight, vec3_t outUp ) {
    if ( !entity || !outForward || !outRight || !outUp ) {
        return false;
    }

    vec3_t axis[ 3 ] = {};
    AnglesToAxis( &entity->lerpAngles.x, axis );
    VectorCopy( axis[ 0 ], outForward );
    VectorCopy( axis[ 1 ], outRight );
    VectorCopy( axis[ 2 ], outUp );

    if ( VectorLength( outForward ) <= 0.001f ) {
        VectorSet( outForward, 1.0f, 0.0f, 0.0f );
    }
    if ( VectorLength( outRight ) <= 0.001f ) {
        VectorSet( outRight, 0.0f, 1.0f, 0.0f );
    }
    if ( VectorLength( outUp ) <= 0.001f ) {
        VectorSet( outUp, 0.0f, 0.0f, 1.0f );
    }

    VectorNormalize( outForward );
    VectorNormalize( outRight );
    VectorNormalize( outUp );
    return true;
}

static bool CLG_DecalClip_WorldPointToInlineLocal( const centity_t *entity, const vec3_t worldPoint, vec3_t outLocalPoint ) {
    if ( !entity || !worldPoint || !outLocalPoint ) {
        return false;
    }

    vec3_t basisForward = {};
    vec3_t basisRight = {};
    vec3_t basisUp = {};
    if ( !CLG_DecalClip_BuildInlineEntityBasis( entity, basisForward, basisRight, basisUp ) ) {
        return false;
    }

    vec3_t toPoint = {};
    toPoint[ 0 ] = worldPoint[ 0 ] - entity->lerpOrigin.x;
    toPoint[ 1 ] = worldPoint[ 1 ] - entity->lerpOrigin.y;
    toPoint[ 2 ] = worldPoint[ 2 ] - entity->lerpOrigin.z;

    outLocalPoint[ 0 ] = DotProduct( toPoint, basisForward );
    outLocalPoint[ 1 ] = DotProduct( toPoint, basisRight );
    outLocalPoint[ 2 ] = DotProduct( toPoint, basisUp );
    return true;
}

static bool CLG_DecalClip_InlineLocalPointToWorld( const centity_t *entity, const vec3_t localPoint, vec3_t outWorldPoint ) {
    if ( !entity || !localPoint || !outWorldPoint ) {
        return false;
    }

    vec3_t basisForward = {};
    vec3_t basisRight = {};
    vec3_t basisUp = {};
    if ( !CLG_DecalClip_BuildInlineEntityBasis( entity, basisForward, basisRight, basisUp ) ) {
        return false;
    }

    outWorldPoint[ 0 ] = entity->lerpOrigin.x;
    outWorldPoint[ 1 ] = entity->lerpOrigin.y;
    outWorldPoint[ 2 ] = entity->lerpOrigin.z;
    VectorMA( outWorldPoint, localPoint[ 0 ], basisForward, outWorldPoint );
    VectorMA( outWorldPoint, localPoint[ 1 ], basisRight, outWorldPoint );
    VectorMA( outWorldPoint, localPoint[ 2 ], basisUp, outWorldPoint );
    return true;
}

static bool CLG_DecalClip_InlineLocalNormalToWorld( const centity_t *entity, const vec3_t localNormal, vec3_t outWorldNormal ) {
    if ( !entity || !localNormal || !outWorldNormal ) {
        return false;
    }

    vec3_t basisForward = {};
    vec3_t basisRight = {};
    vec3_t basisUp = {};
    if ( !CLG_DecalClip_BuildInlineEntityBasis( entity, basisForward, basisRight, basisUp ) ) {
        return false;
    }

    VectorClear( outWorldNormal );
    VectorMA( outWorldNormal, localNormal[ 0 ], basisForward, outWorldNormal );
    VectorMA( outWorldNormal, localNormal[ 1 ], basisRight, outWorldNormal );
    VectorMA( outWorldNormal, localNormal[ 2 ], basisUp, outWorldNormal );
    if ( VectorLength( outWorldNormal ) <= 0.001f ) {
        return false;
    }

    VectorNormalize( outWorldNormal );
    return true;
}

static bool CLG_DecalClip_BuildInlineVolumeLocalBounds( const clg_decal_clip_context_t &context, const centity_t *inlineBrushEntity, vec3_t outLocalMins, vec3_t outLocalMaxs ) {
    if ( !inlineBrushEntity || !outLocalMins || !outLocalMaxs ) {
        return false;
    }

    vec3_t worldCorners[ 8 ] = {};
    vec3_t localCorner = {};
    CLG_DecalClip_BuildVolumeCorners( context, worldCorners );

    if ( !CLG_DecalClip_WorldPointToInlineLocal( inlineBrushEntity, worldCorners[ 0 ], localCorner ) ) {
        return false;
    }

    VectorCopy( localCorner, outLocalMins );
    VectorCopy( localCorner, outLocalMaxs );
    for ( int32_t i = 1; i < 8; i++ ) {
        if ( !CLG_DecalClip_WorldPointToInlineLocal( inlineBrushEntity, worldCorners[ i ], localCorner ) ) {
            return false;
        }

        for ( int32_t axis = 0; axis < 3; axis++ ) {
            outLocalMins[ axis ] = std::min( outLocalMins[ axis ], localCorner[ axis ] );
            outLocalMaxs[ axis ] = std::max( outLocalMaxs[ axis ], localCorner[ axis ] );
        }
    }

    return true;
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

    float angles[ CLG_DECAL_CLIP_POLYGON_MAX_VERTICES ] = {};
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

static bool CLG_DecalClip_SurfaceHasConcretePolygonSource( const clg_world_surface_t *surface ) {
    if ( !surface ) {
        return false;
    }

    return ( surface->bspFace != nullptr || ( surface->collisionBrush != nullptr && surface->collisionBrushSide != nullptr ) );
}

static bool CLG_DecalClip_TryAddSurfaceCandidate( const clg_decal_clip_context_t &context, const vec3_t origin, const vec3_t normal, const bool containsImpactPoint, const mface_t *bspFace, const int32_t entityNumber, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return false;
    }

    vec3_t orientedNormal = {};
    VectorCopy( normal, orientedNormal );
    CLG_DecalClip_AlignNormalToProjection( context, orientedNormal );
    const bool hasConcretePolygonSource = ( bspFace != nullptr );

    /**
    *    Keep receiver domains constrained:
    *    - world impacts clip only against world BSP faces/fallbacks
    *    - inline brush impacts accept inline brush receivers first, with optional world
    *      rescue when inline gather cannot produce usable candidates.
    *    Mesh assembly later locks output to one chosen domain so one decal still does
    *    not mix brush and world receivers in the final submission.
    **/
    const centity_t *inlineBrushEntity = CLG_DecalClip_GetInlineBrushEntity( context.spawn.hitEntityNumber );
    if ( inlineBrushEntity ) {
        if ( entityNumber != context.spawn.hitEntityNumber && entityNumber != ENTITYNUM_WORLD ) {
            return false;
        }
    } else if ( entityNumber != ENTITYNUM_WORLD ) {
        return false;
    }

    /**
    *    Keep facing rejection for plane-only fallback receivers, but allow concrete BSP faces
    *    to continue into polygon-vs-OBB clipping even when they are nearly perpendicular.
    *    Wrapped corner decals need those adjacent faces to survive this stage.
    **/
    if ( !hasConcretePolygonSource && !CLG_DecalClip_IsSurfaceFacingProjection( context, orientedNormal ) ) {
        return false;
    }

    /**
    *    For concrete BSP faces, defer depth overlap decisions to polygon-vs-OBB clipping.
    *    Center-depth rejection here can incorrectly drop adjacent edge faces that should
    *    contribute to wrapped decals.
    **/
    if ( !hasConcretePolygonSource ) {
        const float signedDepth = CLG_DecalClip_ComputeSignedDepth( context, origin );
        if ( !CLG_DecalClip_IsDepthInsideVolume( context, signedDepth ) ) {
            return false;
        }
    }

    /**
    *    Keep one candidate per concrete BSP face, and only keep plane-only fallbacks
    *    when no resolved receiver on that plane already exists.
    **/
    for ( int32_t i = 0; i < *inOutCount; i++ ) {
        const clg_world_surface_t *existing = &outSurfaces[ i ];
        if ( bspFace && existing->bspFace == bspFace && existing->entityNumber == entityNumber ) {
            return false;
        }

        const float normalDot = fabsf( DotProduct( orientedNormal, existing->normal ) );
		const float signedDepth = CLG_DecalClip_ComputeSignedDepth( context, origin );
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
	surface->collisionBrush = nullptr;
	surface->collisionBrushSide = nullptr;

    vec3_t tangent = {};
    vec3_t bitangent = {};
    CLG_DecalClip_BuildSurfaceBasis( context, surface->normal, tangent, bitangent );
    VectorCopy( tangent, surface->tangent );
    VectorCopy( bitangent, surface->bitangent );
    return true;
}

static bool CLG_DecalClip_TryAddCollisionBrushSideCandidate( const clg_decal_clip_context_t &context, const mbrush_t *brush, const mbrushside_t *brushSide, const vec3_t referencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !brush || !brushSide || !brush->firstbrushside || brush->numsides <= 0 || !brushSide->plane || !brushSide->texinfo || !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return false;
    }

    if ( ( brushSide->texinfo->c.flags & ( CM_SURFACE_FLAG_SKY | CM_SURFACE_NODRAW ) ) != 0 ) {
        return false;
    }

    for ( int32_t i = 0; i < *inOutCount; i++ ) {
        const clg_world_surface_t *existing = &outSurfaces[ i ];
        if ( existing->collisionBrush == brush && existing->collisionBrushSide == brushSide ) {
            return false;
        }
    }

    clg_decal_clip_polygon_t facePolygon = {};
    if ( !CLG_DecalClip_BuildPolygonFromCollisionBrushSide( brush, brushSide, referencePoint, &facePolygon ) ) {
        return false;
    }

    vec3_t projectedOrigin = {};
    if ( !CLG_DecalClip_ProjectPointOntoCollisionBrushSidePlane( brushSide, referencePoint, projectedOrigin ) ) {
        return false;
    }

    float impactInset = -FLT_MAX;
    const bool containsImpactPoint = CLG_DecalClip_ComputeProjectedPointInsetToPolygon( facePolygon, projectedOrigin, brushSide->plane->normal, &impactInset ) && impactInset >= -CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON;

    vec3_t orientedNormal = {};
    VectorCopy( brushSide->plane->normal, orientedNormal );
    CLG_DecalClip_AlignNormalToProjection( context, orientedNormal );

    const centity_t *inlineBrushEntity = CLG_DecalClip_GetInlineBrushEntity( context.spawn.hitEntityNumber );
    if ( inlineBrushEntity ) {
        return false;
    }

    if ( *inOutCount >= maxSurfaces ) {
        return false;
    }

    clg_world_surface_t *surface = &outSurfaces[ ( *inOutCount )++ ];
    VectorCopy( projectedOrigin, surface->origin );
    VectorCopy( orientedNormal, surface->normal );
    surface->containsImpactPoint = containsImpactPoint;
    surface->entityNumber = ENTITYNUM_WORLD;
    surface->bspFace = nullptr;
    surface->collisionBrush = brush;
    surface->collisionBrushSide = brushSide;

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

static bool CLG_DecalClip_TryAddInlineBspFaceCandidate( const clg_decal_clip_context_t &context, const centity_t *inlineBrushEntity, const mface_t *face, const vec3_t localReferencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !inlineBrushEntity || !face || !face->plane || !face->texinfo || face->numsurfedges < 3 ) {
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

    vec3_t faceNormalLocal = {};
    CLG_DecalClip_GetBspFaceNormal( face, faceNormalLocal );

    vec3_t projectedOriginLocal = {};
    if ( !CLG_DecalClip_ProjectPointOntoBspFacePlane( face, localReferencePoint, projectedOriginLocal ) ) {
        return false;
    }

    float impactInset = -FLT_MAX;
    const bool containsImpactPoint = CLG_DecalClip_ComputeProjectedPointInsetToFace( face, projectedOriginLocal, faceNormalLocal, &impactInset ) && impactInset >= -CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON;

    vec3_t projectedOriginWorld = {};
    if ( !CLG_DecalClip_InlineLocalPointToWorld( inlineBrushEntity, projectedOriginLocal, projectedOriginWorld ) ) {
        return false;
    }

    vec3_t faceNormalWorld = {};
    if ( !CLG_DecalClip_InlineLocalNormalToWorld( inlineBrushEntity, faceNormalLocal, faceNormalWorld ) ) {
        return false;
    }

    return CLG_DecalClip_TryAddSurfaceCandidate( context, projectedOriginWorld, faceNormalWorld, containsImpactPoint, face, context.spawn.hitEntityNumber, outSurfaces, maxSurfaces, inOutCount );
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

static const mface_t *CLG_DecalClip_FindWorldFaceBySurfaceHandle( const clg_decal_clip_context_t &context, const uintptr_t surfaceHandle ) {
    if ( surfaceHandle == 0u || !clgi.client || !clgi.client->collisionModel.cache ) {
        return nullptr;
    }

    const bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp->faces || worldBsp->numfaces <= 0 || !worldBsp->brushsides || worldBsp->numbrushsides <= 0 ) {
        return nullptr;
    }

    const mbrushside_t *impactBrushSide = (const mbrushside_t *)surfaceHandle;
    if ( !CLG_DecalClip_IsAddressRangeInsideSpan( impactBrushSide, sizeof( *impactBrushSide ), worldBsp->brushsides, (size_t)worldBsp->numbrushsides * sizeof( *impactBrushSide ) ) ) {
        return nullptr;
    }

    const mface_t *bestFace = nullptr;
    float bestImpactInset = -FLT_MAX;

    /**
    *    Collision traces identify the struck world brush side. Resolve it back to the one render
    *    face on the same plane/texinfo combination that actually contains the impact point.
    **/
    for ( int32_t faceIndex = 0; faceIndex < worldBsp->numfaces; faceIndex++ ) {
        const mface_t *face = &worldBsp->faces[ faceIndex ];
        if ( !face || !face->texinfo ) {
            continue;
        }

        if ( face->texinfo != impactBrushSide->texinfo || face->plane != impactBrushSide->plane ) {
            continue;
        }

        if ( !CLG_DecalClip_IsValidBspFaceGeometry( worldBsp, face ) ) {
            continue;
        }

        vec3_t projectedImpactPoint = {};
        if ( !CLG_DecalClip_ProjectPointOntoBspFacePlane( face, context.spawn.origin, projectedImpactPoint ) ) {
            continue;
        }

        vec3_t faceNormal = {};
        CLG_DecalClip_GetBspFaceNormal( face, faceNormal );

        float impactInset = -FLT_MAX;
        if ( !CLG_DecalClip_ComputeProjectedPointInsetToFace( face, projectedImpactPoint, faceNormal, &impactInset ) ) {
            continue;
        }

        if ( impactInset < -CLG_DECAL_FACE_CONTAINMENT_EDGE_EPSILON ) {
            continue;
        }

        if ( !bestFace || impactInset > bestImpactInset ) {
            bestFace = face;
            bestImpactInset = impactInset;
        }
    }

    return bestFace;
}

static const mbrush_t *CLG_DecalClip_FindOwningWorldBrushForSide( const mbrushside_t *brushSide ) {
    if ( !brushSide || !clgi.client || !clgi.client->collisionModel.cache ) {
        return nullptr;
    }

    const bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp->brushes || worldBsp->numbrushes <= 0 ) {
        return nullptr;
    }

    for ( int32_t brushIndex = 0; brushIndex < worldBsp->numbrushes; brushIndex++ ) {
        const mbrush_t *brush = &worldBsp->brushes[ brushIndex ];
        if ( !brush || !brush->firstbrushside || brush->numsides <= 0 ) {
            continue;
        }

        if ( CLG_DecalClip_IsAddressRangeInsideSpan( brushSide, sizeof( *brushSide ), brush->firstbrushside, (size_t)brush->numsides * sizeof( *brushSide ) ) ) {
            return brush;
        }
    }

    return nullptr;
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

static const mmodel_t *CLG_DecalClip_FindInlineBrushModel( const centity_t *inlineBrushEntity ) {
    if ( !inlineBrushEntity || !clgi.GetEntityHullNode || !clgi.client || !clgi.client->collisionModel.cache ) {
        return nullptr;
    }

    const bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp->models || worldBsp->nummodels <= 0 ) {
        return nullptr;
    }

    const mnode_t *inlineHeadNode = clgi.GetEntityHullNode( inlineBrushEntity );
    if ( !inlineHeadNode ) {
        return nullptr;
    }

    for ( int32_t modelIndex = 0; modelIndex < worldBsp->nummodels; modelIndex++ ) {
        const mmodel_t *model = &worldBsp->models[ modelIndex ];
        if ( !model || model->headnode != inlineHeadNode ) {
            continue;
        }

        if ( model->numfaces <= 0 || !model->firstface ) {
            continue;
        }

        return model;
    }

    return nullptr;
}

static void CLG_DecalClip_GatherInlineModelFaceCandidates( const clg_decal_clip_context_t &context, const centity_t *inlineBrushEntity, const mmodel_t *inlineModel, const vec3_t localQueryMins, const vec3_t localQueryMaxs, const vec3_t localReferencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !inlineBrushEntity || !inlineModel || !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return;
    }

    if ( !clgi.client || !clgi.client->collisionModel.cache ) {
        return;
    }

    const bsp_t *worldBsp = clgi.client->collisionModel.cache;
    for ( int32_t faceIndex = 0; faceIndex < inlineModel->numfaces && *inOutCount < maxSurfaces; faceIndex++ ) {
        const mface_t *face = inlineModel->firstface + faceIndex;
        if ( !CLG_DecalClip_IsFacePointerInWorldBsp( worldBsp, face ) ) {
            continue;
        }

        clg_decal_clip_polygon_t localFacePolygon = {};
        if ( !CLG_DecalClip_BuildPolygonFromBspFace( face, &localFacePolygon ) ) {
            continue;
        }

        vec3_t faceMins = {};
        vec3_t faceMaxs = {};
        VectorCopy( localFacePolygon.positions[ 0 ], faceMins );
        VectorCopy( localFacePolygon.positions[ 0 ], faceMaxs );
        for ( int32_t vertexIndex = 1; vertexIndex < localFacePolygon.vertexCount; vertexIndex++ ) {
            for ( int32_t axis = 0; axis < 3; axis++ ) {
                faceMins[ axis ] = std::min( faceMins[ axis ], localFacePolygon.positions[ vertexIndex ][ axis ] );
                faceMaxs[ axis ] = std::max( faceMaxs[ axis ], localFacePolygon.positions[ vertexIndex ][ axis ] );
            }
        }

        if ( !CLG_DecalClip_DoBoundsOverlap( localQueryMins, localQueryMaxs, faceMins, faceMaxs ) ) {
            continue;
        }

        (void)CLG_DecalClip_TryAddInlineBspFaceCandidate( context, inlineBrushEntity, face, localReferencePoint, outSurfaces, maxSurfaces, inOutCount );
    }
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

static void CLG_DecalClip_GatherInlineNodeFaceCandidatesRecursive( const clg_decal_clip_context_t &context, const centity_t *inlineBrushEntity, const mnode_t *node, const vec3_t localQueryMins, const vec3_t localQueryMaxs, const vec3_t localReferencePoint, clg_world_surface_t *outSurfaces, const int32_t maxSurfaces, int32_t *inOutCount ) {
    if ( !inlineBrushEntity || !node || !node->plane || !outSurfaces || !inOutCount || *inOutCount >= maxSurfaces ) {
        return;
    }

    if ( !CLG_DecalClip_DoBoundsOverlap( localQueryMins, localQueryMaxs, node->mins, node->maxs ) ) {
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

        (void)CLG_DecalClip_TryAddInlineBspFaceCandidate( context, inlineBrushEntity, face, localReferencePoint, outSurfaces, maxSurfaces, inOutCount );
    }

    CLG_DecalClip_GatherInlineNodeFaceCandidatesRecursive( context, inlineBrushEntity, node->children[ 0 ], localQueryMins, localQueryMaxs, localReferencePoint, outSurfaces, maxSurfaces, inOutCount );
    CLG_DecalClip_GatherInlineNodeFaceCandidatesRecursive( context, inlineBrushEntity, node->children[ 1 ], localQueryMins, localQueryMaxs, localReferencePoint, outSurfaces, maxSurfaces, inOutCount );
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

static bool CLG_DecalClip_BuildPolygonFromCollisionBrushSide( const mbrush_t *brush, const mbrushside_t *brushSide, const vec3_t referencePoint, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !brush || !brushSide || !brush->firstbrushside || brush->numsides < 3 || !brushSide->plane || !referencePoint || !outPolygon ) {
        return false;
    }

    memset( outPolygon, 0, sizeof( *outPolygon ) );

    vec3_t tangent = {};
    vec3_t bitangent = {};
    vec3_t normal = {};
    CLG_DecalClip_BuildBasis( brushSide->plane->normal, tangent, bitangent, normal );

    /**
    *    Seed collision-side winding recovery around the projected impact point instead of
    *    normal*dist from world origin. This keeps the temporary winding local to the
    *    actually struck area on large/tangent-offset planes.
    **/
    vec3_t projectedSeedCenter = {};
    VectorCopy( referencePoint, projectedSeedCenter );
    const float seedPlaneDistance = PlaneDiff( referencePoint, brushSide->plane );
    VectorMA( projectedSeedCenter, -seedPlaneDistance, brushSide->plane->normal, projectedSeedCenter );

    const float seedExtent = CLG_DECAL_COLLISION_FACE_SEED_EXTENT;
    vec3_t seedCorners[ 4 ] = {};

    VectorCopy( projectedSeedCenter, seedCorners[ 0 ] );
    VectorMA( seedCorners[ 0 ], seedExtent, tangent, seedCorners[ 0 ] );
    VectorMA( seedCorners[ 0 ], seedExtent, bitangent, seedCorners[ 0 ] );

    VectorCopy( projectedSeedCenter, seedCorners[ 1 ] );
    VectorMA( seedCorners[ 1 ], -seedExtent, tangent, seedCorners[ 1 ] );
    VectorMA( seedCorners[ 1 ], seedExtent, bitangent, seedCorners[ 1 ] );

    VectorCopy( projectedSeedCenter, seedCorners[ 2 ] );
    VectorMA( seedCorners[ 2 ], -seedExtent, tangent, seedCorners[ 2 ] );
    VectorMA( seedCorners[ 2 ], -seedExtent, bitangent, seedCorners[ 2 ] );

    VectorCopy( projectedSeedCenter, seedCorners[ 3 ] );
    VectorMA( seedCorners[ 3 ], seedExtent, tangent, seedCorners[ 3 ] );
    VectorMA( seedCorners[ 3 ], -seedExtent, bitangent, seedCorners[ 3 ] );

    for ( int32_t cornerIndex = 0; cornerIndex < 4; cornerIndex++ ) {
        if ( !CLG_DecalClip_TryAppendUniqueVertex( seedCorners[ cornerIndex ], outPolygon ) ) {
            return false;
        }
    }

    /**
    *    Recover the actual collision-side polygon by clipping the large on-plane seed quad
    *    against every other side of the owning convex brush.
    **/
    for ( int32_t sideIndex = 0; sideIndex < brush->numsides; sideIndex++ ) {
        const mbrushside_t *clipSide = brush->firstbrushside + sideIndex;
        if ( !clipSide || clipSide == brushSide || !clipSide->plane ) {
            continue;
        }

        vec3_t clipPlanePoint = {};
        if ( !CLG_DecalClip_ProjectPointOntoPlane( clipSide->plane, projectedSeedCenter, clipPlanePoint ) ) {
            return false;
        }

        clg_decal_clip_polygon_t scratchPolygon = {};
        if ( !CLG_DecalClip_ClipPolygonAgainstPlane( *outPolygon, clipPlanePoint, clipSide->plane->normal, &scratchPolygon ) ) {
            return false;
        }

        *outPolygon = scratchPolygon;
    }

    return ( outPolygon->vertexCount >= 3 );
}

static bool CLG_DecalClip_ProjectPointOntoPlane( const cm_plane_t *plane, const vec3_t point, vec3_t outProjectedPoint ) {
    if ( !plane || !outProjectedPoint ) {
        return false;
    }

    const float normalLengthSquared = DotProduct( plane->normal, plane->normal );
    if ( normalLengthSquared <= 0.000001f ) {
        return false;
    }

    VectorCopy( point, outProjectedPoint );
    const float signedDistance = PlaneDiff( point, plane ) / normalLengthSquared;
    VectorMA( outProjectedPoint, -signedDistance, plane->normal, outProjectedPoint );
    return true;
}

static bool CLG_DecalClip_ProjectPointOntoBspFacePlane( const mface_t *face, const vec3_t point, vec3_t outProjectedPoint ) {
    if ( !face || !face->plane || !outProjectedPoint ) {
        return false;
    }

    return CLG_DecalClip_ProjectPointOntoPlane( face->plane, point, outProjectedPoint );
}

static bool CLG_DecalClip_ProjectPointOntoCollisionBrushSidePlane( const mbrushside_t *brushSide, const vec3_t point, vec3_t outProjectedPoint ) {
    if ( !brushSide || !brushSide->plane || !outProjectedPoint ) {
        return false;
    }

    return CLG_DecalClip_ProjectPointOntoPlane( brushSide->plane, point, outProjectedPoint );
}

static bool CLG_DecalClip_ComputeProjectedPointInsetToPolygon( const clg_decal_clip_polygon_t &polygon, const vec3_t projectedPoint, const vec3_t faceNormal, float *outInset ) {
    if ( !outInset || polygon.vertexCount < 3 ) {
        return false;
    }

    vec3_t polygonNormal = {};

    /**
    *    Accumulate a stable polygon normal so edge tests can respect the stored winding.
    **/
    for ( int32_t i = 0; i < polygon.vertexCount; i++ ) {
        const int32_t nextIndex = ( i + 1 ) % polygon.vertexCount;
        vec3_t edgeCross = {};
        CrossProduct( polygon.positions[ i ], polygon.positions[ nextIndex ], edgeCross );
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
    for ( int32_t i = 0; i < polygon.vertexCount; i++ ) {
        const int32_t nextIndex = ( i + 1 ) % polygon.vertexCount;
        vec3_t edge = {};
        VectorSubtract( polygon.positions[ nextIndex ], polygon.positions[ i ], edge );
        const float edgeLength = VectorLength( edge );
        if ( edgeLength <= 0.001f ) {
            continue;
        }

        vec3_t toPoint = {};
        VectorSubtract( projectedPoint, polygon.positions[ i ], toPoint );

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

static bool CLG_DecalClip_ComputeProjectedPointInsetToFace( const mface_t *face, const vec3_t projectedPoint, const vec3_t faceNormal, float *outInset ) {
    if ( !outInset ) {
        return false;
    }

    clg_decal_clip_polygon_t facePolygon = {};
    if ( !CLG_DecalClip_BuildPolygonFromBspFace( face, &facePolygon ) ) {
        return false;
    }

    return CLG_DecalClip_ComputeProjectedPointInsetToPolygon( facePolygon, projectedPoint, faceNormal, outInset );
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

/**
*    @brief  Print projection-space bounds for a world face that failed concrete OBB clipping.
*    @param  context Decal clip context.
*    @param  surface Concrete world BSP candidate.
*    @param  polygon Working polygon state at the rejecting clip plane.
*    @param  rejectPlaneIndex Clip-plane index that emptied the polygon.
**/
static void CLG_DecalClip_DebugLogConcreteWorldFaceClipFailure( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, const clg_decal_clip_polygon_t &polygon, const int32_t rejectPlaneIndex ) {
    if ( !CLG_DecalClip_IsDebugLevel( 2 ) || !surface || !surface->bspFace || surface->entityNumber != ENTITYNUM_WORLD || polygon.vertexCount < 3 ) {
        return;
    }

    float rightMin = FLT_MAX;
    float rightMax = -FLT_MAX;
    float upMin = FLT_MAX;
    float upMax = -FLT_MAX;
    float forwardMin = FLT_MAX;
    float forwardMax = -FLT_MAX;

    /**
    *    Measure the candidate face extent in decal-local projection space so failed world
    *    clips can report whether the polygon actually overlaps the footprint or dies on a
    *    specific local axis range.
    **/
    for ( int32_t i = 0; i < polygon.vertexCount; i++ ) {
        vec3_t toVertex = {};
        VectorSubtract( polygon.positions[ i ], context.spawn.origin, toVertex );

        const float rightDistance = DotProduct( toVertex, context.basisRight );
        const float upDistance = DotProduct( toVertex, context.basisUp );
        const float forwardDistance = DotProduct( toVertex, context.basisForward );

        rightMin = std::min( rightMin, rightDistance );
        rightMax = std::max( rightMax, rightDistance );
        upMin = std::min( upMin, upDistance );
        upMax = std::max( upMax, upDistance );
        forwardMin = std::min( forwardMin, forwardDistance );
        forwardMax = std::max( forwardMax, forwardDistance );
    }

    const float planeDistance = surface->bspFace->plane ? fabsf( PlaneDiff( context.spawn.origin, surface->bspFace->plane ) ) : -1.0f;
    const float facingDot = fabsf( DotProduct( surface->normal, context.basisForward ) );

    clgi.Print( PRINT_DEVELOPER,
        "[CLG Decals][ClipDbg] world-face clip-fail reject-plane:%d contains-impact:%s surfedges:%d plane-dist:%.3f facing:%.3f right:[%.2f,%.2f] up:[%.2f,%.2f] fwd:[%.2f,%.2f]\n",
        rejectPlaneIndex,
        surface->containsImpactPoint ? "yes" : "no",
        surface->bspFace->numsurfedges,
        planeDistance,
        facingDot,
        rightMin,
        rightMax,
        upMin,
        upMax,
        forwardMin,
        forwardMax );
}

/**
*    @brief  Print projection-space bounds for a collision brush-side concrete clip failure.
*    @param  context Decal clip context.
*    @param  surface Concrete collision brush-side candidate.
*    @param  polygon Working polygon state at the rejecting clip plane.
*    @param  rejectPlaneIndex Clip-plane index that emptied the polygon.
**/
static void CLG_DecalClip_DebugLogConcreteCollisionSideClipFailure( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, const clg_decal_clip_polygon_t &polygon, const int32_t rejectPlaneIndex ) {
    if ( !CLG_DecalClip_IsDebugLevel( 2 ) || !surface || !surface->collisionBrushSide || surface->entityNumber != ENTITYNUM_WORLD || polygon.vertexCount < 3 ) {
        return;
    }

    float rightMin = FLT_MAX;
    float rightMax = -FLT_MAX;
    float upMin = FLT_MAX;
    float upMax = -FLT_MAX;
    float forwardMin = FLT_MAX;
    float forwardMax = -FLT_MAX;

    for ( int32_t i = 0; i < polygon.vertexCount; i++ ) {
        vec3_t toVertex = {};
        VectorSubtract( polygon.positions[ i ], context.spawn.origin, toVertex );

        const float rightDistance = DotProduct( toVertex, context.basisRight );
        const float upDistance = DotProduct( toVertex, context.basisUp );
        const float forwardDistance = DotProduct( toVertex, context.basisForward );

        rightMin = std::min( rightMin, rightDistance );
        rightMax = std::max( rightMax, rightDistance );
        upMin = std::min( upMin, upDistance );
        upMax = std::max( upMax, upDistance );
        forwardMin = std::min( forwardMin, forwardDistance );
        forwardMax = std::max( forwardMax, forwardDistance );
    }

    const float planeDistance = surface->collisionBrushSide->plane ? fabsf( PlaneDiff( context.spawn.origin, surface->collisionBrushSide->plane ) ) : -1.0f;
    const float facingDot = fabsf( DotProduct( surface->normal, context.basisForward ) );

    clgi.Print( PRINT_DEVELOPER,
        "[CLG Decals][ClipDbg] world-collision clip-fail reject-plane:%d contains-impact:%s plane-dist:%.3f facing:%.3f right:[%.2f,%.2f] up:[%.2f,%.2f] fwd:[%.2f,%.2f]\n",
        rejectPlaneIndex,
        surface->containsImpactPoint ? "yes" : "no",
        planeDistance,
        facingDot,
        rightMin,
        rightMax,
        upMin,
        upMax,
        forwardMin,
        forwardMax );
}

static bool CLG_DecalClip_ClipBspFaceToDecalVolume( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon ) {
    if ( !surface || !outPolygon ) {
        return false;
    }

    clg_decal_clip_polygon_t workingPolygon = {};
    clg_decal_clip_polygon_t scratchPolygon = {};

    /**
    *    Build the concrete receiver polygon from either the resolved render face or the
    *    traced collision brush side when face recovery misses the actual struck receiver.
    **/
    if ( surface->bspFace ) {
        if ( !CLG_DecalClip_BuildPolygonFromBspFace( surface->bspFace, &workingPolygon ) ) {
            return false;
        }
    } else if ( surface->collisionBrush && surface->collisionBrushSide ) {
        if ( !CLG_DecalClip_BuildPolygonFromCollisionBrushSide( surface->collisionBrush, surface->collisionBrushSide, surface->origin, &workingPolygon ) ) {
            return false;
        }
    } else {
        return false;
    }

    /**
    *    Inline brush-model BSP geometry is stored in model-local space. Convert the concrete
    *    winding into world space before clipping against the world-space decal OBB.
    **/
    if ( surface->entityNumber != ENTITYNUM_WORLD ) {
        const centity_t *inlineBrushEntity = CLG_DecalClip_GetInlineBrushEntity( surface->entityNumber );
        if ( !inlineBrushEntity ) {
            return false;
        }

        for ( int32_t i = 0; i < workingPolygon.vertexCount; i++ ) {
            vec3_t worldVertex = {};
            if ( !CLG_DecalClip_InlineLocalPointToWorld( inlineBrushEntity, workingPolygon.positions[ i ], worldVertex ) ) {
                return false;
            }

            VectorCopy( worldVertex, workingPolygon.positions[ i ] );
        }
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

    vec3_t depthOrigin = {};
    VectorCopy( context.spawn.origin, depthOrigin );

    /**
    *    For collision-side concrete receivers, center only the forward/back depth slab on the
    *    side's projected impact point while keeping right/up footprint bounds anchored at the
    *    original impact origin. This preserves decal footprint locality and enables edge wrap
    *    when the owning brush side is offset along projection depth.
    **/
    if ( !surface->bspFace && surface->collisionBrushSide ) {
        VectorCopy( surface->origin, depthOrigin );
    }

    VectorCopy( context.spawn.origin, planePoints[ 0 ] );
    VectorMA( planePoints[ 0 ], context.halfSize, context.basisRight, planePoints[ 0 ] );
    VectorCopy( context.spawn.origin, planePoints[ 1 ] );
    VectorMA( planePoints[ 1 ], -context.halfSize, context.basisRight, planePoints[ 1 ] );
    VectorCopy( context.spawn.origin, planePoints[ 2 ] );
    VectorMA( planePoints[ 2 ], context.halfSize, context.basisUp, planePoints[ 2 ] );
    VectorCopy( context.spawn.origin, planePoints[ 3 ] );
    VectorMA( planePoints[ 3 ], -context.halfSize, context.basisUp, planePoints[ 3 ] );
    VectorCopy( depthOrigin, planePoints[ 4 ] );
    VectorMA( planePoints[ 4 ], context.halfDepth, context.basisForward, planePoints[ 4 ] );
    VectorCopy( depthOrigin, planePoints[ 5 ] );
    VectorMA( planePoints[ 5 ], -context.halfDepth, context.basisForward, planePoints[ 5 ] );

    /**
    *    Clip the actual face winding against each side of the oriented decal box.
    **/
    for ( int32_t planeIndex = 0; planeIndex < 6; planeIndex++ ) {
        if ( !CLG_DecalClip_ClipPolygonAgainstPlane( workingPolygon, planePoints[ planeIndex ], planeNormals[ planeIndex ], &scratchPolygon ) ) {
            if ( surface->bspFace ) {
                CLG_DecalClip_DebugLogConcreteWorldFaceClipFailure( context, surface, workingPolygon, planeIndex );
            } else if ( surface->collisionBrushSide ) {
                CLG_DecalClip_DebugLogConcreteCollisionSideClipFailure( context, surface, workingPolygon, planeIndex );
            }
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
    bool impactSeededConcreteFace = false;
    vec3_t queryMins = {};
    vec3_t queryMaxs = {};
    CLG_DecalClip_BuildVolumeBounds( context, queryMins, queryMaxs );
    const centity_t *inlineBrushEntity = CLG_DecalClip_GetInlineBrushEntity( context.spawn.hitEntityNumber );

    /**
    *    Seed world impacts with the exact traced brush-side face when available. This anchors
    *    clipped decals to the real hit receiver before nearby leaf/node scans add neighboring faces
    *    for edge wrapping.
    **/
    if ( !inlineBrushEntity && context.spawn.hitSurfaceHandle != 0u ) {
        const mbrushside_t *impactBrushSide = (const mbrushside_t *)context.spawn.hitSurfaceHandle;
        const mbrush_t *impactBrush = CLG_DecalClip_FindOwningWorldBrushForSide( impactBrushSide );
        if ( impactBrush ) {
            impactSeededConcreteFace = CLG_DecalClip_TryAddCollisionBrushSideCandidate( context, impactBrush, impactBrushSide, context.spawn.origin, outSurfaces, maxSurfaces, &outCount );

            /**
            *    Seed connected sides from the same collision brush so edge-adjacent wrap faces
            *    survive even when render-face lookup falls back to distant coplanar geometry.
            **/
            for ( int32_t sideIndex = 0; sideIndex < impactBrush->numsides && outCount < maxSurfaces; sideIndex++ ) {
                const mbrushside_t *neighborSide = impactBrush->firstbrushside + sideIndex;
                if ( !neighborSide || neighborSide == impactBrushSide ) {
                    continue;
                }

                (void)CLG_DecalClip_TryAddCollisionBrushSideCandidate( context, impactBrush, neighborSide, context.spawn.origin, outSurfaces, maxSurfaces, &outCount );
            }
        } else {
            const mface_t *impactFace = CLG_DecalClip_FindWorldFaceBySurfaceHandle( context, context.spawn.hitSurfaceHandle );
            if ( impactFace ) {
                impactSeededConcreteFace = CLG_DecalClip_TryAddBspFaceCandidate( context, impactFace, context.spawn.origin, outSurfaces, maxSurfaces, &outCount );
            }
        }
    }

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
    *    Gather from overlapping leaves first, then include intersecting node-owned faces
    *    to catch adjacent receivers that are not referenced directly by leaf face lists.
    **/
    if ( inlineBrushEntity ) {
        vec3_t localQueryMins = {};
        vec3_t localQueryMaxs = {};
        vec3_t localReferencePoint = {};
        const mmodel_t *inlineModel = CLG_DecalClip_FindInlineBrushModel( inlineBrushEntity );
        const mnode_t *inlineHeadNode = clgi.GetEntityHullNode( inlineBrushEntity );
        if ( CLG_DecalClip_BuildInlineVolumeLocalBounds( context, inlineBrushEntity, localQueryMins, localQueryMaxs ) &&
            CLG_DecalClip_WorldPointToInlineLocal( inlineBrushEntity, context.spawn.origin, localReferencePoint ) ) {
            for ( int32_t axis = 0; axis < 3; axis++ ) {
                localQueryMins[ axis ] -= queryExpansion;
                localQueryMaxs[ axis ] += queryExpansion;
            }

            if ( inlineModel ) {
                CLG_DecalClip_GatherInlineModelFaceCandidates( context, inlineBrushEntity, inlineModel, localQueryMins, localQueryMaxs, localReferencePoint, outSurfaces, maxSurfaces, &outCount );
            }

            if ( outCount <= 0 && inlineHeadNode ) {
                CLG_DecalClip_GatherInlineNodeFaceCandidatesRecursive( context, inlineBrushEntity, inlineHeadNode, localQueryMins, localQueryMaxs, localReferencePoint, outSurfaces, maxSurfaces, &outCount );
            }

            if ( outCount > 0 ) {
                impactSeededConcreteFace = true;
            }
        }
    } else {
        CLG_DecalClip_GatherLeafFaceCandidates( context, queryMins, queryMaxs, outSurfaces, maxSurfaces, &outCount );
        CLG_DecalClip_GatherNodeFaceCandidatesRecursive( context, clgi.client->collisionModel.cache->nodes, queryMins, queryMaxs, outSurfaces, maxSurfaces, &outCount );
    }
    const int32_t broadPhaseCandidateCount = outCount;

    bool worldRescueGatherUsed = false;

    int32_t concreteCandidateCount = 0;
    for ( int32_t i = 0; i < outCount; i++ ) {
        if ( CLG_DecalClip_SurfaceHasConcretePolygonSource( &outSurfaces[ i ] ) ) {
            concreteCandidateCount++;
        }
    }

    /**
    *    Some static/detail brush slopes can report a non-world hit entity even though
    *    world BSP face gather is the only path that yields concrete clip faces.
    *    If inline-domain gather produced no candidates at all, retry once in world domain.
    **/
    if ( inlineBrushEntity && concreteCandidateCount <= 0 ) {
        const int32_t preRescueCount = outCount;
        clg_decal_clip_context_t worldContext = context;
        worldContext.spawn.hitEntityNumber = ENTITYNUM_WORLD;
        CLG_DecalClip_GatherLeafFaceCandidates( worldContext, queryMins, queryMaxs, outSurfaces, maxSurfaces, &outCount );
        CLG_DecalClip_GatherNodeFaceCandidatesRecursive( worldContext, clgi.client->collisionModel.cache->nodes, queryMins, queryMaxs, outSurfaces, maxSurfaces, &outCount );
        worldRescueGatherUsed = ( outCount > preRescueCount );

        concreteCandidateCount = 0;
        for ( int32_t i = 0; i < outCount; i++ ) {
            if ( CLG_DecalClip_SurfaceHasConcretePolygonSource( &outSurfaces[ i ] ) ) {
                concreteCandidateCount++;
            }
        }
    }

    if ( CLG_DecalClip_IsDebugLevel( 1 ) ) {
	    clgi.Print( PRINT_DEVELOPER, "[CLG Decals][ClipDbg] hit-entity:%d impact-seed:%s broad-phase candidates:%d concrete:%d world-rescue:%s\n",
		    context.spawn.hitEntityNumber,
		    impactSeededConcreteFace ? "yes" : "no",
            broadPhaseCandidateCount,
            concreteCandidateCount,
            worldRescueGatherUsed ? "yes" : "no" );
    }

    return outCount;
}

const bool CLG_DecalClip_ClipSurfaceToDecal( const clg_decal_clip_context_t &context, const clg_world_surface_t *surface, clg_decal_clip_polygon_t *outPolygon, bool *outUsedConcreteBspClip ) {
    if ( outUsedConcreteBspClip ) {
        *outUsedConcreteBspClip = false;
    }

    if ( !surface || !outPolygon ) {
        return false;
    }

    /**
    *    Only concrete BSP receivers are valid in the clipping path. Plane-only fallback
    *    receivers are handled by the legacy impact-plane submission path when needed.
    **/
    if ( !CLG_DecalClip_SurfaceHasConcretePolygonSource( surface ) ) {
        return false;
    }

    memset( outPolygon, 0, sizeof( *outPolygon ) );

    /**
    *    Prefer clipping the real BSP face polygon against the decal OBB.
    **/
    if ( !CLG_DecalClip_ClipBspFaceToDecalVolume( context, surface, outPolygon ) ) {
        return false;
    }

    if ( outUsedConcreteBspClip ) {
        *outUsedConcreteBspClip = true;
    }

    /**
    *    Project UVs from decal-local axes and sort the convex polygon before triangulation.
    **/
    for ( int32_t i = 0; i < outPolygon->vertexCount; i++ ) {
        CLG_DecalClip_ProjectPointToUv( context, outPolygon->positions[ i ], outPolygon->uv[ i ] );
    }

    /**
    *    Sort all clipped polygons before fan triangulation. This normalizes winding when
    *    clipping introduces near-duplicate points and prevents self-crossing fan artifacts.
    **/
    CLG_DecalClip_SortPolygonVertices( surface, outPolygon );

    return true;
}

