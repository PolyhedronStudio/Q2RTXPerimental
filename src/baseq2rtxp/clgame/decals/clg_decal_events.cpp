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
    if ( entity->current.solid != BOUNDS_BRUSHMODEL ) {
        return nullptr;
    }

    if ( clgi.GetEntityHullNode( entity ) == nullptr ) {
        return nullptr;
    }

    return entity;
}

/**
*    @brief  Refines quantized temp-event impact origin by tracing along resolved impact normal.
*    @param  origin Quantized event origin.
*    @param  inOutNormal [in/out] Normal used for decal basis; updated from hit plane when available.
*    @param  hitEntityNumber Impacted entity number from event payload.
*    @param  outOrigin [out] Refined impact origin projected onto contacted collision plane.
*    @note   Temp entity origins use truncated-float encoding on the wire, so integer-snapped
*            origins can place the spawn point slightly above/below the receiver plane.
**/
static void CLG_DecalEvents_RefineImpactOrigin( const vec3_t origin, vec3_t inOutNormal, const int32_t hitEntityNumber, vec3_t outOrigin ) {
    if ( !outOrigin ) {
        return;
    }

    VectorCopy( origin, outOrigin );
    if ( !inOutNormal || VectorLength( inOutNormal ) <= 0.001f ) {
        return;
    }

    const centity_t *clipEntity = CLG_DecalEvents_GetClipEntity( hitEntityNumber );

    vec3_t traceStart = {};
    vec3_t traceEnd = {};
    VectorCopy( origin, traceStart );
    VectorCopy( origin, traceEnd );
    VectorMA( traceStart, 4.0f, inOutNormal, traceStart );
    VectorMA( traceEnd, -8.0f, inOutNormal, traceEnd );

    const cm_trace_t trace = CLG_Clip( traceStart, nullptr, nullptr, traceEnd, clipEntity, CM_CONTENTMASK_SOLID );
    if ( trace.allsolid || trace.startsolid || trace.fraction >= 1.0f ) {
        return;
    }

    VectorCopy( trace.endpos, outOrigin );

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
    CLG_DecalEvents_RefineImpactOrigin( origin, normal, hitEntityNumber, refinedOrigin );

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
