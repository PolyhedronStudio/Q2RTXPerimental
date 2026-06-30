
/********************************************************************
*
*
*	ClientGame: 'ET_MONSTER' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"

#include "sharedgame/sg_entity_flags.h"

/**
*\t@brief\tSelect a readable debug color for one skeletal hitbox wireframe.
*\t@param\thitboxIndex\tCurrent hitbox iteration index.
*\t@param\thitBodyID\tBody-part identifier from the hitbox definition.
*\t@note\tAlternating a small palette improves separation when many hitboxes overlap.
**/
static uint32_t CLG_DebugColorForMonsterHitbox( const uint32_t hitboxIndex, const int32_t hitBodyID ) {
    static constexpr uint32_t palette[] = {
        U32_CYAN,
        U32_MAGENTA,
        U32_RED,
        U32_WHITE,
        U32_BLUE,
        U32_GREEN,
    };

    const uint32_t paletteCount = static_cast<uint32_t>( sizeof( palette ) / sizeof( palette[ 0 ] ) );
    const uint32_t bodyHash = static_cast<uint32_t>( hitBodyID >= 0 ? hitBodyID : 0 );
    return palette[ ( hitboxIndex + bodyHash ) % paletteCount ];
}


/**
*\t@brief\tDraw posed skeletal hitbox boxes for a monster packet entity when the client debug cvar is enabled.
*\t@param\trefreshEntity\tRefresh entity carrying the current pose state.
*\t@param\tmodel\tModel resource for the monster entity.
*\t@note\tThis mirrors the server-side hitbox geometry but stays entirely local to the client render path.
**/
static void CLG_DebugDrawMonsterSkeletalHitboxes( const entity_t *refreshEntity, const model_t *model ) {
    // Sanity: require the debug toggle and the model/pose data needed to build the box corners.
    if ( !clg_debug_draw_skeletal_hitboxes || !clg_debug_draw_skeletal_hitboxes->integer ) {
        return;
    }

    // Require explicit developer mode for skeletal hitbox overlays.
    if ( !developer || !developer->integer ) {
        return;
    }

    if ( !refreshEntity || !model || !model->skmData ) {
        return;
    }

    const skm_model_t *skmData = model->skmData;
    if ( skmData->num_hitboxes <= 0 || skmData->num_joints <= 0 || !skmData->poses || skmData->num_poses <= 0 ) {
        return;
    }

    // Build the same entity-space basis that the render path uses so the overlay stays aligned to the model.
    Vector3 entityForward = { 0.0f, 0.0f, 0.0f };
    Vector3 entityRight = { 0.0f, 0.0f, 0.0f };
    Vector3 entityUp = { 0.0f, 0.0f, 0.0f };
    QM_AngleVectors( refreshEntity->angles, &entityForward, &entityRight, &entityUp );
    // Match renderer AnglesToAxis convention: axis[1] is inverted right vector.
    entityRight = -entityRight;

    // Recompute the current frame's bone transforms locally so the hitboxes match the displayed animation.
    static skm_transform_t relativeBonePoses[ SKM_MAX_BONES ] = {};
    static float boneLocalMatrices[ SKM_MAX_BONES ][ 12 ] = {};
    SG_SKM_ComputeLerpBonePoses( model, refreshEntity->frame, refreshEntity->oldframe,
        1.0f - refreshEntity->backlerp, refreshEntity->backlerp, relativeBonePoses,
        refreshEntity->rootMotionBoneID,
        refreshEntity->rootMotionFlags );
    SG_SKM_TransformBonePosesLocalSpace( skmData, relativeBonePoses, &boneLocalMatrices[ 0 ][ 0 ] );

    // Draw every hitbox as a world-space wireframe box so the client can inspect the final posed extents.
    static constexpr uint8_t edgePairs[ 12 ][ 2 ] = {
        { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
        { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };
    for ( uint32_t hitboxIndex = 0; hitboxIndex < skmData->num_hitboxes; hitboxIndex++ ) {
        const skm_hitbox_t &hitbox = skmData->hitboxes[ hitboxIndex ];
        if ( hitbox.boneIndex < 0 || hitbox.boneIndex >= (int32_t)skmData->num_joints ) {
            continue;
        }

        const uint32_t hitboxColor = CLG_DebugColorForMonsterHitbox( hitboxIndex, hitbox.hitBodyID );

        const Vector3 localMins = Vector3( hitbox.localMins );
        const Vector3 localMaxs = Vector3( hitbox.localMaxs );
        const Vector3 localCorners[ 8 ] = {
            { localMins.x, localMins.y, localMins.z },
            { localMaxs.x, localMins.y, localMins.z },
            { localMins.x, localMaxs.y, localMins.z },
            { localMaxs.x, localMaxs.y, localMins.z },
            { localMins.x, localMins.y, localMaxs.z },
            { localMaxs.x, localMins.y, localMaxs.z },
            { localMins.x, localMaxs.y, localMaxs.z },
            { localMaxs.x, localMaxs.y, localMaxs.z },
        };

        const float *boneLocalMatrix = boneLocalMatrices[ hitbox.boneIndex ];
        Vector3 worldCorners[ 8 ] = {};
        for ( int32_t cornerIndex = 0; cornerIndex < 8; cornerIndex++ ) {
            const Vector3 modelPoint = {
                boneLocalMatrix[ 0 ] * localCorners[ cornerIndex ].x + boneLocalMatrix[ 1 ] * localCorners[ cornerIndex ].y + boneLocalMatrix[ 2 ] * localCorners[ cornerIndex ].z + boneLocalMatrix[ 3 ],
                boneLocalMatrix[ 4 ] * localCorners[ cornerIndex ].x + boneLocalMatrix[ 5 ] * localCorners[ cornerIndex ].y + boneLocalMatrix[ 6 ] * localCorners[ cornerIndex ].z + boneLocalMatrix[ 7 ],
                boneLocalMatrix[ 8 ] * localCorners[ cornerIndex ].x + boneLocalMatrix[ 9 ] * localCorners[ cornerIndex ].y + boneLocalMatrix[ 10 ] * localCorners[ cornerIndex ].z + boneLocalMatrix[ 11 ]
            };
            worldCorners[ cornerIndex ] = refreshEntity->origin
                + ( entityForward * modelPoint.x )
                + ( entityRight * modelPoint.y )
                + ( entityUp * modelPoint.z );
        }

        for ( int32_t edgeIndex = 0; edgeIndex < 12; edgeIndex++ ) {
            clgi.R_DrawDebugLine( &worldCorners[ edgePairs[ edgeIndex ][ 0 ] ].x, &worldCorners[ edgePairs[ edgeIndex ][ 1 ] ].x, hitboxColor, 1.0f, 0.0f, 0 );
        }
    }
}

/**
*\t@brief\tDraw default packet-entity world AABB for monsters while skeletal overlay is enabled.
*\t@param\tpacketEntity\tMonster packet entity with decoded bounds and lerped origin.
**/
static void CLG_DebugDrawMonsterDefaultBounds( const centity_t *packetEntity ) {
    if ( !clg_debug_draw_skeletal_hitboxes || !clg_debug_draw_skeletal_hitboxes->integer ) {
        return;
    }

    // Require explicit developer mode for packet-entity bounds overlays.
    if ( !developer || !developer->integer ) {
        return;
    }

    if ( !packetEntity ) {
        return;
    }

    if ( packetEntity->current.solid == SOLID_NOT || packetEntity->current.solid == BOUNDS_BRUSHMODEL ) {
        return;
    }

    vec3_t worldMins = {};
    vec3_t worldMaxs = {};
    worldMins[ 0 ] = packetEntity->lerpOrigin.x + packetEntity->mins.x;
    worldMins[ 1 ] = packetEntity->lerpOrigin.y + packetEntity->mins.y;
    worldMins[ 2 ] = packetEntity->lerpOrigin.z + packetEntity->mins.z;
    worldMaxs[ 0 ] = packetEntity->lerpOrigin.x + packetEntity->maxs.x;
    worldMaxs[ 1 ] = packetEntity->lerpOrigin.y + packetEntity->maxs.y;
    worldMaxs[ 2 ] = packetEntity->lerpOrigin.z + packetEntity->maxs.z;

    // Default gameplay collision box in high-contrast yellow.
    clgi.R_DrawDebugBox( worldMins, worldMaxs, U32_YELLOW, 1.0f, 0.0f, 0 );

    // Draw some simple "X" mark over top of the bounding box to make monster standing positions clearer.
    const Vector3 topDiagStart = { worldMins[ 0 ], worldMins[ 1 ], worldMaxs[ 2 ] };
    const Vector3 topDiagEnd = { worldMaxs[ 0 ], worldMaxs[ 1 ], worldMaxs[ 2 ] };
    const Vector3 topDiagStart2 = { worldMaxs[ 0 ], worldMins[ 1 ], worldMaxs[ 2 ] };
    const Vector3 topDiagEnd2 = { worldMins[ 0 ], worldMaxs[ 1 ], worldMaxs[ 2 ] };
    clgi.R_DrawDebugLine( &topDiagStart.x, &topDiagEnd.x, U32_YELLOW, 1.0f, 0.0f, 0 );
    clgi.R_DrawDebugLine( &topDiagStart2.x, &topDiagEnd2.x, U32_YELLOW, 1.0f, 0.0f, 0 );
}


/**
*	@brief	Will setup the refresh entity for the ET_MONSTER centity with the nextState.
**/
void CLG_PacketEntity_AddMonster( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState ) {
	//if ( packetEntity->serverframe == clgi.client->frame.number ) {
	//	if ( packetEntity->snapShotTime != clgi.client->time ) {
	//	// Not yet printed for this frame.
	//		clgi.Print( PRINT_DEVELOPER, "CLG_PacketEntity_AddMonster: Entity(#%d), serverframe(%llu)\n", packetEntity->current.number, packetEntity->serverframe );
	//	}
	//}
	
	//
	// Lerp Origin:
	
	//
    // Lerp Origin:
    //   
    // Step origin discretely, because the frames do the animation properly:
    if ( nextState->renderfx & RF_OLD_FRAME_LERP ) {
        VectorCopy( packetEntity->current.origin, refreshEntity->origin );
        VectorCopy( packetEntity->current.old_origin, refreshEntity->oldorigin );  // FIXME
    } else {
        // Lerp Origin:
        Vector3 lerpedOrigin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
        VectorCopy( lerpedOrigin, refreshEntity->origin );
        VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );

    }

    //
    // Lerp Angles.
    //
    LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );

    // If no rotation flag is set, add specified trail flags. We don't need it spamming
    // a blood trail of entities when it basically stopped motion.
    if ( nextState->entityFlags & ~EF_ROTATE ) {
        if ( nextState->entityFlags & EF_GIB ) {
            CLG_FX_DiminishingTrail( packetEntity->lerpOrigin, refreshEntity->origin, packetEntity, nextState->entityFlags | EF_GIB );
        }
    }

    //
    // Special RF_STAIR_STEP lerp for Z axis.
    // 
    // Handle the possibility of a stair step occuring.
    static constexpr int64_t STEP_TIME = 150; // Smooths it out over 150ms, this used to be 100ms.
    //uint64_t realTime = clgi.GetRealTime();
    //if ( packetEntity->step_realtime >= realTime - STEP_TIME ) {
    if ( packetEntity->step_servertime >= clgi.client->extrapolatedTime - STEP_TIME ) {
        uint64_t stair_step_delta = clgi.GetRealTime() - packetEntity->step_realtime;
        //uint64_t stair_step_delta = clgi.client->extrapolatedTime - packetEntity->step_servertime;
        //uint64_t stair_step_delta = clgi.client->time - ( packetEntity->step_servertime - clgi.client->sv_frametime );

        // Smooth out stair step over 200ms.
        if ( stair_step_delta <= STEP_TIME ) {
            static constexpr double STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

            // Smooth it out further for smaller steps.
            //static constexpr float PHYS_STEP_SMALL_SIZE = 18.f;
			// This defines the maximum step height that will be smoothed out more, by doubling the stair_step_delta, which results in a faster lerp and thus smoother appearance for smaller steps. The value of 15.f is chosen based on typical step heights in the game, but can be adjusted as needed for better visual results.
            if ( fabs( packetEntity->step_height ) <= PHYS_STEP_SMALL_SIZE ) {
                stair_step_delta <<= 1; // small steps
            }

            // Calculate step time.
            int64_t stair_step_time = STEP_TIME - std::min<int64_t>( stair_step_delta, STEP_TIME );

            // Calculate lerped Z origin.
            //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            refreshEntity->origin[ 2 ] = QM_Lerp<double>( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
            VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
        }
    }

    //
    // Add Refresh Entity Model:
    // 
    // Model Index #1:
    if ( nextState->modelindex ) {
        // Skin.
        refreshEntity->skinnum = nextState->skinnum;
        refreshEntity->skin = 0;
        // Model.
        refreshEntity->model = clgi.client->model_draw[ nextState->modelindex ];
        // Render entityFlags.
        refreshEntity->flags = nextState->renderfx;

        // Allow skin override for remaster.
        if ( nextState->renderfx & RF_CUSTOMSKIN && (unsigned)nextState->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
            if ( nextState->skinnum >= 0 && nextState->skinnum < 512 ) {
                refreshEntity->skin = clgi.client->image_precache[ nextState->skinnum ];
            }
            refreshEntity->skinnum = 0;
        }

        //
        // Animation Frame Lerping:
        //
        // Initialize these every frame so rendering/debug paths never see stale or zeroed root-motion settings.
        refreshEntity->frame = packetEntity->current_frame;
        refreshEntity->oldframe = packetEntity->last_frame;
        refreshEntity->backlerp = 0.0f;
        refreshEntity->rootMotionBoneID = 0;
        refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z | SKM_POSE_TRANSLATE_Y;

        if ( !( refreshEntity->model & 0x80000000 ) && packetEntity->last_frame != packetEntity->current_frame ) {
            // Calculate back lerpfraction using clgi.client->time. (40hz.)
            constexpr int32_t animationHz = BASE_FRAMERATE;
            constexpr float animationMs = 1.f / ( animationHz ) * 1000.f;
            refreshEntity->backlerp = 1.f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / animationMs );
            refreshEntity->backlerp = QM_Clamp( refreshEntity->backlerp, 0.0f, 1.f );
        }

        // Add refresh entity to scene.
        clgi.V_AddEntity( refreshEntity );

        // Draw the same posed hitbox boxes locally for debug validation without involving the server debug-draw channel.
        CLG_DebugDrawMonsterSkeletalHitboxes( refreshEntity, clgi.R_GetModelDataForHandle( refreshEntity->model ) );

    }
    // Model Index #2:
    if ( nextState->modelindex2 ) {
        // Add Model.
        refreshEntity->model = clgi.client->model_draw[ nextState->modelindex2 ];
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #3:
    if ( nextState->modelindex3 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        // Add Model.
        refreshEntity->model = clgi.client->model_draw[ nextState->modelindex3 ];
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #4:
    if ( nextState->modelindex4 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        refreshEntity->model = clgi.client->model_draw[ nextState->modelindex4 ];
        // Add Model.
        clgi.V_AddEntity( refreshEntity );
    }

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerpOrigin );

    // Keep default monster collision bounds visible when using the skeletal hitbox overlay.
    CLG_DebugDrawMonsterDefaultBounds( packetEntity );
}