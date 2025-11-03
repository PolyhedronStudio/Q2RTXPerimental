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

#include "sharedgame/sg_entity_flags.h"


//! Enables strafe affecting view weapon roll and pitch angles.
#define ENABLE_STRAFE_VIEWMODEL_ANGLES 1


//=============
//
// development tools for weapons
//
qhandle_t gun;



/**
*
*
*
*   Weapon View Model:
*
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
    if ( ent->current.entityFlags & EF_PENT ) {
        flags |= RF_SHELL_RED;
    }
    if ( ent->current.entityFlags & EF_QUAD ) {
        flags |= RF_SHELL_BLUE;
    }
    if ( ent->current.entityFlags & EF_DOUBLE ) {
        flags |= RF_SHELL_DOUBLE;
    }
    if ( ent->current.entityFlags & EF_HALF_DAMAGE ) {
        flags |= RF_SHELL_HALF_DAM;
    }
    // For godmode.
    if ( ent->current.entityFlags & EF_COLOR_SHELL ) {
        flags |= ent->current.renderfx; //( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE );
    }

    return flags;
}


/**
*   @brief  Calculates the gun view origin offset, based on the bobcycle and its sinfrac2.
*           After that, it lerps the gun offset to the target position based on whether the player is moving or not.
* 	@note   Will lerp back to center position when not moving or secondary firing.
**/
void CLG_ViewWeapon_CalculateOffset( player_state_t *ops, player_state_t *ps, const double lerpFrac ) {
    // Static variables to track gun offset lerping when idle and secondary fire
    static Vector3 targetGunOffset = QM_Vector3Zero();
    static Vector3 currentGunOffset = QM_Vector3Zero();
    static bool wasMoving = false;
    static bool wasSecondaryFiring = false;

    static constexpr double IDLE_LERP_SPEED = 5.0; // Speed of lerp back to idle position (higher = faster).
    static constexpr double MOVEMENT_LERP_SPEED = 8.0; // Speed of lerp back to movement (higher = faster).
    static constexpr double SECONDARY_FIRE_LERP_SPEED = 6.0; // Speed of lerp for secondary fire transition (higher = faster).

    // Check if player is moving
    const bool isMoving = ( ps->xySpeed >= 0.1 || ps->xyzSpeed >= 0.1 );

    // Check if secondary fire is being held (pistol aiming)
    const bool isSecondaryFiring = ( game.predictedState.cmd.cmd.buttons & BUTTON_SECONDARY_FIRE ) != 0;

    /**
    *
    *
    *   View Weapon `Bob` Offset Calculation:
    *
    *
    **/
    #if 0
    // Calculate the normal bobbing gun offset
    static double xRandomizer = drandom( 0.f, 0.075f );
    static double yRandomizer = drandom( 0.f, 0.185f );

    Vector3 bobbingGunOffset = {
        // We use random values here to prevent repetitive continuous motions.
        ( -xRandomizer * (double)level.viewBob.fracSin2 ),
        ( -yRandomizer * (double)level.viewBob.fracSin2 ),
        -.25
    };

    if ( level.viewBob.cycle & 1 ) {
        bobbingGunOffset.x = -bobbingGunOffset.x;
        xRandomizer = drandom( 0.f, 0.075f );
        yRandomizer = drandom( 0.f, 0.185f );
    }
    #else
    // Bobbing gun origin offset.
    Vector3 bobbingGunOffset = {
        ( -.075f * (float)level.viewBob.fracSin2 ),
        ( -.185f * (float)level.viewBob.fracSin2 ),
        -.25f
    };
	// Change the direction of the bobbing based on the cycle.
    if ( level.viewBob.cycle & 1 ) {
        bobbingGunOffset.x = -bobbingGunOffset.x;
    }
    #endif

    // Handle secondary fire state transitions.
    if ( isSecondaryFiring ) {
        if ( !wasSecondaryFiring ) {
            wasSecondaryFiring = true;
        }

        Vector3 baseOffset = bobbingGunOffset;
        // If the player is moving, use default bobbingGunOffset.
        if ( isMoving ) {
            wasMoving = true;
            // Otherwise, if the player is not moving, we want to adjust the gun offset to be more pronounced.
        } else {
            if ( wasMoving ) {
                wasMoving = false;
            }
            baseOffset = { 0.0f, 0.0f, -.25f };
        }
        // Determine target gun offset origin based on secondary fire state.
        targetGunOffset = { baseOffset.x, baseOffset.y, 0.0f };
        // Lerp it over time to create a smooth transition effect.
        const double deltaTime = clgi.GetFrameTime();
        const double lerpFactor = std::min( 1.0, SECONDARY_FIRE_LERP_SPEED * deltaTime );
        currentGunOffset = QM_Vector3Lerp( currentGunOffset, targetGunOffset, lerpFactor );
        // Was secondary firing:
    } else {
        if ( wasSecondaryFiring ) {
            wasSecondaryFiring = false;
        }

        // If the player is moving, use default bobbingGunOffset.
        if ( isMoving ) {
            if ( !wasMoving ) {
                wasMoving = true;
            }
            // Determine target gun offset origin based on secondary fire state.
            targetGunOffset = bobbingGunOffset;
            // Lerp it over time to create a smooth transition effect.
            const double deltaTime = clgi.GetFrameTime();
            const double lerpFactor = std::min( 1.0, MOVEMENT_LERP_SPEED * deltaTime );
            currentGunOffset = QM_Vector3Lerp( currentGunOffset, targetGunOffset, lerpFactor );
            // Lerp the offset back to idle position when not moving or secondary firing.
        } else {
            if ( wasMoving ) {
                wasMoving = false;
            }
            // Determine target gun offset origin based on secondary fire state.
            targetGunOffset = { 0.0f, 0.0f, -.25f };
            // Lerp it over time to create a smooth transition effect.
            const double deltaTime = clgi.GetFrameTime();
            const double lerpFactor = std::min( 1.0, IDLE_LERP_SPEED * deltaTime );
            currentGunOffset = QM_Vector3Lerp( currentGunOffset, targetGunOffset, lerpFactor );

            if ( QM_Vector3Length( currentGunOffset - targetGunOffset ) < 0.001 ) {
                currentGunOffset = targetGunOffset;
            }
        }
    }

    // Apply the calculated gun offset
    Vector3 gunOrigin = QM_Vector3Zero();
    for ( int32_t i = 0; i < 3; i++ ) {
        gunOrigin[ i ] += clgi.client->v_forward[ i ] * currentGunOffset.y;
        gunOrigin[ i ] += clgi.client->v_right[ i ] * currentGunOffset.x;
        gunOrigin[ 2 ] += clgi.client->v_up[ i ] * currentGunOffset.z;
    }
    // Copy into the final player state gunoffset.
    VectorCopy( gunOrigin, ps->gunoffset );
}

/**
*   @brief  Calculates the gun view angles by lerping between the old and new angles, adding a
*           swing-like delay effect based on view movement.
**/
#define DEBUG_VIEWWEAPON_ANGLES 1
void CLG_ViewWeapon_CalculateAngles( player_state_t *ops, player_state_t *ps, const double lerpFrac ) {
    #ifndef DEBUG_VIEWWEAPON_ANGLES
    // Swing properties.
    static constexpr double VIEW_SWING_RESPONSIVENESS = 14.0; // How quickly gun responds to view changes (higher = faster).
    static constexpr double VIEW_SWING_RECOVERY = 3.0; // How quickly gun returns to center (higher = faster).
    static constexpr double VIEW_SWING_INTENSITY = 8.0; // How much the gun swings (higher = more swing).
    static constexpr double MAX_SWING_ANGLE = 12.5; // Maximum swing angle in degrees.
    static constexpr double SWING_DAMPING = 0.75; // Damping factor for momentum (0.0-1.0, higher = less damping).
    // Input decay velocty properties.
    static constexpr double MIN_INPUT_THRESHOLD = 0.05; // Minimum input to consider as movement.
    static constexpr double VELOCITY_DECAY = 0.82; // How quickly velocity decays when no input (0.0-1.0).
        #else
    const double VIEW_SWING_RESPONSIVENESS   = clgi.CVar( "cl_viewswing_responsiveness", "14.0", 0 )->value;
    const double VIEW_SWING_RECOVERY         = clgi.CVar( "cl_viewswing_recovery", "3.0", 0 )->value;
    const double VIEW_SWING_INTENSITY        = clgi.CVar( "cl_viewswing_intensity", "8.0", 0 )->value;

    const double MAX_SWING_ANGLE    = clgi.CVar( "cl_viewswing_max_angle", "12.5", 0 )->value;
    const double SWING_DAMPING      = clgi.CVar( "cl_viewswing_damping", "0.75", 0 )->value; // Was 0.88

    const double MIN_INPUT_THRESHOLD    = clgi.CVar( "cl_viewswing_input_threshold", "0.05", 0 )->value;
    const double VELOCITY_DECAY         = clgi.CVar( "cl_viewswing_velocity_decay", "0.82", 0 )->value;
    #endif

    // Enhanced static variables for swing-like delay effect on gun angles
    //static Vector3 lastViewAngles = QM_Vector3Zero();
    //static Vector3 gunAngleDelta = QM_Vector3Zero();
    //static Vector3 targetGunAngleDelta = QM_Vector3Zero();
    //static Vector3 swingVelocity = QM_Vector3Zero(); // Track angular momentum
    //static bool firstFrame = true;

    // Check if player is moving
    const bool isMoving = ( ps->xySpeed >= 0.1 || ps->xyzSpeed >= 0.1 );

    // Check if secondary fire is being held (pistol aiming)
    const bool isSecondaryFiring = ( game.predictedState.cmd.cmd.buttons & BUTTON_SECONDARY_FIRE ) != 0;

    /**
    *
    *
    *   View Weapon `Bob` Angle Swing Calculation:
    *
    *
    **/
    // Enhanced swing-like delay effect for gun angles with momentum and proper decay
    const Vector3 currentViewAngles = clgi.client->refdef.viewangles;

    // Use framerate-independent delta time calculation like CLG_ViewWeapon_CalculateOffset
    const double deltaTime = clgi.GetFrameTime();

    // Handle first frame initialization
    if ( game.viewWeapon.firstFrame || deltaTime <= 0.0 ) {
        game.viewWeapon.lastViewAngles = currentViewAngles;
        game.viewWeapon.gunAngleDelta = QM_Vector3Zero();
        game.viewWeapon.targetGunAngleDelta = QM_Vector3Zero();
        game.viewWeapon.swingVelocity = QM_Vector3Zero();
        game.viewWeapon.firstFrame = false;
        ps->gunangles[ PITCH ] = 0.0;
        ps->gunangles[ YAW ] = 0.0;
        ps->gunangles[ ROLL ] = 0.0;
        return;
    }

    // Calculate view angle delta (how much the view has changed) - PROPER ANGLE DIFFERENCE
    Vector3 viewAngleDelta = QM_Vector3Zero();
    double totalInputMagnitude = 0.0;

    for ( int32_t i = 0; i < 3; i++ ) {
        #if 1
            // Calculate raw difference using QM_AngleMod first, and then normalize 
            // the difference to the shortest path (-180 to +180 range).
            double delta = QM_Angle_Normalize180( QM_AngleMod( currentViewAngles[ i ] - game.viewWeapon.lastViewAngles[ i ] ) );
        // <Q2RTXP>: WID: This bugs out fetching incorrect shortest angle path.
        #else
            // Calculate raw difference without using QM_AngleMod first
            double delta = currentViewAngles[ i ] - lastViewAngles[ i ];

            // Normalize the difference to the shortest path (-180 to +180 range)
            while ( delta > 180.0 ) {
                delta -= 360.0;
            }
            while ( delta < -180.0 ) {
                delta += 360.0;
            }
        #endif

        viewAngleDelta[ i ] = delta;
        totalInputMagnitude += fabs( delta );
    }

    // Determine if there's significant input
    const bool hasSignificantInput = totalInputMagnitude > MIN_INPUT_THRESHOLD;

    // Calculate the target gun angle delta based on view movement
    Vector3 newTargetGunAngleDelta = QM_Vector3Zero();

    if ( hasSignificantInput ) {
        // Apply input-based swing (opposite to view movement for inertia effect)
        newTargetGunAngleDelta[ PITCH ] = -viewAngleDelta[ PITCH ] * VIEW_SWING_INTENSITY;
        newTargetGunAngleDelta[ YAW ] = -viewAngleDelta[ YAW ] * VIEW_SWING_INTENSITY;
        newTargetGunAngleDelta[ ROLL ] = -viewAngleDelta[ YAW ] * VIEW_SWING_INTENSITY * 0.4;

        // Add to swing velocity for momentum - framerate independent scaling
        game.viewWeapon.swingVelocity = QM_Vector3Add( game.viewWeapon.swingVelocity, QM_Vector3Scale( newTargetGunAngleDelta, deltaTime * 60.0 ) );
    } else {
        // No significant input - apply velocity decay using framerate-independent calculation
        const double decayFactor = std::pow( VELOCITY_DECAY, deltaTime * 60.0 ); // 60fps reference
        game.viewWeapon.swingVelocity = QM_Vector3Scale( game.viewWeapon.swingVelocity, decayFactor );

        // If velocity is very small, zero it out to prevent floating point drift
        if ( QM_Vector3Length( game.viewWeapon.swingVelocity ) < 0.01 ) {
            game.viewWeapon.swingVelocity = QM_Vector3Zero();
        }
    }

    // Apply damping to swing velocity - framerate independent
    const double dampingFactor = std::pow( SWING_DAMPING, deltaTime * 60.0 ); // 60fps reference
    game.viewWeapon.swingVelocity = QM_Vector3Scale( game.viewWeapon.swingVelocity, dampingFactor );

    // Calculate target based on velocity (momentum-based swing)
    newTargetGunAngleDelta = QM_Vector3Scale( game.viewWeapon.swingVelocity, 1.0 );
    
    // Add angles that rely on the strafe directional velocity dotproducts.
    #ifdef ENABLE_STRAFE_VIEWMODEL_ANGLES
        const double clg_run_pitch = cl_run_pitch->value;
        const double clg_run_roll = cl_run_roll->value;
        const Vector3 strafeVelocity = { game.predictedState.currentPs.pmove.velocity[ 0 ], game.predictedState.currentPs.pmove.velocity[ 1 ], 0.0f };

        // Running Pitch Angles.
        double delta = QM_Vector3DotProduct( strafeVelocity, clgi.client->v_forward );
        newTargetGunAngleDelta[ PITCH ] += -delta * clg_run_pitch;

        // Strafing Roll Angles.
        delta = QM_Vector3DotProduct( strafeVelocity, clgi.client->v_right );
        newTargetGunAngleDelta[ ROLL ] += delta * clg_run_roll;
    #endif

    // Clamp the target swing angles to prevent excessive movement
    for ( int32_t i = 0; i < 3; i++ ) {
        newTargetGunAngleDelta[ i ] = std::clamp( (double)newTargetGunAngleDelta[ i ], -MAX_SWING_ANGLE, MAX_SWING_ANGLE );
    }

    // Update target with responsiveness (how quickly gun reacts to changes) - framerate independent
    const double responseLerpFactor = std::min( 1.0, VIEW_SWING_RESPONSIVENESS * deltaTime );
    game.viewWeapon.targetGunAngleDelta = QM_Vector3Lerp( game.viewWeapon.targetGunAngleDelta, newTargetGunAngleDelta, responseLerpFactor );

    // Smoothly lerp gun angle delta towards target (creates the swing effect) - framerate independent
    const double recoveryLerpFactor = std::min( 1.0, VIEW_SWING_RECOVERY * deltaTime );
    game.viewWeapon.gunAngleDelta = QM_Vector3Lerp( game.viewWeapon.gunAngleDelta, game.viewWeapon.targetGunAngleDelta, recoveryLerpFactor );

    // Apply additional recovery force towards zero when no input - framerate independent
    if ( !hasSignificantInput ) {
        const double zeroingForce = 1.0 - std::pow( 0.95, deltaTime * 60.0 ); // Frame-rate independent decay
        game.viewWeapon.gunAngleDelta = QM_Vector3Scale( game.viewWeapon.gunAngleDelta, 1.0 - zeroingForce );

        // Snap to zero when very close to prevent drift
        if ( QM_Vector3Length( game.viewWeapon.gunAngleDelta ) < 0.02 ) {
            game.viewWeapon.gunAngleDelta = QM_Vector3Zero();
        }
    }

    // Calculate movement bobbing separately and add it to the swing effect
    Vector3 movementBobAngles = QM_Vector3Zero();
    if ( isMoving ) {
        // Calculate bobbing angles separately
        double rollBob = level.viewBob.xySpeed * level.viewBob.fracSin * 0.001;
        double pitchBob = level.viewBob.xySpeed * level.viewBob.fracSin * 0.001;
        // Apply cycle direction only to the bobbing component
        if ( level.viewBob.cycle & 1 ) {
            rollBob = -rollBob;
        }
        // Assign the calculated bobbing angles to the movementBobAngles vector.
        movementBobAngles[ ROLL ] = rollBob;
        movementBobAngles[ PITCH ] = pitchBob;
    }

    // Apply the final combined gun angles (swing + bobbing)
    ps->gunangles[ PITCH ] = game.viewWeapon.gunAngleDelta[ PITCH ] + movementBobAngles[ PITCH ];
    ps->gunangles[ YAW ] = game.viewWeapon.gunAngleDelta[ YAW ] + movementBobAngles[ YAW ];
    ps->gunangles[ ROLL ] = game.viewWeapon.gunAngleDelta[ ROLL ] + movementBobAngles[ ROLL ];

    // Store current view angles for next frame
    game.viewWeapon.lastViewAngles = currentViewAngles;
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
        //QMTime::FromMilliseconds( clgi.client->accumulatedRealTime ), QMTime::FromMilliseconds( game.viewWeapon.real_time ),
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
void CLG_AddViewWeapon( void ) {
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

    // Find states to interpolate between
    // Vanilla behavior:
    #if 1
    player_state_t *ps = &clgi.client->frame.ps;
    player_state_t *ops = &clgi.client->oldframe.ps;
    #endif
    // Extrapolated behavior:
    #if 0
    player_state_t *ps = &( clgi.client->frame.valid ? game.predictedState.currentPs : clgi.client->frame.ps );
    player_state_t *ops = &( clgi.client->frame.valid ? clgi.client->frame.ps : clgi.client->oldframe.ps );
    #endif
    // Full on client behavior:
    #if 0
    player_state_t *ps = &game.predictedState.currentPs;
    player_state_t *ops = &game.predictedState.lastPs;
    #endif

    memset( &gun, 0, sizeof( gun ) );

    if ( gun_model ) {
        gun.model = gun_model;  // development tool
    } else {
        gun.model = clgi.client->model_draw[ ps->gun.modelIndex ];
        #if 0
        // If no model handle is set, use the model from the player state.
        int32_t skinnum = clgi.client->clientEntity->current.skinnum;
        // Fetch client info ID. (encoded in skinnum)
        clientinfo_t *ci = &clgi.client->clientinfo[ skinnum & 0xff ];
        // Fetch weapon ID. (encoded in skinnum).
        int32_t weaponModelIndex = ( skinnum >> 8 ); // 0 is default weapon model
        if ( weaponModelIndex < 0 || weaponModelIndex > precache.numViewModels - 1 ) {
            weaponModelIndex = 0;
        }
        gun.model = clgi.client->model_draw[ weaponModelIndex ];
        if ( !gun.model ) {
            if ( weaponModelIndex != 0 ) {
                gun.model = ci->weaponmodel[ 0 ];
            }
            if ( !gun.model ) {
                gun.model = clgi.client->baseclientinfo.weaponmodel[ 0 ];
            }
        }
        #endif
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

    // Model entity index ID is reserved for the view weapon.
    gun.id = RENTITIY_RESERVED_GUN;

    // Calculate the lerped gun position.
    //Vector3 gunOrigin = clgi.client->refdef.vieworg + QM_Vector3Lerp( game.predictedState.lastPs.gunoffset, game.predictedState.currentPs.gunoffset, clgi.client->xerpFraction );
	Vector3 gunOrigin = clgi.client->refdef.vieworg + game.predictedState.currentPs.gunoffset;
    // Calculate the lerped gun angles.
    //Vector3 gunAngles = clgi.client->refdef.viewangles + /*QM_Vector3LerpAngles*/( game.predictedState.lastPs.gunangles, game.predictedState.currentPs.gunangles/*, clgi.client->lerpfrac*/ );
    Vector3 gunAngles = QM_Vector3AngleMod( clgi.client->refdef.viewangles + game.predictedState.currentPs.gunangles );
    // Copy the calculated gun position and angles into vec3_t's from refresh 'entity_t'.
    VectorCopy( gunOrigin, gun.origin );
    VectorCopy( gunAngles, gun.angles );

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
                //game.viewWeapon.real_time = clgi.client->accumulatedRealTime;
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