/********************************************************************
*
*
*	ClientGame: View Scene Management.
*
*
********************************************************************/
#include "clg_local.h"
#include "clg_effects.h"
#include "clg_entities.h"
#include "clg_local_entities.h"
#include "clg_packet_entities.h"
#include "clg_predict.h"
#include "clg_temp_entities.h"
#include "clg_view.h"



//=============
//
// development tools for weapons
//
qhandle_t gun;



/**
*
*
*   Test Model:
*
*
**/
//void CLG_AddTestModel( void ) {
//    static float frame = 0.f;
//    static int prevtime = 0;
////
//    if ( cl_testmodel_handle != -1 ) {
//        model_t *model = MOD_ForHandle( cl_testmodel_handle );
//
//        if ( model != NULL && model->meshes != NULL ) {
//            entity_t entity = { 0 };
//            entity.model = cl_testmodel_handle;
//            entity.id = RENTITIY_RESERVED_TESTMODEL;
//
//            VectorCopy( cl_testmodel_position, entity.origin );
//            VectorCopy( cl_testmodel_position, entity.oldorigin );
//
//            entity.alpha = cl_testalpha->value;
//            clamp( entity.alpha, 0.f, 1.f );
//            if ( entity.alpha < 1.f )
//                entity.flags |= RF_TRANSLUCENT;
//
//            int numframes = model->numframes;
//            if ( model->iqmData )
//                numframes = (int)model->iqmData->num_poses;
//
//            if ( numframes > 1 && prevtime != 0 ) {
//                const float millisecond = 1e-3f;
//
//                int timediff = cl.time - prevtime;
//                frame += (float)timediff * millisecond * max( cl_testfps->value, 0.f );
//
//                if ( frame >= (float)numframes || frame < 0.f )
//                    frame = 0.f;
//
//                float frac = frame - floorf( frame );
//
//                entity.oldframe = (int)frame;
//                entity.frame = entity.oldframe + 1;
//                entity.backlerp = 1.f - frac;
//            }
//
//            prevtime = clgi.client->time;
//
//            clgi.V_AddEntity( &entity );
//        }
//    }
//}

/**
*
*
*   Weapon View Model:
*
*
**/
static int32_t     gun_frame;
static qhandle_t   gun_model;

// Gun frame debugging functions
static void V_Gun_Next_f( void ) {
    gun_frame++;
    Com_Printf( "frame %i\n", gun_frame );
}

static void V_Gun_Prev_f( void ) {
    gun_frame--;
    if ( gun_frame < 0 )
        gun_frame = 0;
    Com_Printf( "frame %i\n", gun_frame );
}

static void V_Gun_Model_f( void ) {
    char    name[ MAX_QPATH ];

    if ( clgi.Cmd_Argc() != 2 ) {
        gun_model = 0;
        return;
    }

    Q_concat( name, sizeof( name ), "models/", clgi.Cmd_Argv( 1 ) );
    gun_model = clgi.R_RegisterModel( name );
}

// Model Debugging Cmds:
cmdreg_t clg_view_cmds[] = {
    { "gun_next", V_Gun_Next_f },
    { "gun_prev", V_Gun_Prev_f },
    { "gun_model", V_Gun_Model_f },
    { NULL, NULL }
};

/**
*   @brief
**/
static const int32_t shell_effect_hack( void ) {
    int32_t flags = 0;

    // Oh noes.
    if ( clgi.client->frame.clientNum == CLIENTNUM_NONE ) {
        return 0;
    }

    // Determine whether the entity was in the current frame or not.
    centity_t *ent = &clg_entities[ clgi.client->frame.clientNum + 1 ]; //ent = ENTITY_FOR_NUMBER( clgi.client->frame.clientNum + 1 );//ent = &cl_entities[clgi.client->frame.clientNum + 1];
    if ( ent->serverframe != clgi.client->frame.number ) {
        return 0;
    }

    // We need a model index to continue.
    if ( !ent->current.modelindex ) {
        return 0;
    }

    // Determine distinct shell effect.
    if ( ent->current.effects & EF_PENT ) {
        flags |= RF_SHELL_RED;
    }
    if ( ent->current.effects & EF_QUAD ) {
        flags |= RF_SHELL_BLUE;
    }
    if ( ent->current.effects & EF_DOUBLE ) {
        flags |= RF_SHELL_DOUBLE;
    }
    if ( ent->current.effects & EF_HALF_DAMAGE ) {
        flags |= RF_SHELL_HALF_DAM;
    }

    return flags;
}

/**
*   @brief  Adds the first person view its weapon model entity.
**/
static void CLG_AddViewWeapon( void ) {
    player_state_t *ps, *ops;
    entity_t    gun;        // view model
    int         i, shell_flags;

    // allow the gun to be completely removed
    if ( cl_player_model->integer == CL_PLAYER_MODEL_DISABLED ) {
        return;
    }

    if ( info_hand->integer == 2 ) {
        return;
    }

    // find states to interpolate between
    ps = &clgi.client->frame.ps;
    ops = &clgi.client->oldframe.ps;

    memset( &gun, 0, sizeof( gun ) );

    if ( gun_model ) {
        gun.model = gun_model;  // development tool
    } else {
        gun.model = clgi.client->model_draw[ ps->gunindex ];
    }
    if ( !gun.model ) {
        return;
    }

    gun.id = RENTITIY_RESERVED_GUN;

    // set up gun position
    for ( i = 0; i < 3; i++ ) {
        gun.origin[ i ] = clgi.client->refdef.vieworg[ i ] + ops->gunoffset[ i ] +
            clgi.client->lerpfrac * ( ps->gunoffset[ i ] - ops->gunoffset[ i ] );
        gun.angles[ i ] = clgi.client->refdef.viewangles[ i ] + LerpAngle( ops->gunangles[ i ],
            ps->gunangles[ i ], clgi.client->lerpfrac );
    }

    // Adjust the gun scale so that the gun doesn't intersect with walls.
    // The gun models are not exactly centered at the camera, so adjusting its scale makes them
    // shift on the screen a little when reasonable scale values are used. When extreme values are used,
    // such as 0.01, they move significantly - so we clamp the scale value to an expected range here.
    gun.scale = clgi.CVar_ClampValue( cl_gunscale, 0.1f, 1.0f );

    VectorMA( gun.origin, cl_gun_y->value * gun.scale, clgi.client->v_forward, gun.origin );
    VectorMA( gun.origin, cl_gun_x->value * gun.scale, clgi.client->v_right, gun.origin );
    VectorMA( gun.origin, cl_gun_z->value * gun.scale, clgi.client->v_up, gun.origin );

    VectorCopy( gun.origin, gun.oldorigin );      // don't lerp at all

    if ( gun_frame ) {
        gun.frame = gun_frame;  // development tool
        gun.oldframe = gun_frame;   // development tool
    } else {

        // WID: 40hz - Does proper gun lerping.
        // TODO: Add gunrate, and transfer it over the wire.
        if ( ops->gunindex != ps->gunindex ) { // just changed weapons, don't lerp from old
            game.viewWeapon.frame = game.viewWeapon.last_frame = ps->gunframe;
            game.viewWeapon.server_time = clgi.client->servertime;
        } else if ( game.viewWeapon.frame == -1 || game.viewWeapon.frame != ps->gunframe ) {
            game.viewWeapon.frame = ps->gunframe;
            game.viewWeapon.last_frame = ops->gunframe;
            game.viewWeapon.server_time = clgi.client->servertime;
        }

        const int32_t playerstate_gun_rate = ps->gunrate;
        const float gun_ms = 1.f / ( !playerstate_gun_rate ? 10 : playerstate_gun_rate ) * 1000.f;
        gun.backlerp = 1.f - ( ( clgi.client->time - ( (float)game.viewWeapon.server_time - clgi.client->sv_frametime ) ) / gun_ms );
        clamp( gun.backlerp, 0.0f, 1.f );
        gun.frame = game.viewWeapon.frame;
        gun.oldframe = game.viewWeapon.last_frame;
        // WID: 40hz

        //gun.frame = ps->gunframe;
        //if (gun.frame == 0) {
        //    gun.oldframe = 0;   // just changed weapons, don't lerp from old
        //} else {
        //    gun.oldframe = ops->gunframe;
        //    gun.backlerp = 1.0f - clgi.client->lerpfrac;
        //}
    }

    gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;

    if ( cl_gunalpha->value != 1 ) {
        gun.alpha = clgi.CVar_ClampValue( cl_gunalpha, 0.1f, 1.0f );
        gun.flags |= RF_TRANSLUCENT;
    }

    // add shell effect from player entity
    shell_flags = shell_effect_hack();

    // Shells are applied to the entity itself in rtx mode
    if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
        gun.flags |= shell_flags;
    }
    // Add gun entity.
    clgi.V_AddEntity( &gun );

    // Shells are applied to another separate entity in non-rtx mode
    if ( shell_flags && clgi.GetRefreshType() != REF_TYPE_VKPT ) {
        // Apply alpha to the shell entity.
        gun.alpha = 0.30f * cl_gunalpha->value;
        // Apply shell flags, and translucency.
        gun.flags |= shell_flags | RF_TRANSLUCENT;
        // Add gun shell entity.
        clgi.V_AddEntity( &gun );
    }
}


/**
*
*
*   View Type & Field of View:
* 
*
**/
/**
*   @brief  Calculates the client's field of view.
**/
const float PF_CalculateFieldOfView( const float fov_x, const float width, const float height ) {
    float    a;
    float    x;

    if ( fov_x <= 0 || fov_x > 179 ) {
        Com_Error( ERR_DROP, "%s: bad fov: %f", __func__, fov_x );
    }

    x = width / tan( fov_x * ( M_PI / 360 ) );

    a = atan( height / x );
    a = a * ( 360 / M_PI );

    return a;
}

/**
*   @brief
**/
static void CLG_SetupFirstPersonView( void ) {
    const float lerp = clgi.client->lerpfrac;

    // Add kick angles
    if ( cl_kickangles->integer ) {
        player_state_t *ps = &clgi.client->frame.ps;
        player_state_t *ops = &clgi.client->oldframe.ps;

        const Vector3 kickAngles = QM_Vector3LerpAngles( ops->kick_angles, ps->kick_angles, lerp );
        VectorAdd( clgi.client->refdef.viewangles, kickAngles, clgi.client->refdef.viewangles );
    }

    // Add the view weapon model.
    CLG_AddViewWeapon();

    // Inform client state we're not in third-person view.
    clgi.client->thirdPersonView = false;
}

/**
*   @brief
**/
static void CLG_SetupThirdPersionView( void ) {
    vec3_t focus;
    float fscale, rscale;
    float dist, angle, range;
    trace_t trace;
    static const vec3_t mins = { -4, -4, -4 }, maxs = { 4, 4, 4 };

    // if dead, set a nice view angle
    if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 ) {
        clgi.client->refdef.viewangles[ ROLL ] = 0;
        clgi.client->refdef.viewangles[ PITCH ] = 10;
    }

    VectorMA( clgi.client->refdef.vieworg, 512, clgi.client->v_forward, focus );

    clgi.client->refdef.vieworg[ 2 ] += 8;

    clgi.client->refdef.viewangles[ PITCH ] *= 0.5f;
    AngleVectors( clgi.client->refdef.viewangles, clgi.client->v_forward, clgi.client->v_right, clgi.client->v_up );

    angle = DEG2RAD( cl_thirdperson_angle->value );
    range = cl_thirdperson_range->value;
    fscale = cos( angle );
    rscale = sin( angle );
    VectorMA( clgi.client->refdef.vieworg, -range * fscale, clgi.client->v_forward, clgi.client->refdef.vieworg );
    VectorMA( clgi.client->refdef.vieworg, -range * rscale, clgi.client->v_right, clgi.client->refdef.vieworg );

    //CM_BoxTrace( &trace, clgi.client->playerEntityOrigin, clgi.client->refdef.vieworg,
    //    mins, maxs, clgi.client->bsp->nodes, MASK_SOLID );
    trace = clgi.Clip( clgi.client->playerEntityOrigin, mins, maxs, clgi.client->refdef.vieworg, nullptr, (contents_t)( MASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP ) );
    //trace = clgi.Trace( clgi.client->playerEntityOrigin, mins, maxs, clgi.client->refdef.vieworg, &clg_entities[ 1 ], (contents_t)( MASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP ));
    if ( trace.fraction != 1.0f ) {
        VectorCopy( trace.endpos, clgi.client->refdef.vieworg );
    }

    VectorSubtract( focus, clgi.client->refdef.vieworg, focus );
    dist = sqrtf( focus[ 0 ] * focus[ 0 ] + focus[ 1 ] * focus[ 1 ] );

    clgi.client->refdef.viewangles[ PITCH ] = -RAD2DEG( atan2( focus[ 2 ], dist ) );
    clgi.client->refdef.viewangles[ YAW ] -= cl_thirdperson_angle->value;

    clgi.client->thirdPersonView = true;
}

/**
*   @brief
**/
static void CLG_FinishViewValues( void ) {
    centity_t *ent;

    if ( cl_player_model->integer != CL_PLAYER_MODEL_THIRD_PERSON ) {
        goto first;
    }

    ent = &clg_entities[ clgi.client->frame.clientNum + 1 ]; //ENTITY_FOR_NUMBER( clgi.client->frame.clientNum + 1 );//ent = &cl_entities[clgi.client->frame.clientNum + 1];
    if ( ent->serverframe != clgi.client->frame.number ) {
        goto first;
    }

    // Need a model to display if we want to go third-person mode.
    if ( !ent->current.modelindex ) {
        goto first;
    }

    CLG_SetupThirdPersionView();
    return;

first:
    CLG_SetupFirstPersonView();
}

/**
*   @brief
**/
static inline const float lerp_client_fov( float ofov, float nfov, const float lerp ) {
    if ( clgi.IsDemoPlayback() ) {
        int fov = info_fov->integer;

        if ( fov < 1 ) {
            fov = 90;
        } else if ( fov > 160 ) {
            fov = 160;
        }

        if ( info_uf->integer & UF_LOCALFOV ) {
            return fov;
        }

        if ( !( info_uf->integer & UF_PLAYERFOV ) ) {
            if ( ofov >= 90 ) {
                ofov = fov;
            }
            if ( nfov >= 90 ) {
                nfov = fov;
            }
        }
    }

    return ofov + lerp * ( nfov - ofov );
}



/**
*
*
*   View Finalizing: Calculate vieworg, offsets, viewheight, screenblends, thirdperson origin, etc.
*
*
**/
/**
*   @return The predicted and smooth lerped viewheight for the current running prediction frame.
**/
const double CLG_SmoothedOutViewHeightChange() {
    static constexpr int32_t HEIGHT_CHANGE__TIME = 100;
    static constexpr double HEIGHT_CHANGE__BASE_1_FRAMETIME = ( 1. / HEIGHT_CHANGE__TIME );
    uint64_t timeDelta = HEIGHT_CHANGE__TIME - min( ( clgi.client->time - clgi.client->predictedState.time.height_changed ), HEIGHT_CHANGE__TIME );
    return clgi.client->predictedState.view.height[ 0 ] + (double)( clgi.client->predictedState.view.height[ 1 ] - clgi.client->predictedState.view.height[ 0 ] ) * timeDelta * HEIGHT_CHANGE__BASE_1_FRAMETIME;
}

/**
*   @brief  Smoothly transit the 'step' offset.
**/
static void CLG_SmoothStepOffset() {
    // TODO: In the future, when we got this moved into ClientGame, use PM_STEP_CHANGE_.. values from SharedGame.
    // TODO: Is this accurate?
    // PM_MAX_STEP_CHANGE_HEIGHT = 18
    // PM_MIN_STEP_CHANGE_HEIGHT = 4
    //
    // However, the original code was 127 * 0.125 = 15.875, which is a 2.125 difference to PM_MAX_STEP_CHANGE_HEIGHT
    //static constexpr float STEP_CHANGE_HEIGHT = PM_MAX_STEP_CHANGE_HEIGHT - PM_MIN_STEP_CHANGE_HEIGHT // This seems like what would be more accurate?
    static constexpr double STEP_CHANGE_HEIGHT = 15.f;//15.875;
    // Time in miliseconds to smooth the view for the step offset with.
    static constexpr int32_t STEP_CHANGE_TIME = 100;
    // Base 1 FrameTime.
    static constexpr double STEP_CHANGE_BASE_1_FRAMETIME = ( 1. / STEP_CHANGE_TIME );

    // Time delta.
    uint64_t delta = clgi.GetRealTime() - clgi.client->predictedState.time.step_changed;

    // Smooth out stair climbing.
    if ( fabsf( clgi.client->predictedState.view.step ) < STEP_CHANGE_HEIGHT ) {
        delta <<= 1; // Small steps.
    }

    // Adjust view org by step height change.
    if ( delta < STEP_CHANGE_TIME ) {
        clgi.client->refdef.vieworg[ 2 ] -= clgi.client->predictedState.view.step * ( STEP_CHANGE_TIME - delta ) * STEP_CHANGE_BASE_1_FRAMETIME;
    }
}

/**
*   @brief  Will lerp the screen blend colors between frames. If the frame was invalid, it'll 'hard-set' the values instead.
**/
void CLG_LerpScreenBlend( player_state_t *ops, player_state_t *ps, client_predicted_state_t *predictedState ) {
    // Interpolate blend colors if the last frame wasn't clear
    if ( !ops->screen_blend[ 3 ] ) {
        Vector4Copy( ps->screen_blend, clgi.client->refdef.screen_blend );
    } else {
        float blendfrac = ops->screen_blend[ 3 ] ? clgi.client->lerpfrac : 1;
        Vector4Lerp( ops->screen_blend, ps->screen_blend, blendfrac, clgi.client->refdef.screen_blend );
        //float damageblendfrac = ops->damage_blend[ 3 ] ? clgi.client->lerpfrac : 1;
        //Vector4Lerp( ops->damage_blend, ps->damage_blend, damageblendfrac, clgi.client->refdef.damage_blend );
    }

    // Mix in screen_blend from cgame pmove
    // FIXME: Should also be interpolated?...
    if ( predictedState->view.screen_blend.z > 0 ) {
        float a2 = clgi.client->refdef.screen_blend[ 3 ] + ( 1 - clgi.client->refdef.screen_blend[ 3 ] ) * predictedState->view.screen_blend.z; // new total alpha
        float a3 = clgi.client->refdef.screen_blend[ 3 ] / a2; // fraction of color from old

        LerpVector( predictedState->view.screen_blend, clgi.client->refdef.screen_blend, a3, clgi.client->refdef.screen_blend );
        clgi.client->refdef.screen_blend[ 3 ] = a2;
    }
}

void CLG_LerpDeltaAngles( player_state_t *ops, player_state_t *ps, const float lerpFrac ) {
    // Calculate delta angles between old and current player state.
    clgi.client->delta_angles[ 0 ] = AngleMod( LerpAngle( ops->pmove.delta_angles[ 0 ], ps->pmove.delta_angles[ 0 ], lerpFrac ) );
    clgi.client->delta_angles[ 1 ] = AngleMod( LerpAngle( ops->pmove.delta_angles[ 1 ], ps->pmove.delta_angles[ 1 ], lerpFrac ) );
    clgi.client->delta_angles[ 2 ] = AngleMod( LerpAngle( ops->pmove.delta_angles[ 2 ], ps->pmove.delta_angles[ 2 ], lerpFrac ) );
    //#endif
}

/**
*   @brief  Sets clgi.client->refdef view values and sound spatialization params.
*           Usually called from CL_PrepareViewEntities, but may be directly called from the main
*           loop if rendering is disabled but sound is running.
**/
void PF_CalculateViewValues( void ) {
    // Store the final view offset.
    Vector3 finalViewOffset = QM_Vector3Zero();

    // Get out of here if we had an invalid frame.
    if ( !clgi.client->frame.valid ) {
        return;
    }

    // Find states to interpolate between
    player_state_t *ps = &clgi.client->frame.ps;
    player_state_t *ops = &clgi.client->oldframe.ps;

    // Acquire lerp fraction to use for general lerping.
    const double lerpFrac = clgi.client->lerpfrac;

    // calculate the origin
    //if (!cls.demo.playback && cl_predict->integer && !(ps->pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION) ) {
    const int32_t usePrediction = PF_UsePrediction();
    if ( usePrediction && !( ps->pmove.pm_flags & PMF_NO_POSITIONAL_PREDICTION ) ) {
        // use predicted values
        const double backLerp = 1.0 - lerpFrac;
        // Take the prediction error into account.
        VectorMA( clgi.client->predictedState.view.origin, backLerp, clgi.client->predictedState.error, clgi.client->refdef.vieworg );
    } else {
        // Just use interpolated values.
        Vector3 viewOrg = QM_Vector3Lerp( ops->pmove.origin, ps->pmove.origin, lerpFrac );
        VectorCopy( viewOrg, clgi.client->refdef.vieworg );
    }

    // Smooth out step offset.
    CLG_SmoothStepOffset();

    // if not running a demo or on a locked frame, add the local angle movement
    if ( clgi.IsDemoPlayback() ) {
        LerpAngles( ops->viewangles, ps->viewangles, lerpFrac, clgi.client->refdef.viewangles );
    } else if ( ps->pmove.pm_type < PM_DEAD && !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {
        // use predicted values
        VectorCopy( clgi.client->predictedState.view.angles, clgi.client->refdef.viewangles );
    } else if ( ops->pmove.pm_type < PM_DEAD /*&& !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION )*/ ) {/*cls.serverProtocol > PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {*/
        // lerp from predicted angles, since enhanced servers
        // do not send viewangles each frame
        LerpAngles( clgi.client->predictedState.view.angles, ps->viewangles, lerpFrac, clgi.client->refdef.viewangles );
    } else {
        if ( !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {
            // Just use interpolated values.
            LerpAngles( ops->viewangles, ps->viewangles, lerpFrac, clgi.client->refdef.viewangles );
        } else {
            VectorCopy( ps->viewangles, clgi.client->refdef.viewangles );
        }
    }

    // Interpolate old and current player state delta angles.
    CLG_LerpDeltaAngles( ops, ps, lerpFrac );

    // Interpolate blend colors if the last frame wasn't clear.
    CLG_LerpScreenBlend( ops, ps, &clgi.client->predictedState );

    // Interpolate Field of View.
    clgi.client->fov_x = lerp_client_fov( ops->fov, ps->fov, lerpFrac );
    clgi.client->fov_y = PF_CalculateFieldOfView( clgi.client->fov_x, 4, 3 );

    // Lerp the view offset.
    LerpVector( ops->viewoffset, ps->viewoffset, lerpFrac, finalViewOffset );
    if ( clgi.client->frame.valid ) {
        finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
    } else {
        finalViewOffset = ops->viewoffset; //finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
    }
    // Smooth out the ducking view height change over 100ms
    finalViewOffset.z += CLG_SmoothedOutViewHeightChange();

    // Calculate the angle vectors for the current view angles.
    AngleVectors( clgi.client->refdef.viewangles, clgi.client->v_forward, clgi.client->v_right, clgi.client->v_up );

    // Copy the view origin and angles for the thirdperson(and also shadow casing) entity.
    VectorCopy( clgi.client->refdef.vieworg, clgi.client->playerEntityOrigin );
    VectorCopy( clgi.client->refdef.viewangles, clgi.client->playerEntityAngles );
    // Keep pitch in bounds.
    if ( clgi.client->playerEntityAngles[ PITCH ] > 180 ) {
        clgi.client->playerEntityAngles[ PITCH ] -= 360;
    }
    // Adjust pitch slightly.
    clgi.client->playerEntityAngles[ PITCH ] = clgi.client->playerEntityAngles[ PITCH ] / 3;

    // Add the resulting final viewOffset to the refdef view origin.
    VectorAdd( clgi.client->refdef.vieworg, finalViewOffset, clgi.client->refdef.vieworg );

    // Setup the new position for spatial audio recognition.
    clgi.S_SetupSpatialListener( clgi.client->refdef.vieworg, clgi.client->v_forward, clgi.client->v_right, clgi.client->v_up );
}



/**
*
*
*   View/Scene Entities:
*
*
**/
/**
*   @brief  
**/
void PF_ClearViewScene( void ) {
    //cl.viewScene.r_numdlights = 0;
    //cl.viewScene.r_numentities = 0;
    //cl.viewScene.r_numparticles = 0;
}

/**
*   @brief  Prepares the current frame's view scene for the refdef by
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void PF_PrepareViewEntites( void ) {
    // Calculate view and spatial audio listener origins.
    PF_CalculateViewValues();
    // Finish it off by determing third or first -person view, and the required thirdperson/firstperson view model.
    CLG_FinishViewValues();

    // Now calculate the client's local PVS.
    clgi.V_CalculateLocalPVS( clgi.client->refdef.vieworg );

    // Add all 'in-frame' received packet entities to the rendered view.
    CLG_AddPacketEntities();

    // Add all 'in-frame' local entities to the rendered view.
    CLG_AddLocalEntities();

    // Add special effects 'refresh entities'.
    CLG_AddTEnts();
    CLG_AddParticles();
    CLG_AddDLights();
    CLG_AddLightStyles();

    // Add in .md2/.md3/.iqm model 'debugger' entity.
    //CLG_AddTestModel();

    //! Add in the client-side flashlight.
    //if ( cl_flashlight->integer ) {
    //    //CLG_View_Flashlight();
    //}
}