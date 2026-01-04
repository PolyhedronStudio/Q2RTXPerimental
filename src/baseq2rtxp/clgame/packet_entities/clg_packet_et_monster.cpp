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
*	@brief	Will setup the refresh entity for the ET_MONSTER centity with the nextState.
**/
void CLG_PacketEntity_AddMonster( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState ) {
			//clgi.Print( PRINT_DEVELOPER, "CLG_PacketEntity_AddMonster: Entity(#%d), serverframe(%ull)\n", packetEntity->current.number, packetEntity->serverframe );

	
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
        //uint64_t stair_step_delta = clgi.GetRealTime() - packetEntity->step_realtime;
        uint64_t stair_step_delta = clgi.client->extrapolatedTime - packetEntity->step_servertime;
        //uint64_t stair_step_delta = clgi.client->time - ( packetEntity->step_servertime - clgi.client->sv_frametime );

        // Smooth out stair step over 200ms.
        if ( stair_step_delta <= STEP_TIME ) {
            static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

            // Smooth it out further for smaller steps.
            //static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
            static constexpr float STEP_MAX_SMALL_STEP_SIZE = 15.f;
            if ( fabs( packetEntity->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
                stair_step_delta <<= 1; // small steps
            }

            // Calculate step time.
            int64_t stair_step_time = STEP_TIME - std::min<int64_t>( stair_step_delta, STEP_TIME );

            // Calculate lerped Z origin.
            //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            refreshEntity->origin[ 2 ] = QM_Lerp<double>( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            //VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
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
        if ( !( refreshEntity->model & 0x80000000 ) && packetEntity->last_frame != packetEntity->current_frame ) {
            // Calculate back lerpfraction using clgi.client->time. (40hz.)
            constexpr int32_t animationHz = BASE_FRAMERATE;
            constexpr float animationMs = 1.f / ( animationHz ) * 1000.f;
            refreshEntity->backlerp = 1.f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / animationMs );
            refreshEntity->backlerp = QM_Clamp( refreshEntity->backlerp, 0.0f, 1.f );
            refreshEntity->frame = packetEntity->current_frame;
            refreshEntity->oldframe = packetEntity->last_frame;
            refreshEntity->rootMotionBoneID = 0;
            refreshEntity->rootMotionFlags = SKM_POSE_TRANSLATE_Z | SKM_POSE_TRANSLATE_Y;
        }

        // Add refresh entity to scene.
        clgi.V_AddEntity( refreshEntity );
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
}