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

/**
*	@brief	Will setup the refresh entity for the ET_MONSTER centity with the newState.
**/
void CLG_PacketEntity_AddMonster( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    //
    // Lerp Origin:
    //   
    // Step origin discretely, because the frames do the animation properly:
    if ( newState->renderfx & RF_FRAMELERP ) {
        VectorCopy( packetEntity->current.origin, refreshEntity->origin );
        VectorCopy( packetEntity->current.old_origin, refreshEntity->oldorigin );  // FIXME
        // Interpolate start and end points for beams.
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
    if ( newState->effects & ~EF_ROTATE ) {
        if ( newState->effects & EF_GIB ) {
            CLG_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, newState->effects | EF_GIB );
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
        uint64_t stair_step_delta = clgi.client->extrapolatedTime - packetEntity->step_realtime;
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
            int64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );

            // Calculate lerped Z origin.
            //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            refreshEntity->origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
            //VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
            VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
        }
    }

    //
    // Add Refresh Entity Model:
    // 
    // Model Index #1:
    if ( newState->modelindex ) {
        // Skin.
        refreshEntity->skinnum = newState->skinnum;
        refreshEntity->skin = 0;
        // Model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
        // Render effects.
        refreshEntity->flags = newState->renderfx;

        // Allow skin override for remaster.
        if ( newState->renderfx & RF_CUSTOMSKIN && (unsigned)newState->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
            if ( newState->skinnum >= 0 && newState->skinnum < 512 ) {
                refreshEntity->skin = clgi.client->image_precache[ newState->skinnum ];
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
            refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
            refreshEntity->frame = packetEntity->current_frame;
            refreshEntity->oldframe = packetEntity->last_frame;
        }

        // Add refresh entity to scene.
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #2:
    if ( newState->modelindex2 ) {
        // Add Model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex2 ];
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #3:
    if ( newState->modelindex3 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        // Add Model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex3 ];
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #4:
    if ( newState->modelindex4 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex4 ];
        // Add Model.
        clgi.V_AddEntity( refreshEntity );
    }

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}