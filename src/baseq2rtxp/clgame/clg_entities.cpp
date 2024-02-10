/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clg_local.h"

/**
*
*
*	Entity Utilities:
*
*
**/
#if USE_DEBUG
/**
*	@brief	For debugging problems when out - of - date entity origin is referenced
**/
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
*	@return		The entity bound to the client's view. 
*	@remarks	(Can be the one we're chasing, instead of the player himself.)
**/
centity_t *CLG_Self( void ) {
	int32_t index = clgi.client->clientNum;

	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] ) {
		index = clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS;
	}

	return &clg_entities[ index + 1 ];
}

/**
 * @return True if the specified entity is bound to the local client's view.
 */
const bool CLG_IsSelf( const centity_t *ent ) {
	if ( ent == clgi.client->clientEntity ) {
		return true;
	}
	if ( ent->current.modelindex == MODELINDEX_PLAYER ) {
		return true;
	}

	//if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] > 0 ) {
	//if ( ( ent->current.effects & EF_CORPSE ) == 0 ) {

	//	if ( ent->current.model1 == MODELINDEX_PLAYER ) {

	//		if ( ent->current.client == cgi.client->client_num ) {
	//			return true;
	//		}

	//		const int16_t chase = cgi.client->frame.ps.stats[ STAT_CHASE ] - CS_CLIENTS;

	//		if ( ent->current.client == chase ) {
	//			return true;
	//		}
	//	}
	//}

	return false;
}


/*
===============
CL_AddPacketEntities

===============
*/
#define RESERVED_ENTITIY_GUN 1
#define RESERVED_ENTITIY_TESTMODEL 2
#define RESERVED_ENTITIY_COUNT 3

static int adjust_shell_fx( int renderfx ) {
    // PMM - at this point, all of the shells have been handled
    // if we're in the rogue pack, set up the custom mixing, otherwise just
    // keep going
    //if ( !strcmp( fs_game->string, "rogue" ) ) {
    //    // all of the solo colors are fine.  we need to catch any of the combinations that look bad
    //    // (double & half) and turn them into the appropriate color, and make double/quad something special
    //    if ( renderfx & RF_SHELL_HALF_DAM ) {
    //        // ditch the half damage shell if any of red, blue, or double are on
    //        if ( renderfx & ( RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) )
    //            renderfx &= ~RF_SHELL_HALF_DAM;
    //    }

    //    if ( renderfx & RF_SHELL_DOUBLE ) {
    //        // lose the yellow shell if we have a red, blue, or green shell
    //        if ( renderfx & ( RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_GREEN ) )
    //            renderfx &= ~RF_SHELL_DOUBLE;
    //        // if we have a red shell, turn it to purple by adding blue
    //        if ( renderfx & RF_SHELL_RED )
    //            renderfx |= RF_SHELL_BLUE;
    //        // if we have a blue shell (and not a red shell), turn it to cyan by adding green
    //        else if ( renderfx & RF_SHELL_BLUE ) {
    //            // go to green if it's on already, otherwise do cyan (flash green)
    //            if ( renderfx & RF_SHELL_GREEN )
    //                renderfx &= ~RF_SHELL_BLUE;
    //            else
    //                renderfx |= RF_SHELL_GREEN;
    //        }
    //    }
    //}

    return renderfx;
}

/*
==========================================================================

DYNLIGHT ENTITY SUPPORT:

==========================================================================
*/
void CL_PacketEntity_AddSpotlight( centity_t *cent, entity_t *ent, entity_state_t *s1 ) {
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


void CLG_AddPacketEntities( void ) {
    int32_t base_entity_flags = 0;

    // bonus items rotate at a fixed rate
    const float autorotate = AngleMod( clgi.client->time * BASE_FRAMETIME_1000 );//AngleMod(clgi.client->time * 0.1f); // WID: 40hz: Adjusted.

    // brush models can auto animate their frames
    int64_t autoanim = 2 * clgi.client->time / 1000;

    entity_t ent = { };
    //memset( &ent, 0, sizeof( ent ) );

    for ( int32_t pnum = 0; pnum < clgi.client->frame.numEntities; pnum++ ) {
        int32_t i = ( clgi.client->frame.firstEntity + pnum ) & PARSE_ENTITIES_MASK;
        entity_state_t *s1 = &clgi.client->entityStates[ i ];

        centity_t *cent = &clg_entities[ s1->number ];//cent = &cl_entities[s1->number];
        ent.id = cent->id + RESERVED_ENTITIY_COUNT;

        uint32_t effects = s1->effects;
        int32_t renderfx = s1->renderfx;

        // set frame
        if ( effects & EF_ANIM01 )
            ent.frame = autoanim & 1;
        else if ( effects & EF_ANIM23 )
            ent.frame = 2 + ( autoanim & 1 );
        else if ( effects & EF_ANIM_ALL )
            ent.frame = autoanim;
        else if ( effects & EF_ANIM_ALLFAST )
            ent.frame = clgi.client->time / BASE_FRAMETIME; // WID: 40hz: Adjusted. clgi.client->time / 100;
        else
            ent.frame = s1->frame;

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

        ent.oldframe = cent->prev.frame;
        ent.backlerp = 1.0f - clgi.client->lerpfrac;

        // WID: 40hz - Does proper frame lerping for 10hz models.
                // TODO: Add a specific render flag for this perhaps? 
                // TODO: must only do this on alias models
                // Don't do this for 'world' model?
        //if ( ent.model != 0 && cent->last_frame != cent->current_frame ) {
        if ( !( ent.model & 0x80000000 ) && cent->last_frame != cent->current_frame ) {
            // Calculate back lerpfraction. (10hz.)
            ent.backlerp = 1.0f - ( ( clgi.client->time - ( (float)cent->frame_servertime - clgi.client->sv_frametime ) ) / 100.f );
            clamp( ent.backlerp, 0.0f, 1.0f );
            ent.frame = cent->current_frame;
            ent.oldframe = cent->last_frame;
        }
        // WID: 40hz

        if ( renderfx & RF_FRAMELERP ) {
            // step origin discretely, because the frames
            // do the animation properly
            VectorCopy( cent->current.origin, ent.origin );
            VectorCopy( cent->current.old_origin, ent.oldorigin );  // FIXME
        } else if ( renderfx & RF_BEAM ) {
            // interpolate start and end points for beams
            //LerpVector( cent->prev.origin, cent->current.origin,
            //    clgi.client->lerpfrac, ent.origin );
            //LerpVector( cent->prev.old_origin, cent->current.old_origin,
            //    clgi.client->lerpfrac, ent.oldorigin );
            Vector3 cent_origin = QM_Vector3Lerp( cent->prev.origin, cent->current.origin, clgi.client->lerpfrac );
            VectorCopy( cent_origin, ent.origin );
            Vector3 cent_old_origin = QM_Vector3Lerp( cent->prev.old_origin, cent->current.old_origin, clgi.client->lerpfrac );
            VectorCopy( cent_old_origin, ent.oldorigin );
        } else {
            if ( s1->number == clgi.client->frame.clientNum + 1 ) {
                // use predicted origin
                VectorCopy( clgi.client->playerEntityOrigin, ent.origin );
                VectorCopy( clgi.client->playerEntityOrigin, ent.oldorigin );
            } else {
                // interpolate origin
                //LerpVector(cent->prev.origin, cent->current.origin,
                //           clgi.client->lerpfrac, ent.origin);
                Vector3 cent_origin = QM_Vector3Lerp( cent->prev.origin, cent->current.origin, clgi.client->lerpfrac );
                VectorCopy( cent_origin, ent.origin );
                VectorCopy( ent.origin, ent.oldorigin );
            }
            //#if USE_FPS
            //            // run alias model animation
            //            if (cent->prev_frame != s1->frame) {
            //                int delta = clgi.client->time - cent->anim_start;
            //                float frac;
            //
            //                if (delta > BASE_FRAMETIME) {
            //                    cent->prev_frame = s1->frame;
            //                    frac = 1;
            //                } else if (delta > 0) {
            //                    frac = delta * BASE_1_FRAMETIME;
            //                } else {
            //                    frac = 0;
            //                }
            //
            //                ent.oldframe = cent->prev_frame;
            //                ent.backlerp = 1.0f - frac;
            //            }
            //#endif
        }

        // WID: RF_STAIR_STEP smooth interpolation:
        // TODO: Generalize STEP_ constexpr stuff.
        static constexpr int64_t STEP_TIME = 200; // 100ms.
        uint64_t stair_step_delta = clgi.GetRealTime() - ( cent->step_servertime - clgi.client->sv_frametime );
        // Smooth out stair step over 100ms.
        if ( stair_step_delta <= STEP_TIME ) {
            static constexpr float STEP_BASE_1_FRAMETIME = 1.0f / STEP_TIME; // 0.01f;

            // Smooth it out further for small steps.
            static constexpr float STEP_MAX_SMALL_STEP_SIZE = 18.f;
            if ( fabs( cent->step_height ) <= STEP_MAX_SMALL_STEP_SIZE ) {
                stair_step_delta <<= 1; // small steps
            }

            // Calculate step time.
            uint64_t stair_step_time = STEP_TIME - min( stair_step_delta, STEP_TIME );

            // Calculate lerped Z origin.
            //const float stair_step_lerp_z = cent->current.origin[ 2 ] + ( cent->prev.origin[ 2 ] - cent->current.origin[ 2 ] ) * stair_step_time * STEP_BASE_1_FRAMETIME;
            cent->current.origin[ 2 ] = QM_Lerp( cent->prev.origin[ 2 ], cent->current.origin[ 2 ], stair_step_time * STEP_BASE_1_FRAMETIME );

            // Assign to render entity.
            VectorCopy( cent->current.origin, ent.origin );
            VectorCopy( cent->current.origin, ent.oldorigin );
        }

        // Goto skip if gibs are disabled.
        if ( ( effects & EF_GIB ) && !cl_gibs->integer ) {
            goto skip;
        }

        // create a new entity

        // tweak the color of beams
        if ( renderfx & RF_BEAM ) {
            // the four beam colors are encoded in 32 bits of skinnum (hack)
            ent.alpha = 0.30f;
            ent.skinnum = ( s1->skinnum >> ( ( Q_rand() % 4 ) * 8 ) ) & 0xff;
            ent.model = 0;
        } else {
            // set skin
            if ( s1->modelindex == MODELINDEX_PLAYER ) {
                // use custom player skin
                ent.skinnum = 0;
                clientinfo_t *ci = &clgi.client->clientinfo[ s1->skinnum & 0xff ];
                ent.skin = ci->skin;
                ent.model = ci->model;
                if ( !ent.skin || !ent.model ) {
                    ent.skin = clgi.client->baseclientinfo.skin;
                    ent.model = clgi.client->baseclientinfo.model;
                    ci = &clgi.client->baseclientinfo;
                }
                if ( renderfx & RF_USE_DISGUISE ) {
                    char buffer[ MAX_QPATH ];

                    Q_concat( buffer, sizeof( buffer ), "players/", ci->model_name, "/disguise.pcx" );
                    ent.skin = clgi.R_RegisterSkin( buffer );
                }
            } else {
                ent.skinnum = s1->skinnum;
                ent.skin = 0;
                ent.model = clgi.client->model_draw[ s1->modelindex ];
                if ( ent.model == precache.cl_mod_laser || ent.model == precache.cl_mod_dmspot ) {
                    renderfx |= RF_NOSHADOW;
                }
            }
        }

        // allow skin override for remaster
        if ( renderfx & RF_CUSTOMSKIN && (unsigned)s1->skinnum < CS_IMAGES + MAX_IMAGES /* CS_MAX_IMAGES */ ) {
            ent.skin = clgi.client->image_precache[ s1->skinnum ];
            ent.skinnum = 0;
        }

        // only used for black hole model right now, FIXME: do better
        if ( ( renderfx & RF_TRANSLUCENT ) && !( renderfx & RF_BEAM ) ) {
            ent.alpha = 0.70f;
        }

        // render effects (fullbright, translucent, etc)
        if ( ( effects & EF_COLOR_SHELL ) ) {
            ent.flags = 0;  // renderfx go on color shell entity
        } else {
            ent.flags = renderfx;
        }

        // calculate angles
        if ( effects & EF_ROTATE ) {  // some bonus items auto-rotate
            ent.angles[ 0 ] = 0;
            ent.angles[ 1 ] = autorotate;
            ent.angles[ 2 ] = 0;
        } else if ( effects & EF_SPINNINGLIGHTS ) {
            vec3_t forward;
            vec3_t start;

            ent.angles[ 0 ] = 0;
            ent.angles[ 1 ] = AngleMod( clgi.client->time / 2 ) + s1->angles[ 1 ];
            ent.angles[ 2 ] = 180;

            AngleVectors( ent.angles, forward, NULL, NULL );
            VectorMA( ent.origin, 64, forward, start );
            clgi.V_AddLight( start, 100, 1, 0, 0 );
        } else if ( s1->number == clgi.client->frame.clientNum + 1 ) {
            VectorCopy( clgi.client->playerEntityAngles, ent.angles );      // use predicted angles
        } else { // interpolate angles
            LerpAngles( cent->prev.angles, cent->current.angles,
                clgi.client->lerpfrac, ent.angles );

            // mimic original ref_gl "leaning" bug (uuugly!)
            if ( s1->modelindex == MODELINDEX_PLAYER && cl_rollhack->integer ) {
                ent.angles[ ROLL ] = -ent.angles[ ROLL ];
            }
        }

        //int base_entity_flags = 0; // WID: C++20: Moved to the top of function.
        base_entity_flags = 0; // WID: C++20: Make sure to however, reset it to 0.

        if ( s1->number == clgi.client->frame.clientNum + 1 ) {
            if ( effects & EF_FLAG1 )
                clgi.V_AddLight( ent.origin, 225, 1.0f, 0.1f, 0.1f );
            else if ( effects & EF_FLAG2 )
                clgi.V_AddLight( ent.origin, 225, 0.1f, 0.1f, 1.0f );
            else if ( effects & EF_TAGTRAIL )
                clgi.V_AddLight( ent.origin, 225, 1.0f, 1.0f, 0.0f );
            else if ( effects & EF_TRACKERTRAIL )
                clgi.V_AddLight( ent.origin, 225, -1.0f, -1.0f, -1.0f );

            if ( !clgi.client->thirdPersonView ) {
                if ( clgi.GetRefreshType()  == REF_TYPE_VKPT )
                    base_entity_flags |= RF_VIEWERMODEL;    // only draw from mirrors
                else
                    goto skip;
            }

            // don't tilt the model - looks weird
            ent.angles[ 0 ] = 0.f;

            // offset the model back a bit to make the view point located in front of the head
            vec3_t angles = { 0.f, ent.angles[ 1 ], 0.f };
            vec3_t forward;
            AngleVectors( angles, forward, NULL, NULL );

            float offset = -15.f;
            VectorMA( ent.origin, offset, forward, ent.origin );
            VectorMA( ent.oldorigin, offset, forward, ent.oldorigin );
        }

        // Spotlight
        //if ( s1->entityType == ET_SPOTLIGHT ) {
        if ( s1->effects & EF_SPOTLIGHT ) {
            CL_PacketEntity_AddSpotlight( cent, &ent, s1 );
            return;
        }

        // if set to invisible, skip
        if ( !s1->modelindex ) {
            goto skip;
        }

        if ( effects & EF_BFG ) {
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.30f;
        }

        if ( effects & EF_PLASMA ) {
            ent.flags |= RF_TRANSLUCENT;
            ent.alpha = 0.6f;
        }

        //if (effects & EF_SPHERETRANS) {
        //    ent.flags |= RF_TRANSLUCENT;
        //    if (effects & EF_TRACKERTRAIL)
        //        ent.alpha = 0.6f;
        //    else
        //        ent.alpha = 0.3f;
        //}

        ent.flags |= base_entity_flags;

        // in rtx mode, the base entity has the renderfx for shells
        if ( ( effects & EF_COLOR_SHELL ) && clgi.GetRefreshType() == REF_TYPE_VKPT ) {
            renderfx = adjust_shell_fx( renderfx );
            ent.flags |= renderfx;
        }

        // add to refresh list
        clgi.V_AddEntity( &ent );

        //// add dlights for flares
        //model_t *model;
        //if ( ent.model && !( ent.model & 0x80000000 ) &&
        //    ( model = MOD_ForHandle( ent.model ) ) ) {
        //    if ( model->model_class == MCLASS_FLARE ) {
        //        float phase = (float)clgi.client->time * 0.03f + (float)ent.id;
        //        float anim = sinf( phase );

        //        float offset = anim * 1.5f + 5.f;
        //        float brightness = anim * 0.2f + 0.8f;

        //        vec3_t origin;
        //        VectorCopy( ent.origin, origin );
        //        origin[ 2 ] += offset;

        //        V_AddSphereLight( origin, 500.f, 1.6f * brightness, 1.0f * brightness, 0.2f * brightness, 5.f );
        //    }
        //}

        // color shells generate a separate entity for the main model
        if ( ( effects & EF_COLOR_SHELL ) && clgi.GetRefreshType() != REF_TYPE_VKPT ) {
            renderfx = adjust_shell_fx( renderfx );
            ent.flags = renderfx | RF_TRANSLUCENT | base_entity_flags;
            ent.alpha = 0.30f;
            clgi.V_AddEntity( &ent );
        }

        ent.skin = 0;       // never use a custom skin on others
        ent.skinnum = 0;
        ent.flags = base_entity_flags;
        ent.alpha = 0;

        // duplicate for linked models
        if ( s1->modelindex2 ) {
            if ( s1->modelindex2 == MODELINDEX_PLAYER ) {
                // custom weapon
                clientinfo_t *ci = &clgi.client->clientinfo[ s1->skinnum & 0xff ];
                i = ( s1->skinnum >> 8 ); // 0 is default weapon model
                if ( i < 0 || i > clgi.client->numWeaponModels - 1 )
                    i = 0;
                ent.model = ci->weaponmodel[ i ];
                if ( !ent.model ) {
                    if ( i != 0 )
                        ent.model = ci->weaponmodel[ 0 ];
                    if ( !ent.model )
                        ent.model = clgi.client->baseclientinfo.weaponmodel[ 0 ];
                }
            } else {
                ent.model = clgi.client->model_draw[ s1->modelindex2 ];
            }

            // PMM - check for the defender sphere shell .. make it translucent
            if ( !Q_strcasecmp( clgi.client->configstrings[ CS_MODELS + ( s1->modelindex2 ) ], "models/items/shell/tris.md2" ) ) {
                ent.alpha = 0.32f;
                ent.flags = RF_TRANSLUCENT;
            }

            if ( ( effects & EF_COLOR_SHELL ) && clgi.GetRefreshType() == REF_TYPE_VKPT ) {
                ent.flags |= renderfx;
            }

            clgi.V_AddEntity( &ent );

            //PGM - make sure these get reset.
            ent.flags = base_entity_flags;
            ent.alpha = 0;
        }

        if ( s1->modelindex3 ) {
            ent.model = clgi.client->model_draw[ s1->modelindex3 ];
            clgi.V_AddEntity( &ent );
        }

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

        // add automatic particle trails
        if ( effects & ~EF_ROTATE ) {
            if ( effects & EF_ROCKET ) {
                if ( !( cl_disable_particles->integer & NOPART_ROCKET_TRAIL ) ) {
                    CLG_RocketTrail( cent->lerp_origin, ent.origin, cent );
                }
                if ( cl_dlight_hacks->integer & DLHACK_ROCKET_COLOR )
                    clgi.V_AddLight( ent.origin, 200, 1, 0.23f, 0 );
                else
                    clgi.V_AddLight( ent.origin, 200, 0.6f, 0.4f, 0.12f );
            } else if ( effects & EF_BLASTER ) {
                if ( effects & EF_TRACKER ) {
                    CLG_BlasterTrail2( cent->lerp_origin, ent.origin );
                    clgi.V_AddLight( ent.origin, 200, 0.1f, 0.4f, 0.12f );
                } else {
                    CLG_BlasterTrail( cent->lerp_origin, ent.origin );
                    clgi.V_AddLight( ent.origin, 200, 0.6f, 0.4f, 0.12f );
                }
            } else if ( effects & EF_HYPERBLASTER ) {
                if ( effects & EF_TRACKER )
                    clgi.V_AddLight( ent.origin, 200, 0.1f, 0.4f, 0.12f );
                else
                    clgi.V_AddLight( ent.origin, 200, 0.6f, 0.4f, 0.12f );
            } else if ( effects & EF_GIB ) {
                CLG_DiminishingTrail( cent->lerp_origin, ent.origin, cent, effects );
            } else if ( effects & EF_GRENADE ) {
                if ( !( cl_disable_particles->integer & NOPART_GRENADE_TRAIL ) ) {
                    CLG_DiminishingTrail( cent->lerp_origin, ent.origin, cent, effects );
                }
            } else if ( effects & EF_FLIES ) {
                CLG_FlyEffect( cent, ent.origin );
            } else if ( effects & EF_BFG ) {
                if ( effects & EF_ANIM_ALLFAST ) {
                    CLG_BfgParticles( &ent );
                    i = 100;
                } else {
                    static const int bfg_lightramp[ 6 ] = { 300, 400, 600, 300, 150, 75 };
                    i = s1->frame;
                    clamp( i, 0, 5 );
                    i = bfg_lightramp[ i ];
                }
                const vec3_t nvgreen = { 0.2716f, 0.5795f, 0.04615f };
                clgi.V_AddSphereLight( ent.origin, i, nvgreen[ 0 ], nvgreen[ 1 ], nvgreen[ 2 ], 20.f );
            } else if ( effects & EF_TRAP ) {
                ent.origin[ 2 ] += 32;
                CLG_TrapParticles( cent, ent.origin );
                i = ( Q_rand() % 100 ) + 100;
                clgi.V_AddLight( ent.origin, i, 1, 0.8f, 0.1f );
            } else if ( effects & EF_FLAG1 ) {
                CLG_FlagTrail( cent->lerp_origin, ent.origin, 242 );
                clgi.V_AddLight( ent.origin, 225, 1, 0.1f, 0.1f );
            } else if ( effects & EF_FLAG2 ) {
                CLG_FlagTrail( cent->lerp_origin, ent.origin, 115 );
                clgi.V_AddLight( ent.origin, 225, 0.1f, 0.1f, 1 );
            } else if ( effects & EF_TAGTRAIL ) {
                CLG_TagTrail( cent->lerp_origin, ent.origin, 220 );
                clgi.V_AddLight( ent.origin, 225, 1.0f, 1.0f, 0.0f );
            } else if ( effects & EF_TRACKERTRAIL ) {
                if ( effects & EF_TRACKER ) {
                    float intensity = 50 + ( 500 * ( sin( clgi.client->time / 500.0f ) + 1.0f ) );
                    clgi.V_AddLight( ent.origin, intensity, -1.0f, -1.0f, -1.0f );
                } else {
                    CLG_Tracker_Shell( cent->lerp_origin );
                    clgi.V_AddLight( ent.origin, 155, -1.0f, -1.0f, -1.0f );
                }
            } else if ( effects & EF_TRACKER ) {
                CLG_TrackerTrail( cent->lerp_origin, ent.origin, 0 );
                clgi.V_AddLight( ent.origin, 200, -1, -1, -1 );
            } else if ( effects & EF_GREENGIB ) {
                CLG_DiminishingTrail( cent->lerp_origin, ent.origin, cent, effects );
            } else if ( effects & EF_IONRIPPER ) {
                CLG_IonripperTrail( cent->lerp_origin, ent.origin );
                clgi.V_AddLight( ent.origin, 100, 1, 0.5f, 0.5f );
            } else if ( effects & EF_BLUEHYPERBLASTER ) {
                clgi.V_AddLight( ent.origin, 200, 0, 0, 1 );
            } else if ( effects & EF_PLASMA ) {
                if ( effects & EF_ANIM_ALLFAST ) {
                    CLG_BlasterTrail( cent->lerp_origin, ent.origin );
                }
                clgi.V_AddLight( ent.origin, 130, 1, 0.5f, 0.5f );
            }
        }

    skip:
        VectorCopy( ent.origin, cent->lerp_origin );
    }
}

static int shell_effect_hack( void ) {
    centity_t *ent;
    int         flags = 0;

    ent = &clg_entities[ clgi.client->frame.clientNum + 1 ];//ent = &cl_entities[clgi.client->frame.clientNum + 1];
    if ( ent->serverframe != clgi.client->frame.number )
        return 0;

    if ( !ent->current.modelindex )
        return 0;

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