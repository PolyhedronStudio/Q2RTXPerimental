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
#include "clgame/clg_view_state_transition.h"
#include "clgame/clg_view_weapon.h"

#include "sharedgame/sg_entity_effects.h"



/**
*
*
*
*   View/Scene Entities:
*
*
*
**/
/**
*   @brief
**/
void CLG_ClearViewScene( void ) {
    //cl.viewScene.r_numdlights = 0;
    //cl.viewScene.r_numentities = 0;
    //cl.viewScene.r_numparticles = 0;
}

/**
*   @brief  Prepares the current frame's view scene for the refdef by
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void CLG_PrepareViewEntities( void ) {
    // Get the current local client's player view entity. (Can be one we're chasing.)
    clgi.client->clientEntity = CLG_GetViewBoundEntity();
    // Get the current frames' chasing player view entity. (Can be one we're chasing.)
    clgi.client->chaseEntity = CLG_GetChaseBoundEntity();

    // Calculate view and spatial audio listener origins.
    CLG_CalculateViewValues();
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
const refdef_flags_t CLG_GetViewRenderDefinitionFlags( void ) {
    return game.predictedState.currentPs.rdflags;
}

/**
*   @brief  Calculates the client's field of view.
**/
const float CLG_CalculateFieldOfView( const float fov_x, const float width, const float height ) {
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
*   @brief  Calculates additional offsets based on the bobCycle for both 
*           viewOrigin and viewAngles(thus, additional client side kickangles).
**/
static void CLG_CalculateViewOffset( player_state_t *ops, player_state_t *ps, const double lerpFraction ) {
    /*
    *   <Q2RTXP>: WID: We don't really use this anymore, do we? 
    **/
    //float *viewAngles = clgi.client->refdef.viewangles;
    Vector3 viewAngles = QM_Vector3Zero();
    #if 0
    // TODO: Turn into cvars.
    const float clg_run_pitch = cl_run_pitch->value;
    const float clg_run_roll = cl_run_roll->value;
    const float clg_bob_up = cl_bob_up->value;
    const float clg_bob_pitch = cl_bob_pitch->value;
    const float clg_bob_roll = cl_bob_roll->value;

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
    #endif
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
*   @brief  Calculate the bob cycle and apply bob angles as well as a view offset.
**/
static void CLG_View_CycleBob( player_state_t *ps ) {
	// Scalar bob cycle, normalized to radians..
    const double scalarBobCycle = (double)( ( ps->bobCycle & 127 ) / 127.0 * M_PI );

    // Calculate base next bob cycle.
    level.viewBob.cycle = ( ps->bobCycle & 128 ) >> 7;

	// Calculate the fractional sine and cosine values for the bob cycle.
    level.viewBob.fracSin = fabs( sin( scalarBobCycle ) );
    level.viewBob.fracSin2 = fabs( sin( scalarBobCycle ) + sin( scalarBobCycle ) );
    level.viewBob.fracCos = fabs( cos( scalarBobCycle ) );
    level.viewBob.fracCos2 = fabs( cos( scalarBobCycle ) + cos( scalarBobCycle ) );

	// Ensure that the player speeds are set correctly for the view bob.
    level.viewBob.xySpeed = ps->xySpeed;
    level.viewBob.xyzSpeed = ps->xyzSpeed;
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
    const double lerpFrac = clgi.client->lerpfrac;

    // Cycle the view bob on our predicted state.
    CLG_View_CycleBob( predictingPlayerState );

    // WID: TODO: This requires proper player state damage summing and 'wiring' as well as proper
    // player event predicting.
    // Apply all the damage taken this frame
    //CLG_DamageFeedback( ent );
    
    // Determine the view offsets for the current predicting player state.
    CLG_CalculateViewOffset( lastPredictingPlayerState, predictingPlayerState, lerpFrac );

    // Determine the gun origin and angle offsets.based on the bobCycles of the predicted player states.
    //CLG_ViewWeapon_CalculateOffset( &predictedState->lastPs, &predictedState->currentPs );
    if ( clgi.client->frame.valid ) {
        // Will calculaate the view weapon offset necessary for adding on to the vieworigin of the player.
        CLG_ViewWeapon_CalculateOffset( serverFramePlayerState, predictingPlayerState, lerpFrac );
		// Will calculate the view weapon angles necessary for adding on to the viewangles of the player,
        // and also apply weapon swing/drag.
        CLG_ViewWeapon_CalculateAngles( serverFramePlayerState, predictingPlayerState, lerpFrac );
    } else {
        // Will calculaate the view weapon offset necessary for adding on to the vieworigin of the player.
        CLG_ViewWeapon_CalculateOffset( lastServerFramePlayerState, serverFramePlayerState, lerpFrac );
        // Will calculate the view weapon angles necessary for adding on to the viewangles of the player,
        // and also apply weapon swing/drag.
        CLG_ViewWeapon_CalculateAngles( lastServerFramePlayerState, serverFramePlayerState, lerpFrac );
    }

    // Add server sided frame kick angles.
    if ( cl_kickangles->integer ) {
        const Vector3 kickAngles = QM_Vector3LerpAngles( lastServerFramePlayerState->kick_angles, serverFramePlayerState->kick_angles, lerpFrac );
        VectorAdd( clgi.client->refdef.viewangles, kickAngles, clgi.client->refdef.viewangles );
    }

    // Calculate bob cycle on the current predicting player state.    
    //CLG_View_CycleBob( currentPredictingPlayerState );
    //CLG_View_CycleBob( predictingPlayerState );

    // Inform client state we're not in third-person view.
    clgi.client->thirdPersonView = false;
}

/**
*   @brief  <Q2RTXP>: WID: This isn't a thing we have focussed on yet so expect it to suck.
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
*   @brief  Finishes the view values, or rather, determines whether to setup a third or first person view.
**/
void CLG_FinishViewValues( void ) {
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