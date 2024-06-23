/********************************************************************
*
*
*	ClientGame: 'ET_PLAYER' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_temp_entities.h"

/**
*	@brief	Will setup the refresh entity for the ET_PLAYER centity with the newState.
**/
void CLG_PacketEntity_AddPlayer( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    //
    // Lerp Origin:
    //   
    // If client entity, use predicted origin instead of Lerped:
    if ( CLG_IsFrameClientEntity( newState ) ) {
        // We actually need to offset the Z axis origin by half the bbox height.
        Vector3 correctedOrigin = clgi.client->playerEntityOrigin;
        // For being Dead:
        if ( clgi.client->predictedState.currentPs.stats[ STAT_HEALTH ] <= -40 ) {
            correctedOrigin.z += PM_BBOX_GIBBED_MINS.z;
        // For being Ducked:
        } else if ( clgi.client->predictedState.currentPs.pmove.pm_flags & PMF_DUCKED ) {
            correctedOrigin.z += PM_BBOX_DUCKED_MINS.z;
        } else {
            correctedOrigin.z += PM_BBOX_STANDUP_MINS.z;
        }

        // Now apply the corrected origin to our refresh entity.
        VectorCopy( correctedOrigin, refreshEntity->origin );
        VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
    // Lerp Origin:
    } else {
        Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
        VectorCopy( cent_origin, refreshEntity->origin );
        VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
    }

    //
    // Lerp Angles.
    //
    if ( CLG_IsFrameClientEntity( newState ) ) {
        VectorCopy( clgi.client->playerEntityAngles, refreshEntity->angles );      // use predicted angles
    } else {
        LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );
    }

    // If no rotation flag is set, add specified trail flags. We don't need it spamming
    // a blood trail of entities when it basically stopped motion.
    if ( newState->effects & ~EF_ROTATE ) {
        if ( newState->effects & EF_GIB ) {
            CLG_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, newState->effects | EF_GIB );
        }
    }

    ////
    //// Special RF_STAIR_STEP lerp for Z axis.
    //// 
    //// Handle the possibility of a stair step occuring.
    //static constexpr int64_t STEP_TIME = 150; // Smooths it out over 150ms, this used to be 100ms.
    //uint64_t realTime = clgi.GetRealTime();
    //if ( packetEntity->step_realtime >= realTime - STEP_TIME ) {
    //    uint64_t stair_step_delta = clgi.GetRealTime() - packetEntity->step_realtime;
    //    //uint64_t stair_step_delta = clgi.client->time - ( packetEntity->step_servertime - clgi.client->sv_frametime );

    //    // Smooth out stair step over 200ms.
    //    if ( stair_step_delta <= STEP_TIME ) {
    //        static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

    //        // Smooth it out further for smaller steps.
    //        //static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
    //        static constexpr float STEP_MAX_SMALL_STEP_SIZE = 15.f;
    //        if ( fabs( packetEntity->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
    //            stair_step_delta <<= 1; // small steps
    //        }

    //        // Calculate step time.
    //        int64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );

    //        // Calculate lerped Z origin.
    //        //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
    //        refreshEntity->origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
    //        VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
    //    }
    //}

    //
    // Add Refresh Entity Model:
    // 
    // Model Index #1:
    if ( newState->modelindex ) {
        // A client player model index.
        if ( newState->modelindex == MODELINDEX_PLAYER ) {
            // Parse and use custom player skin.
            refreshEntity->skinnum = 0;
            clientinfo_t *ci = &clgi.client->clientinfo[ newState->skinnum & 0xff ];
            refreshEntity->skin = ci->skin;
            refreshEntity->model = ci->model;

            // On failure to find any custom client skin and model, resort to defaults being baseclientinfo.
            if ( !refreshEntity->skin || !refreshEntity->model ) {
                refreshEntity->skin = clgi.client->baseclientinfo.skin;
                refreshEntity->model = clgi.client->baseclientinfo.model;
                ci = &clgi.client->baseclientinfo;
            }

            // In case of the DISGUISE renderflag set, use the disguise skin.
            if ( newState->renderfx & RF_USE_DISGUISE ) {
                char buffer[ MAX_QPATH ];

                Q_concat( buffer, sizeof( buffer ), "players/", ci->model_name, "/disguise.pcx" );
                refreshEntity->skin = clgi.R_RegisterSkin( buffer );
            }
            // A regular alias entity model instead:
        } else {
            refreshEntity->skinnum = newState->skinnum;
            refreshEntity->skin = 0;
            refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
        }

        //
        // Animation Frame Lerping:
        //
        if ( !( refreshEntity->model & 0x80000000 ) && packetEntity->last_frame != packetEntity->current_frame ) {
            // Calculate back lerpfraction. (10hz.)
            //refreshEntity->backlerp = 1.0f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / 100.f );
            //refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
            //refreshEntity->frame = packetEntity->current_frame;
            //refreshEntity->oldframe = packetEntity->last_frame;

            // Calculate back lerpfraction using RealTime. (40hz.)
            //refreshEntity->backlerp = 1.0f - ( ( clgi.GetRealTime()  - ( (float)packetEntity->frame_realtime - clgi.client->sv_frametime ) ) / 25.f );
            //refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
            //refreshEntity->frame = packetEntity->current_frame;
            //refreshEntity->oldframe = packetEntity->last_frame;

            // Calculate back lerpfraction using clgi.client->time. (40hz.)
            constexpr int32_t animationHz = BASE_FRAMERATE;
            constexpr float animationMs = 1.f / ( animationHz ) * 1000.f;
            refreshEntity->backlerp = 1.f - ( ( clgi.client->time - ( (float)packetEntity->frame_servertime - clgi.client->sv_frametime ) ) / animationMs );
            refreshEntity->backlerp = QM_Clampf( refreshEntity->backlerp, 0.0f, 1.f );
            refreshEntity->frame = packetEntity->current_frame;
            refreshEntity->oldframe = packetEntity->last_frame;
        }

        // Allow skin override for remaster.
        if ( newState->renderfx & RF_CUSTOMSKIN && (unsigned)newState->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
            if ( newState->skinnum >= 0 && newState->skinnum < 512 ) {
                refreshEntity->skin = clgi.client->image_precache[ newState->skinnum ];
            }
            refreshEntity->skinnum = 0;
        }

        // Render effects.
        refreshEntity->flags = newState->renderfx;

        // In case of the state belonging to the frame's viewed client number:
        if ( CLG_IsFrameClientEntity( newState ) ) {
            // When not in third person mode:
            if ( !clgi.client->thirdPersonView ) {
                // If we're running RTX, we want the player entity to render for shadow/reflection reasons:
                //if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
                    // Make it so it isn't seen by the FPS view, only from mirrors.
                refreshEntity->flags |= RF_VIEWERMODEL;    // only draw from mirrors
                // In OpenGL mode we're already done, so we skip.
                //} else {
                //    goto skip;
                //}
            }

            // Don't tilt the model - looks weird.
            refreshEntity->angles[ 0 ] = 0.f;

            // Offset the model back a bit to make the view point located in front of the head
            vec3_t angles = { 0.f, refreshEntity->angles[ 1 ], 0.f };
            vec3_t forward;
            AngleVectors( angles, forward, NULL, NULL );

            float offset = -15.f;
            VectorMA( refreshEntity->origin, offset, forward, refreshEntity->origin );
            VectorMA( refreshEntity->oldorigin, offset, forward, refreshEntity->oldorigin );
        }

        // Add model.
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #2:
    if ( newState->modelindex2 ) {
        // Client Entity Weapon Model:
        if ( newState->modelindex2 == MODELINDEX_PLAYER ) {
            // Fetch client info ID. (encoded in skinnum)
            clientinfo_t *ci = &clgi.client->clientinfo[ newState->skinnum & 0xff ];
            // Fetch weapon ID. (encoded in skinnum).
            int32_t weaponModelIndex = ( newState->skinnum >> 8 ); // 0 is default weapon model
            if ( weaponModelIndex < 0 || weaponModelIndex > precache.numViewModels - 1 ) {
                weaponModelIndex = 0;
            }
            // See if we got a precached weapon model index for the matching client info.
            refreshEntity->model = ci->weaponmodel[ weaponModelIndex ];
            // If not:
            if ( !refreshEntity->model ) {
                // Try using its default index 0 model.
                if ( weaponModelIndex != 0 ) {
                    refreshEntity->model = ci->weaponmodel[ 0 ];
                }
                // If not, use our own baseclient info index 0 weapon model:
                if ( !refreshEntity->model ) {
                    refreshEntity->model = clgi.client->baseclientinfo.weaponmodel[ 0 ];
                }
            }
        // Regular 2nd model index.
        } else {
            refreshEntity->model = clgi.client->model_draw[ newState->modelindex2 ];
        }
        // Add shell effect.
        //if ( newState->effects & EF_COLOR_SHELL ) {
        //    refreshEntity->flags = renderfx;
        //}
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #3:
    if ( newState->modelindex3 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        // Add model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex3 ];
        clgi.V_AddEntity( refreshEntity );
    }
    // Model Index #4:
    if ( newState->modelindex4 ) {
        // Reset.
        refreshEntity->skinnum = 0;
        refreshEntity->skin = 0;
        refreshEntity->flags = 0;
        // Add model.
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex4 ];
        clgi.V_AddEntity( refreshEntity );
    }

    // skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}