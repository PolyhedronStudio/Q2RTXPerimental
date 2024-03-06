/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clg_local.h"


#define RESERVED_ENTITIY_GUN 1
#define RESERVED_ENTITIY_TESTMODEL 2
#define RESERVED_ENTITIY_COUNT 3

/**
*
*
*	Entity Utilities:
*
*
**/
/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what ) {
	server_frame_t *frame = &clgi.client->frame;

	if ( entityNumber == clgi.client->frame.clientNum + 1 ) {
		return; // player entity = current.
	}

	centity_t *e = &clg_entities[ entityNumber ]; //e = &cl_entities[entnum];
	if ( e && e->serverframe == frame->number ) {
		return; // current
	}

	if ( e && e->serverframe ) {
		Com_LPrintf( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d last seen %d frames ago\n", what, entityNumber, frame->number - e->serverframe );
	} else {
		Com_LPrintf( PRINT_DEVELOPER, "SERVER BUG: %s on entity %d never seen before\n", what, entityNumber );
	}
}
#endif

/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void PF_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org ) {
    if ( entityNumber < 0 || entityNumber >= MAX_EDICTS ) {
        Com_Error( ERR_DROP, "%s: bad entnum: %d", __func__, entityNumber );
    }

    if ( !entityNumber || entityNumber == clgi.client->listener_spatialize.entnum ) {
        // Should this ever happen?
        VectorCopy( clgi.client->listener_spatialize.origin, org );
        return;
    }

    // Get entity pointer.
    centity_t *ent = &clg_entities[ entityNumber ]; // ENTITY_FOR_NUMBER( entityNumber );

    // If for whatever reason it is invalid:
    if ( !ent ) {
        Com_Error( ERR_DROP, "%s: nullptr entity for entnum: %d", __func__, entityNumber );
        return;
    }

    // Just regular lerp for most of all entities.
    LerpVector( ent->prev.origin, ent->current.origin, clgi.client->lerpfrac, org );

    // Use the closest point on the line for beam entities:
    if ( ent->current.renderfx & RF_BEAM ) {
        const Vector3 oldOrg = QM_Vector3Lerp( ent->prev.old_origin, ent->current.old_origin, clgi.client->lerpfrac );
        const Vector3 vec = oldOrg - org;

        const Vector3 playerEntityOrigin = clgi.client->playerEntityOrigin;
        const Vector3 p = playerEntityOrigin - org;

        const float t = QM_Clampf( QM_Vector3DotProduct( p, vec ) / QM_Vector3DotProduct( vec, vec ), 0.f, 1.f );

        const Vector3 closestPoint = QM_Vector3MultiplyAdd( org, t, vec );
        VectorCopy( closestPoint, org );
        // Calculate origin for BSP models to be closest point
        // from listener to the bmodel's aabb:
    } else {
        if ( ent->current.solid == PACKED_BSP ) {
            mmodel_t *cm = clgi.client->model_clip[ ent->current.modelindex ];
            if ( cm ) {
                Vector3 absmin = org, absmax = org;
                absmin += cm->mins;
                absmax += cm->maxs;

                const Vector3 closestPoint = QM_Vector3ClosestPointToBox( clgi.client->playerEntityOrigin, absmin, absmax );
                VectorCopy( closestPoint, org );
            }
        }
    }
}

/**
*	@return		A pointer to the entity bound to the client game's view. Unless STAT_CHASE is set to
*               a specific client number the current received frame, this'll point to the entity that
*               is of the local client player himself.
**/
centity_t *CLG_ViewBoundEntity( void ) {
    // Default to clgi.client->clientNumberl.
	int32_t index = clgi.client->clientNumber;

    // Chase entity.
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] ) {
		index = clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS;
	}

    // + 1, because 0 == world.
	return &clg_entities[ index + 1 ];
}


/*
*
*
*   CL_AddPacketEntities
*
* 
*/
static int adjust_shell_fx( int renderfx ) {
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
    rgb[ 0 ] = ( 1.0f / 255.f ) * s1->rgb[ 0 ];
    rgb[ 1 ] = ( 1.0f / 255.f ) * s1->rgb[ 1 ];
    rgb[ 2 ] = ( 1.0f / 255.f ) * s1->rgb[ 2 ];

    // Extract light intensity from "frame".
    float lightIntensity = s1->intensity;

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
            s1->angle_width, spotlightPicHandle );
    } else
    #endif
    {
        clgi.V_AddSpotLight( ent->origin, view_dir, lightIntensity,
            // TODO: Multiply the RGB?
            rgb[ 0 ] * 2, rgb[ 1 ] * 2, rgb[ 2 ] * 2,
            s1->angle_width, s1->angle_falloff );
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
    ent->model = ( clgi.client->model_draw[ cent->current.modelindex ] );

    // Only do this for non-brush models, aka alias models.
    if ( !( ent->model & 0x80000000 ) && cent->last_frame != cent->current_frame ) {
        // Calculate back lerpfraction. (10hz.)
        ent->backlerp = 1.0f - ( ( clgi.client->time - ( (float)cent->frame_servertime - clgi.client->sv_frametime ) ) / 100.f );
        clamp( ent->backlerp, 0.0f, 1.0f );
        ent->frame = cent->current_frame;
        ent->oldframe = cent->last_frame;
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
    static constexpr int64_t STEP_TIME = 200; // Smooths it out over 200ms, this used to be 100ms.
    uint64_t stair_step_delta = clgi.GetRealTime() - ( cent->step_servertime - clgi.client->sv_frametime );

    // Smooth out stair step over 200ms.
    if ( stair_step_delta <= STEP_TIME ) {
        static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

        // Smooth it out further for smaller steps.
        static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
        if ( fabs( cent->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
            stair_step_delta <<= 1; // small steps
        }

        // Calculate step time.
        uint64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );

        // Calculate lerped Z origin.
        cent->current.origin[ 2 ] = QM_Lerp( cent->prev.origin[ 2 ], cent->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );

        // Assign to render entity.
        VectorCopy( cent->current.origin, ent->origin );
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
    // For SPINNING LIGHTS:
    } else if ( effects & EF_SPINNINGLIGHTS ) {
        vec3_t forward;
        vec3_t start;

        ent->angles[ 0 ] = 0;
        ent->angles[ 1 ] = AngleMod( clgi.client->time / 2 ) + newState->angles[ 1 ];
        ent->angles[ 2 ] = 180;

        AngleVectors( ent->angles, forward, NULL, NULL );
        VectorMA( ent->origin, 64, forward, start );
        clgi.V_AddLight( start, 100, 1, 0, 0 );

    // We are dealing with the frame's client entity, thus we use the predicted entity angles instead:
    } else if ( newState->number == clgi.client->frame.clientNum + 1 ) {
        VectorCopy( clgi.client->playerEntityAngles, ent->angles );      // use predicted angles

    // Reguler entity angle interpolation:
    } else { // interpolate angles
        LerpAngles( cent->prev.angles, cent->current.angles, clgi.client->lerpfrac, ent->angles );

        // mimic original ref_gl "leaning" bug (uuugly!)
        if ( newState->modelindex == MODELINDEX_PLAYER && cl_rollhack->integer ) {
            ent->angles[ ROLL ] = -ent->angles[ ROLL ];
        }
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
        if ( ent->model == precache.cl_mod_laser || ent->model == precache.cl_mod_dmspot ) {
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

    if ( effects & EF_DOUBLE ) {
        effects &= ~EF_DOUBLE;
        effects |= EF_COLOR_SHELL;
        renderfx |= RF_SHELL_DOUBLE;
    }

    if ( effects & EF_HALF_DAMAGE ) {
        effects &= ~EF_HALF_DAMAGE;
        effects |= EF_COLOR_SHELL;
        renderfx |= RF_SHELL_HALF_DAM;
    }

    // optionally remove the glowing effect
    if ( cl_noglow->integer )
        renderfx &= ~RF_GLOW;
}

/**
*   @brief  Adds trail effects to the entity.
**/
void CLG_PacketEntity_AddTrailEffects( centity_t *cent, entity_t *ent, entity_state_t *newState, const uint32_t effects ) {
    // Add automatic particle trails
    if ( effects & ~EF_ROTATE ) {
        if ( effects & EF_ROCKET ) {
            if ( !( cl_disable_particles->integer & NOPART_ROCKET_TRAIL ) ) {
                CLG_RocketTrail( cent->lerp_origin, ent->origin, cent );
            }
            if ( cl_dlight_hacks->integer & DLHACK_ROCKET_COLOR )
                clgi.V_AddLight( ent->origin, 200, 1, 0.23f, 0 );
            else
                clgi.V_AddLight( ent->origin, 200, 0.6f, 0.4f, 0.12f );
        } else if ( effects & EF_BLASTER ) {
            if ( effects & EF_TRACKER ) {
                CLG_BlasterTrail2( cent->lerp_origin, ent->origin );
                clgi.V_AddLight( ent->origin, 200, 0.1f, 0.4f, 0.12f );
            } else {
                CLG_BlasterTrail( cent->lerp_origin, ent->origin );
                clgi.V_AddLight( ent->origin, 200, 0.6f, 0.4f, 0.12f );
            }
        } else if ( effects & EF_HYPERBLASTER ) {
            if ( effects & EF_TRACKER )
                clgi.V_AddLight( ent->origin, 200, 0.1f, 0.4f, 0.12f );
            else
                clgi.V_AddLight( ent->origin, 200, 0.6f, 0.4f, 0.12f );
        } else if ( effects & EF_GIB ) {
            CLG_DiminishingTrail( cent->lerp_origin, ent->origin, cent, effects );
        } else if ( effects & EF_GRENADE ) {
            if ( !( cl_disable_particles->integer & NOPART_GRENADE_TRAIL ) ) {
                CLG_DiminishingTrail( cent->lerp_origin, ent->origin, cent, effects );
            }
        } else if ( effects & EF_FLIES ) {
            CLG_FlyEffect( cent, ent->origin );
        } else if ( effects & EF_BFG ) {
            int32_t i = 0;
            if ( effects & EF_ANIM_ALLFAST ) {
                CLG_BfgParticles( ent );
                i = 100;
            } else {
                static const int bfg_lightramp[ 6 ] = { 300, 400, 600, 300, 150, 75 };
                i = newState->frame;
                clamp( i, 0, 5 );
                i = bfg_lightramp[ i ];
            }
            const vec3_t nvgreen = { 0.2716f, 0.5795f, 0.04615f };
            clgi.V_AddSphereLight( ent->origin, i, nvgreen[ 0 ], nvgreen[ 1 ], nvgreen[ 2 ], 20.f );
        } else if ( effects & EF_TRAP ) {
            ent->origin[ 2 ] += 32;
            CLG_TrapParticles( cent, ent->origin );
            const int32_t i = ( irandom( 100 ) ) + 100;
            clgi.V_AddLight( ent->origin, i, 1, 0.8f, 0.1f );
        } else if ( effects & EF_FLAG1 ) {
            CLG_FlagTrail( cent->lerp_origin, ent->origin, 242 );
            clgi.V_AddLight( ent->origin, 225, 1, 0.1f, 0.1f );
        } else if ( effects & EF_FLAG2 ) {
            CLG_FlagTrail( cent->lerp_origin, ent->origin, 115 );
            clgi.V_AddLight( ent->origin, 225, 0.1f, 0.1f, 1 );
        } else if ( effects & EF_TAGTRAIL ) {
            CLG_TagTrail( cent->lerp_origin, ent->origin, 220 );
            clgi.V_AddLight( ent->origin, 225, 1.0f, 1.0f, 0.0f );
        } else if ( effects & EF_TRACKERTRAIL ) {
            if ( effects & EF_TRACKER ) {
                float intensity = 50 + ( 500 * ( sin( clgi.client->time / 500.0f ) + 1.0f ) );
                clgi.V_AddLight( ent->origin, intensity, -1.0f, -1.0f, -1.0f );
            } else {
                CLG_Tracker_Shell( cent->lerp_origin );
                clgi.V_AddLight( ent->origin, 155, -1.0f, -1.0f, -1.0f );
            }
        } else if ( effects & EF_TRACKER ) {
            CLG_TrackerTrail( cent->lerp_origin, ent->origin, 0 );
            clgi.V_AddLight( ent->origin, 200, -1, -1, -1 );
        } else if ( effects & EF_GREENGIB ) {
            CLG_DiminishingTrail( cent->lerp_origin, ent->origin, cent, effects );
        } else if ( effects & EF_IONRIPPER ) {
            CLG_IonripperTrail( cent->lerp_origin, ent->origin );
            clgi.V_AddLight( ent->origin, 100, 1, 0.5f, 0.5f );
        } else if ( effects & EF_BLUEHYPERBLASTER ) {
            clgi.V_AddLight( ent->origin, 200, 0, 0, 1 );
        } else if ( effects & EF_PLASMA ) {
            if ( effects & EF_ANIM_ALLFAST ) {
                CLG_BlasterTrail( cent->lerp_origin, ent->origin );
            }
            clgi.V_AddLight( ent->origin, 130, 1, 0.5f, 0.5f );
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
    for ( int32_t pnum = 0; pnum < clgi.client->frame.numEntities; pnum++ ) {
        // Get the entity state index for the entity's newly received state.
        int32_t i = ( clgi.client->frame.firstEntity + pnum ) & PARSE_ENTITIES_MASK;
        // Get the pointer to the newly received entity's state.
        entity_state_t *s1 = &clgi.client->entityStates[ i ];

        // Get a pointer to the client  game entity that matches the state's number.
        centity_t *cent = &clg_entities[ s1->number ];

        // Setup the refresh entity ID to match that of the client game entity with the RESERVED_ENTITY_COUNT in mind.
        ent.id = cent->id + RESERVED_ENTITIY_COUNT;

        // Acquire the state's effects, and render effects.
        uint32_t effects = s1->effects;
        int32_t renderfx = s1->renderfx;

        /**
        *   Set refresh entity frame:
        **/
        CLG_PacketEntity_AnimateFrame( cent, &ent, s1, effects );
        /**
        *   Apply 'Shell' render effects based on various effects that are set:
        **/
        CLG_PacketEntity_ApplyShellEffects( effects, renderfx );
        /**
        *   Lerp entity origins:
        **/
        CLG_PacketEntity_LerpOrigin( cent, &ent, s1 );

        // Goto skip if gibs are disabled.
        if ( ( effects & EF_GIB ) && !cl_gibs->integer ) {
            goto skip;
        }

        // create a new entity

        // Tweak the color of beams
        if ( renderfx & RF_BEAM ) {
            // the four beam colors are encoded in 32 bits of skinnum (hack)
            ent.alpha = 0.30f;
            ent.skinnum = ( s1->skinnum >> ( ( irandom( 4 ) ) * 8 ) ) & 0xff;
            ent.model = 0;
        // Assume that this is thus not a beam, but an alias model entity instead:
        } else {
            CLG_PacketEntity_SetModelAndSkin( cent, &ent, s1, renderfx );
        }

        // Only used for black hole model right now, FIXME: do better
        if ( ( renderfx & RF_TRANSLUCENT ) && !( renderfx & RF_BEAM ) ) {
            ent.alpha = 0.70f;
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


        //int base_entity_flags = 0; // WID: C++20: Moved to the top of function.

        // Reset the base refresh entity flags.
        base_entity_flags = 0; // WID: C++20: Make sure to however, reset it to 0.

        // In case of the state belonging to the frame's viewed client number:
        if ( s1->number == clgi.client->frame.clientNum + 1 ) {
            // Add various effects.
            if ( effects & EF_FLAG1 ) {
                clgi.V_AddLight( ent.origin, 225, 1.0f, 0.1f, 0.1f );
            } else if ( effects & EF_FLAG2 ) {
                clgi.V_AddLight( ent.origin, 225, 0.1f, 0.1f, 1.0f );
            } else if ( effects & EF_TAGTRAIL ) {
                clgi.V_AddLight( ent.origin, 225, 1.0f, 1.0f, 0.0f );
            } else if ( effects & EF_TRACKERTRAIL ) {
                clgi.V_AddLight( ent.origin, 225, -1.0f, -1.0f, -1.0f );
            }

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
        *   BFG and Plasma Effects:
        **/
        if ( effects & EF_BFG ) {
            // Make BFG Effect entities 60% transparent.
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.30f;
        }
        if ( effects & EF_PLASMA ) {
            // Make Plasma Effect entities 60% transparent.
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.6f;
        }
        // Make Sphere Trans Effect entities 30% transparent, 60% if it is its trail.
        //if (effects & EF_SPHERETRANS) {
        //    ent.flags |= RF_TRANSLUCENT;
        //    if (effects & EF_TRACKERTRAIL)
        //        ent.alpha = 0.6f;
        //    else
        //        ent.alpha = 0.3f;
        //}

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

        if ( effects & EF_POWERSCREEN ) {
            ent.model = precache.cl_mod_powerscreen;
            ent.oldframe = 0;
            ent.frame = 0;
            ent.flags |= ( RF_TRANSLUCENT | RF_SHELL_GREEN );
            ent.alpha = 0.30f;
            clgi.V_AddEntity( &ent );
        }

        // Add automatic particle trails
        CLG_PacketEntity_AddTrailEffects( cent, &ent, s1, effects );

// 
    skip:
        VectorCopy( ent.origin, cent->lerp_origin );
    }
}

static int shell_effect_hack( void ) {
    centity_t *ent;
    int         flags = 0;

    ent = &clg_entities[ clgi.client->frame.clientNum + 1 ];//ent = &cl_entities[clgi.client->frame.clientNum + 1];
    if ( ent->serverframe != clgi.client->frame.number ) {
        return 0;
    }

    if ( !ent->current.modelindex ) {
        return 0;
    }

    if ( ent->current.effects & EF_PENT )
        flags |= RF_SHELL_RED;
    if ( ent->current.effects & EF_QUAD )
        flags |= RF_SHELL_BLUE;
    if ( ent->current.effects & EF_DOUBLE )
        flags |= RF_SHELL_DOUBLE;
    if ( ent->current.effects & EF_HALF_DAMAGE )
        flags |= RF_SHELL_HALF_DAM;

    return flags;
}