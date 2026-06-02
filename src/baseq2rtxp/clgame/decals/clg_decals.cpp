/********************************************************************
*
*
*    ClientGame: Decal Runtime.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/decals/clg_decals.h"
#include "clgame/decals/clg_decal_pool.h"
#include "clgame/decals/clg_decal_clip.h"
#include "clgame/decals/clg_decal_mesh.h"
#include "clgame/clg_world.h"
#include <cstdint>

//! Enables client decal processing.
static cvar_t *clg_decals_enable = nullptr;
//! Selects renderer mode hint used by CLGame.
static cvar_t *clg_decals_mode = nullptr;
//! Fixed pool capacity used for runtime decals.
static cvar_t *clg_decals_max = nullptr;

//! Tracks decal module initialization status.
static bool s_clgDecalsInitialized = false;
//! Fixed-capacity runtime pool for active decals.
static clg_decal_pool_t s_clgDecalPool = {};
//! Tracks total spawn requests sent to the runtime queue.
static uint32_t s_clgDecalSpawnRequests = 0u;
//! Tracks total spawn requests accepted by the runtime queue.
static uint32_t s_clgDecalSpawnAccepted = 0u;
//! Tracks total spawn requests rejected by the runtime queue.
static uint32_t s_clgDecalSpawnRejected = 0u;
//! Tracks rejections caused by runtime not being initialized.
static uint32_t s_clgDecalRejectNotInitialized = 0u;
//! Tracks rejections caused by clg_decals_enable being disabled.
static uint32_t s_clgDecalRejectDisabled = 0u;
//! Tracks rejections caused by render mode being disabled.
static uint32_t s_clgDecalRejectModeDisabled = 0u;
//! Tracks rejections caused by invalid or zero lifetime.
static uint32_t s_clgDecalRejectInvalidLife = 0u;
//! Tracks rejections caused by pool allocation failure.
static uint32_t s_clgDecalRejectAllocFail = 0u;
//! Tracks active dynamic decals for split lifecycle diagnostics.
static int32_t s_clgDecalActiveDynamic = 0;
//! Tracks active static decals for split lifecycle diagnostics.
static int32_t s_clgDecalActiveStatic = 0;
//! Last generated triangle count from Phase 3 clip/mesh validation.
static int32_t s_clgDecalLastTriangleCount = 0;
//! Last generated candidate surface count from Phase 3 clip gather.
static int32_t s_clgDecalLastCandidateCount = 0;
//! Enables per-frame debug dumps for each active decal instance.
static cvar_t *clg_decals_debug = nullptr;
//! Material hash used by the manual test-spawn helper.
static constexpr sg_decal_material_hash_t CLG_DECAL_TEST_SPAWN_MATERIAL = SG_DECAL_MATERIAL_HASH_DEFAULT;

/**
*    @brief  Pushes CLGame-owned decal material mappings to the renderer.
*    @note   Renderer remains data-driven and does not hardcode sharedgame hashes or paths.
**/
static void CLG_Decals_ConfigureRendererMaterialMappings( void ) {
    if ( !clgi.R_ClearDecalMaterialMappings || !clgi.R_SetDecalMaterialMapping ) {
        return;
    }

    clgi.R_ClearDecalMaterialMappings();
    clgi.R_SetDecalMaterialMapping( SG_DECAL_MATERIAL_HASH_DEFAULT, SG_DECAL_MATERIAL_NAME_DEFAULT );
    clgi.R_SetDecalMaterialMapping( SG_DECAL_MATERIAL_HASH_GUNSHOT_CONCRETE, SG_DECAL_MATERIAL_PATH_GUNSHOT_CONCRETE );
    clgi.R_SetDecalMaterialMapping( SG_DECAL_MATERIAL_HASH_SPARKS_METAL, SG_DECAL_MATERIAL_PATH_SPARKS_METAL );
    clgi.R_SetDecalMaterialMapping( SG_DECAL_MATERIAL_HASH_BLOOD_FLESH, SG_DECAL_MATERIAL_PATH_BLOOD_FLESH );
    clgi.R_SetDecalMaterialMapping( SG_DECAL_MATERIAL_HASH_SPLINTER_WOOD, SG_DECAL_MATERIAL_PATH_SPLINTER_WOOD );
    clgi.R_SetDecalMaterialMapping( SG_DECAL_MATERIAL_HASH_CRACK_GLASS, SG_DECAL_MATERIAL_PATH_CRACK_GLASS );
}

/**
*    @brief  Pushes the CLGame-controlled decal render mode to the renderer.
**/
static void CLG_Decals_ConfigureRendererMode( void ) {
    if ( !clgi.R_SetDecalRenderMode ) {
        return;
    }

    const int32_t renderMode = ( clg_decals_mode ) ? clg_decals_mode->integer : SG_DECAL_RENDER_DISABLED;
    clgi.R_SetDecalRenderMode( renderMode );
}

/**
*    @brief  Maps one surface class to a stable decal material ID.
**/
static uint32_t CLG_Decals_GetMaterialHashForSurfaceClass( const sg_decal_surface_class_t surfaceClass ) {
    switch ( surfaceClass ) {
        case SG_DECAL_SURFACE_CONCRETE:
            return SG_DECAL_MATERIAL_HASH_GUNSHOT_CONCRETE;
        case SG_DECAL_SURFACE_METAL:
            return SG_DECAL_MATERIAL_HASH_SPARKS_METAL;
        case SG_DECAL_SURFACE_FLESH:
            return SG_DECAL_MATERIAL_HASH_BLOOD_FLESH;
        case SG_DECAL_SURFACE_WOOD:
            return SG_DECAL_MATERIAL_HASH_SPLINTER_WOOD;
        case SG_DECAL_SURFACE_GLASS:
            return SG_DECAL_MATERIAL_HASH_CRACK_GLASS;
        case SG_DECAL_SURFACE_DEFAULT:
        default:
            return SG_DECAL_MATERIAL_HASH_DEFAULT;
    }
}

/**
*    @brief  Returns debug-visible decal texture path for one stable material ID.
*    @note   Renderer resolves the same IDs to the same concrete texture paths.
**/
static const char *CLG_Decals_GetMaterialPathForHash( const uint32_t materialHash ) {
    switch ( materialHash ) {
        case SG_DECAL_MATERIAL_HASH_GUNSHOT_CONCRETE:
            return SG_DECAL_MATERIAL_PATH_GUNSHOT_CONCRETE;
        case SG_DECAL_MATERIAL_HASH_SPARKS_METAL:
            return SG_DECAL_MATERIAL_PATH_SPARKS_METAL;
        case SG_DECAL_MATERIAL_HASH_BLOOD_FLESH:
            return SG_DECAL_MATERIAL_PATH_BLOOD_FLESH;
        case SG_DECAL_MATERIAL_HASH_SPLINTER_WOOD:
            return SG_DECAL_MATERIAL_PATH_SPLINTER_WOOD;
        case SG_DECAL_MATERIAL_HASH_CRACK_GLASS:
            return SG_DECAL_MATERIAL_PATH_CRACK_GLASS;
        case SG_DECAL_MATERIAL_HASH_DEFAULT:
        default:
            return SG_DECAL_MATERIAL_NAME_DEFAULT;
    }
}

/**
*    @brief  Returns one valid brush-model attachment entity for mover decals.
*    @param  entityNumber Entity number carried by the spawn params.
*    @return Brush-model centity when the target can anchor a decal, otherwise nullptr.
*    @note   Server-side impact code forwards only mover BSP entities here because ClientGame
*            does not have movetype for arbitrary targets.
**/
static const centity_t *CLG_Decals_GetMoverAttachmentEntity( const int32_t entityNumber ) {
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
*    @brief  Builds one entity basis from interpolated mover angles.
*    @param  angles Interpolated entity angles.
*    @param  outForward [out] Local forward axis.
*    @param  outRight [out] Local right axis.
*    @param  outUp [out] Local up axis.
**/
static void CLG_Decals_BuildEntityBasis( const vec3_t angles, vec3_t outForward, vec3_t outRight, vec3_t outUp ) {
    vec3_t axis[ 3 ] = {};
    AnglesToAxis( angles, axis );
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
}

/**
*    @brief  Converts one world-space vector into mover-local coordinates.
**/
static void CLG_Decals_WorldVectorToLocal( const vec3_t vector, const vec3_t basisForward, const vec3_t basisRight, const vec3_t basisUp, vec3_t outLocal ) {
    outLocal[ 0 ] = DotProduct( vector, basisForward );
    outLocal[ 1 ] = DotProduct( vector, basisRight );
    outLocal[ 2 ] = DotProduct( vector, basisUp );
}

/**
*    @brief  Converts one mover-local vector into world-space coordinates.
**/
static void CLG_Decals_LocalVectorToWorld( const vec3_t localVector, const vec3_t basisForward, const vec3_t basisRight, const vec3_t basisUp, vec3_t outWorld ) {
    VectorClear( outWorld );
    VectorMA( outWorld, localVector[ 0 ], basisForward, outWorld );
    VectorMA( outWorld, localVector[ 1 ], basisRight, outWorld );
    VectorMA( outWorld, localVector[ 2 ], basisUp, outWorld );
}

/**
*    @brief  Computes the signed in-plane rotation from one canonical decal basis to a desired right axis.
*    @param  surfaceNormal Current world-space decal normal.
*    @param  desiredRight Current world-space right axis that should be preserved.
*    @return Signed angle in radians to store in spawn.rotationRadians.
**/
static float CLG_Decals_ComputeRotationRadiansFromRightAxis( const vec3_t surfaceNormal, const vec3_t desiredRight ) {
    sg_decal_spawn_params_t basisSpawn = {};
    clg_decal_clip_context_t basisContext = {};
    vec3_t projectedRight = {};

    VectorCopy( surfaceNormal, basisSpawn.normal );
    basisSpawn.radius = 1.0f;
    basisSpawn.depth = 1.0f;
    basisSpawn.rotationRadians = 0.0f;

    if ( !CLG_DecalClip_BuildContext( basisSpawn, &basisContext ) ) {
        return 0.0f;
    }

    const float rightDotNormal = DotProduct( desiredRight, surfaceNormal );
    projectedRight[ 0 ] = desiredRight[ 0 ] - ( surfaceNormal[ 0 ] * rightDotNormal );
    projectedRight[ 1 ] = desiredRight[ 1 ] - ( surfaceNormal[ 1 ] * rightDotNormal );
    projectedRight[ 2 ] = desiredRight[ 2 ] - ( surfaceNormal[ 2 ] * rightDotNormal );

    if ( VectorLength( projectedRight ) <= 0.001f ) {
        return 0.0f;
    }

    VectorNormalize( projectedRight );

    vec3_t cross = {};
    CrossProduct( basisContext.basisRight, projectedRight, cross );
    const float sinAngle = DotProduct( cross, surfaceNormal );
    const float cosAngle = DotProduct( basisContext.basisRight, projectedRight );

    return atan2f( sinAngle, cosAngle );
}

/**
*    @brief  Caches mover-local attachment data for one decal instance.
*    @param  instance Runtime decal instance to update.
**/
static void CLG_Decals_CacheMoverAttachment( clg_decal_instance_t *instance ) {
    if ( !instance ) {
        return;
    }

    instance->attachedEntityNumber = ENTITYNUM_WORLD;
    VectorClear( instance->attachedLocalOrigin );
    VectorClear( instance->attachedLocalNormal );
    VectorClear( instance->attachedLocalRight );

    const centity_t *entity = CLG_Decals_GetMoverAttachmentEntity( instance->spawnParams.hitEntityNumber );
    if ( !entity ) {
        return;
    }

    vec3_t basisForward = {};
    vec3_t basisRight = {};
    vec3_t basisUp = {};
    clg_decal_clip_context_t spawnContext = {};
    CLG_Decals_BuildEntityBasis( &entity->lerpAngles.x, basisForward, basisRight, basisUp );

    vec3_t localOriginDelta = {};
    VectorSubtract( instance->spawnParams.origin, &entity->lerpOrigin.x, localOriginDelta );

    CLG_Decals_WorldVectorToLocal( localOriginDelta, basisForward, basisRight, basisUp, instance->attachedLocalOrigin );
    CLG_Decals_WorldVectorToLocal( instance->spawnParams.normal, basisForward, basisRight, basisUp, instance->attachedLocalNormal );
    VectorNormalize( instance->attachedLocalNormal );

    /**
    *    Cache the fully rotated decal right-axis so movers that spin around the hit normal
    *    preserve the decal's in-plane orientation instead of only keeping its position/normal.
    **/
    if ( CLG_DecalClip_BuildContext( instance->spawnParams, &spawnContext ) ) {
        CLG_Decals_WorldVectorToLocal( spawnContext.basisRight, basisForward, basisRight, basisUp, instance->attachedLocalRight );
        VectorNormalize( instance->attachedLocalRight );
    }

    instance->attachedEntityNumber = entity->current.number;
}

/**
*    @brief  Ensures one decal instance has cached mover attachment data after the snapshot is fully updated.
*    @param  instance Runtime decal instance to inspect.
*    @note   Impact temp-entity events can be processed before the struck mover centity has consumed the
*            same snapshot update. Deferring local attachment caching avoids baking local coordinates
*            against a previous-frame mover transform.
**/
static void CLG_Decals_EnsureMoverAttachmentCached( clg_decal_instance_t *instance ) {
    if ( !instance ) {
        return;
    }

    if ( instance->attachedEntityNumber > ENTITYNUM_WORLD ) {
        return;
    }

    if ( instance->spawnParams.hitEntityNumber <= ENTITYNUM_WORLD ) {
        return;
    }

    CLG_Decals_CacheMoverAttachment( instance );
}

/**
*    @brief  Builds current render-time spawn params for one active decal.
*    @param  instance Active decal instance.
*    @param  outParams [out] World-space spawn params to submit this frame.
*    @return True when a valid render-time spawn payload was produced.
**/
static const bool CLG_Decals_BuildRenderSpawnParams( const clg_decal_instance_t *instance, sg_decal_spawn_params_t *outParams ) {
    if ( !instance || !outParams || instance->runtime.active == qfalse ) {
        return false;
    }

    *outParams = instance->spawnParams;
    if ( instance->attachedEntityNumber <= ENTITYNUM_WORLD ) {
        return true;
    }

    const centity_t *entity = CLG_Decals_GetMoverAttachmentEntity( instance->attachedEntityNumber );
    if ( !entity ) {
        return false;
    }

    vec3_t basisForward = {};
    vec3_t basisRight = {};
    vec3_t basisUp = {};
    vec3_t worldRight = {};
    CLG_Decals_BuildEntityBasis( &entity->lerpAngles.x, basisForward, basisRight, basisUp );

    vec3_t worldOriginDelta = {};
    CLG_Decals_LocalVectorToWorld( instance->attachedLocalOrigin, basisForward, basisRight, basisUp, worldOriginDelta );
    VectorAdd( &entity->lerpOrigin.x, worldOriginDelta, outParams->origin );

    CLG_Decals_LocalVectorToWorld( instance->attachedLocalNormal, basisForward, basisRight, basisUp, outParams->normal );
    if ( VectorLength( outParams->normal ) <= 0.001f ) {
        VectorCopy( instance->spawnParams.normal, outParams->normal );
    } else {
        VectorNormalize( outParams->normal );
    }

    /**
    *    Rebuild the decal's in-plane orientation from the mover-local right axis so brush
    *    rotation around the hit normal keeps the decal visually locked to the struck face.
    **/
    CLG_Decals_LocalVectorToWorld( instance->attachedLocalRight, basisForward, basisRight, basisUp, worldRight );
    if ( VectorLength( worldRight ) > 0.001f ) {
        VectorNormalize( worldRight );
        outParams->rotationRadians = CLG_Decals_ComputeRotationRadiansFromRightAxis( outParams->normal, worldRight );
    }

    outParams->hitEntityNumber = entity->current.number;
    return true;
}

/**
*    @brief  Submits a simple impact-plane quad through the mesh decal API.
*    @param  context Built decal context containing the trace endpoint basis.
*    @param  albedo Decal albedo tint.
*    @param  alpha Decal alpha tint.
*    @param  materialHash Stable material hash used by the renderer.
*    @param  lifeSeconds Renderer lifetime in seconds; zero means static.
*    @return True when the quad was submitted to the renderer.
*    @note   This intentionally bypasses BSP receiver clipping for dead-zone-safe behavior.
**/
static const bool CLG_Decals_SubmitImpactPlaneMesh( const clg_decal_clip_context_t &context, const vec3_t albedo, const float alpha, const uint32_t materialHash, const float lifeSeconds ) {
    /**
    *    Sanity: mesh submission is only possible when the refresh API is available.
    **/
    if ( !clgi.R_AddDecalMesh ) {
        return false;
    }

    decal_mesh_vertex_t vertices[ 6 ] = {};
    vec3_t corners[ 4 ] = {};

    /**
    *    Build one compact quad directly on the original trace impact plane.
    **/
    VectorCopy( context.spawn.origin, corners[ 0 ] );
    VectorMA( corners[ 0 ], -context.halfSize, context.basisRight, corners[ 0 ] );
    VectorMA( corners[ 0 ], -context.halfSize, context.basisUp, corners[ 0 ] );

    VectorCopy( context.spawn.origin, corners[ 1 ] );
    VectorMA( corners[ 1 ], context.halfSize, context.basisRight, corners[ 1 ] );
    VectorMA( corners[ 1 ], -context.halfSize, context.basisUp, corners[ 1 ] );

    VectorCopy( context.spawn.origin, corners[ 2 ] );
    VectorMA( corners[ 2 ], context.halfSize, context.basisRight, corners[ 2 ] );
    VectorMA( corners[ 2 ], context.halfSize, context.basisUp, corners[ 2 ] );

    VectorCopy( context.spawn.origin, corners[ 3 ] );
    VectorMA( corners[ 3 ], -context.halfSize, context.basisRight, corners[ 3 ] );
    VectorMA( corners[ 3 ], context.halfSize, context.basisUp, corners[ 3 ] );

    static const int32_t triangleIndices[ 6 ] = { 0, 1, 2, 0, 2, 3 };
    static const vec2_t cornerUv[ 4 ] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };

    /**
    *    Expand the quad into two triangles so the renderer can consume it directly.
    **/
    for ( int32_t i = 0; i < 6; i++ ) {
        const int32_t cornerIndex = triangleIndices[ i ];
        VectorCopy( corners[ cornerIndex ], vertices[ i ].position );
        VectorCopy( context.basisForward, vertices[ i ].normal );
        //VectorCopy( cornerUv[ cornerIndex ], vertices[ i ].uv );
		vertices[ i ].uv[0] = ( cornerUv[ cornerIndex ] )[0];
		vertices[ i ].uv[ 1 ] = ( cornerUv[ cornerIndex ] )[ 1 ];

    }

    clgi.R_AddDecalMesh( vertices, (int32_t)std::size( vertices ), albedo, alpha, materialHash, lifeSeconds );
    return true;
}

/**
*	@brief	Compute the world-space area of one clipped polygon.
*	@param	polygon	Polygon produced by receiver clipping.
*	@param	referenceNormal	Receiver normal used to orient the area.
*	@return	Unsigned polygon area in world units squared.
**/
static float CLG_Decals_ComputePolygonArea( const clg_decal_clip_polygon_t &polygon, const vec3_t referenceNormal ) {
    if ( polygon.vertexCount < 3 ) {
        return 0.0f;
    }

    float signedAreaTwice = 0.0f;

    /**
    *	Accumulate a fan area against the receiver normal so non-triangular clipped
    *	polygons can still be compared by visible overlap before triangulation.
    **/
    for ( int32_t i = 1; i < ( polygon.vertexCount - 1 ); i++ ) {
        vec3_t edgeA = {};
        vec3_t edgeB = {};
        VectorSubtract( polygon.positions[ i ], polygon.positions[ 0 ], edgeA );
        VectorSubtract( polygon.positions[ i + 1 ], polygon.positions[ 0 ], edgeB );

        vec3_t cross = {};
        CrossProduct( edgeA, edgeB, cross );
        signedAreaTwice += DotProduct( cross, referenceNormal );
    }

    return fabsf( signedAreaTwice ) * 0.5f;
}

/**
*	@brief	Return true when one clipped receiver is a raw world trace-plane fallback.
*	@param	surface	Candidate receiver surface.
*	@return	True when the receiver is world-owned plane fallback without BSP face geometry.
**/
static bool CLG_Decals_IsWorldPlaneFallbackSurface( const clg_world_surface_t &surface ) {
    return ( surface.bspFace == nullptr && surface.entityNumber == ENTITYNUM_WORLD );
}

/**
*	@brief	Build one clipped decal mesh from gathered receiver surfaces.
*	@param	context		Built decal clip context.
*	@param	outMesh		[out] Accumulated clipped triangle mesh.
*	@param	outCandidateCount	[out] Number of gathered receiver candidates.
*	@return	True if at least one clipped triangle was generated.
*	@note	Runtime mesh submission and clip debug visualization should exercise the same
*			receiver gather and clipping path so slope issues reproduce consistently.
**/
static const bool CLG_Decals_BuildClippedMesh( const clg_decal_clip_context_t &context, clg_decal_mesh_t *outMesh, int32_t *outCandidateCount ) {
    if ( outCandidateCount ) {
        *outCandidateCount = 0;
    }

    if ( !outMesh ) {
        return false;
    }

    CLG_DecalMesh_Clear( outMesh );

    clg_world_surface_t surfaces[ 64 ] = {};
    const int32_t candidateCount = CLG_DecalClip_GatherCandidateSurfaces( context, surfaces, (int32_t)std::size( surfaces ) );
    if ( outCandidateCount ) {
        *outCandidateCount = candidateCount;
    }

    clg_decal_clip_polygon_t clippedPolygons[ 64 ] = {};
    bool clippedCandidateMask[ 64 ] = {};
    float clippedAreas[ 64 ] = {};
    int32_t clippedCandidateCount = 0;
    bool hasClippedConcreteWorldFace = false;
    bool hasClippedConcreteWorldImpactFace = false;

    /**
    *	Clip every gathered receiver first so primary-plane filtering only reasons about
    *	faces that actually overlap the decal volume. This avoids choosing a dead primary
    *	plane from bevel or stair neighbors before polygon clipping has disqualified them.
    **/
    for ( int32_t i = 0; i < candidateCount; i++ ) {
        if ( !CLG_DecalClip_ClipSurfaceToDecal( context, &surfaces[ i ], &clippedPolygons[ i ] ) ) {
            continue;
        }

        clippedCandidateMask[ i ] = true;
        clippedAreas[ i ] = CLG_Decals_ComputePolygonArea( clippedPolygons[ i ], surfaces[ i ].normal );
        clippedCandidateCount++;

        if ( surfaces[ i ].bspFace != nullptr && surfaces[ i ].entityNumber == ENTITYNUM_WORLD ) {
            hasClippedConcreteWorldFace = true;

            if ( surfaces[ i ].containsImpactPoint ) {
                hasClippedConcreteWorldImpactFace = true;
            }
        }
    }

    if ( clippedCandidateCount <= 0 ) {
        return false;
    }

    int32_t primaryCandidateIndex = -1;
    float primaryCandidateAbsDepth = 1.0e30f;
    float primaryCandidateArea = -1.0f;
    float primaryCandidateFacing = -1.0f;
    bool primaryContainsImpact = false;

    /**
    *	Pick the primary receiver from successful clipped polygons only. Faces that still
    *	contain the original impact point win first, then we prefer the closest plane and
    *	the largest surviving overlap before falling back to normal alignment.
    **/
    for ( int32_t i = 0; i < candidateCount; i++ ) {
        if ( !clippedCandidateMask[ i ] ) {
            continue;
        }

        /**
        *	Once at least one concrete world BSP face clipped successfully, treat the raw
        *	world trace-plane fallback as a dead-zone-only backup and keep it out of primary
        *	selection. Otherwise the plane fallback can outrank the real face simply because
        *	it is marked as impact-containing, which reintroduces the skewed slope result.
        **/
        if ( hasClippedConcreteWorldFace && CLG_Decals_IsWorldPlaneFallbackSurface( surfaces[ i ] ) ) {
            continue;
        }

        /**
        *	If a concrete world BSP face actually contains the impact point, keep primary
        *	selection anchored to that one face instead of allowing neighboring stair-step
        *	or bevel faces on the same plane to participate in the same decal mesh.
        **/
        if ( hasClippedConcreteWorldImpactFace &&
            surfaces[ i ].entityNumber == ENTITYNUM_WORLD &&
            surfaces[ i ].bspFace != nullptr &&
            !surfaces[ i ].containsImpactPoint ) {
            continue;
        }

        vec3_t toSurface = {};
        VectorSubtract( surfaces[ i ].origin, context.spawn.origin, toSurface );
        const float candidateAbsDepth = fabsf( DotProduct( toSurface, context.basisForward ) );
        const float candidateFacing = fabsf( DotProduct( surfaces[ i ].normal, context.basisForward ) );
        const bool containsImpactPoint = surfaces[ i ].containsImpactPoint;

        if ( primaryCandidateIndex < 0 ||
            ( containsImpactPoint && !primaryContainsImpact ) ||
            ( containsImpactPoint == primaryContainsImpact && candidateAbsDepth < primaryCandidateAbsDepth ) ||
            ( containsImpactPoint == primaryContainsImpact && fabsf( candidateAbsDepth - primaryCandidateAbsDepth ) <= 0.001f && clippedAreas[ i ] > primaryCandidateArea ) ||
            ( containsImpactPoint == primaryContainsImpact && fabsf( candidateAbsDepth - primaryCandidateAbsDepth ) <= 0.001f && fabsf( clippedAreas[ i ] - primaryCandidateArea ) <= 0.01f && candidateFacing > primaryCandidateFacing ) ) {
            primaryCandidateIndex = i;
            primaryCandidateAbsDepth = candidateAbsDepth;
            primaryCandidateArea = clippedAreas[ i ];
            primaryCandidateFacing = candidateFacing;
            primaryContainsImpact = containsImpactPoint;
        }
    }

    float primarySignedDepth = 0.0f;
    vec3_t primaryNormal = {};
    if ( primaryCandidateIndex >= 0 ) {
        vec3_t toPrimarySurface = {};
        VectorSubtract( surfaces[ primaryCandidateIndex ].origin, context.spawn.origin, toPrimarySurface );
        primarySignedDepth = DotProduct( toPrimarySurface, context.basisForward );
        VectorCopy( surfaces[ primaryCandidateIndex ].normal, primaryNormal );
    }

    /**
    *	Append only clipped polygons that stay on the chosen receiver plane so split faces
    *	can merge while bevels and neighboring trims stay excluded.
    **/
    for ( int32_t i = 0; i < candidateCount; i++ ) {
        if ( !clippedCandidateMask[ i ] ) {
            continue;
        }

        if ( hasClippedConcreteWorldFace && CLG_Decals_IsWorldPlaneFallbackSurface( surfaces[ i ] ) ) {
            continue;
        }

        if ( hasClippedConcreteWorldImpactFace &&
            surfaces[ i ].entityNumber == ENTITYNUM_WORLD &&
            surfaces[ i ].bspFace != nullptr &&
            i != primaryCandidateIndex ) {
            continue;
        }

        if ( primaryCandidateIndex >= 0 && clippedCandidateCount > 1 ) {
            vec3_t toSurface = {};
            VectorSubtract( surfaces[ i ].origin, context.spawn.origin, toSurface );
            const float signedDepth = DotProduct( toSurface, context.basisForward );
            const float normalDot = fabsf( DotProduct( surfaces[ i ].normal, primaryNormal ) );

            if ( normalDot < 0.95f || fabsf( signedDepth - primarySignedDepth ) > 1.0f ) {
                continue;
            }
        }

        // Stop once the fixed mesh budget is exhausted; already-clipped triangles remain valid.
        if ( !CLG_DecalMesh_AppendPolygon( outMesh, clippedPolygons[ i ], surfaces[ i ].normal ) ) {
            break;
        }
    }

    return ( outMesh->triangleCount > 0 && outMesh->vertexCount >= 3 );
}

/**
*	@brief	Submit one clipped receiver mesh through the mesh decal API.
*	@param	context		Built decal clip context.
*	@param	albedo		Decal albedo tint.
*	@param	alpha		Decal alpha tint.
*	@param	materialHash	Stable material hash used by the renderer.
*	@param	lifeSeconds	Renderer lifetime in seconds; zero means static.
*	@return	True when a clipped mesh was submitted to the renderer.
**/
static const bool CLG_Decals_SubmitClippedMesh( const clg_decal_clip_context_t &context, const vec3_t albedo, const float alpha, const uint32_t materialHash, const float lifeSeconds ) {
    /**
    *	Sanity: mesh submission is only possible when the refresh API is available.
    **/
    if ( !clgi.R_AddDecalMesh ) {
        return false;
    }

    clg_decal_mesh_t mesh = {};
    int32_t candidateCount = 0;
    if ( !CLG_Decals_BuildClippedMesh( context, &mesh, &candidateCount ) ) {
        s_clgDecalLastCandidateCount = candidateCount;
        s_clgDecalLastTriangleCount = mesh.triangleCount;
        return false;
    }

    decal_mesh_vertex_t submittedVertices[ 256 ] = {};

    /**
    *	Copy the CLGame mesh into the shared refresh payload explicitly so submission
    *	does not rely on aliasing distinct struct types.
    **/
    for ( int32_t i = 0; i < mesh.vertexCount; i++ ) {
        VectorCopy( mesh.vertices[ i ].position, submittedVertices[ i ].position );
        VectorCopy( mesh.vertices[ i ].normal, submittedVertices[ i ].normal );
        VectorCopy( mesh.vertices[ i ].uv, submittedVertices[ i ].uv );
    }

    s_clgDecalLastCandidateCount = candidateCount;
    s_clgDecalLastTriangleCount = mesh.triangleCount;
    clgi.R_AddDecalMesh( submittedVertices, mesh.vertexCount, albedo, alpha, materialHash, lifeSeconds );
    return true;
}

/**
*    @brief  Prints one active decal's mover attachment state for frame-by-frame debugging.
*    @param  instance Active decal instance.
*    @param  renderParams Rebuilt world-space spawn params for this frame.
**/
static void CLG_Decals_DebugPrintInstance( const clg_decal_instance_t *instance, const sg_decal_spawn_params_t &renderParams ) {
    if ( !clg_decals_debug || clg_decals_debug->integer == 0 || !instance ) {
        return;
    }

    clgi.Print(
        PRINT_DEVELOPER,
        "[CLG Decals][Frame] id=%u srcEnt=%d attachedEnt=%d active=%d origin=(%.2f %.2f %.2f) renderOrigin=(%.2f %.2f %.2f) normal=(%.3f %.3f %.3f) renderNormal=(%.3f %.3f %.3f) rot=%.3f renderRot=%.3f life=%.2fs\n",
        instance->runtime.decalId,
        instance->spawnParams.hitEntityNumber,
        instance->attachedEntityNumber,
        instance->runtime.active ? 1 : 0,
        instance->spawnParams.origin[ 0 ],
        instance->spawnParams.origin[ 1 ],
        instance->spawnParams.origin[ 2 ],
        renderParams.origin[ 0 ],
        renderParams.origin[ 1 ],
        renderParams.origin[ 2 ],
        instance->spawnParams.normal[ 0 ],
        instance->spawnParams.normal[ 1 ],
        instance->spawnParams.normal[ 2 ],
        renderParams.normal[ 0 ],
        renderParams.normal[ 1 ],
        renderParams.normal[ 2 ],
        instance->spawnParams.rotationRadians,
        renderParams.rotationRadians,
        instance->spawnParams.lifeSeconds );
}

/**
*    @brief  Prints current decal module status.
**/
static void CLG_Decals_Status_f( void ) {
    clgi.Print( PRINT_ALL, "[CLG Decals] initialized: %s\n", s_clgDecalsInitialized ? "yes" : "no" );
    clgi.Print( PRINT_ALL, "[CLG Decals] active decals: %d\n", CLG_DecalPool_GetActiveCount( &s_clgDecalPool ) );
    clgi.Print( PRINT_ALL, "[CLG Decals] active split dynamic:%d static:%d\n", s_clgDecalActiveDynamic, s_clgDecalActiveStatic );
    clgi.Print( PRINT_ALL, "[CLG Decals] capacity: %d\n", s_clgDecalPool.capacity );
    clgi.Print( PRINT_ALL, "[CLG Decals] enable: %d\n", clg_decals_enable ? clg_decals_enable->integer : 0 );
    clgi.Print( PRINT_ALL, "[CLG Decals] render mode: %d\n", clg_decals_mode ? clg_decals_mode->integer : SG_DECAL_RENDER_DISABLED );
    clgi.Print( PRINT_ALL, "[CLG Decals] queue requests: %u accepted: %u rejected: %u\n", s_clgDecalSpawnRequests, s_clgDecalSpawnAccepted, s_clgDecalSpawnRejected );
    clgi.Print( PRINT_ALL, "[CLG Decals] reject reasons init:%u disabled:%u mode:%u life:%u alloc:%u\n",
        s_clgDecalRejectNotInitialized,
        s_clgDecalRejectDisabled,
        s_clgDecalRejectModeDisabled,
        s_clgDecalRejectInvalidLife,
        s_clgDecalRejectAllocFail );
    clgi.Print( PRINT_ALL, "[CLG Decals] phase3 candidates:%d triangles:%d\n", s_clgDecalLastCandidateCount, s_clgDecalLastTriangleCount );
}

/**
*    @brief  Dumps CLGame and renderer decal material mappings for runtime validation.
**/
static void CLG_Decals_DumpMaterials_f( void ) {
    clgi.Print( PRINT_ALL, "[CLG Decals] configured material mappings:\n" );
    clgi.Print( PRINT_ALL, "  0x%08x -> %s\n", SG_DECAL_MATERIAL_HASH_DEFAULT, SG_DECAL_MATERIAL_NAME_DEFAULT );
    clgi.Print( PRINT_ALL, "  0x%08x -> %s\n", SG_DECAL_MATERIAL_HASH_GUNSHOT_CONCRETE, SG_DECAL_MATERIAL_PATH_GUNSHOT_CONCRETE );
    clgi.Print( PRINT_ALL, "  0x%08x -> %s\n", SG_DECAL_MATERIAL_HASH_SPARKS_METAL, SG_DECAL_MATERIAL_PATH_SPARKS_METAL );
    clgi.Print( PRINT_ALL, "  0x%08x -> %s\n", SG_DECAL_MATERIAL_HASH_BLOOD_FLESH, SG_DECAL_MATERIAL_PATH_BLOOD_FLESH );
    clgi.Print( PRINT_ALL, "  0x%08x -> %s\n", SG_DECAL_MATERIAL_HASH_SPLINTER_WOOD, SG_DECAL_MATERIAL_PATH_SPLINTER_WOOD );
    clgi.Print( PRINT_ALL, "  0x%08x -> %s\n", SG_DECAL_MATERIAL_HASH_CRACK_GLASS, SG_DECAL_MATERIAL_PATH_CRACK_GLASS );

    if ( clgi.R_DumpDecalMaterialMappings ) {
        clgi.R_DumpDecalMaterialMappings();
    }
}

/**
*    @brief  Submits one decal to refresh using clipped mesh triangles when supported.
*    @note   Falls back to legacy center-projected quad payload when mesh API is unavailable.
**/
static void CLG_Decals_SubmitLegacyDecal( const sg_decal_spawn_params_t &params ) {
    if ( !clgi.R_AddDecalMesh && !clgi.R_AddDecal ) {
        return;
    }

    decal_t legacyDecal = {};
    VectorCopy( params.origin, legacyDecal.pos );
    VectorCopy( params.normal, legacyDecal.dir );

    legacyDecal.spread = params.radius;
    if ( legacyDecal.spread <= 0.05f ) {
        legacyDecal.spread = 1.0f;
    }

    legacyDecal.length = params.depth;
    if ( legacyDecal.length <= 0.05f ) {
        legacyDecal.length = 1.0f;
    }

    // ClientGame owns decal receiver classification, so choose decal albedo here.
    VectorSet( legacyDecal.albedo, 0.62f, 0.62f, 0.62f );
    switch ( params.surfaceClass ) {
        case SG_DECAL_SURFACE_CONCRETE:
            VectorSet( legacyDecal.albedo, 0.58f, 0.58f, 0.58f );
            break;
        case SG_DECAL_SURFACE_METAL:
            VectorSet( legacyDecal.albedo, 0.68f, 0.70f, 0.72f );
            break;
        case SG_DECAL_SURFACE_FLESH:
            VectorSet( legacyDecal.albedo, 0.42f, 0.10f, 0.10f );
            break;
        case SG_DECAL_SURFACE_WOOD:
            VectorSet( legacyDecal.albedo, 0.38f, 0.25f, 0.12f );
            break;
        case SG_DECAL_SURFACE_GLASS:
            VectorSet( legacyDecal.albedo, 0.72f, 0.82f, 0.88f );
            break;
        case SG_DECAL_SURFACE_DEFAULT:
        default:
            break;
    }

    legacyDecal.alpha = 0.90f;

    // Resolve one explicit decal material path in CLGame before handing off.
    const uint32_t resolvedMaterialHash = ( params.materialHash != 0u )
        ? params.materialHash
        : CLG_Decals_GetMaterialHashForSurfaceClass( params.surfaceClass );
    legacyDecal.materialHash = resolvedMaterialHash;

    // Keep this as an intentional read so CLGame owns/locks path mapping for debug and tooling.
    const char *resolvedMaterialPath = CLG_Decals_GetMaterialPathForHash( resolvedMaterialHash );
    (void)resolvedMaterialPath;

    /**
    *    Simplified runtime policy: in path-traced mode, submit one impact-plane quad
    *    from the event-normal basis and refined client-side impact origin. This bypasses
    *    broad-phase BSP receiver gather/clip variability on overlap seams.
    **/
    const bool shouldSubmitImpactPlaneMesh = ( clgi.R_AddDecalMesh != nullptr && clg_decals_mode && clg_decals_mode->integer == SG_DECAL_RENDER_PATH_TRACED );
    if ( shouldSubmitImpactPlaneMesh ) {
        clg_decal_clip_context_t context = {};
        if ( CLG_DecalClip_BuildContext( params, &context ) ) {
            const float rendererLifeSeconds = 0.0f;
            if ( CLG_Decals_SubmitImpactPlaneMesh( context, legacyDecal.albedo, legacyDecal.alpha, resolvedMaterialHash, rendererLifeSeconds ) ) {
                s_clgDecalLastCandidateCount = 0;
                s_clgDecalLastTriangleCount = 2;
                return;
            }
        }
    }

    if ( clgi.R_AddDecal ) {
        clgi.R_AddDecal( &legacyDecal );
    }
}

/**
*    @brief  Rebuilds renderer decal submissions from the active CLGame pool.
*    @note   Renderer retained state is cleared first so mover-attached decals can be
*            regenerated from current interpolated transforms without duplication.
**/
static void CLG_Decals_RebuildRendererDecals( void ) {
    if ( !s_clgDecalsInitialized || !s_clgDecalPool.instances || !clgi.R_ClearDecals ) {
        return;
    }

    clgi.R_ClearDecals();

    if ( !clg_decals_enable || clg_decals_enable->integer == 0 ) {
        return;
    }

    if ( clg_decals_mode && clg_decals_mode->integer == SG_DECAL_RENDER_DISABLED ) {
        return;
    }

    for ( int32_t i = 0; i < s_clgDecalPool.capacity; i++ ) {
        clg_decal_instance_t *instance = &s_clgDecalPool.instances[ i ];
        if ( instance->runtime.active == qfalse ) {
            continue;
        }

        CLG_Decals_EnsureMoverAttachmentCached( instance );

        sg_decal_spawn_params_t renderParams = {};
        if ( !CLG_Decals_BuildRenderSpawnParams( instance, &renderParams ) ) {
            continue;
        }

        CLG_Decals_DebugPrintInstance( instance, renderParams );
        CLG_Decals_SubmitLegacyDecal( renderParams );
    }
}

/**
*    @brief  Queues a deterministic debug decal at the local player origin.
**/
static void CLG_Decals_TestSpawn_f( void ) {
    sg_decal_spawn_params_t params = {};

    // Trace from current view to place the debug decal on the aimed surface.
    vec3_t traceStart = {};
    vec3_t traceEnd = {};
    vec3_t traceForward = { 0.0f, 0.0f, 0.0f };

    VectorCopy( clgi.client->refdef.vieworg, traceStart );
    VectorSet( traceForward, clgi.client->vForward.x, clgi.client->vForward.y, clgi.client->vForward.z );
    if ( VectorLength( traceForward ) <= 0.001f ) {
        AngleVectors( clgi.client->refdef.viewangles, traceForward, nullptr, nullptr );
    }
    VectorNormalize( traceForward );
    VectorMA( traceStart, 2048.0f, traceForward, traceEnd );

    const cm_trace_t trace = CLG_Clip( traceStart, nullptr, nullptr, traceEnd, nullptr, CM_CONTENTMASK_SOLID );
    if ( trace.fraction < 1.0f && !trace.startsolid ) {
        VectorCopy( trace.endpos, params.origin );
        VectorCopy( trace.plane.normal, params.normal );
        VectorMA( params.origin, 0.10f, params.normal, params.origin );
        params.hitEntityNumber = ( trace.entityNumber > ENTITYNUM_WORLD ) ? trace.entityNumber : ENTITYNUM_WORLD;
    } else {
        VectorCopy( &clgi.client->playerEntityOrigin.x, params.origin );
        VectorSet( params.normal, 0.0f, 0.0f, 1.0f );
        params.hitEntityNumber = ENTITYNUM_WORLD;
    }

    params.materialHash = CLG_DECAL_TEST_SPAWN_MATERIAL;
    params.radius = 6.0f;
    params.depth = 1.0f;
    params.rotationRadians = 0.0f;
    params.lifeSeconds = 12.0f;
    params.fadeInSeconds = 0.05f;
    params.fadeOutSeconds = 0.35f;
    params.surfaceClass = SG_DECAL_SURFACE_DEFAULT;
    params.flags = SG_DECAL_FLAG_DYNAMIC;

    if ( clgi.Cmd_Argc() > 1 ) {
        params.lifeSeconds = atof( clgi.Cmd_Argv( 1 ) );
        if ( params.lifeSeconds <= 0.0f ) {
            params.lifeSeconds = 12.0f;
        }
    }

    const bool queued = CLG_Decals_QueueSpawn( params );
    clgi.Print( PRINT_ALL, "[CLG Decals] test spawn queued=%s active=%d traceFrac=%.3f\n", queued ? "yes" : "no", CLG_Decals_GetActiveCount(), trace.fraction );
}

/**
*    @brief  Generates and optionally draws Phase 3 clip/mesh debug output.
**/
static void CLG_Decals_DebugClipTest_f( void ) {
    sg_decal_spawn_params_t spawn = {};

    // Prefer newest real impact decal to preserve current impact context.
    const clg_decal_instance_t *newestImpactInstance = nullptr;
    const clg_decal_instance_t *newestAnyInstance = nullptr;
    for ( int32_t i = 0; i < s_clgDecalPool.capacity; i++ ) {
        const clg_decal_instance_t *instance = &s_clgDecalPool.instances[ i ];
        if ( instance->runtime.active == qfalse ) {
            continue;
        }

        if ( !newestAnyInstance || instance->runtime.spawnTime > newestAnyInstance->runtime.spawnTime ) {
            newestAnyInstance = instance;
        }

        if ( instance->spawnParams.materialHash == CLG_DECAL_TEST_SPAWN_MATERIAL ) {
            continue;
        }

        if ( !newestImpactInstance || instance->runtime.spawnTime > newestImpactInstance->runtime.spawnTime ) {
            newestImpactInstance = instance;
        }
    }

    if ( newestImpactInstance ) {
        spawn = newestImpactInstance->spawnParams;
    } else if ( newestAnyInstance ) {
        spawn = newestAnyInstance->spawnParams;
    } else {
        VectorCopy( &clgi.client->playerEntityOrigin.x, spawn.origin );
        VectorSet( spawn.normal, 0.0f, 0.0f, 1.0f );
        spawn.radius = 24.0f;
        spawn.depth = 32.0f;
        spawn.lifeSeconds = 5.0f;
        spawn.fadeInSeconds = 0.0f;
        spawn.fadeOutSeconds = 0.0f;
        spawn.surfaceClass = SG_DECAL_SURFACE_DEFAULT;
    }

    if ( spawn.radius <= 0.0f ) {
        spawn.radius = 24.0f;
    }
    if ( spawn.depth <= 0.0f ) {
        spawn.depth = 32.0f;
    }
    if ( VectorLength( spawn.normal ) <= 0.001f ) {
        VectorSet( spawn.normal, 0.0f, 0.0f, 1.0f );
    }

    clg_decal_clip_context_t context = {};
    if ( !CLG_DecalClip_BuildContext( spawn, &context ) ) {
        clgi.Print( PRINT_ALL, "[CLG Decals] phase3: failed to build clip context\n" );
        return;
    }

    clg_decal_mesh_t mesh = {};
    int32_t candidateCount = 0;
    (void)CLG_Decals_BuildClippedMesh( context, &mesh, &candidateCount );

    s_clgDecalLastCandidateCount = candidateCount;
    s_clgDecalLastTriangleCount = mesh.triangleCount;

    clgi.Print( PRINT_ALL, "[CLG Decals] phase3 clip test candidates=%d triangles=%d vertices=%d\n", candidateCount, mesh.triangleCount, mesh.vertexCount );

    if ( clgi.Cmd_Argc() > 1 && atoi( clgi.Cmd_Argv( 1 ) ) != 0 ) {
        const uint32_t triColor = U32_CYAN;
        for ( int32_t i = 0; i + 2 < mesh.vertexCount; i += 3 ) {
            clgi.R_DrawDebugLine( mesh.vertices[ i + 0 ].position, mesh.vertices[ i + 1 ].position, triColor );
            clgi.R_DrawDebugLine( mesh.vertices[ i + 1 ].position, mesh.vertices[ i + 2 ].position, triColor );
            clgi.R_DrawDebugLine( mesh.vertices[ i + 2 ].position, mesh.vertices[ i + 0 ].position, triColor );
        }
    }
}

/**
*    @brief  Dumps newest active decals for Phase 1 runtime validation.
**/
static void CLG_Decals_DumpRecent_f( void ) {
    if ( !s_clgDecalsInitialized || !s_clgDecalPool.instances ) {
        clgi.Print( PRINT_ALL, "[CLG Decals] pool is not initialized.\n" );
        return;
    }

    int32_t maxEntries = 16;
    if ( clgi.Cmd_Argc() > 1 ) {
        maxEntries = atoi( clgi.Cmd_Argv( 1 ) );
    }
    if ( maxEntries < 1 ) {
        maxEntries = 1;
    }
    if ( maxEntries > 128 ) {
        maxEntries = 128;
    }

    clgi.Print( PRINT_ALL, "[CLG Decals] recent active decals (up to %d):\n", maxEntries );

    int32_t printed = 0;
    int32_t printedIndices[ 128 ] = {};

    while ( printed < maxEntries ) {
        int32_t newestIndex = -1;

        for ( int32_t i = 0; i < s_clgDecalPool.capacity; i++ ) {
            clg_decal_instance_t *instance = &s_clgDecalPool.instances[ i ];
            if ( instance->runtime.active == qfalse ) {
                continue;
            }

            bool alreadyPrinted = false;
            for ( int32_t p = 0; p < printed; p++ ) {
                if ( printedIndices[ p ] == i ) {
                    alreadyPrinted = true;
                    break;
                }
            }
            if ( alreadyPrinted ) {
                continue;
            }

            if ( newestIndex == -1 || instance->runtime.spawnTime > s_clgDecalPool.instances[ newestIndex ].runtime.spawnTime ) {
                newestIndex = i;
            }
        }

        if ( newestIndex == -1 ) {
            break;
        }

        clg_decal_instance_t *instance = &s_clgDecalPool.instances[ newestIndex ];
        const float ageSeconds = (float)( level.time - instance->runtime.spawnTime ).Milliseconds() * 0.001f;

        clgi.Print( PRINT_ALL,
            "  #%u gen=%u age=%.2fs alpha=%.2f radius=%.2f life=%.2fs\n",
            instance->runtime.decalId,
            instance->runtime.generation,
            ageSeconds,
            instance->runtime.alpha,
            instance->spawnParams.radius,
            instance->spawnParams.lifeSeconds );

        printed++;
        printedIndices[ printed - 1 ] = newestIndex;
    }

    if ( printed == 0 ) {
        clgi.Print( PRINT_ALL, "  (no active decals)\n" );
    }
}

void CLG_Decals_Init( void ) {
    clg_decals_enable = clgi.CVar_Get( "clg_decals_enable", "1", CVAR_ARCHIVE );
    clg_decals_mode = clgi.CVar_Get( "clg_decals_mode", "2", CVAR_ARCHIVE );
    clg_decals_max = clgi.CVar_Get( "clg_decals_max", "256", CVAR_ARCHIVE );
    clg_decals_debug = clgi.CVar_Get( "clg_decals_debug", "0", CVAR_ARCHIVE );

    int32_t maxDecals = clg_decals_max ? clg_decals_max->integer : 256;
    if ( maxDecals < 16 ) {
        maxDecals = 16;
    }

    if ( !CLG_DecalPool_Init( &s_clgDecalPool, maxDecals ) ) {
        clgi.Print( PRINT_ALL, "[CLG Decals] failed to initialize decal pool with capacity %d\n", maxDecals );
    }

    s_clgDecalSpawnRequests = 0u;
    s_clgDecalSpawnAccepted = 0u;
    s_clgDecalSpawnRejected = 0u;
    s_clgDecalRejectNotInitialized = 0u;
    s_clgDecalRejectDisabled = 0u;
    s_clgDecalRejectModeDisabled = 0u;
    s_clgDecalRejectInvalidLife = 0u;
    s_clgDecalRejectAllocFail = 0u;
    s_clgDecalLastTriangleCount = 0;
    s_clgDecalLastCandidateCount = 0;
    s_clgDecalActiveDynamic = 0;
    s_clgDecalActiveStatic = 0;

    // CLGame is the source of truth for decal material hash->material path mapping.
    CLG_Decals_ConfigureRendererMaterialMappings();
    // CLGame is also the source of truth for active decal render mode.
    CLG_Decals_ConfigureRendererMode();

    s_clgDecalsInitialized = true;

    clgi.Cmd_AddCommand( "clg_decal_status", CLG_Decals_Status_f );
    clgi.Cmd_AddCommand( "clg_decal_materials", CLG_Decals_DumpMaterials_f );
    clgi.Cmd_AddCommand( "clg_decal_dump_recent", CLG_Decals_DumpRecent_f );
    clgi.Cmd_AddCommand( "clg_decal_test_spawn", CLG_Decals_TestSpawn_f );
    clgi.Cmd_AddCommand( "clg_decal_clip_test", CLG_Decals_DebugClipTest_f );
}

void CLG_Decals_Shutdown( void ) {
    CLG_DecalPool_Shutdown( &s_clgDecalPool );
    s_clgDecalsInitialized = false;
    s_clgDecalSpawnRequests = 0u;
    s_clgDecalSpawnAccepted = 0u;
    s_clgDecalSpawnRejected = 0u;
    s_clgDecalRejectNotInitialized = 0u;
    s_clgDecalRejectDisabled = 0u;
    s_clgDecalRejectModeDisabled = 0u;
    s_clgDecalRejectInvalidLife = 0u;
    s_clgDecalRejectAllocFail = 0u;
    s_clgDecalLastTriangleCount = 0;
    s_clgDecalLastCandidateCount = 0;
    s_clgDecalActiveDynamic = 0;
    s_clgDecalActiveStatic = 0;

    clgi.Cmd_RemoveCommand( "clg_decal_clip_test" );
    clgi.Cmd_RemoveCommand( "clg_decal_test_spawn" );
    clgi.Cmd_RemoveCommand( "clg_decal_dump_recent" );
    clgi.Cmd_RemoveCommand( "clg_decal_materials" );
    clgi.Cmd_RemoveCommand( "clg_decal_status" );
}

void CLG_Decals_Clear( void ) {
    CLG_DecalPool_Clear( &s_clgDecalPool );
}

void CLG_Decals_BeginFrame( void ) {
    CLG_Decals_Update( level.time );
}

void CLG_Decals_EndFrame( void ) {
    CLG_Decals_RebuildRendererDecals();
}

void CLG_Decals_Update( const QMTime now ) {
    if ( !s_clgDecalsInitialized || !s_clgDecalPool.instances || s_clgDecalPool.capacity <= 0 ) {
        return;
    }

    // Keep renderer mode synchronized with CLGame policy.
    CLG_Decals_ConfigureRendererMode();

    s_clgDecalActiveDynamic = 0;
    s_clgDecalActiveStatic = 0;

    for ( int32_t i = 0; i < s_clgDecalPool.capacity; i++ ) {
        clg_decal_instance_t *instance = &s_clgDecalPool.instances[ i ];
        if ( instance->runtime.active == qfalse ) {
            continue;
        }

        const bool isStaticDecal = ( ( instance->spawnParams.flags & SG_DECAL_FLAG_STATIC ) != 0u );
        if ( !isStaticDecal && now >= instance->runtime.expireTime ) {
            memset( instance, 0, sizeof( *instance ) );
            s_clgDecalPool.activeCount = ( s_clgDecalPool.activeCount > 0 ) ? ( s_clgDecalPool.activeCount - 1 ) : 0;
            continue;
        }

        if ( isStaticDecal ) {
            s_clgDecalActiveStatic++;
        } else {
            s_clgDecalActiveDynamic++;
        }

        const QMTime ageTime = now - instance->runtime.spawnTime;
        const float lifeSeconds = ( instance->spawnParams.lifeSeconds > 0.0001f ) ? instance->spawnParams.lifeSeconds : 0.0001f;
        const float ageSeconds = (float)ageTime.Milliseconds() * 0.001f;
        const float normalizedAge = isStaticDecal ? 0.0f : ( ageSeconds / lifeSeconds );

        instance->runtime.normalizedAge = normalizedAge;
        if ( instance->runtime.normalizedAge < 0.0f ) {
            instance->runtime.normalizedAge = 0.0f;
        } else if ( instance->runtime.normalizedAge > 1.0f ) {
            instance->runtime.normalizedAge = 1.0f;
        }
        instance->runtime.alpha = 1.0f;

        if ( instance->spawnParams.fadeInSeconds > 0.0f && ageSeconds < instance->spawnParams.fadeInSeconds ) {
            instance->runtime.alpha = ageSeconds / instance->spawnParams.fadeInSeconds;
            if ( instance->runtime.alpha < 0.0f ) {
                instance->runtime.alpha = 0.0f;
            } else if ( instance->runtime.alpha > 1.0f ) {
                instance->runtime.alpha = 1.0f;
            }
        }

        const float secondsRemaining = (float)( instance->runtime.expireTime - now ).Milliseconds() * 0.001f;
        if ( !isStaticDecal && instance->spawnParams.fadeOutSeconds > 0.0f && secondsRemaining < instance->spawnParams.fadeOutSeconds ) {
            float fadeOutAlpha = secondsRemaining / instance->spawnParams.fadeOutSeconds;
            if ( fadeOutAlpha < 0.0f ) {
                fadeOutAlpha = 0.0f;
            } else if ( fadeOutAlpha > 1.0f ) {
                fadeOutAlpha = 1.0f;
            }

            if ( fadeOutAlpha < instance->runtime.alpha ) {
                instance->runtime.alpha = fadeOutAlpha;
            }
        }
    }
}

const bool CLG_Decals_QueueSpawn( const sg_decal_spawn_params_t &params ) {
    s_clgDecalSpawnRequests++;

    if ( !s_clgDecalsInitialized || !s_clgDecalPool.instances ) {
        s_clgDecalSpawnRejected++;
        s_clgDecalRejectNotInitialized++;
        return false;
    }

    if ( !clg_decals_enable || clg_decals_enable->integer == 0 ) {
        s_clgDecalSpawnRejected++;
        s_clgDecalRejectDisabled++;
        return false;
    }

    if ( clg_decals_mode && clg_decals_mode->integer == SG_DECAL_RENDER_DISABLED ) {
        s_clgDecalSpawnRejected++;
        s_clgDecalRejectModeDisabled++;
        return false;
    }

    const bool isStaticDecal = ( ( params.flags & SG_DECAL_FLAG_STATIC ) != 0u );
    if ( !isStaticDecal && params.lifeSeconds <= 0.0f ) {
        s_clgDecalSpawnRejected++;
        s_clgDecalRejectInvalidLife++;
        return false;
    }

    clg_decal_instance_t *instance = CLG_DecalPool_Alloc( &s_clgDecalPool );
    if ( !instance ) {
        s_clgDecalSpawnRejected++;
        s_clgDecalRejectAllocFail++;
        return false;
    }

    instance->spawnParams = params;
    instance->runtime.decalId = s_clgDecalPool.nextId++;
    instance->runtime.generation = s_clgDecalPool.generation;
    instance->runtime.spawnTime = level.time;
    if ( isStaticDecal ) {
        instance->runtime.expireTime = QMTime::FromMilliseconds( INT64_MAX / 4 );
    } else {
        instance->runtime.expireTime = level.time + QMTime::FromMilliseconds( (int64_t)( params.lifeSeconds * 1000.0f ) );
    }
    instance->runtime.normalizedAge = 0.0f;
    instance->runtime.alpha = 0.0f;
    instance->runtime.active = qtrue;
    instance->randomSeed = instance->runtime.decalId * 2654435761u;
    s_clgDecalSpawnAccepted++;

    return true;
}

int32_t CLG_Decals_GetActiveCount( void ) {
    return CLG_DecalPool_GetActiveCount( &s_clgDecalPool );
}
