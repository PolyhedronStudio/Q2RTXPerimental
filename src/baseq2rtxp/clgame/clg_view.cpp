/********************************************************************
*
*
*	ClientGame: View Scene Management.
*
*
********************************************************************/
#include "clgame/clg_local.h"

#include "clgame/clg_effects.h"

#include "clgame/clg_local_entities.h"
#include "clgame/clg_packet_entities.h"

#include "clgame/clg_temp_entities.h"
#include "clgame/clg_screen.h"
#include "clgame/clg_view.h"
#include "clgame/clg_world.h"

#include "clgame/clg_view_weapon.h"

//=============
//
// development tools for weapons
//
int         gun_frame;
qhandle_t   gun_model;

//// gun frame debugging functions
//static void V_Gun_Next_f( void ) {
//    gun_frame++;
//    Com_Printf( "frame %i\n", gun_frame );
//}
//
//static void V_Gun_Prev_f( void ) {
//    gun_frame--;
//    if ( gun_frame < 0 )
//        gun_frame = 0;
//    Com_Printf( "frame %i\n", gun_frame );
//}
//
//static void V_Gun_Model_f( void ) {
//    char    name[ MAX_QPATH ];
//
//    if ( Cmd_Argc() != 2 ) {
//        gun_model = 0;
//        return;
//    }
//    Q_concat( name, sizeof( name ), "models/", Cmd_Argv( 1 ), "/tris.iqm" );
//    gun_model = R_RegisterModel( name );
//}

//=============

//void V_Flashlight( void ) {
//    if ( cls.ref_type == REF_TYPE_VKPT ) {
//        player_state_t *ps = &cl.frame.ps;
//        player_state_t *ops = &cl.oldframe.ps;
//
//        // Flashlight origin
//        vec3_t light_pos;
//        // Flashlight direction (as angles)
//        vec3_t flashlight_angles;
//
//        if ( cls.demo.playback ) {
//            /* If a demo is played we don't have predicted_angles,
//             * and we can't use cl.refdef.viewangles for the same reason
//             * below. However, lerping the angles from the old & current frame
//             * work nicely. */
//            LerpAngles( cl.oldframe.ps.viewangles, cl.frame.ps.viewangles, cl.lerpfrac, flashlight_angles );
//        } else {
//            /* Use cl.playerEntityOrigin+viewoffset, playerEntityAngles instead of
//             * cl.refdef.vieworg, cl.refdef.viewangles as as the cl.refdef values
//             * are the camera values, but not the player "eye" values in 3rd person mode. */
//            VectorCopy( cl.predictedState.view.angles, flashlight_angles );
//        }
//        // Add a bit of gun bob to the flashlight as well
//        vec3_t gunangles;
//        LerpVector( ops->gunangles, ps->gunangles, cl.lerpfrac, gunangles );
//        VectorAdd( flashlight_angles, gunangles, flashlight_angles );
//
//        vec3_t view_dir, right_dir, up_dir;
//        AngleVectors( flashlight_angles, view_dir, right_dir, up_dir );
//
//        /* Start off with the player eye position. */
//        vec3_t viewoffset;
//        LerpVector( ops->viewoffset, ps->viewoffset, cl.lerpfrac, viewoffset );
//        VectorAdd( cl.playerEntityOrigin, viewoffset, light_pos );
//
//        /* Slightly move position downward, right, and forward to get a position
//         * that looks somewhat as if it was attached to the gun.
//         * Generally, the spot light origin should be placed away from the player model
//         * to avoid interactions with it (mainly unexpected shadowing).
//         * When adjusting the offsets care must be taken that
//         * the flashlight doesn't also light the view weapon. */
//        VectorMA( light_pos, flashlight_offset[ 2 ] * clg_gunscale->value, view_dir, light_pos );
//        float leftright = flashlight_offset[ 0 ] * clg_gunscale->value;
//        if ( info_hand->integer == 1 )
//            leftright = -leftright; // left handed
//        else if ( info_hand->integer == 2 )
//            leftright = 0.f; // "center" handed
//        VectorMA( light_pos, leftright, right_dir, light_pos );
//        VectorMA( light_pos, flashlight_offset[ 1 ] * clg_gunscale->value, up_dir, light_pos );
//
//        V_AddSpotLightTexEmission( light_pos, view_dir, cl_flashlight_intensity->value, 1.f, 1.f, 1.f, 90.0f, flashlight_profile_tex );
//    } else {
//        // Flashlight is VKPT only
//    }
//}
//==================================================================

static cvar_t *cl_adjustfov = nullptr;

static cvar_t *clg_view_add_blend = nullptr;
static cvar_t *clg_view_add_entities = nullptr;
static cvar_t *clg_view_add_lights = nullptr;
static cvar_t *clg_view_add_particles = nullptr;

static cvar_t *clg_show_lights = nullptr;

static cvar_t *clg_flashlight = nullptr;
static cvar_t *clg_flashlight_intensity = nullptr;

//static cvar_t *clg_flashlight_offset;
//static Vector3 flashlight_offset;

#if USE_DEBUG
static cvar_t *clg_testblend = nullptr;
static cvar_t *clg_testentities = nullptr;
static cvar_t *clg_testlights = nullptr;
static cvar_t *clg_testparticles = nullptr;

static cvar_t *clg_view_stats = nullptr;
#endif



/**
*
*
*   View Debugging Stuff:
*
*
**/
#if USE_DEBUG
/*
================
V_TestParticles

If clg_testparticles is set, create 4096 particles in the view
================
*/
static void V_TestParticles( void ) {
    particle_t *p;
    int         i, j;
    float       d, r, u;

    clgi.client->viewScene.r_numparticles = MAX_PARTICLES;
    for ( i = 0; i < clgi.client->viewScene.r_numparticles; i++ ) {
        d = i * 0.25f;
        r = 4 * ( ( i & 7 ) - 3.5f );
        u = 4 * ( ( ( i >> 3 ) & 7 ) - 3.5f );
        p = &clgi.client->viewScene.r_particles[ i ];

        for ( j = 0; j < 3; j++ )
            p->origin[ j ] = clgi.client->refdef.vieworg[ j ] + clgi.client->vForward[ j ] * d +
            clgi.client->vRight[ j ] * r + clgi.client->vUp[ j ] * u;

        p->color = 8;
        p->alpha = 1;
    }
}

/*
================
V_TestEntities

If clg_testentities is set, create 32 player models
================
*/
static void V_TestEntities( void ) {
    int         i, j;
    float       f, r;
    entity_t *ent;

    clgi.client->viewScene.r_numentities = 32;
    memset( clgi.client->viewScene.r_entities, 0, sizeof( clgi.client->viewScene.r_entities ) );

    for ( i = 0; i < clgi.client->viewScene.r_numentities; i++ ) {
        ent = &clgi.client->viewScene.r_entities[ i ];

        r = 64 * ( ( i % 4 ) - 1.5f );
        f = 64 * ( i / 4 ) + 128;

        for ( j = 0; j < 3; j++ )
            ent->origin[ j ] = clgi.client->refdef.vieworg[ j ] + clgi.client->vForward[ j ] * f +
            clgi.client->vRight[ j ] * r;

        ent->model = clgi.client->baseclientinfo.model;
        ent->skin = clgi.client->baseclientinfo.skin;
    }
}

/*
================
V_TestLights

If clg_testlights is set, create 32 lights models
================
*/
static void V_TestLights( void ) {
    int         i, j;
    float       f, r;
    dlight_t *dl;

    if ( clg_testlights->integer != 1 ) {
        dl = &clgi.client->viewScene.r_dlights[ 0 ];
        memset( dl, 0, sizeof( dlight_t ) );
        clgi.client->viewScene.r_numdlights = 1;

        VectorMA( clgi.client->refdef.vieworg, 256, clgi.client->vForward, dl->origin );
        if ( clg_testlights->integer == -1 )
            VectorSet( dl->color, -1, -1, -1 );
        else
            VectorSet( dl->color, 1, 1, 1 );
        dl->intensity = 256;
        dl->radius = 16;
        return;
    }

    clgi.client->viewScene.r_numdlights = 32;
    memset( clgi.client->viewScene.r_dlights, 0, sizeof( clgi.client->viewScene.r_dlights ) );

    for ( i = 0; i < clgi.client->viewScene.r_numdlights; i++ ) {
        dl = &clgi.client->viewScene.r_dlights[ i ];

        r = 64 * ( ( i % 4 ) - 1.5f );
        f = 64 * ( i / 4 ) + 128;

        for ( j = 0; j < 3; j++ )
            dl->origin[ j ] = clgi.client->refdef.vieworg[ j ] + clgi.client->vForward[ j ] * f +
            clgi.client->vRight[ j ] * r;
        dl->color[ 0 ] = ( ( i % 6 ) + 1 ) & 1;
        dl->color[ 1 ] = ( ( ( i % 6 ) + 1 ) & 2 ) >> 1;
        dl->color[ 2 ] = ( ( ( i % 6 ) + 1 ) & 4 ) >> 2;
        dl->intensity = 200;
        dl->radius = 16;
    }
}
#endif


/**
*
*
*   View Commands:
*
*
**/
static void V_Cmd_ViewPosition_f( void ) {
    clgi.Print( PRINT_NOTICE, "ViewOrigin:(%s) : ViewAngles(%.f,%.f,%.f)\n", 
        vtos( clgi.client->refdef.vieworg ),
        clgi.client->refdef.viewangles[ PITCH ],
        clgi.client->refdef.viewangles[ YAW ],
        clgi.client->refdef.viewangles[ ROLL ]
    );
}

static const cmdreg_t viewCommands[] = {
    //{ "gun_next", V_Gun_Next_f },
    //{ "gun_prev", V_Gun_Prev_f },
    //{ "gun_model", V_Gun_Model_f },
    { "viewpos", V_Cmd_ViewPosition_f },
    { NULL }
};

/**
*
*
*   CVar Callbacks:
*
*
**/
static void clg_add_blend_changed( cvar_t *self ) {
}



/**
*
*
*   View Init/Shutdown:
*
*
**/
/**
*	@brief	Called whenever the refresh module is (re-)initialized.
**/
void CLG_InitViewScene( void ) {
    /**
    *   Engine View CVars:
    **/
    cl_adjustfov = clgi.CVar_Get( "cl_adjustfov", nullptr, 0 );

    /**
    *   Client Game View CVars:
    **/
    #if USE_DEBUG
    clg_testblend = clgi.CVar_Get( "clg_testblend", "0", 0 );
    clg_testparticles = clgi.CVar_Get( "clg_testparticles", "0", 0 );
    clg_testentities = clgi.CVar_Get( "clg_testentities", "0", 0 );
    clg_testlights = clgi.CVar_Get( "clg_testlights", "0", CVAR_CHEAT );

    clg_view_stats = clgi.CVar_Get( "clg_view_stats", "0", 0 );
    #endif

    clg_view_add_lights = clgi.CVar_Get( "clg_view_add_lights", "1", 0 );
    clg_show_lights = clgi.CVar_Get( "clg_show_lights", "0", 0 );
    //clg_flashlight = Cvar_Get("clg_flashlight", "0", 0);
    //clg_flashlight_intensity = Cvar_Get("clg_flashlight_intensity", "10000", CVAR_CHEAT);
    //if(cls.ref_type == REF_TYPE_VKPT) {
    //    flashlight_profile_tex = R_RegisterImage("flashlight_profile", IT_PIC, static_cast<imageflags_t>( IF_PERMANENT | IF_BILERP) ); // WID: C++20: Added static cast
    //} else {
    //    flashlight_profile_tex = -1;
    //}
    clg_view_add_particles = clgi.CVar_Get( "clg_view_add_particles", "1", 0 );
    clg_view_add_entities = clgi.CVar_Get( "clg_view_add_entities", "1", 0 );
    clg_view_add_blend = clgi.CVar_Get( "clg_view_add_blend", "1", 0 );
    clg_view_add_blend->changed = clg_add_blend_changed;

    // Register view scene related commands.
    clgi.Cmd_Register( viewCommands );
}
/**
*	@brief	Called whenever the refresh module is shutdown.
**/
void CLG_ShutdownViewScene( void ) {
    //if(flashlight_profile_tex != -1) {
    //    R_UnregisterImage(flashlight_profile_tex);
	//}
    // Deregister view scene related commands.
    clgi.Cmd_Deregister( viewCommands );
}



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
*   @brief  Resets the counters for the current frame's view scene.
*           The actual memories of the entities, particles, and dynamic lights
*           are not cleared, only the counters are reset to zero.
* 
*           This is more efficient than clearing all the memories each frame.
**/
void CLG_ClearViewScene( void ) {
    clgi.client->viewScene.r_numdlights = 0;
    clgi.client->viewScene.r_numentities = 0;
    clgi.client->viewScene.r_numparticles = 0;
}

/**
*   @brief  Prepares the current frame's view scene for the refdef by
*           emitting all frame data(entities, particles, dynamic lights, lightstyles,
*           and temp entities) to the refresh definition.
**/
void CLG_PrepareViewEntities( void ) {
    // Add all 'in-frame' received packet entities to the rendered view.
    CLG_AddPacketEntities();
    // Add all 'in-frame' local entities to the rendered view.
    CLG_AddLocalEntities();

    // Add special effects 'refresh entities'.
    CLG_TemporaryEntities_Add();
	// Add particles.
    CLG_AddParticles();
	// Add dynamic lights.
    CLG_AddDynamicLights();
	// Add/Update the entity light styles.
    CLG_AddLightStyles();

    // Add the view weapon model.
    CLG_AddViewWeapon();

    // Add in .md2/.md3/.iqm model 'debugger' entity.
    //CLG_AddTestModel();

    //! Add in the client-side flashlight.
    //if ( clg_flashlight->integer ) {
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
//            entity.id = REFRESHENTITIY_RESERVED_TESTMODEL;
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
//                int timediff = clgi.client->time - prevtime;
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
    const float clg_run_pitch = clg_run_pitch->value;
    const float clg_run_roll = clg_run_roll->value;
    const float clg_bob_up = clg_bob_up->value;
    const float clg_bob_pitch = clg_bob_pitch->value;
    const float clg_bob_roll = clg_bob_roll->value;

    // Lerp the velocity so it doesn't just "snap" back to nothing when we suddenly are standing idle.
    Vector3 velocityLerp = QM_Vector3Lerp( ops->pmove.velocity, ps->pmove.velocity, lerpFraction );

    // Add angles based on velocity.
    float delta = QM_Vector3DotProduct( velocityLerp, clgi.client->vForward );
    viewAngles[ PITCH ] += delta * clg_run_pitch;/*cg_runpitch.value*/;

    delta = QM_Vector3DotProduct( velocityLerp, clgi.client->vRight );
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
    if ( clg_kickangles->integer ) {
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
    static const Vector3 mins = { -4, -4, -4 }, maxs = { 4, 4, 4 };

    // if dead, set a nice view angle
    if ( clgi.client->frame.ps.stats[ STAT_HEALTH ] <= 0 ) {
        clgi.client->refdef.viewangles[ ROLL ] = 0;
        clgi.client->refdef.viewangles[ PITCH ] = 10;
        clgi.client->refdef.viewangles[ YAW ] = clgi.client->frame.ps.stats[ STAT_KILLER_YAW ];
    }

    VectorMA( clgi.client->refdef.vieworg, 512, clgi.client->vForward, focus );

    clgi.client->refdef.vieworg[ 2 ] += 8;

    clgi.client->refdef.viewangles[ PITCH ] *= 0.5f;
    QM_AngleVectors( clgi.client->refdef.viewangles, &clgi.client->vForward, &clgi.client->vRight, &clgi.client->vUp );

    angle = DEG2RAD( clg_thirdperson_angle->value );
    range = clg_thirdperson_range->value;
    fscale = cos( angle );
    rscale = sin( angle );
    VectorMA( clgi.client->refdef.vieworg, -range * fscale, clgi.client->vForward, clgi.client->refdef.vieworg );
    VectorMA( clgi.client->refdef.vieworg, -range * rscale, clgi.client->vRight, clgi.client->refdef.vieworg );

    //CM_BoxTrace( &trace, clgi.client->playerEntityOrigin, clgi.client->refdef.vieworg,
    //    mins, maxs, clgi.client->bsp->nodes, CM_CONTENTMASK_SOLID );
    // When clipping we 
    //trace = clgi.Clip( clgi.client->playerEntityOrigin, mins, maxs, clgi.client->refdef.vieworg, nullptr, (cm_contents_t)( CM_CONTENTMASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP ) );
    Vector3 vorg = clgi.client->refdef.vieworg;
    trace = CLG_Trace( 
        clgi.client->playerEntityOrigin,
        &mins, &maxs, 
        vorg,
        game.clientEntity/*&clg_entities[ 1 ]*/, 
        CM_CONTENTMASK_SOLID//(cm_contents_t)( CM_CONTENTMASK_PLAYERSOLID & ~CONTENTS_PLAYERCLIP )
    );
    if ( trace.fraction != 1.0f ) {
        VectorCopy( trace.endpos, clgi.client->refdef.vieworg );
    }

    VectorSubtract( focus, clgi.client->refdef.vieworg, focus );
    dist = sqrtf( focus[ 0 ] * focus[ 0 ] + focus[ 1 ] * focus[ 1 ] );

    clgi.client->refdef.viewangles[ PITCH ] = -RAD2DEG( atan2( focus[ 2 ], dist ) );
    clgi.client->refdef.viewangles[ YAW ] -= clg_thirdperson_angle->value;

    clgi.client->thirdPersonView = true;
}

/**
*   @brief  Finishes the view values, or rather, determines whether to setup a third or first person view.
**/
void CLG_FinishViewValues( void ) {
    centity_t *ent;

    /**
    *   If we aren't even desiring third-person view, instantly prepare a first-person view.
    **/
    if ( cl_player_model->integer != CL_PLAYER_MODEL_THIRD_PERSON ) {
        goto setup_fps_view;
    }

    /**
    *   Ensure the client's own entity is valid for third-person view.
    **/
    ent = &clg_entities[ clgi.client->frame.ps.clientNumber + 1 ]; //ENTITY_FOR_NUMBER( clgi.client->frame.clientNum + 1 );//ent = &cl_entities[clgi.client->frame.clientNum + 1];
    if ( ent->serverframe != clgi.client->frame.number ) {
        goto setup_fps_view;
    }

    /**
    *   Need a model to display if we want to go third - person mode.
    **/
    if ( !ent->current.modelindex ) {
        goto setup_fps_view;
    }

    // Free to go into third-person view.
    CLG_SetupThirdPersionView();
    // Obviously exit here to prevent still setting up first-person view.
    return;

    /**
    *   Escape hatch to first - person view setup.
    **/
setup_fps_view:
    CLG_SetupFirstPersonView();
}

/**
 * @brief   Comparison function for qsort to sort entities by model and skin.
 * @param _a
 * @param _b
 * @return
 */
static int entitycmpfnc( const void *_a, const void *_b ) {
    const entity_t *a = (const entity_t *)_a;
    const entity_t *b = (const entity_t *)_b;

    // all other models are sorted by model then skin
    if ( a->model == b->model ) {
        return a->skin - b->skin;
    } else {
        return a->model - b->model;
    }
}

/**
*   @details    Whenever we have received a valid frame from the server, we will
*               build up a fresh refdef for rendering the scene from the player's point of view.
* 
*               In doing so we'll prepare the scene by clearing out the previous scene, transitioning
*               the entities to their new states, interpolating where necessary. Similarly the player's 
*               state will be predicted, the states transitioned and at last we'll
*               calculate the view values calculated from that.
**/
void CLG_DrawActiveViewState( void ) {
    // An invalid frame will just use the exact previous refdef
    // we can't use the old frame if the video mode has changed, though...
    if ( clgi.client->frame.valid ) {
        // Clear out the scene.
        CLG_ClearViewScene();

        // Calculate view and spatial audio listener origins.
        CLG_CalculateViewValues();
        // Finish it off by determing third or first -person view, and the required thirdperson/firstperson view model.
        CLG_FinishViewValues();

        // Now calculate the client's local PVS.
        clgi.V_CalculateLocalPVS( clgi.client->refdef.vieworg );

        // Build a refresh entity list
        CLG_PrepareViewEntities();

        #if USE_DEBUG
        if ( clg_testparticles->integer ) {
            V_TestParticles();
        }
        if ( clg_testentities->integer ) {
            V_TestEntities();
        }
        if ( clg_testlights->integer ) {
            V_TestLights();
        }
        if ( clg_testblend->integer ) {
            clgi.client->refdef.screen_blend[ 0 ] = 1;
            clgi.client->refdef.screen_blend[ 1 ] = 0.5f;
            clgi.client->refdef.screen_blend[ 2 ] = 0.25f;
            clgi.client->refdef.screen_blend[ 3 ] = 0.5f;
        }
        #endif

        //if(clg_flashlight->integer)
        //    V_Flashlight();

        #if 0
            // never let it sit exactly on a node line, because a water plane can
            // dissapear when viewed with the eye exactly on it.
            // the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
        clgi.client->refdef.vieworg[ 0 ] += 1.0f / 16;
        clgi.client->refdef.vieworg[ 1 ] += 1.0f / 16;
        clgi.client->refdef.vieworg[ 2 ] += 1.0f / 16;
        #endif
        vrect_t *scr_vrect = CLG_GetScreenVideoRect();
        clgi.client->refdef.x = scr_vrect->x;
        clgi.client->refdef.y = scr_vrect->y;
        clgi.client->refdef.width = scr_vrect->width;
        clgi.client->refdef.height = scr_vrect->height;

        // adjust for non-4/3 screens
        if ( cl_adjustfov->integer ) {
            clgi.client->refdef.fov_y = clgi.client->fov_y;
            clgi.client->refdef.fov_x = CLG_CalculateFieldOfView( clgi.client->refdef.fov_y, clgi.client->refdef.height, clgi.client->refdef.width );
        } else {
            clgi.client->refdef.fov_x = clgi.client->fov_x;
            clgi.client->refdef.fov_y = CLG_CalculateFieldOfView( clgi.client->refdef.fov_x, clgi.client->refdef.width, clgi.client->refdef.height );
        }

        // Setup the refresh def time to that of the client.
        clgi.client->refdef.time = clgi.client->time * 0.001f;

        // Determine the area bits for the PVS.
        if ( clgi.client->frame.areabytes ) {
            clgi.client->refdef.areabits = clgi.client->frame.areabits;
            // No area bits for this frame.
        } else {
            clgi.client->refdef.areabits = NULL;
        }

        /**
        *   Disable/Enable various scene elements based on the view CVars.
        *   Simply by setting the counts to zero, or for the blend, clear it.
        **/
        if ( !clg_view_add_entities->integer ) {
            clgi.client->viewScene.r_numentities = 0;
        }
        if ( !clg_view_add_particles->integer ) {
            clgi.client->viewScene.r_numparticles = 0;
        }
        if ( !clg_view_add_lights->integer ) {
            clgi.client->viewScene.r_numdlights = 0;
        }
        if ( !clg_view_add_blend->integer ) {
            Vector4Clear( clgi.client->refdef.screen_blend );
        }

        // Set the refdef scene data from the view scene data.
        clgi.client->refdef.num_entities = clgi.client->viewScene.r_numentities;
        clgi.client->refdef.entities = clgi.client->viewScene.r_entities;
        clgi.client->refdef.num_particles = clgi.client->viewScene.r_numparticles;
        clgi.client->refdef.particles = clgi.client->viewScene.r_particles;
        clgi.client->refdef.num_dlights = clgi.client->viewScene.r_numdlights;
        clgi.client->refdef.dlights = clgi.client->viewScene.r_dlights;
        clgi.client->refdef.lightstyles = clgi.client->viewScene.r_lightstyles;

        //clgi.client->refdef.rdflags = clgi.client->frame.ps.rdflags | CLG_GetViewRenderDefinitionFlags();
        clgi.client->refdef.rdflags = /*clgi.client->predictedFrame.ps.rdflags |*/ CLG_GetViewRenderDefinitionFlags();

        // sort entities for better cache locality
        qsort( clgi.client->refdef.entities, clgi.client->refdef.num_entities, sizeof( clgi.client->refdef.entities[ 0 ] ), entitycmpfnc );
    }

    // Render the scene.
    clgi.R_RenderFrame( &clgi.client->refdef );

    // 
    #if USE_DEBUG
    if ( clg_view_stats->integer ) {
        Com_Printf( "refdef_stats:[ r_entities:(#%i)  dlights:(#%i)  particles:(#%i) ]\n",
            clgi.client->refdef.num_entities,
            clgi.client->refdef.num_dlights,
            clgi.client->refdef.num_particles
        );
    }
    #endif
}