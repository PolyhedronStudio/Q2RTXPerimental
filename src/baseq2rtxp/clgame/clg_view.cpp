/********************************************************************
*
*
*	ClientGame: View Scene Management.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_main.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_local_entities.h"
#include "clgame/clg_packet_entities.h"
#include "clgame/clg_predict.h"
#include "clgame/clg_temp_entities.h"
#include "clgame/clg_view.h"

#include "sharedgame/sg_entity_effects.h"



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
//    if ( cl_testmodel_handle != -1 ) {
//        const model_t *model = clgi.R_GetModelDataForHandle( cl_testmodel_handle );
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
static const int32_t weapon_shell_effect( void ) {
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
    // For godmode.
    if ( ent->current.effects & EF_COLOR_SHELL ) {
        flags |= ent->current.renderfx; //( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE );
    }

    return flags;
}

/**
*   
**/
static void CLG_ApplyViewWeaponDrag( player_state_t *ops, player_state_t *ps, entity_t *weaponModelEntity ) {
    // Last facing direction.
    static Vector3 lastFacingAngles = QM_Vector3Zero();
    // Actual lag we allow to stay behind.
    static constexpr float maxViewModelLag = 0.15f;

    //Calculate the difference between current and last facing forward angles.
    Vector3 difference = clgi.client->v_forward - lastFacingAngles;

    // Actual speed to move at.
    constexpr float constSpeed = 1.5;
    float speed = constSpeed;

    // When the yaw is rotating too fast, it'll get choppy and "lag" behind. Interpolate to neatly catch up,
    // gives a more realism effect to go along with it.
    const float distance = QM_Vector3Length( difference );
    if ( distance > maxViewModelLag ) {
        speed *= distance / maxViewModelLag;
    }

    //lastFacingAngles = QM_Vector3MultiplyAdd( lastFacingAngles, 1, difference );
    lastFacingAngles = QM_Vector3MultiplyAdd( lastFacingAngles, speed * clgi.GetFrameTime(), difference );
    lastFacingAngles = QM_Vector3Normalize( lastFacingAngles );

    difference = QM_Vector3Negate( difference );
    Vector3 gunOrigin = weaponModelEntity->origin;
    Vector3 draggedGunOrigin = QM_Vector3MultiplyAdd( gunOrigin, constSpeed, difference );

    // Calculate Pitch.
    float pitch = QM_AngleMod( weaponModelEntity->angles[ PITCH ] );
    if ( pitch > 180.0f ) {
        pitch -= 360.0f;
    } else if ( pitch < -180.0f ) {
        pitch += 360.0f;
    }

    // Now apply to our origin.
    draggedGunOrigin = QM_Vector3MultiplyAdd( draggedGunOrigin, -pitch * 0.035f, clgi.client->v_forward );
    draggedGunOrigin = QM_Vector3MultiplyAdd( draggedGunOrigin, -pitch * 0.03f, clgi.client->v_right );
    draggedGunOrigin = QM_Vector3MultiplyAdd( draggedGunOrigin, -pitch * 0.02f, clgi.client->v_up );

    // Copy back into refresh entity vec3_t origin.
    VectorCopy( draggedGunOrigin, weaponModelEntity->origin );

    // Debug...
    clgi.Print( PRINT_DEVELOPER, "%s: ps->gunorgin[%f, %f, %f], draggedGunOrigin[ %f, %f, %f] \n",
        __func__, gunOrigin[ 0 ], gunOrigin[ 1 ], gunOrigin[ 2 ], draggedGunOrigin[ 0 ], draggedGunOrigin[ 1 ], draggedGunOrigin[ 2 ] );
}

/**
*   @brief  Takes care of applying the proper animation frame based on time.
**/
static void CLG_AnimateViewWeapon( entity_t *refreshEntity, const int32_t firstFrame, const int32_t lastFrame ) {
    // Backup the previously 'current' frame as its last frame.
    game.viewWeapon.last_frame = game.viewWeapon.frame;

    // Calculate the actual current frame for the moment in time of the active animation.
    int32_t frameForTime = -1;
    double lerpFraction = SG_AnimationFrameForTime( &frameForTime,
        //QMTime::FromMilliseconds( clgi.GetRealTime() ), QMTime::FromMilliseconds( game.viewWeapon.real_time ),
        QMTime::FromMilliseconds( clgi.client->extrapolatedTime ), QMTime::FromMilliseconds( game.viewWeapon.server_time ),
        BASE_FRAMETIME,
        firstFrame, lastFrame,
        1, false
    );

    // Animation is running:
    if ( frameForTime != -1 || lerpFraction < 1.0 ) {
        // Apply animation to gun model refresh entity.
        game.viewWeapon.frame = refreshEntity->frame;
        refreshEntity->frame = frameForTime;
        refreshEntity->oldframe = ( refreshEntity->frame > firstFrame && refreshEntity->frame <= lastFrame ? refreshEntity->frame - 1 : game.viewWeapon.last_frame );
        // Enforce lerp of 0.0 to ensure that the first frame does not 'bug out'.
        if ( refreshEntity->frame == firstFrame ) {
            refreshEntity->backlerp = 0.0;
            //clgi.Print( PRINT_NOTICE, "%s: refreshEntity->backlerp = 0.0; [refreshEntity->frame(%i), refreshEntity->oldframe(%i), firstFrame(%i), lastFrame(%i) ]\n", __func__, refreshEntity->frame, refreshEntity->oldframe, firstFrame, lastFrame );
        // Enforce lerp of 1.0 if the calculated frame is equal or exceeds the last one.
        } else if ( refreshEntity->frame == lastFrame ) {
            refreshEntity->backlerp = 1.0;
            //clgi.Print( PRINT_NOTICE, "%s: refreshEntity->backlerp = 1.0; [refreshEntity->frame(%i), refreshEntity->oldframe(%i), firstFrame(%i), lastFrame(%i) ]\n", __func__, refreshEntity->frame, refreshEntity->oldframe, firstFrame, lastFrame );
        // Otherwise just subtract the resulting lerpFraction.
        } else {
            refreshEntity->backlerp = 1.0 - lerpFraction;
            //clgi.Print( PRINT_NOTICE, "%s: refreshEntity->backlerp = 1.0 - lerpFraction; [refreshEntity->frame(%i), refreshEntity->oldframe(%i), firstFrame(%i), lastFrame(%i) ]\n", __func__, refreshEntity->frame, refreshEntity->oldframe, firstFrame, lastFrame );
        }
        // Clamp just to be sure.
        refreshEntity->backlerp = std::clamp( refreshEntity->backlerp, 0.f, 1.f );
        // Reached the end of the animation:
    } else {
        //clgi.Print( PRINT_NOTICE, "%s: frameForTime != -1; [refreshEntity->frame(%i), refreshEntity->oldframe(%i), firstFrame(%i), lastFrame(%i) ]\n", __func__, refreshEntity->frame, refreshEntity->oldframe, firstFrame, lastFrame  );
        // Otherwise, oldframe now equals the current(end) frame.
        refreshEntity->oldframe = refreshEntity->frame = lastFrame;
        // No more lerping.
        refreshEntity->backlerp = 1.0;
    }
}
/**
*   @brief  Adds the first person view its weapon model entity.
**/
static void CLG_AddViewWeapon( void ) {
    // view model
    static entity_t gun = {};        
    int32_t shell_flags = 0;

    // allow the gun to be completely removed
    if ( cl_player_model->integer == CL_PLAYER_MODEL_DISABLED
        || cl_player_model->integer == CL_PLAYER_MODEL_THIRD_PERSON ) {
            return;
    }

    if ( info_hand->integer == 2 ) {
        return;
    }

    // find states to interpolate between
    player_state_t *ps = &clgi.client->frame.ps;
    player_state_t *ops = &clgi.client->oldframe.ps;

    memset( &gun, 0, sizeof( gun ) );
    if ( gun_model ) {
        gun.model = gun_model;  // development tool
    } else {
        gun.model = clgi.client->model_draw[ ps->gun.modelIndex ];
    }
    if ( !ps->gun.modelIndex || !gun.model ) {
        return;
    }

    // Get a model_t pointer for the gun model so we can operate with its animation data.
    const model_t *weaponModel = clgi.R_GetModelDataForHandle( gun.model );
    // If no data to it, exit.
    if ( !weaponModel ) {
        return;
    }
    // Get IQM data.
    const skm_model_t *skmData = weaponModel->skmData;
    if ( !skmData ) {
        return;
    }


    gun.id = RENTITIY_RESERVED_GUN;

    // Calculate the lerped gun position.
    //Vector3 gunOrigin = clgi.client->refdef.vieworg + QM_Vector3Lerp( ops->gunoffset, ps->gunoffset, clgi.client->lerpfrac );
    //// Calculate the lerped gun angles.
    //Vector3 gunAngles = clgi.client->refdef.viewangles + QM_Vector3LerpAngles( ops->gunangles, ps->gunangles, clgi.client->lerpfrac );
    Vector3 gunOrigin = clgi.client->refdef.vieworg + QM_Vector3Lerp( game.predictedState.lastPs.gunoffset, game.predictedState.currentPs.gunoffset, clgi.client->lerpfrac );
    // Calculate the lerped gun angles.
    Vector3 gunAngles = clgi.client->refdef.viewangles + QM_Vector3LerpAngles( game.predictedState.lastPs.gunangles, game.predictedState.currentPs.gunangles, clgi.client->lerpfrac );
    // Copy the calculated gun position and angles into vec3_t's from refresh 'entity_t'.
    VectorCopy( gunOrigin, gun.origin );
    VectorCopy( gunAngles, gun.angles );

    // Calculate view weapon drag to prevent origin jumps when yaw changes rapidly.
    //if ( !sv_paused->integer ) {
    //    CLG_ApplyViewWeaponDrag( &game.predictedState.lastPs, &game.predictedState.currentPs, &gun );
    //}
    //gunOrigin = QM_Vector3Lerp( gunOrigin, gun.origin, clgi.client->lerpfrac );

    // Adjust the gun scale so that the gun doesn't intersect with walls.
    // The gun models are not exactly centered at the camera, so adjusting its scale makes them
    // shift on the screen a little when reasonable scale values are used. When extreme values are used,
    // such as 0.01, they move significantly - so we clamp the scale value to an expected range here.
    gun.scale = clgi.CVar_ClampValue( cl_gunscale, 0.1f, 1.0f );

    VectorMA( gun.origin, cl_gun_y->value * gun.scale, clgi.client->v_forward, gun.origin );
    VectorMA( gun.origin, cl_gun_x->value * gun.scale, clgi.client->v_right, gun.origin );
    VectorMA( gun.origin, cl_gun_z->value * gun.scale, clgi.client->v_up, gun.origin );

    // Don't lerp at all.
    VectorCopy( gun.origin, gun.oldorigin );

    if ( gun_frame ) {
        gun.frame = gun_frame;  // development tool
        gun.oldframe = gun_frame;   // development tool
    } else {
        // Detect whether animation restarted, or changed. (Toggle Bit helps determining the difference.)
        bool animationIDChanged = ps->gun.animationID != ops->gun.animationID;

        // Acquire the actual animationIDs.
        const int32_t animationID = ( ps->gun.animationID & ~GUN_ANIMATION_TOGGLE_BIT );
        const int32_t oldAnimationID = ( ops->gun.animationID & ~GUN_ANIMATION_TOGGLE_BIT );
        // If the ID is invalid, resort to zero valued defaults.
        if ( ( animationID < 0 || animationID >= skmData->num_animations ) ) {
            gun.frame = gun.oldframe = 0;
            gun.backlerp = 0.0;
            return;
        }
        // Get IQM Animation.
        const iqm_anim_t *skmAnimation = &skmData->animations[ animationID ];
        if ( !skmAnimation ) {
            gun.frame = gun.oldframe = 0;
            gun.backlerp = 0.0;
            return;
        }

        // Animation frames.
        const int32_t firstFrame = skmAnimation->first_frame;
        const int32_t lastFrame = skmAnimation->first_frame + skmAnimation->num_frames;

        // If the animationID differed, or had its toggle bit flipped, reinitialize the animation.
        if ( animationIDChanged ) {
            // Check the time to prevent this from firing multiple times in a single game world frame.
            if ( game.viewWeapon.server_time < clgi.client->servertime ) {
                // Local real time of animation change.
                game.viewWeapon.real_time = clgi.GetRealTime();
                // Server time of animation change.
                game.viewWeapon.server_time = clgi.client->servertime/* - BASE_FRAMETIME*/;
                // Reset animation frame.
                game.viewWeapon.frame = firstFrame;
                // Debug output.
                #if 0
                clgi.Print( PRINT_NOTICE, "%s: Animation Restarted(#%i), firstFrame(%i), lastFrame(%i), server_time(%llu)\n", __func__, animationID, firstFrame, lastFrame, game.viewWeapon.server_time );
                #endif
            }
        }

        // When a model switch has occurred, make sure to set our model to the first frame
        // regardless of the actual time determined frame.
        if ( ops->gun.modelIndex != ps->gun.modelIndex ) {
            // Apply first frame.
            game.viewWeapon.frame = game.viewWeapon.last_frame = gun.oldframe = gun.frame = firstFrame;
            // Ensure backlerp is zeroed.
            gun.backlerp = 0.0f;
        // Process Weapon Animation:
        } else {
            CLG_AnimateViewWeapon( &gun, firstFrame, lastFrame );
        }
    }

    // APply gun model flags.
    gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;

    if ( cl_gunalpha->value != 1 ) {
        gun.alpha = clgi.CVar_ClampValue( cl_gunalpha, 0.1f, 1.0f );
        gun.flags |= RF_TRANSLUCENT;
    }

    // add shell effect from player entity
    shell_flags = weapon_shell_effect();

    // Shells are applied to the entity itself in rtx mode
    if ( clgi.GetRefreshType() == REF_TYPE_VKPT ) {
        gun.flags |= shell_flags;
    }
    // Add gun entity.
    clgi.V_AddEntity( &gun );

// WID: Keeps code somewhat cleaner since we aren't using(nor not supporting per se), OpenGL.
#if 0
    // Shells are applied to another separate entity in non-rtx mode
    if ( shell_flags && clgi.GetRefreshType() != REF_TYPE_VKPT ) {
        // Apply alpha to the shell entity.
        gun.alpha = 0.30f * cl_gunalpha->value;
        // Apply shell flags, and translucency.
        gun.flags |= shell_flags | RF_TRANSLUCENT;
        // Add gun shell entity.
        clgi.V_AddEntity( &gun );
    }
#endif
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
    if ( fov_x <= 0 || fov_x > 179 ) {
        Com_Error( ERR_DROP, "%s: bad fov: %f", __func__, fov_x );
    }

    // WID: Quake 3 Method:
    const float x = width / tan( fov_x / 360 * M_PI );
    float fov_y = atan2( height, x );
    fov_y = fov_y * 360 / M_PI;

    return fov_y;

    // Q2RTX/Q2PRO Method:
    //const float x = width / tan( fov_x * ( M_PI / 360 ) );

    //float a = atan( height / x );
    //a = a * ( 360 / M_PI );
    // 
    //return a;   
}

/**
*   @brief  Calculate the bob cycle and apply bob angles as well as a view offset.
**/
static void CLG_CycleViewBob( player_state_t *ps ) {
    // Calculate base bob data.
    level.viewBob.cycle = ( ps->bobCycle & 128 ) >> 7;
    level.viewBob.fracSin = fabs( sin( (double)( ps->bobCycle & 127 ) / 127.0 * M_PI ) );
    level.viewBob.xySpeed = ps->xySpeed;
        
#if 0
    // calculate speed and cycle to be used for all cyclic walking effects
    level.viewBob.xySpeed = ps->xySpeed; // sqrt( cg.predictedPlayerState.pmove.velocity[ 0 ] * cg.predictedPlayerState.pmove.velocity[ 0 ] + cg.predictedPlayerState.pmove.velocity[ 1 ] * cg.predictedPlayerState.pmove.velocity[ 1 ] );

    level.viewBob.bobScale = 0;
    if ( level.viewBob.xySpeed < 0.1 ) {
        level.viewBob.move = 0;
        level.viewBob.oldTime = 0;  // start at beginning of cycle again
    } else if ( ps->pmove.pm_flags & PMF_ON_GROUND )/*if ( cg_gunbob->integer ) */ {
        if ( level.viewBob.xySpeed > 210 ) {
            level.viewBob.move = clgi.frame_time_ms / 400.f;
        } else if ( level.viewBob.xySpeed > 100 ) {
            level.viewBob.move = clgi.frame_time_ms / 800.f;
        } else {
            level.viewBob.move = clgi.frame_time_ms / 1600.f;
       }
    } else {
        level.viewBob.move = 0;
    }

    //level.viewBob.move = clgi.frame_time_ms * level.viewBob.bobScale * 0.001f;
    level.viewBob.time = ( level.viewBob.oldTime += level.viewBob.move );
    //level.viewBob.timeRun = level.viewBob.time;
    level.viewBob.cycle = (int64_t)level.viewBob.time;
    level.viewBob.cycle_run = (int64_t)level.viewBob.time;
    level.viewBob.fracSin = fabs( sin( level.viewBob.time * M_PI ) );
#endif
}

/**
*   @brief  Calculates the gun offset as well as the gun angles based on the bobCycle.
**/
void CLG_CalculateViewWeaponOffset( player_state_t *ops, player_state_t *ps ) {
    int     i;
    double   delta;

    // Gun angles from bobbing
    ps->gunangles[ ROLL ] = level.viewBob.xySpeed * level.viewBob.fracSin * 0.005;
    ps->gunangles[ YAW ] = level.viewBob.xySpeed * level.viewBob.fracSin * 0.01;
    if ( level.viewBob.cycle & 1 ) {
        ps->gunangles[ ROLL ] = -ps->gunangles[ ROLL ];
        ps->gunangles[ YAW ] = -ps->gunangles[ YAW ];
    }

    ps->gunangles[ PITCH ] = level.viewBob.xySpeed * level.viewBob.fracSin * 0.005;
    #if 1
    // Gun angles from delta movement
    for ( i = 0; i < 3; i++ ) {
        delta = QM_AngleMod( ops->viewangles[ i ] - ps->viewangles[ i ] );
        if ( delta > 180. ) {
            delta -= 360.;
        }
        if ( delta < -180. ) {
            delta += 360.;
        }
        delta = std::clamp( delta, -45., 45. );

        if ( i == YAW ) {
            ps->gunangles[ ROLL ] += 0.1 * delta;
        }
        ps->gunangles[ i ] += 0.2 * delta;

        #if 0
        float reduction_factor = delta ? 0.05f : 0.15f;

        if ( delta > 0 )
            delta = std::max( 0., delta - clgi.frame_time_ms * reduction_factor );
        else if ( delta < 0 )
            delta = std::min( 0., delta + clgi.frame_time_ms * reduction_factor );
        #endif
    }
    #else
    static Vector3 slowAngles = {};
    Vector3 viewAnglesDelta = {};
    // Gun angles from delta movement
    for ( i = 0; i < 3; i++ ) {
        viewAnglesDelta[ i ] = QM_AngleMod(ops->viewangles[i] - ps->viewangles[i]);
    }
    slowAngles += viewAnglesDelta;

    for ( i = 0; i < 3; i++ ) {
        float &delta = slowAngles[ i ];

        if ( !delta )
            continue;

        if ( delta > 180 ) {
            delta -= 360;
        }
        if ( delta < -180 ) {
            delta += 360;
        }
        if ( delta > 45 ) {
            delta = 45;
        }
        if ( delta < -45 ) {
            delta = -45;
        }
        //clamp( delta, -45, 45 );

        if ( i == YAW ) {
            ps->gunangles[ ROLL ] += ( 0.1f * delta ) * 0.5f;
        }
        ps->gunangles[ i ] += ( 0.2f * delta ) * 0.5f;
        float reduction_factor = viewAnglesDelta[ i ] ? 0.05f : 0.15f;

        if ( delta > 0 )
            delta = std::max( 0.f, delta - clgi.frame_time_ms * reduction_factor );
        else if ( delta < 0 )
            delta = std::min( 0.f, delta + clgi.frame_time_ms * reduction_factor );
    }
    #endif
    // gun height
    VectorClear( ps->gunoffset );
    //  ent->ps->gunorigin[2] += bob;

    // gun_x / gun_y / gun_z are development tools
    for ( int32_t i = 0; i < 3; i++ ) {
        ps->gunoffset[ i ] += clgi.client->v_forward[ i ] * ( cl_gun_y->value );
        ps->gunoffset[ i ] += clgi.client->v_right[ i ] * cl_gun_x->value;
        ps->gunoffset[ i ] += clgi.client->v_up[ i ] * ( -cl_gun_z->value );
    }
}

/**
*   @brief  Calculates additional offsets based on the bobCycle for both 
*           viewOrigin and viewAngles(thus, additional client side kickangles).
**/
static void CLG_CalculateViewOffset( player_state_t *ops, player_state_t *ps, const float lerpFraction ) {
    //clgi.Print( PRINT_DEVELOPER, "%s: cycle(%i), fracSin(%f), xySpeed(%f)\n", __func__, ps->bobCycle, level.viewBob.fracSin, level.viewBob.xySpeed );

    //run_pitch = gi.cvar( "run_pitch", "0.002", 0 );
    //run_roll = gi.cvar( "run_roll", "0.005", 0 );
    //bob_up = gi.cvar( "bob_up", "0.005", 0 );
    //bob_pitch = gi.cvar( "bob_pitch", "0.002", 0 );
    //bob_roll = gi.cvar( "bob_roll", "0.002", 0 );

    // TODO: Turn into cvars.
    const float clg_run_pitch = cl_run_pitch->value;
    const float clg_run_roll = cl_run_roll->value;
    const float clg_bob_up = cl_bob_up->value;
    const float clg_bob_pitch = cl_bob_pitch->value;
    const float clg_bob_roll = cl_bob_roll->value;
    // TODO: Make vector, assign result later?
    //float *viewAngles = clgi.client->refdef.viewangles;
    Vector3 viewAngles = QM_Vector3Zero();

    // Lerp the velocity so it doesn't just "snap" back to nothing when we suddenly are standing idle.
    Vector3 velocityLerp = QM_Vector3Lerp( ops->pmove.velocity, ps->pmove.velocity, lerpFraction );

    // Add angles based on velocity.
    float delta = QM_Vector3DotProduct( velocityLerp, clgi.client->v_forward );
    viewAngles[ PITCH ] += delta * clg_run_pitch;/*cg_runpitch.value*/;

    delta = QM_Vector3DotProduct( velocityLerp, clgi.client->v_right );
    viewAngles[ ROLL ] += delta * clg_run_roll;/*cg_runroll.value*/;

    // Add angles based on bob.
    // WID: make sure the bob is visible even at low speeds
    //----
    // Now is just done in pmove itself.
    const float speed = level.viewBob.xySpeed;
    // Pitch:
    float pitchDelta = level.viewBob.fracSin * clg_bob_pitch /*cg_bobpitch.value */* speed;
    if ( ps->pmove.pm_flags & PMF_DUCKED ) {
        // Crouching:
        pitchDelta *= 3;
    }
    pitchDelta = std::min( pitchDelta, 1.2f );
    viewAngles[ PITCH ] += pitchDelta;
    // Roll:
    float rollDelta = level.viewBob.fracSin * clg_bob_roll /*cg_bobroll.value */* speed;
    if ( ps->pmove.pm_flags & PMF_DUCKED ) {
        // Crouching accentuates roll:
        rollDelta *= 3;
    }
    rollDelta = std::min( rollDelta, 1.2f );
    if ( level.viewBob.cycle & 1 ) {
        rollDelta = -rollDelta;
    }
    viewAngles[ ROLL ] += rollDelta;

    // Add bob height
    float bobHeight = level.viewBob.fracSin * level.viewBob.xySpeed * clg_bob_up;/*cg_bobup.value*/;
    if ( bobHeight > 6 ) {
        bobHeight = 6;
    }
    clgi.client->refdef.vieworg[ 2 ] += bobHeight;

    // Clamp angles.
    viewAngles = QM_Vector3Clamp( viewAngles,
        {
            -31.f,
            -31.f,
            -31.f,
        },
        {
            31.f,
            31.f,
            31.f,
        }
    );
    VectorAdd( clgi.client->refdef.viewangles, viewAngles, clgi.client->refdef.viewangles );
}

/**
*   @brief  Setup a first person scene view.
**/
static void CLG_SetupFirstPersonView( void ) {
    // Server received frames Player States:
    player_state_t *serverFramePlayerState = &clgi.client->frame.ps;
    player_state_t *lastServerFramePlayerState = &clgi.client->oldframe.ps;

    // Client predicting and predicted frames Player States:
    client_predicted_state_t *predictedState = &game.predictedState;
    player_state_t *predictingPlayerState = &predictedState->currentPs; //( PF_UsePrediction() ? &predictedState->currentPs : serverFramePlayerState );
    player_state_t *lastPredictingPlayerState = &predictedState->lastPs; //( PF_UsePrediction() ? &predictedState->lastPs : lastServerFramePlayerState );

    // Lerp fraction.
    const float lerp = clgi.client->lerpfrac;

    // Cycle the view bob on our predicted state.
    //CLG_CycleViewBob( predictingPlayerState );

    // WID: TODO: This requires proper player state damage summing and 'wiring' as well as proper
    // player event predicting.
    // Apply all the damage taken this frame
    //CLG_DamageFeedback( ent );
    
    // Determine the view offsets for the current predicting player state.
    CLG_CalculateViewOffset( lastPredictingPlayerState, predictingPlayerState, lerp );

    // Determine the gun origin and angle offsets.based on the bobCycles of the predicted player states.
    CLG_CalculateViewWeaponOffset( &predictedState->lastPs, &predictedState->currentPs );

    // Add server sided frame kick angles.
    if ( cl_kickangles->integer ) {
        const Vector3 kickAngles = QM_Vector3LerpAngles( lastServerFramePlayerState->kick_angles, serverFramePlayerState->kick_angles, lerp );
        VectorAdd( clgi.client->refdef.viewangles, kickAngles, clgi.client->refdef.viewangles );
    }

    // Calculate bob cycle on the current predicting player state.    
    //CLG_CycleViewBob( currentPredictingPlayerState );
    //CLG_CycleViewBob( predictingPlayerState );

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
    cm_trace_t trace;
    static const vec3_t mins = { -4, -4, -4 }, maxs = { 4, 4, 4 };

    // if dead, set a nice view angle
    if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 ) {
        clgi.client->refdef.viewangles[ ROLL ] = 0;
        clgi.client->refdef.viewangles[ PITCH ] = 10;
        clgi.client->refdef.viewangles[ YAW ] = clgi.client->frame.ps.stats[ STAT_KILLER_YAW ];
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
    //    mins, maxs, clgi.client->bsp->nodes, CM_CONTENTMASK_SOLID );
    // When clipping we 
    //trace = clgi.Clip( clgi.client->playerEntityOrigin, mins, maxs, clgi.client->refdef.vieworg, nullptr, (cm_contents_t)( CM_CONTENTMASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP ) );
    trace = clgi.Trace( 
        clgi.client->playerEntityOrigin, 
        mins, maxs, 
        clgi.client->refdef.vieworg, 
        clgi.client->clientEntity/*&clg_entities[ 1 ]*/, 
        CM_CONTENTMASK_SOLID//(cm_contents_t)( CM_CONTENTMASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP )
    );
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
const double CLG_SmoothViewHeight() {
    //! Miliseconds for smoothing out over.
    static constexpr int32_t HEIGHT_CHANGE_TIME = 100;
    //! Base 1 Frametime.
    static constexpr double HEIGHT_CHANGE_BASE_1_FRAMETIME = ( 1. / HEIGHT_CHANGE_TIME );
    //! Determine delta time.
    int64_t timeDelta = HEIGHT_CHANGE_TIME - std::min<int64_t>( ( clgi.client->time - game.predictedState.transition.view.timeHeightChanged ), HEIGHT_CHANGE_TIME );
    //! Return the frame's adjustment for viewHeight which is added on top of the final vieworigin + viweoffset.
    return game.predictedState.transition.view.height[ 0 ] + (double)( game.predictedState.transition.view.height[ 1 ] - game.predictedState.transition.view.height[ 0 ] ) * timeDelta * HEIGHT_CHANGE_BASE_1_FRAMETIME;
}

/**
*   @brief  Smoothly transit the 'step' offset.
**/
static void CLG_SmoothStepOffset() {
    // The original code was short integer coordinate based and had the following 'formula':
    // 127 * 0.125 = 15.875, which is a 2.125 difference to PM_MAX_STEP_CHANGE_HEIGHT(18) resulting in 15.875 as STEP_SMALL_HEIGHT
    // What is considered to be a 'small' step.
    static constexpr double STEP_SMALL_HEIGHT = PM_MAX_STEP_SIZE - PM_MIN_STEP_SIZE;
    //static constexpr double STEP_SMALL_HEIGHT = 15.f; //15.875;
    // Time in miliseconds to smooth the view for the step offset with.
    static constexpr int32_t STEP_CHANGE_TIME = 100;
    // Base 1 FrameTime.
    static constexpr double STEP_CHANGE_BASE_1_FRAMETIME = ( 1. / STEP_CHANGE_TIME );

    // Time delta.
    uint64_t delta = ( clgi.GetRealTime() - game.predictedState.transition.step.timeChanged );
    // Smooth out stair climbing.
    if ( fabsf( game.predictedState.transition.step.height ) < STEP_SMALL_HEIGHT ) {
        delta <<= 1; // Small steps.
    }
    // Adjust view org by step height change.
    if ( delta < STEP_CHANGE_TIME ) {
        clgi.client->refdef.vieworg[ 2 ] -= game.predictedState.transition.step.height * ( STEP_CHANGE_TIME - delta ) * STEP_CHANGE_BASE_1_FRAMETIME;
    }
}

/**
*   @brief  Lerp the view angles if desired.
**/
static void CLG_LerpViewAngles( player_state_t *ops, player_state_t *ps, client_predicted_state_t *predictedState, const double lerpFraction ) {
    // Predicted player state.
    player_state_t *pps = &predictedState->currentPs;

    // Lerp from ops to ps in demo playbacks.
    if ( clgi.IsDemoPlayback() ) {
        const Vector3 lerpedAngles = QM_Vector3LerpAngles( ops->viewangles, ps->viewangles, lerpFraction );
        VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );
        //clgi.Print( PRINT_DEVELOPER, "%s: returned from first if #1\n", __func__ );
    // Use 'Predicted State' if we're alive.
    } else if ( ps->pmove.pm_type < PM_DEAD && !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION ) ) {
        // use predicted values
        const Vector3 predictedAngles = pps->viewangles;
        VectorCopy( predictedAngles, clgi.client->refdef.viewangles );
    // Else if the old state was PM_DEAD, lerp from predicted state(pps) to playerstate(ps).
    } else if ( ops->pmove.pm_type < PM_DEAD /*&& !( ps->pmove.pm_flags & PMF_NO_ANGULAR_PREDICTION )*/ ) {/*cls.serverProtocol > PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {*/
        // lerp from predicted angles, since enhanced servers
        // do not send viewangles each frame
        const Vector3 lerpedAngles = QM_Vector3LerpAngles( pps->viewangles, ps->viewangles, lerpFraction );
        VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );
    // Under any other circumstances, just use lerped angles from ops to ps.
    } else {
        const Vector3 lerpedAngles = QM_Vector3LerpAngles( ops->viewangles, ps->viewangles, lerpFraction );
        VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );
    }
}

/**
*   @brief  Smooth lerp the old and current player state delta angles.
**/
static void CLG_LerpDeltaAngles( player_state_t *ops, player_state_t *ps, const float lerpFrac ) {
    // Calculate delta angles between old and current player state.
    clgi.client->delta_angles = QM_Vector3AngleMod( QM_Vector3LerpAngles( ops->pmove.delta_angles, ps->pmove.delta_angles, lerpFrac ) );
}


/**
*   @brief  Lerp the client's POV range.
**/
static void CLG_LerpPointOfView( player_state_t *ops, player_state_t *ps, const float lerpFrac ) {
    // Amount of MS to lerp FOV over.
    static constexpr QMTime FOV_EASE_DURATION = 225_ms;
    static QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

    /**
    *   Determine whether the FOV changed, whether it is zoomed(lower or higher than previous FOV), and
    *   store the old FOV as well as the realtime of change.
    **/
    static QMEaseState fovEaseState;
    //! Resort to cvar default value.
    static double oldFOV = info_fov->value;
    //! If fov changed, determine whether to start an ease in or out transition.
    if ( ops->fov != ps->fov ) {
        if ( ops->fov > ps->fov/* && povLerp.state = FOV_LERP_STATE_IN */) {
            oldFOV = ops->fov;
            fovEaseState = QMEaseState::new_ease_state( realTime, FOV_EASE_DURATION );
        } else {
            oldFOV = ops->fov;
            fovEaseState = QMEaseState::new_ease_state( realTime, FOV_EASE_DURATION );
        }
    }
    /**
    *   Determine the lerp fraction and use cubic ease in/out for the FOV to lerp by.
    * 
    *   We clamp to avoid discrepancies.
    **/
    if ( !CLG_IsGameplayPaused() ) {
        realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );
    }
    // Ease In:
    static double easeLerpFactor = 0.;
    if ( fovEaseState.mode == fovEaseState.QM_EASE_STATE_TYPE_IN ) {
        easeLerpFactor = fovEaseState.EaseIn( realTime, QM_CubicEaseIn );
    // Ease Out:
    } else {
        easeLerpFactor = fovEaseState.EaseOut( realTime, QM_CubicEaseOut );
    }

    /**
    *   Calculate appropriate fov for use.
    **/
    clgi.client->fov_x = lerp_client_fov( oldFOV, ps->fov, easeLerpFactor );
    clgi.client->fov_y = PF_CalculateFieldOfView( clgi.client->fov_x, 4, 3 );
}
/**
*   @brief  Lerp the client's viewOffset.
**/
static void CLG_LerpViewOffset( player_state_t *ops, player_state_t *ps, const float lerpFrac, Vector3 &finalViewOffset ) {
    if ( clgi.client->frame.valid ) {
        finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
    } else {
        finalViewOffset = ops->viewoffset; //finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
    }
}

/**
*   @brief  Will lerp the screen blend colors between frames. If the frame was invalid, it'll 'hard-set' the values instead.
**/
static void CLG_LerpScreenBlend( player_state_t *ops, player_state_t *ps, client_predicted_state_t *predictedState ) {
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
    if ( predictedState->currentPs.screen_blend[ 3 ] > 0 ) {
        float a2 = clgi.client->refdef.screen_blend[ 3 ] + ( 1 - clgi.client->refdef.screen_blend[ 3 ] ) * predictedState->currentPs.screen_blend[ 3 ]; // new total alpha
        float a3 = clgi.client->refdef.screen_blend[ 3 ] / a2; // fraction of color from old

        LerpVector( predictedState->currentPs.screen_blend, clgi.client->refdef.screen_blend, a3, clgi.client->refdef.screen_blend );
        clgi.client->refdef.screen_blend[ 3 ] = a2;
    }
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
        // Lerp the error.
        const Vector3 errorLerp = QM_Vector3Scale( game.predictedState.error, backLerp );

        // Take the prediction error into account.
        //VectorMA( game.predictedState.currentPs.pmove.origin, backLerp, game.predictedState.error, clgi.client->refdef.vieworg );
		VectorAdd( game.predictedState.currentPs.pmove.origin, errorLerp, clgi.client->refdef.vieworg );
    } else {
        // Just use interpolated values.
        Vector3 viewOrg = QM_Vector3Lerp( ops->pmove.origin, ps->pmove.origin, lerpFrac );
        VectorCopy( viewOrg, clgi.client->refdef.vieworg );
        
        // WID: NOTE: If things break, look for this here.
        // WID: This should fix demos or cl_nopredict
        //VectorCopy( clgi.client->refdef.vieworg, game.predictedState.currentPs.pmove.origin );
        //VectorCopy( game.predictedState.currentPs.pmove.origin, clgi.client->refdef.vieworg );
        game.predictedState.currentPs.pmove.origin = clgi.client->refdef.vieworg;
        #if 0
        clgi.client->refdef.vieworg[2] -= ops->pmove.origin.z;
        #endif
    }

    // Smooth out step offset.
    CLG_SmoothStepOffset();

    #if 1
    // use predicted values
    const double backLerp = lerpFrac;
    // Lerp View Angles.
    CLG_LerpViewAngles( ops, ps, &game.predictedState, backLerp );
    // Interpolate old and current player state delta angles.
    CLG_LerpDeltaAngles( ops, ps, lerpFrac );
    // Interpolate blend colors if the last frame wasn't clear.
    CLG_LerpScreenBlend( ops, ps, &game.predictedState );
    // Interpolate Field of View.
    CLG_LerpPointOfView( ops, ps, backLerp );
    // Lerp the view offset.
    CLG_LerpViewOffset( ops, ps, backLerp, finalViewOffset );
    // Smooth out the ducking view height change over 100ms
    finalViewOffset.z += CLG_SmoothViewHeight();
    #else
    // Lerp View Angles.
    CLG_LerpViewAngles( ops, ps, &game.predictedState, lerpFrac );
    // Interpolate old and current player state delta angles.
    CLG_LerpDeltaAngles( ops, ps, lerpFrac );
    // Interpolate blend colors if the last frame wasn't clear.
    CLG_LerpScreenBlend( ops, ps, &game.predictedState );
    // Interpolate Field of View.
    CLG_LerpPointOfView( ops, ps, lerpFrac );
    // Lerp the view offset.
    CLG_LerpViewOffset( ops, ps, lerpFrac, finalViewOffset );
    // Smooth out the ducking view height change over 100ms
    finalViewOffset.z += CLG_SmoothViewHeight();
    #endif

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
    clgi.client->predictedFrame = clgi.client->frame;
    clgi.client->predictedFrame.ps = game.predictedState.currentPs;

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
void PF_PrepareViewEntities( void ) {
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
    CLG_TemporaryEntities_Add();
    CLG_AddParticles();
    CLG_AddDLights();
    CLG_AddLightStyles();

    // Add the view weapon model.
    CLG_AddViewWeapon();

    // Add in .md2/.md3/.iqm model 'debugger' entity.
    //CLG_AddTestModel();

    //! Add in the client-side flashlight.
    //if ( cl_flashlight->integer ) {
    //    //CLG_View_Flashlight();
    //}
}

/**
*	@brief	Returns the predictedState based player view render definition flags.
**/
const refdef_flags_t PF_GetViewRenderDefinitionFlags( void ) {
    return game.predictedState.currentPs.rdflags;
}