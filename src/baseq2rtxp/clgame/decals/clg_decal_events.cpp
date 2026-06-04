/********************************************************************
*
*
*    ClientGame: Decal Event Bridge.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/decals/clg_decals.h"
#include "clgame/clg_world.h"

/**
*    @brief  Creates a normalized impact normal from legacy direction encoding.
*    @param  direction Packed direction byte from temp entity events.
*    @param  outNormal [out] Normalized direction vector.
**/
static void CLG_DecalEvents_DecodeDirection( const uint8_t direction, vec3_t outNormal ) {
    ByteToDir( direction, outNormal );

    if ( VectorLength( outNormal ) <= 0.001f ) {
        VectorSet( outNormal, 0.0f, 0.0f, 1.0f );
    }

    VectorNormalize( outNormal );
}

/**
*    @brief  Resolves the impact normal from full-precision temp-event data or legacy fallback encoding.
*    @param  exactNormal Full-precision normal carried in temp-event state when available.
*    @param  direction Packed direction byte from temp entity events.
*    @param  outNormal [out] Normalized direction vector used for the decal basis.
*    @note   Prefer the exact temp-event normal because the legacy byte encoding is too coarse
*            for sloped and beveled world surfaces.
**/
static void CLG_DecalEvents_ResolveImpactNormal( const vec3_t exactNormal, const uint8_t direction, vec3_t outNormal ) {
    if ( !outNormal ) {
        return;
    }

    if ( exactNormal && VectorLength( exactNormal ) > 0.001f ) {
        VectorCopy( exactNormal, outNormal );
    } else {
        CLG_DecalEvents_DecodeDirection( direction, outNormal );
    }

    if ( VectorLength( outNormal ) <= 0.001f ) {
        VectorSet( outNormal, 0.0f, 0.0f, 1.0f );
    }

    VectorNormalize( outNormal );
}

/**
*    @brief  Resolves optional inline brush clip entity from the hit entity number.
*    @param  hitEntityNumber Impacted entity number from event payload.
*    @return Brush-model centity for clip traces, nullptr for world or invalid targets.
**/
static const centity_t *CLG_DecalEvents_GetClipEntity( const int32_t hitEntityNumber ) {
    if ( hitEntityNumber <= ENTITYNUM_WORLD || hitEntityNumber >= MAX_EDICTS ) {
        return nullptr;
    }

    if ( !clgi.GetEntityHullNode ) {
        return nullptr;
    }

    const centity_t *entity = &clg_entities[ hitEntityNumber ];
    /**
    *    Accept any entity that exposes an inline hull for transformed clipping. Some mover
    *    snapshots can momentarily use alternate solid encodings while retaining brush hulls.
    **/
    if ( clgi.GetEntityHullNode( entity ) == nullptr ) {
        return nullptr;
    }

    return entity;
}

/**
*    @brief  Tries one refinement trace pair around the quantized impact origin.
*    @param  origin Quantized impact origin.
*    @param  normal Current impact normal used for trace direction.
*    @param  startOffset Forward offset along normal for trace start.
*    @param  endOffset Reverse offset along normal for trace end.
*    @param  clipEntity Optional inline brush entity for transformed clipping.
*    @param  outTrace [out] Valid trace result when a hit was found.
*    @return True when the trace produced a usable impact hit.
**/
static bool CLG_DecalEvents_TryRefineImpactTrace( const vec3_t origin, const vec3_t normal, const float startOffset, const float endOffset, const centity_t *clipEntity, cm_trace_t *outTrace ) {
    if ( !origin || !normal || !outTrace ) {
        return false;
    }

    vec3_t traceStart = {};
    vec3_t traceEnd = {};
    VectorCopy( origin, traceStart );
    VectorCopy( origin, traceEnd );
    VectorMA( traceStart, startOffset, normal, traceStart );
    VectorMA( traceEnd, -endOffset, normal, traceEnd );

    const cm_trace_t trace = CLG_Clip( traceStart, nullptr, nullptr, traceEnd, clipEntity, CM_CONTENTMASK_SOLID );
    if ( trace.allsolid || trace.startsolid || trace.fraction >= 1.0f ) {
        return false;
    }

    *outTrace = trace;
    return true;
}

/**
*    @brief  Resolves one stable world brush-side handle from the refined impact trace.
*    @param  trace Collision trace returned by `CLG_Clip`.
*    @return Pointer-sized handle to the best-matching world brush side, or zero when none matched.
*    @note   World traces expose only `cm_surface_t` plus the impact plane. Resolve the owning
*            brush side locally so later decal clipping can recover the actual struck BSP face.
**/
static uintptr_t CLG_DecalEvents_FindWorldBrushSideHandle( const cm_trace_t &trace ) {
    if ( !clgi.client || !clgi.client->collisionModel.cache || !trace.surface || trace.surface == clgi.CM_GetNullSurface() ) {
        return 0u;
    }

    const bsp_t *worldBsp = clgi.client->collisionModel.cache;
    if ( !worldBsp->leafbrushes || worldBsp->numleafbrushes <= 0 || !worldBsp->brushsides || worldBsp->numbrushsides <= 0 ) {
        return 0u;
    }

    vec3_t queryMins = {};
    vec3_t queryMaxs = {};
    constexpr float queryRadius = 2.0f;
    for ( int32_t axis = 0; axis < 3; axis++ ) {
        queryMins[ axis ] = trace.endpos[ axis ] - queryRadius;
        queryMaxs[ axis ] = trace.endpos[ axis ] + queryRadius;
    }

    mleaf_t *leafs[ 64 ] = {};
    mnode_t *topnode = nullptr;
    const int32_t leafCount = clgi.CM_BoxLeafs( queryMins, queryMaxs, leafs, (int32_t)std::size( leafs ), &topnode );
    (void)topnode;

    const mbrush_t *visitedBrushes[ 128 ] = {};
    int32_t visitedBrushCount = 0;
    const mbrushside_t *bestBrushSide = nullptr;
    float bestPlaneOffset = FLT_MAX;

    /**
    *    Scan only brushes touching the impact point and select the side whose texinfo surface and
    *    plane match the trace result most closely. This is more specific than later face gather.
    **/
    for ( int32_t leafIndex = 0; leafIndex < leafCount; leafIndex++ ) {
        const mleaf_t *leaf = leafs[ leafIndex ];
        if ( !leaf || !leaf->firstleafbrush ) {
            continue;
        }

        for ( int32_t brushIndex = 0; brushIndex < leaf->numleafbrushes; brushIndex++ ) {
            const mbrush_t *brush = leaf->firstleafbrush[ brushIndex ];
            if ( !brush || !brush->firstbrushside || brush->numsides <= 0 ) {
                continue;
            }

            bool alreadyVisited = false;
            for ( int32_t visitedIndex = 0; visitedIndex < visitedBrushCount; visitedIndex++ ) {
                if ( visitedBrushes[ visitedIndex ] == brush ) {
                    alreadyVisited = true;
                    break;
                }
            }
            if ( alreadyVisited ) {
                continue;
            }

            if ( visitedBrushCount < (int32_t)std::size( visitedBrushes ) ) {
                visitedBrushes[ visitedBrushCount++ ] = brush;
            }

            for ( int32_t sideIndex = 0; sideIndex < brush->numsides; sideIndex++ ) {
                const mbrushside_t *brushSide = brush->firstbrushside + sideIndex;
                if ( !brushSide || !brushSide->plane || !brushSide->texinfo ) {
                    continue;
                }

                if ( &brushSide->texinfo->c != trace.surface ) {
                    continue;
                }

                const float planeDot = DotProduct( brushSide->plane->normal, trace.plane.normal );
                if ( planeDot < 0.999f ) {
                    continue;
                }

                const float planeOffset = fabsf( PlaneDiff( trace.endpos, brushSide->plane ) );
                if ( planeOffset > 1.0f ) {
                    continue;
                }

                if ( planeOffset < bestPlaneOffset ) {
                    bestPlaneOffset = planeOffset;
                    bestBrushSide = brushSide;
                }
            }
        }
    }

    return bestBrushSide ? (uintptr_t)bestBrushSide : 0u;
}

/**
*    @brief  Refines quantized temp-event impact origin by tracing along resolved impact normal.
*    @param  origin Quantized event origin.
*    @param  inOutNormal [in/out] Normal used for decal basis; updated from hit plane when available.
*    @param  hitEntityNumber Impacted entity number from event payload.
*    @param  outOrigin [out] Refined impact origin projected onto contacted collision plane.
*    @param  outSurfaceHandle [out] Stable collision-surface handle for later world-face lookup.
*    @note   Temp entity origins use truncated-float encoding on the wire, so integer-snapped
*            origins can place the spawn point slightly above/below the receiver plane.
**/
static void CLG_DecalEvents_RefineImpactOrigin( const vec3_t origin, vec3_t inOutNormal, const int32_t hitEntityNumber, vec3_t outOrigin, uintptr_t *outSurfaceHandle ) {
    if ( !outOrigin ) {
        return;
    }

    if ( outSurfaceHandle ) {
        *outSurfaceHandle = 0u;
    }

    VectorCopy( origin, outOrigin );
    if ( !inOutNormal || VectorLength( inOutNormal ) <= 0.001f ) {
        return;
    }

    const centity_t *clipEntity = CLG_DecalEvents_GetClipEntity( hitEntityNumber );

    cm_trace_t trace = {};
    bool foundRefinedTrace = false;

    /**
    *    Retry with progressively wider trace depths to recover from quantized temp-event
    *    origins that can land above or below large floor/brush surfaces.
    **/
    static constexpr float traceStartOffsets[] = { 4.0f, 12.0f, 24.0f };
    static constexpr float traceEndOffsets[] = { 8.0f, 24.0f, 48.0f };
    for ( int32_t i = 0; i < (int32_t)std::size( traceStartOffsets ); i++ ) {
        if ( CLG_DecalEvents_TryRefineImpactTrace( origin, inOutNormal, traceStartOffsets[ i ], traceEndOffsets[ i ], clipEntity, &trace ) ) {
            foundRefinedTrace = true;
            break;
        }
    }

    if ( !foundRefinedTrace ) {
        return;
    }

    VectorCopy( trace.endpos, outOrigin );

    /**
    *    Preserve the exact traced world brush side so the decal clipper can inject the
    *    real impacted face before broad-phase neighbor gather adds unrelated coplanar faces.
    **/
    if ( outSurfaceHandle && hitEntityNumber == ENTITYNUM_WORLD ) {
        *outSurfaceHandle = CLG_DecalEvents_FindWorldBrushSideHandle( trace );
    }

    if ( VectorLength( trace.plane.normal ) > 0.001f ) {
        vec3_t planeNormal = {};
        VectorCopy( trace.plane.normal, planeNormal );
        if ( DotProduct( planeNormal, inOutNormal ) < 0.0f ) {
            planeNormal[ 0 ] = -planeNormal[ 0 ];
            planeNormal[ 1 ] = -planeNormal[ 1 ];
            planeNormal[ 2 ] = -planeNormal[ 2 ];
        }

        VectorNormalize( planeNormal );
        VectorCopy( planeNormal, inOutNormal );
    }
}

/**
*    @brief  Builds a default spawn request from an impact event payload.
*    @param  origin Impact origin from event payload.
*    @param  exactNormal Full-precision impact normal carried by the temp event when available.
*    @param  direction Packed direction byte from event payload.
*    @param  materialHash Material hash used for renderer selection.
*    @param  surfaceClass Receiver surface class hint.
*    @param  count Impact intensity hint.
*    @param  hitEntityNumber Brush-model entity hit by the impact, or ENTITYNUM_WORLD.
*    @param  outParams [out] Spawn request to queue.
**/
static void CLG_DecalEvents_BuildDefaultSpawn( const vec3_t origin, const vec3_t exactNormal, const uint8_t direction, const sg_decal_material_hash_t materialHash, const sg_decal_surface_class_t surfaceClass, const int32_t count, const int32_t hitEntityNumber, sg_decal_spawn_params_t *outParams ) {
    if ( !outParams ) {
        return;
    }

    vec3_t normal = {};
    CLG_DecalEvents_ResolveImpactNormal( exactNormal, direction, normal );

    vec3_t refinedOrigin = {};
    uintptr_t hitSurfaceHandle = 0u;
    CLG_DecalEvents_RefineImpactOrigin( origin, normal, hitEntityNumber, refinedOrigin, &hitSurfaceHandle );

    int32_t clampedCount = count;
    if ( clampedCount < 1 ) {
        clampedCount = 1;
    }
    if ( clampedCount > 8 ) {
        clampedCount = 8;
    }

    const float sizeScale = 1.0f + ( 0.05f * ( clampedCount - 1 ) );

    memset( outParams, 0, sizeof( *outParams ) );
    VectorCopy( refinedOrigin, outParams->origin );
    VectorCopy( normal, outParams->normal );

    outParams->materialHash = materialHash;
    outParams->radius = 5.0f * sizeScale;
    /**
    *    Temp-event origins are integer-snapped by truncated-float network encoding,
    *    so keep a wider projection depth budget to avoid location-dependent broad-phase
    *    misses near brush overlaps and narrow clearance seams.
    **/
    outParams->depth = 3.0f;
    outParams->rotationRadians = frand() * 6.28318530718f;
    outParams->lifeSeconds = 12.0f;
    outParams->fadeInSeconds = 0.04f;
    outParams->fadeOutSeconds = 0.35f;
    outParams->surfaceClass = surfaceClass;

    // Preserve the original mover entity number so later frames can reattach after the
    // client snapshot has fully settled, even if the immediate clip lookup is not ready.
    if ( hitEntityNumber > ENTITYNUM_WORLD && hitEntityNumber < MAX_EDICTS ) {
        outParams->hitEntityNumber = hitEntityNumber;
    } else {
        outParams->hitEntityNumber = ENTITYNUM_WORLD;
    }

    outParams->hitSurfaceHandle = hitSurfaceHandle;

    outParams->flags = SG_DECAL_FLAG_DYNAMIC;
}

void CLG_DecalEvents_HandleImpactGunShot( const vec3_t origin, const vec3_t exactNormal, const uint8_t direction, const int32_t count, const int32_t hitEntityNumber ) {
    sg_decal_spawn_params_t params = {};
    CLG_DecalEvents_BuildDefaultSpawn( origin, exactNormal, direction, SG_DECAL_MATERIAL_HASH_GUNSHOT_CONCRETE, SG_DECAL_SURFACE_CONCRETE, count, hitEntityNumber, &params );
    (void)CLG_Decals_QueueSpawn( params );
}

void CLG_DecalEvents_HandleImpactBulletSparks( const vec3_t origin, const vec3_t exactNormal, const uint8_t direction, const int32_t count, const int32_t hitEntityNumber ) {
    sg_decal_spawn_params_t params = {};
    CLG_DecalEvents_BuildDefaultSpawn( origin, exactNormal, direction, SG_DECAL_MATERIAL_HASH_SPARKS_METAL, SG_DECAL_SURFACE_METAL, count, hitEntityNumber, &params );
    (void)CLG_Decals_QueueSpawn( params );
}
