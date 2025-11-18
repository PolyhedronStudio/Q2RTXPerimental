/********************************************************************
*
*
*	ClientGame: Player State View Transition
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_main.h"
#include "clgame/clg_effects.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_predict.h"
#include "clgame/clg_view.h"
#include "clgame/clg_view_state_transition.h"
#include "clgame/clg_view_weapon.h"



/**
*
*
*   Transitions(Lerps/Xerps) player view states: 
*   Calculate vieworg, offsets, viewheight, screenblends, and actual origin.
*
*
**/
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
*   @return The predicted and smooth lerped viewheight for the current running prediction frame.
**/
static const double CLG_SmoothViewHeight() {
    //! Miliseconds for smoothing out over.
    static constexpr int32_t HEIGHT_CHANGE_TIME = 100;
    //! Base 1 Frametime.
    static constexpr double HEIGHT_CHANGE_BASE_1_FRAMETIME = ( 1. / HEIGHT_CHANGE_TIME );
    //! Determine delta time.
    int64_t timeDelta = HEIGHT_CHANGE_TIME - std::min<int64_t>( ( clgi.client->accumulatedRealTime - game.predictedState.transition.view.timeHeightChanged ), HEIGHT_CHANGE_TIME );
    //! Return the frame's adjustment for viewHeight which is added on top of the final vieworigin + viweoffset.
    return game.predictedState.transition.view.height[ 0 ] + ( (double)( game.predictedState.transition.view.height[ 1 ] - game.predictedState.transition.view.height[ 0 ] ) * timeDelta * HEIGHT_CHANGE_BASE_1_FRAMETIME );
}

/**
*   @brief  Smoothly transit the 'step' offset.
**/
static const double CLG_SmoothStepOffset() {
    // The original code was short integer coordinate based and had the following 'formula':
    // 127 * 0.125 = 15.875.
    // 
    // Ours differs: PM_MAX_STEP_CHANGE_HEIGHT(18) - STEP_SMALL_HEIGHT(0.25) resulting in 15.75 as STEP_SMALL_HEIGHT.
    // What is considered to be a 'small' step.
    static constexpr double STEP_SMALL_HEIGHT = ( PM_STEP_MAX_SIZE - ( PM_STEP_MIN_SIZE + PM_STEP_GROUND_DIST ) );
    // Time in miliseconds to smooth the view for the step offset with.
    static constexpr int32_t STEP_CHANGE_TIME = 100;
    // Base 1 FrameTime.
    static constexpr double STEP_CHANGE_BASE_1_FRAMETIME = ( 1. / STEP_CHANGE_TIME );

    // Time delta.
    int64_t deltaTime = ( clgi.GetRealTime() - game.predictedState.transition.step.timeChanged );
    // Absolute step size we had.
    const double fabsStepSize = fabsf( game.predictedState.transition.step.size );
    // Smooth out stair climbing.
    if ( fabsStepSize >= PM_STEP_MIN_SIZE && fabsf( game.predictedState.transition.step.size ) < STEP_SMALL_HEIGHT ) {
        // Will multiply it by 2 for smaller steps to still lerp faster over time. 
        // (Thus exceeding the limit faster, visual and logically makes sense.)
        deltaTime <<= 1;
    }
    // Adjust view org by step height change.
    if ( /*fabsStepSize >= PM_STEP_MIN_SIZE && */deltaTime <= STEP_CHANGE_TIME ) {
        // Return negated.
        return -( game.predictedState.transition.step.height * ( STEP_CHANGE_TIME - deltaTime ) * STEP_CHANGE_BASE_1_FRAMETIME );
    }
    // Nothing to see here.
    return 0.;
}

/**
*   @brief  Lerp the view angles if desired.
**/
static void CLG_LerpViewAngles( player_state_t *ops, player_state_t *ps, client_predicted_state_t *predictedState, const double lerpFraction ) {
    // Predicted player state.
    player_state_t *predictedPlayerState = &predictedState->currentPs;

    /**
	*   With demo playback, always lerp from old to new server frame angles.
    **/
    if ( clgi.IsDemoPlayback() ) {
		// Lerp from server frame angles.
        const Vector3 lerpedAngles = QM_Vector3LerpAngles( ops->viewangles, ps->viewangles, lerpFraction );
		// Apply.
        VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );
    /**
    *   Use predicted player state view angles while alive.
    **/
    } else if ( ps->pmove.pm_type < PM_DEAD && !( ps->pmove.pm_flags & PMF_NO_ANGLES_PREDICTION ) ) {
        VectorCopy( predictedPlayerState->viewangles, clgi.client->refdef.viewangles );
    /**
    *   Else if the old state was PM_DEAD, lerp from predicted state to server frame playerstate.
    **/
    } else if ( ops->pmove.pm_type == PM_DEAD && !( ps->pmove.pm_flags & PMF_NO_ANGLES_PREDICTION ) ) {/*cls.serverProtocol > PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {*/
        // Lerp from predicted angles, since enhanced servers
        // do not send viewangles each frame
        const Vector3 lerpedAngles = QM_Vector3LerpAngles( ops->viewangles, ps->viewangles, lerpFraction );
        // Apply.
        VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );

    #if 1
        /**
        *   Use the local predicted player state view angles instead.
        **/
        } else {
            VectorCopy( predictedPlayerState->viewangles, clgi.client->refdef.viewangles );

            //const Vector3 lerpedAngles = QM_Vector3LerpAngles( predictedPlayerState->viewangles, ps->viewangles, lerpFraction );
            //const Vector3 lerpedAngles = QM_Vector3LerpAngles( ps->viewangles, pps->viewangles, lerpFraction );
            //VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );
        }
    #else
        /**
        *   Under any other circumstances, just use lerped angles from ops to ps.
        **/
        } else {
			// Lerp from server frame angles.
            const Vector3 lerpedAngles = QM_Vector3LerpAngles( ops->viewangles, ps->viewangles, lerpFraction );
            // Apply.
            VectorCopy( lerpedAngles, clgi.client->refdef.viewangles );
        }
    #endif
}

/**
*   @brief  Smooth lerp the old and current player state delta angles.
**/
static void CLG_LerpDeltaAngles( player_state_t *ops, player_state_t *ps, const double lerpFrac ) {
    // Calculate delta angles between old and current player state.
    clgi.client->delta_angles = QM_Vector3AngleMod( QM_Vector3LerpAngles( ops->pmove.delta_angles, ps->pmove.delta_angles, lerpFrac ) );
}


/**
*   @brief  Lerp the client's POV range.
**/
static void CLG_LerpPointOfView( player_state_t *ops, player_state_t *ps, const double lerpFrac ) {

    //! <Q2RTXP>: WID: These need to be defined elsewhere but for now this'll to
    static QMEaseState fovEaseState;
    static double easeLerpFactor = 0.;
    // Amount of MS to lerp FOV over.
    static constexpr QMTime FOV_EASE_DURATION = 225_ms;
    static QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

    /**
    *   Determine whether the FOV changed, whether it is zoomed(lower or higher than previous FOV), and
    *   store the old FOV as well as the realtime of change.
    **/

    //! Resort to cvar default value.
    static double oldFOV = info_fov->value;
    //! If fov changed, determine whether to start an ease in or out transition.
    if ( ops->fov != ps->fov ) {
        if ( ops->fov > ps->fov/* && povLerp.state = FOV_LERP_STATE_IN */ ) {
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
    clgi.client->fov_y = CLG_CalculateFieldOfView( clgi.client->fov_x, 4, 3 );
}
/**
*   @brief  Lerp the client's viewOffset.
**/
static void CLG_LerpViewOffset( player_state_t *ops, player_state_t *ps, const double lerpFrac, Vector3 &finalViewOffset ) {
    player_state_t *pps = &game.predictedState.currentPs;

    if ( clgi.client->frame.valid ) {
        #if 1
        finalViewOffset = QM_Vector3Lerp( ps->viewoffset, pps->viewoffset, lerpFrac );
        #else
        finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
        #endif
    } else {
        #if 1
        finalViewOffset = pps->viewoffset; //finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );inalViewOffset = pps->viewoffset; //finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
        #else
        finalViewOffset = ops->viewoffset; //finalViewOffset = QM_Vector3Lerp( ops->viewoffset, ps->viewoffset, lerpFrac );
        #endif
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
void CLG_CalculateViewValues( void ) {
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
    //if (!cls.demo.playback && cl_predict->integer && !(ps->pmove.pm_flags & PMF_NO_ORIGIN_PREDICTION) ) {
    // Use positional prediction?
	const int32_t useOriginPrediction = CLG_UsePrediction() & !( ps->pmove.pm_flags & PMF_NO_ORIGIN_PREDICTION );

    /**
	*   Correct the predicted origin by back lerping the 'error offset' margin.
    **/
    if ( useOriginPrediction ) {
        // Backlerp fraction for the error.
        const double backLerp = lerpFrac - 1.0;
        // Calculate errorLerp and add it to the predicted origin.
        const Vector3 errorLerp = QM_Vector3Scale( game.predictedState.error, backLerp );
        // Add error lerp to the current predicted player state.
        game.predictedState.currentPs.pmove.origin += errorLerp;//= game.predictedState.origin;
        // Calculate errorLerp and add it to the predicted origin.
        game.predictedState.origin += errorLerp;
        // Set the view origin to the predicted origin.
        VectorCopy( game.predictedState.origin, clgi.client->refdef.vieworg );
    /**
    *   Just use interpolated values.
    **/
    } else {
		// Interpolate between the two origins( old player state and current player state. )
        const Vector3 viewOrg = QM_Vector3Lerp( ops->pmove.origin, ps->pmove.origin, lerpFrac );
		// Set the view origin to the interpolated origin.
        VectorCopy( viewOrg, clgi.client->refdef.vieworg );

        // WID: NOTE: If things break, look for this here.
        // WID: This should fix demos or cl_nopredict
        //VectorCopy( clgi.client->refdef.vieworg, game.predictedState.currentPs.pmove.origin );
        //VectorCopy( game.predictedState.currentPs.pmove.origin, clgi.client->refdef.vieworg );

		// Ensure predicted state origin and currentPs origin are the same as refdef vieworg.
        game.predictedState.origin = game.predictedState.currentPs.pmove.origin = clgi.client->refdef.vieworg;

        #if 0
        clgi.client->refdef.vieworg[ 2 ] -= ops->pmove.origin.z;
        #endif
    }

    /**
	*   Transition the view states by lerping/xerping between them.
    **/
	// Back lerp fraction.
    const double backLerp = 1.0 - lerpFrac; // lerpFrac - 1.0;
    // Lerp View Angles.
    CLG_LerpViewAngles( ops, ps, &game.predictedState, lerpFrac );
    // Interpolate old and current player state delta angles.
    CLG_LerpDeltaAngles( ops, ps, lerpFrac );
    // Interpolate blend colors if the last frame wasn't clear.
    CLG_LerpScreenBlend( ops, ps, &game.predictedState );
    // Interpolate Field of View.
    CLG_LerpPointOfView( ops, ps, clgi.client->xerpFraction );
    // Lerp the view offset.
    CLG_LerpViewOffset( ops, ps, lerpFrac, finalViewOffset );
    // Smooth out step offset.
    finalViewOffset.z += CLG_SmoothStepOffset();
    // Smooth out the ducking view height change over 100ms
    finalViewOffset.z += CLG_SmoothViewHeight();

    /**
    *   Calculate the final angle vectors for the current view angles.
    **/
    QM_AngleVectors( clgi.client->refdef.viewangles, &clgi.client->vForward, &clgi.client->vRight, &clgi.client->vUp );

    /**
    *   Copy the view origin and angles for the thirdperson(and also shadow casting) entity.
    **/
    // 
    clgi.client->playerEntityOrigin = clgi.client->refdef.vieworg;// VectorCopy( clgi.client->refdef.vieworg, clgi.client->playerEntityOrigin );
    clgi.client->playerEntityAngles = clgi.client->refdef.viewangles; //VectorCopy( clgi.client->refdef.viewangles, clgi.client->playerEntityAngles );
    // Keep pitch in bounds.
    if ( clgi.client->playerEntityAngles[ PITCH ] > 180 ) {
        clgi.client->playerEntityAngles[ PITCH ] -= 360;
    }
    // Adjust pitch slightly.
    clgi.client->playerEntityAngles[ PITCH ] = clgi.client->playerEntityAngles[ PITCH ] / 3;

    /**
    *   Add the resulting final viewOffset to the refdef view origin.
    **/
    VectorAdd( clgi.client->refdef.vieworg, finalViewOffset, clgi.client->refdef.vieworg );
    
    /**
    *   Setup the new position for spatial audio recognition.
    **/
    clgi.S_SetupSpatialListener( clgi.client->refdef.vieworg, &clgi.client->vForward.x, &clgi.client->vRight.x, &clgi.client->vUp.x );

    /**
    *   Debug Stuff:
    **/
    #if 0
    static uint64_t printFrame = clgi.client->frame.number;
    if ( printFrame != clgi.client->frame.number ) {
        clgi.Print( PRINT_DEVELOPER, "Frame( #%llu ):\n", clgi.client->frame.number );

        const Vector3 framePsOrigin = clgi.client->frame.ps.pmove.origin;
        clgi.Print( PRINT_DEVELOPER, "        org( %f, %f, %f )\n",
            framePsOrigin[ 0 ], framePsOrigin[ 1 ], framePsOrigin[ 2 ] );

        const Vector3 predStateCurPsOrigin = game.predictedState.origin;
        clgi.Print( PRINT_DEVELOPER, "      prorg( %f , %f , %f )\n",
            predStateCurPsOrigin[ 0 ], predStateCurPsOrigin[ 1 ], predStateCurPsOrigin[ 2 ] );

        printFrame = clgi.client->frame.number;
    }
    #endif
}