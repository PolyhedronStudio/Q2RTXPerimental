/********************************************************************
*
*
*	ClientGame: Packet Entities.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_effects.h"
#include "clg_entities.h"
#include "clg_temp_entities.h"

static const int32_t adjust_shell_fx( const int32_t renderfx ) {
    return renderfx;
}

/*
==========================================================================

DYNLIGHT ENTITY SUPPORT:

==========================================================================
*/
void CLG_PacketEntity_AddSpotlight( centity_t *cent, entity_t *ent, entity_state_t *s1 ) {
    // Calculate RGB vector.
    vec3_t rgb = { 1.f, 1.f, 1.f };
    rgb[ 0 ] = ( 1.0f / 255.f ) * s1->spotlight.rgb[ 0 ];
    rgb[ 1 ] = ( 1.0f / 255.f ) * s1->spotlight.rgb[ 1 ];
    rgb[ 2 ] = ( 1.0f / 255.f ) * s1->spotlight.rgb[ 2 ];

    // Extract light intensity from "frame".
    float lightIntensity = s1->spotlight.intensity;

    // Calculate the spotlight's view direction based on set euler angles.
    vec3_t view_dir, right_dir, up_dir;
    AngleVectors( ent->angles, view_dir, right_dir, up_dir );

    // Add the spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
    // Use the image based texture profile in case one is set.
    #if 0
    if ( s1->image_profile ) {
        qhandle_t spotlightPicHandle = R_RegisterImage( "flashlight_profile_01", IT_PIC, static_cast<imageflags_t>( IF_PERMANENT | IF_BILERP ) );
        V_AddSpotLightTexEmission( ent->origin, view_dir, lightIntensity,
            // TODO: Multiply the RGB?
            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
            s1->spotlight.angle_width, spotlightPicHandle );
    } else
        #endif
    {
        clgi.V_AddSpotLight( ent->origin, view_dir, lightIntensity,
            // TODO: Multiply the RGB?
            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
            s1->spotlight.angle_width, s1->spotlight.angle_falloff );
    }

    // Add spotlight. (x = 90, y = 0, z = 0) should give us one pointing right down to the floor. (width 90, falloff 0)
    //V_AddSpotLight( ent->origin, view_dir, 225.0, 1.f, 0.1f, 0.1f, 45, 0 );
    //V_AddSphereLight( ent.origin, 500.f, 1.6f, 1.0f, 0.2f, 10.f );
    //V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
}

/**
*   @brief  Animates the entity's frame for both brush as well as alias models.
**/
static void CLG_PacketEntity_AnimateFrame( centity_t *cent, entity_t *ent, entity_state_t *newState, const uint32_t effects ) {
    // Brush models can auto animate their frames.
    int64_t autoanim = 2 * clgi.client->time / 1000;

    if ( effects & EF_ANIM01 ) {
        ent->frame = autoanim & 1;
    } else if ( effects & EF_ANIM23 ) {
        ent->frame = 2 + ( autoanim & 1 );
    } else if ( effects & EF_ANIM_ALL ) {
        ent->frame = autoanim;
    } else if ( effects & EF_ANIM_ALLFAST ) {
        ent->frame = clgi.client->time / BASE_FRAMETIME; // WID: 40hz: Adjusted. clgi.client->time / 100;
    } else {
        ent->frame = newState->frame;
    }

    // Setup the old frame.
    ent->oldframe = cent->prev.frame;
    // Backlerp.
    ent->backlerp = 1.0f - clgi.client->lerpfrac;

    // WID: 40hz - Does proper frame lerping for 10hz models.
            // TODO: Add a specific render flag for this perhaps? 
            // TODO: must only do this on alias models
            // Don't do this for 'world' model?
    //if ( ent.model != 0 && cent->last_frame != cent->current_frame ) {
    ent->model = ( clgi.client->model_draw[ newState->modelindex ] );

    // Only do this for non-brush models, aka alias models.
    if ( !( ent->model & 0x80000000 ) && cent->last_frame != cent->current_frame ) {
        // Calculate back lerpfraction. (10hz.)
        //ent->backlerp = 1.0f - ( ( clgi.client->time - ( (float)cent->frame_servertime - clgi.client->sv_frametime ) ) / 100.f );
        //clamp( ent->backlerp, 0.0f, 1.0f );
        //ent->frame = cent->current_frame;
        //ent->oldframe = cent->last_frame;

        //// Calculate back lerpfraction. (40hz.)
        //ent->backlerp = 1.0f - ( ( clgi.client->time - ( (float)cent->frame_servertime - clgi.client->sv_frametime ) ) / 25.f );
        //clamp( ent->backlerp, 0.0f, 1.0f );
        //ent->frame = cent->current_frame;
        //ent->oldframe = cent->last_frame;

        // Calculate back lerpfraction. (40hz.)
        //ent->backlerp = 1.0f - ( ( clgi.GetRealTime()  - ( (float)cent->frame_realtime - clgi.client->sv_frametime ) ) / 25.f );
        //clamp( ent->backlerp, 0.0f, 1.0f );
        //ent->frame = cent->current_frame;
        //ent->oldframe = cent->last_frame;

        // 40hz gun rate.
        constexpr int32_t playerstate_gun_rate = 40;
        const float anim_ms = 1.f / ( playerstate_gun_rate ) * 1000.f;
        ent->backlerp = 1.f - ( ( clgi.client->time - ( (float)cent->frame_servertime - clgi.client->sv_frametime ) ) / anim_ms );
        clamp( ent->backlerp, 0.0f, 1.f );
        ent->frame = cent->current_frame;
        ent->oldframe = cent->last_frame;

        // For Skeletal Models:
        //if ( cent->frame_realtime != 0 ) {
        //    static constexpr double millisecond = 1e-3f;

        //    int64_t timediff = clgi.GetRealTime() - cent->frame_realtime; //clgi.client->time - prevtime;
        //    double frame = cent->current_frame + (double)timediff * millisecond * std::max( 40.f, 0.f );

        //    if ( frame >= (double)415 ) {
        //        frame = 415.f;
        //    } else if ( frame < 0 ) {
        //        frame = 0;
        //    }

        //    double frac = frame - std::floor( frame );

        //    ent->oldframe = (int)frame;
        //    ent->frame = frame + 1;
        //    ent->backlerp = 1.f - frac;
        //}
    }
    // WID: 40hz
}
/**
*   @brief  Handles the 'lerping' of the packet and its corresponding refresh entity origins.
**/
static void CLG_PacketEntity_LerpOrigin( centity_t *cent, entity_t *ent, entity_state_t *newState ) {
    if ( newState->renderfx & RF_FRAMELERP ) {
        // step origin discretely, because the frames do the animation properly
        VectorCopy( cent->current.origin, ent->origin );
        VectorCopy( cent->current.old_origin, ent->oldorigin );  // FIXME
    } else if ( newState->renderfx & RF_BEAM ) {
        // interpolate start and end points for beams
        Vector3 cent_origin = QM_Vector3Lerp( cent->prev.origin, cent->current.origin, clgi.client->lerpfrac );
        VectorCopy( cent_origin, ent->origin );
        Vector3 cent_old_origin = QM_Vector3Lerp( cent->prev.old_origin, cent->current.old_origin, clgi.client->lerpfrac );
        VectorCopy( cent_old_origin, ent->oldorigin );
    } else {
        if ( newState->number == clgi.client->frame.clientNum + 1 ) {
            // use predicted origin
            VectorCopy( clgi.client->playerEntityOrigin, ent->origin );
            VectorCopy( clgi.client->playerEntityOrigin, ent->oldorigin );
        } else {
            // interpolate origin
            Vector3 cent_origin = QM_Vector3Lerp( cent->prev.origin, cent->current.origin, clgi.client->lerpfrac );
            VectorCopy( cent_origin, ent->origin );
            VectorCopy( ent->origin, ent->oldorigin );
        }
    }

    // Handle the possibility of a stair step occuring.
    static constexpr int64_t STEP_TIME = 100; // Smooths it out over 200ms, this used to be 100ms.
    uint64_t stair_step_delta = clgi.GetRealTime() - cent->step_realtime;
    //uint64_t stair_step_delta = clgi.client->time - ( cent->step_servertime - clgi.client->sv_frametime );

    // Smooth out stair step over 200ms.
    if ( stair_step_delta <= STEP_TIME ) {
        static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

        // Smooth it out further for smaller steps.
        //static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
        //if ( fabs( cent->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
        //    stair_step_delta <<= 1; // small steps
        //}

        // Calculate step time.
        int64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );

        // Calculate lerped Z origin.
        //cent->current.origin[ 2 ] = QM_Lerp( cent->prev.origin[ 2 ], cent->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
        const float cent_origin_z = QM_Lerp( cent->prev.origin[ 2 ], cent->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );
        const Vector3 cent_origin = { cent->current.origin[ 0 ], cent->current.origin[ 1 ], cent_origin_z };
        // Assign to render entity.
        VectorCopy( cent_origin, ent->origin );
        VectorCopy( cent->current.origin, ent->oldorigin );
    }
}

/**
*   @brief  Handles the 'lerping' of the packet and its corresponding refresh entity angles.
**/
void CLG_PacketEntity_LerpAngles( centity_t *cent, entity_t *ent, entity_state_t *newState, const int32_t effects, const float autorotate ) {
    // For General Rotate: (Some bonus items auto-rotate.)
    if ( effects & EF_ROTATE ) {  // some bonus items auto-rotate
        ent->angles[ 0 ] = 0;
        ent->angles[ 1 ] = autorotate;
        ent->angles[ 2 ] = 0;
    // We are dealing with the frame's client entity, thus we use the predicted entity angles instead:
    } else if ( newState->number == clgi.client->frame.clientNum + 1 ) {
        VectorCopy( clgi.client->playerEntityAngles, ent->angles );      // use predicted angles
    // Reguler entity angle interpolation:
    } else {
        LerpAngles( cent->prev.angles, cent->current.angles, clgi.client->lerpfrac, ent->angles );
    }
}
/**
*   @brief  Sets the packet entity's render skin and model correctly.
**/
static void CLG_PacketEntity_SetModelAndSkin( centity_t *cent, entity_t *ent, entity_state_t *newState, int32_t &renderfx ) {
    // A client player model index.
    if ( newState->modelindex == MODELINDEX_PLAYER ) {
        // Parse and use custom player skin.
        ent->skinnum = 0;
        clientinfo_t *ci = &clgi.client->clientinfo[ newState->skinnum & 0xff ];
        ent->skin = ci->skin;
        ent->model = ci->model;

        // On failure to find any custom client skin and model, resort to defaults being baseclientinfo.
        if ( !ent->skin || !ent->model ) {
            ent->skin = clgi.client->baseclientinfo.skin;
            ent->model = clgi.client->baseclientinfo.model;
            ci = &clgi.client->baseclientinfo;
        }

        // In case of the DISGUISE renderflag set, use the disguise skin.
        if ( renderfx & RF_USE_DISGUISE ) {
            char buffer[ MAX_QPATH ];

            Q_concat( buffer, sizeof( buffer ), "players/", ci->model_name, "/disguise.pcx" );
            ent->skin = clgi.R_RegisterSkin( buffer );
        }
        // A regular alias entity model instead:
    } else {
        ent->skinnum = newState->skinnum;
        ent->skin = 0;
        ent->model = clgi.client->model_draw[ newState->modelindex ];
        if ( ent->model == precache.models.laser || ent->model == precache.models.dmspot ) {
            renderfx |= RF_NOSHADOW;
        }
    }

    // Allow skin override for remaster.
    if ( renderfx & RF_CUSTOMSKIN && (unsigned)newState->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
        if ( newState->skinnum >= 0 && newState->skinnum < 512 ) {
            ent->skin = clgi.client->image_precache[ newState->skinnum ];
        }
        ent->skinnum = 0;
    }
}

/**
*   @brief  Look for any effects demanding a shell renderfx, and apply where needed.
**/
static void CLG_PacketEntity_ApplyShellEffects( uint32_t &effects, int32_t renderfx ) {
    // quad and pent can do different things on client
    if ( effects & EF_PENT ) {
        effects &= ~EF_PENT;
        effects |= EF_COLOR_SHELL;
        renderfx |= RF_SHELL_RED;
    }

    if ( effects & EF_QUAD ) {
        effects &= ~EF_QUAD;
        effects |= EF_COLOR_SHELL;
        renderfx |= RF_SHELL_BLUE;
    }
}

/**
*   @brief  Adds trail effects to the entity.
**/
void CLG_PacketEntity_AddTrailEffects( centity_t *cent, entity_t *ent, entity_state_t *newState, const uint32_t effects ) {
    // If no rotation flag is set, add specified trail flags.
    if ( effects & ~EF_ROTATE ) {
        if ( effects & EF_GIB ) {
            CLG_DiminishingTrail( cent->lerp_origin, ent->origin, cent, effects );
        }
    }
}

/**
*   @brief  Parses all received frame packet entities' new states, iterating over them
*           and (re-)applying any changes to the current and/or new refresh entity that
*           has a matching ID.
**/
void CLG_AddPacketEntities( void ) {
    // Get the current local client's player view entity. (Can be one we're chasing.)
    clgi.client->clientEntity = CLG_ViewBoundEntity();

    // Base entity flags.
    int32_t base_entity_flags = 0;

    // Bonus items rotate at a fixed rate.
    const float autorotate = AngleMod( clgi.client->time * BASE_FRAMETIME_1000 );//AngleMod(clgi.client->time * 0.1f); // WID: 40hz: Adjusted.

    // Clean slate refresh entity. (Yes, I know its name is misleading. Not my fault, vkpt code itself does this.)
    entity_t ent = { };

    // Iterate over this frame's entity states.
    for ( int32_t frameEntityNumber = 0; frameEntityNumber < clgi.client->frame.numEntities; frameEntityNumber++ ) {
        // Get the entity state index for the entity's newly received state.
        const int32_t entityStateIndex = ( clgi.client->frame.firstEntity + frameEntityNumber ) & PARSE_ENTITIES_MASK;
        // Get the pointer to the newly received entity's state.
        entity_state_t *s1 = &clgi.client->entityStates[ entityStateIndex ];

        // Get a pointer to the client game entity that matches the state's number.
        centity_t *cent = &clg_entities[ s1->number ];
        // Setup the refresh entity ID to match that of the client game entity with the RESERVED_ENTITY_COUNT in mind.
        ent.id = RENTITIY_RESERVED_COUNT + cent->id;

        // Acquire the state's effects, and render effects.
        uint32_t effects = s1->effects;
        int32_t renderfx = s1->renderfx;

        // Set refresh entity frame:
        CLG_PacketEntity_AnimateFrame( cent, &ent, s1, effects );
        // Apply 'Shell' render effects based on various effects that are set:
        CLG_PacketEntity_ApplyShellEffects( effects, renderfx );
        // Lerp entity origins:
        CLG_PacketEntity_LerpOrigin( cent, &ent, s1 );


        // Create a new entity
        // Tweak the color of beams
        if ( s1->entityType == ET_BEAM || renderfx & RF_BEAM ) {
            // the four beam colors are encoded in 32 bits of skinnum (hack)
            ent.alpha = 0.30f;
            ent.skinnum = ( s1->skinnum >> ( ( irandom( 4 ) ) * 8 ) ) & 0xff;
            ent.model = 0;
        // Assume that this is thus not a beam, but an alias model entity instead:
        } else {
            CLG_PacketEntity_SetModelAndSkin( cent, &ent, s1, renderfx );
        }

        // Render effects (fullbright, translucent, etc)
        // In case of a shell entity, they are applied to it instead:
        if ( ( effects & EF_COLOR_SHELL ) ) {
            ent.flags = 0;
        // Any other entities apply renderfx to themselves:
        } else {
            ent.flags = renderfx;
        }

        // Calculate Angles, lerp if needed.
        CLG_PacketEntity_LerpAngles( cent, &ent, s1, effects, autorotate );

        // Reset the base refresh entity flags.
        base_entity_flags = 0; // WID: C++20: Make sure to however, reset it to 0.

        // In case of the state belonging to the frame's viewed client number:
        if ( s1->number == clgi.client->frame.clientNum + 1 ) {
            // When not in third person mode:
            if ( !clgi.client->thirdPersonView ) {
                // If we're running RTX, we want the player entity to render for shadow/reflection reasons:
                if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
                    // Make it so it isn't seen by the FPS view, only from mirrors.
                    base_entity_flags |= RF_VIEWERMODEL;    // only draw from mirrors
                    // In OpenGL mode we're already done, so we skip.
                } else {
                    goto skip;
                }
            }

            // Don't tilt the model - looks weird.
            ent.angles[ 0 ] = 0.f;

            // Offset the model back a bit to make the view point located in front of the head
            vec3_t angles = { 0.f, ent.angles[ 1 ], 0.f };
            vec3_t forward;
            AngleVectors( angles, forward, NULL, NULL );

            float offset = -15.f;
            VectorMA( ent.origin, offset, forward, ent.origin );
            VectorMA( ent.oldorigin, offset, forward, ent.oldorigin );
        }

        // Spotlight:
        if ( s1->effects & EF_SPOTLIGHT ) { //if ( s1->entityType == ET_SPOTLIGHT ) {
            CLG_PacketEntity_AddSpotlight( cent, &ent, s1 );
            //return;
        }

        // If set to invisible, skip:
        if ( !s1->modelindex ) {
            goto skip;
        }

        /**
        *   Colored Shells:
        **/
        // Reapply the aforementioned base_entity_flags to the refresh entity.
        ent.flags |= base_entity_flags;
        // RTX Path: 
        // The base entity has the renderfx set on it for 'Shell' effect.
        if ( ( effects & EF_COLOR_SHELL ) && clgi.GetRefreshType() == REF_TYPE_VKPT ) {
            //renderfx = adjust_shell_fx( renderfx );
            ent.flags |= renderfx;
        }
        // Add entity to refresh list
        clgi.V_AddEntity( &ent );
        // OpenGL Path: 
        // Generate a separate render entity for the 'Shell' effect.
        if ( ( effects & EF_COLOR_SHELL ) && clgi.GetRefreshType() != REF_TYPE_VKPT ) {
            //renderfx = adjust_shell_fx( renderfx );
            ent.flags = renderfx | RF_TRANSLUCENT | base_entity_flags;
            ent.alpha = 0.30f;
            // Add the copy.
            clgi.V_AddEntity( &ent );
        }

        /**
        *   #(2nd) Model Index:
        **/
        // Reset skin, skinnum, apply base_entity_flags 
        ent.skin = 0;       // never use a custom skin on others
        ent.skinnum = 0;
        ent.flags = base_entity_flags;
        ent.alpha = 0;

        // 2nd Model Index:
        // Duplicate for linked models.
        if ( s1->modelindex2 ) {
            // Custom weapon:
            if ( s1->modelindex2 == MODELINDEX_PLAYER ) {
                // Fetch client info ID. (encoded in skinnum)
                clientinfo_t *ci = &clgi.client->clientinfo[ s1->skinnum & 0xff ];
                // Fetch weapon ID. (encoded in skinnum).
                int32_t weaponModelIndex = ( s1->skinnum >> 8 ); // 0 is default weapon model
                if ( weaponModelIndex < 0 || weaponModelIndex > precache.numViewModels - 1 ) {
                    weaponModelIndex = 0;
                }
                ent.model = ci->weaponmodel[ weaponModelIndex ];
                if ( !ent.model ) {
                    if ( weaponModelIndex != 0 ) {
                        ent.model = ci->weaponmodel[ 0 ];
                    }
                    if ( !ent.model ) {
                        ent.model = clgi.client->baseclientinfo.weaponmodel[ 0 ];
                    }
                }

            // Regular 2nd model index.
            } else {
                ent.model = clgi.client->model_draw[ s1->modelindex2 ];
            }

            //// PMM - check for the defender sphere shell .. make it translucent
            //if ( !Q_strcasecmp( clgi.client->configstrings[ CS_MODELS + ( s1->modelindex2 ) ], "models/items/shell/tris.md2" ) ) {
            //    ent.alpha = 0.32f;
            //    ent.flags = RF_TRANSLUCENT;
            //}

            if ( ( effects & EF_COLOR_SHELL ) && clgi.GetRefreshType() == REF_TYPE_VKPT ) {
                ent.flags |= renderfx;
            }

            clgi.V_AddEntity( &ent );

            //PGM - make sure these get reset.
            ent.flags = base_entity_flags;
            ent.alpha = 0;
        }

        /**
        *   #(3nd) Model Index:
        **/
        // 3nd Model Index:
        if ( s1->modelindex3 ) {
            ent.model = clgi.client->model_draw[ s1->modelindex3 ];
            clgi.V_AddEntity( &ent );
        }
        /**
        *   #(4nd) Model Index:
        **/
        // 4nd Model Index:
        if ( s1->modelindex4 ) {
            ent.model = clgi.client->model_draw[ s1->modelindex4 ];
            clgi.V_AddEntity( &ent );
        }
        // Add automatic particle trails
        CLG_PacketEntity_AddTrailEffects( cent, &ent, s1, effects );

    // When the entity is skipped, copy over the origin 
    skip:
        VectorCopy( ent.origin, cent->lerp_origin );
    }
}