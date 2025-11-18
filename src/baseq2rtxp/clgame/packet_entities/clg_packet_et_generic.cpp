/********************************************************************
*
*
*	ClientGame: 'ET_GENERIC' Packet Entities.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_precache.h"

#include "sharedgame/sg_entity_flags.h"

static const int32_t adjust_shell_fx( const int32_t renderfx ) {
    return renderfx;
}

/**
*
*
*
*   Entity Type:    Generic
*
*
*
**/
//void CLG_PacketEntity_AddSpotlight( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
////    // Calculate RGB vector.
////    vec3_t rgb = { 1.f, 1.f, 1.f };
////    rgb[ 0 ] = ( 1.0f / 255.f ) * newState->spotlight.rgb[ 0 ];
////    rgb[ 1 ] = ( 1.0f / 255.f ) * newState->spotlight.rgb[ 1 ];
////    rgb[ 2 ] = ( 1.0f / 255.f ) * newState->spotlight.rgb[ 2 ];
////
////    // Extract light intensity from "frame".
////    float lightIntensity = newState->spotlight.intensity;
////
////    // Calculate the spotlight's view direction based on set euler angles.
////    vec3_t view_dir, right_dir, up_dir;
////    AngleVectors( refreshEntity->angles, view_dir, right_dir, up_dir );
////
////    // Add the spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
////    // Use the image based texture profile in case one is set.
////    #if 0
////    if ( newState->image_profile ) {
////        qhandle_t spotlightPicHandle = R_RegisterImage( "flashlight_profile_01", IT_PIC, static_cast<imageflags_t>( IF_PERMANENT | IF_BILERP ) );
////        V_AddSpotLightTexEmission( refreshEntity->origin, view_dir, lightIntensity,
////            // TODO: Multiply the RGB?
////            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
////            newState->spotlight.angle_width, spotlightPicHandle );
////} else
////#endif
////    {
////        clgi.V_AddSpotLight( refreshEntity->origin, view_dir, lightIntensity,
////            // TODO: Multiply the RGB?
////            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
////            newState->spotlight.angle_width, newState->spotlight.angle_falloff );
////    }
////
////// Add spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
//////V_AddSpotLight( refreshEntity->origin, view_dir, 225.0, 1.f, 0.1f, 0.1f, 45, 0 );
//////V_AddSphereLight( refreshEntity->origin, 500.f, 1.6f, 1.0f, 0.2f, 10.f );
//////V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
//}

/**
*   @brief  Animates the entity's frame for both brush as well as alias models.
**/
static void CLG_PacketEntity_AnimateFrame( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const uint32_t entityFlags ) {
    // Brush models can auto animate their frames.
    int64_t autoanim = 2 * clgi.client->time / 1000;

    if ( entityFlags & EF_ANIM01 ) {
        refreshEntity->frame = autoanim & 1;
    } else if ( entityFlags & EF_ANIM23 ) {
        refreshEntity->frame = 2 + ( autoanim & 1 );
    } else if ( entityFlags & EF_ANIM_ALL ) {
        refreshEntity->frame = autoanim;
    } else if ( entityFlags & EF_ANIM_ALLFAST ) {
        refreshEntity->frame = clgi.client->time / BASE_FRAMETIME; // WID: 40hz: Adjusted. clgi.client->time / 100;
    } else {
        refreshEntity->frame = newState->frame;
    }

    // Setup the old frame.
    refreshEntity->oldframe = packetEntity->prev.frame;
    // Backlerp.
    refreshEntity->backlerp = 1.0f - clgi.client->lerpfrac;

    // WID: 40hz - Does proper frame lerping for 10hz models.
            // TODO: Add a specific render flag for this perhaps? 
            // TODO: must only do this on alias models
            // Don't do this for 'world' model?
    //if ( refreshEntity->model != 0 && packetEntity->last_frame != packetEntity->current_frame ) {
    refreshEntity->model = ( clgi.client->model_draw[ newState->modelindex ] );

    // Only do this for non-brush models, aka alias models.
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
        refreshEntity->backlerp = QM_Clamp( refreshEntity->backlerp, 0.0f, 1.f );
        refreshEntity->frame = packetEntity->current_frame;
        refreshEntity->oldframe = packetEntity->last_frame;
    }
    // WID: 40hz
}
/**
*   @brief  Handles the 'lerping' of the packet and its corresponding refresh entity origins.
**/
static void CLG_PacketEntity_LerpOrigin( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Step origin discretely, because the frames do the animation properly:
    if ( newState->renderfx & RF_FRAMELERP ) {
        VectorCopy( packetEntity->current.origin, refreshEntity->origin );
        VectorCopy( packetEntity->current.old_origin, refreshEntity->oldorigin );  // FIXME
        // Interpolate start and end points for beams.
    } else if ( newState->renderfx & RF_BEAM ) {
        Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
        VectorCopy( cent_origin, refreshEntity->origin );
        Vector3 cent_old_origin = QM_Vector3Lerp( packetEntity->prev.old_origin, packetEntity->current.old_origin, clgi.client->lerpfrac );
        VectorCopy( cent_old_origin, refreshEntity->oldorigin );
        // Default to lerp:
    } else {
        // If client entity, use predicted origin instead of Lerped:
        if ( newState->number == clgi.client->clientNumber + 1 ) {
            VectorCopy( clgi.client->playerEntityOrigin, refreshEntity->origin );
            VectorCopy( clgi.client->playerEntityOrigin, refreshEntity->oldorigin );
            // Lerp Origin:
        } else {
            Vector3 cent_origin = QM_Vector3Lerp( packetEntity->prev.origin, packetEntity->current.origin, clgi.client->lerpfrac );
            VectorCopy( cent_origin, refreshEntity->origin );
            VectorCopy( refreshEntity->origin, refreshEntity->oldorigin );
        }
    }

    //// Handle the possibility of a stair step occuring.
    //static constexpr int64_t STEP_TIME = 100; // Smooths it out over 200ms, this used to be 100ms.
    //uint64_t realTime = clgi.GetRealTime();
    //if ( packetEntity->step_realtime >= realTime - STEP_TIME ) {
    //    uint64_t stair_step_delta = clgi.GetRealTime() - packetEntity->step_realtime;
    //    //uint64_t stair_step_delta = clgi.client->time - ( packetEntity->step_servertime - clgi.client->sv_frametime );

    //    // Smooth out stair step over 200ms.
    //    if ( stair_step_delta <= STEP_TIME ) {
    //        static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

    //        // Smooth it out further for smaller steps.
    //        //static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
    //        //if ( fabs( packetEntity->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
    //        //    stair_step_delta <<= 1; // small steps
    //        //}
     
    //        // Calculate step time.
    //        int64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );
     
    //        // Calculate lerped Z origin.
    //        //packetEntity->current.origin[ 2 ] = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
    //        const float cent_origin_z = QM_Lerp( packetEntity->prev.origin[ 2 ], packetEntity->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
    //        const Vector3 cent_origin = { packetEntity->current.origin[ 0 ], packetEntity->current.origin[ 1 ], cent_origin_z };
    //        // Assign to render entity.
    //        VectorCopy( cent_origin, refreshEntity->origin );
    //        VectorCopy( packetEntity->current.origin, refreshEntity->oldorigin );
    //    }
    //}
}

/**
*   @brief  Handles the 'lerping' of the packet and its corresponding refresh entity angles.
**/
static void CLG_PacketEntity_LerpAngles( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const int32_t entityFlags, const float autorotate ) {
    // For General Rotate: (Some bonus items auto-rotate.)
    if ( entityFlags & EF_ROTATE ) {  // some bonus items auto-rotate
        refreshEntity->angles[ 0 ] = 0;
        refreshEntity->angles[ 1 ] = autorotate;
        refreshEntity->angles[ 2 ] = 0;
        // We are dealing with the frame's client entity, thus we use the predicted entity angles instead:
    } else if ( newState->number == clgi.client->frame.clientNum + 1 ) {
        VectorCopy( clgi.client->playerEntityAngles, refreshEntity->angles );      // use predicted angles
        // Reguler entity angle interpolation:
    } else {
        LerpAngles( packetEntity->prev.angles, packetEntity->current.angles, clgi.client->lerpfrac, refreshEntity->angles );
    }
}
/**
*   @brief  Sets the packet entity's render skin and modelindex 1 correctly.
**/
static void CLG_PacketEntity_SetModelAndSkin( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, int32_t &renderfx ) {
    // Special treatment for ET_BEAM/RF_BEAM:
    if ( newState->entityType == ET_BEAM || renderfx & RF_BEAM ) {
        // the four beam colors are encoded in 32 bits of skinnum (hack)
        refreshEntity->alpha = 0.30f;
        refreshEntity->skinnum = ( newState->skinnum >> ( ( irandom( 4 ) ) * 8 ) ) & 0xff;
        refreshEntity->model = 0;
        // Assume that this is thus not a beam, but an alias model entity instead:
    } else {
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
            if ( renderfx & RF_USE_DISGUISE ) {
                char buffer[ MAX_QPATH ];

                Q_concat( buffer, sizeof( buffer ), "players/", ci->model_name, "/disguise.pcx" );
                refreshEntity->skin = clgi.R_RegisterSkin( buffer );
            }
        // A regular alias entity model instead:
        } else {
            refreshEntity->skinnum = newState->skinnum;
            refreshEntity->skin = 0;
            refreshEntity->model = clgi.client->model_draw[ newState->modelindex ];
            //if ( refreshEntity->model == precache.models.laser || refreshEntity->model == precache.models.dmspot ) {
            //    renderfx |= RF_NOSHADOW;
            //}
        }

        // Allow skin override for remaster.
        if ( renderfx & RF_CUSTOMSKIN && (unsigned)newState->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
            if ( newState->skinnum >= 0 && newState->skinnum < 512 ) {
                refreshEntity->skin = clgi.client->image_precache[ newState->skinnum ];
            }
            refreshEntity->skinnum = 0;
        }
    }
}
/**
*   @brief  Sets the packet entity's render skin and modelindex 2 correctly.
**/
static void CLG_PacketEntity_SetModel2AndSkin( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const int32_t entityFlags, int32_t &renderfx ) {
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
        if ( entityFlags & EF_COLOR_SHELL ) {
            refreshEntity->flags |= renderfx;
        }
        clgi.V_AddEntity( refreshEntity );
    }
}
/**
*   @brief  Sets the packet entity's render skin and modelindex 3 correctly.
**/
static void CLG_PacketEntity_SetModel3AndSkin( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const int32_t entityFlags, const int32_t base_entity_flags, int32_t &renderfx ) {
    if ( newState->modelindex3 ) {
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex3 ];
        clgi.V_AddEntity( refreshEntity );
    }
}
/**
*   @brief  Sets the packet entity's render skin and modelindex 4 correctly.
**/
static void CLG_PacketEntity_SetModel4AndSkin( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const int32_t entityFlags, const int32_t base_entity_flags, int32_t &renderfx ) {
    if ( newState->modelindex4 ) {
        refreshEntity->model = clgi.client->model_draw[ newState->modelindex4 ];
        clgi.V_AddEntity( refreshEntity );
    }
}
/**
*   @brief  Look for any entityFlags demanding a shell renderfx, and apply where needed.
**/
static void CLG_PacketEntity_ApplyShellEffects( uint32_t &entityFlags, int32_t &renderfx ) {
    // quad and pent can do different things on client
    if ( entityFlags & EF_PENT ) {
        entityFlags &= ~EF_PENT;
        entityFlags |= EF_COLOR_SHELL;
        renderfx |= RF_SHELL_RED;
    }

    if ( entityFlags & EF_QUAD ) {
        entityFlags &= ~EF_QUAD;
        entityFlags |= EF_COLOR_SHELL;
        renderfx |= RF_SHELL_BLUE;
    }
}

/**
*   @brief  Adds trail entityFlags to the entity.
**/
static void CLG_PacketEntity_AddTrailEffects( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState, const uint32_t entityFlags ) {
    // If no rotation flag is set, add specified trail flags.
    // WID: Why not? Let's just do this.
    //if ( entityFlags & ~EF_ROTATE ) {
    if ( entityFlags & EF_GIB ) {
        CLG_DiminishingTrail( packetEntity->lerp_origin, refreshEntity->origin, packetEntity, entityFlags );
    }
    //}
}

/**
*	@brief	Will setup the refresh entity for the ET_GENERIC centity with the newState.
**/
void CLG_PacketEntity_AddGeneric( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *newState ) {
    // Base entity flags.
    int32_t base_entity_flags = 0;

    // Bonus items rotate at a fixed rate.
    const float autorotate = QM_AngleMod( clgi.client->time * BASE_FRAMETIME_1000 );//AngleMod(clgi.client->time * 0.1f); // WID: 40hz: Adjusted.

    // Acquire the state's entityFlags, and render entityFlags.
    uint32_t entityFlags = newState->entityFlags;
    int32_t renderfx = newState->renderfx;

    // Set refresh entity frame:
    CLG_PacketEntity_AnimateFrame( packetEntity, refreshEntity, newState, entityFlags );
    // Apply 'Shell' render entityFlags based on various entityFlags that are set:
    CLG_PacketEntity_ApplyShellEffects( entityFlags, renderfx );
    // Lerp entity origins:
    CLG_PacketEntity_LerpOrigin( packetEntity, refreshEntity, newState );
    // Set model and skin.
    CLG_PacketEntity_SetModelAndSkin( packetEntity, refreshEntity, newState, renderfx );
    // Calculate Angles, lerp if needed:
    CLG_PacketEntity_LerpAngles( packetEntity, refreshEntity, newState, entityFlags, autorotate );

    // Render entityFlags (fullbright, translucent, etc)
    // In case of a shell entity, they are applied to it instead:
    if ( ( entityFlags & EF_COLOR_SHELL ) ) {
        refreshEntity->flags = 0;
        // Any other entities apply renderfx to themselves:
    } else {
        refreshEntity->flags = renderfx;
    }

    // Reset the base refresh entity flags.
    base_entity_flags = 0; // WID: C++20: Make sure to however, reset it to 0.

    // In case of the state belonging to the frame's viewed client number:
    if ( newState->number == clgi.client->frame.clientNum + 1 ) {
        // When not in third person mode:
        if ( !clgi.client->thirdPersonView ) {
            // If we're running RTX, we want the player entity to render for shadow/reflection reasons:
            //if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
                // Make it so it isn't seen by the FPS view, only from mirrors.
            base_entity_flags |= RF_VIEWERMODEL;    // only draw from mirrors
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

    // Spotlight:
    //if ( newState->entityFlags & EF_SPOTLIGHT ) {
    //    CLG_PacketEntity_AddSpotlight( packetEntity, &refreshEntity, newState );
    //}

    // If set to invisible, skip:
    if ( !newState->modelindex ) {
        goto skip;
    }

    // Colored Shells:
    // Reapply the aforementioned base_entity_flags to the refresh entity.
    if ( clgi.GetRefreshType() == REF_TYPE_GL ) {
        // Add entity to refresh list
        clgi.V_AddEntity( refreshEntity );

        // The base entity has the renderfx set on it for 'Shell' effect.
        if ( ( entityFlags & EF_COLOR_SHELL ) ) {
            // color shells generate a separate entity for the main model
            refreshEntity->flags |= base_entity_flags;
            refreshEntity->flags |= renderfx;
            // Add entity to refresh list
            clgi.V_AddEntity( refreshEntity );
        }
    } else {
        // The base entity has the renderfx set on it for 'Shell' effect.
        if ( ( entityFlags & EF_COLOR_SHELL ) ) {
            refreshEntity->flags |= renderfx;
        }
        // Add entity to refresh list
        clgi.V_AddEntity( refreshEntity );
    }

    // #(2nd) Model Index: Reset skin, skinnum, apply base_entity_flags       
    refreshEntity->skin = 0;
    refreshEntity->skinnum = 0;
    refreshEntity->flags = base_entity_flags;
    refreshEntity->alpha = 0;
    CLG_PacketEntity_SetModel2AndSkin( packetEntity, refreshEntity, newState, entityFlags, renderfx );
    // Reset these:
    refreshEntity->flags = base_entity_flags;
    refreshEntity->alpha = 0;
    // #(3nd) Model Index:
    CLG_PacketEntity_SetModel3AndSkin( packetEntity, refreshEntity, newState, entityFlags, base_entity_flags, renderfx );
    // #(4nd) Model Index:
    CLG_PacketEntity_SetModel4AndSkin( packetEntity, refreshEntity, newState, entityFlags, base_entity_flags, renderfx );
    // Add automatic particle trails
    CLG_PacketEntity_AddTrailEffects( packetEntity, refreshEntity, newState, entityFlags );

    // When the entity is skipped, copy over the origin 
skip:
    VectorCopy( refreshEntity->origin, packetEntity->lerp_origin );
}