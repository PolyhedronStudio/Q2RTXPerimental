/********************************************************************
*
*
*	ClientGame: User Input.
* 
*	Continuous button event tracking is complicated by the fact that two different
*	input sources (say, mouse button 1 and the control key) can both press the
*	same button, but the button should only be released when both of the
*	pressing key have been released.
*
*	When a key event issues a button command (+forward, +attack, etc), it appends
*	its key number as a parameter to the command so it can be matched up with
*	the release.
*
*	state bit 0 is the current state of the key
*	state bit 1 is edge triggered on the up to down transition
*	state bit 2 is edge triggered on the down to up transition
*
*
*	Key_Event (int key, bool down, unsigned time);
*
*	+mlook src time
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_input.h"

/**
*
*   CVars
*
**/
cvar_t *cl_instantpacket = nullptr; 

cvar_t *m_invert = nullptr;
cvar_t *m_pitch = nullptr;
cvar_t *m_yaw = nullptr;
cvar_t *m_forward = nullptr;
cvar_t *m_side = nullptr;

static cvar_t *freelook = nullptr;
static cvar_t *lookspring = nullptr;
static cvar_t *lookstrafe = nullptr;

static cvar_t *cl_upspeed;
static cvar_t *cl_forwardspeed;
static cvar_t *cl_sidespeed;
static cvar_t *cl_yawspeed;
static cvar_t *cl_pitchspeed;
static cvar_t *cl_run;
static cvar_t *cl_anglespeedkey;


/**
* 
*   KeyButtons
* 
**/
//! Keyboard 'look'.
static keybutton_t    in_klook;
//! Directional inputs.
static keybutton_t    in_left, in_right, in_forward, in_back;
//! Look up/down and X/Y movement.
static keybutton_t    in_lookup, in_lookdown, in_moveleft, in_moveright;
//! Strafe and movement modifiers.
static keybutton_t    in_strafe, in_speed;
//! Up/Down directional Z movement.
static keybutton_t    in_up, in_down;
//static keybutton_t    in_holster;

//! Weapon modifiers.
static keybutton_t    in_primary_fire;
static keybutton_t    in_secondary_fire;
static keybutton_t    in_reload;
//! Use Inventory/Target modifiers.
static keybutton_t    in_use_target;
static keybutton_t    in_use_item;

//! Impulses.
static int32_t	in_impulse;
//! Whether mouse looking, or not.
static bool		in_mlooking;



/**
*
*
*	Input:
*
*
**/
static void IN_KLookDown( void ) { clgi.KeyDown( &in_klook ); }
static void IN_KLookUp( void ) { clgi.KeyUp( &in_klook ); }
static void IN_UpDown( void ) { clgi.KeyDown( &in_up ); }
static void IN_UpUp( void ) { clgi.KeyUp( &in_up ); }
static void IN_DownDown( void ) { clgi.KeyDown( &in_down ); }
static void IN_DownUp( void ) { clgi.KeyUp( &in_down ); }
static void IN_LeftDown( void ) { clgi.KeyDown( &in_left ); }
static void IN_LeftUp( void ) { clgi.KeyUp( &in_left ); }
static void IN_RightDown( void ) { clgi.KeyDown( &in_right ); }
static void IN_RightUp( void ) { clgi.KeyUp( &in_right ); }
static void IN_ForwardDown( void ) { clgi.KeyDown( &in_forward ); }
static void IN_ForwardUp( void ) { clgi.KeyUp( &in_forward ); }
static void IN_BackDown( void ) { clgi.KeyDown( &in_back ); }
static void IN_BackUp( void ) { clgi.KeyUp( &in_back ); }
static void IN_LookupDown( void ) { clgi.KeyDown( &in_lookup ); }
static void IN_LookupUp( void ) { clgi.KeyUp( &in_lookup ); }
static void IN_LookdownDown( void ) { clgi.KeyDown( &in_lookdown ); }
static void IN_LookdownUp( void ) { clgi.KeyUp( &in_lookdown ); }
static void IN_MoveleftDown( void ) { clgi.KeyDown( &in_moveleft ); }
static void IN_MoveleftUp( void ) { clgi.KeyUp( &in_moveleft ); }
static void IN_MoverightDown( void ) { clgi.KeyDown( &in_moveright ); }
static void IN_MoverightUp( void ) { clgi.KeyUp( &in_moveright ); }
static void IN_SpeedDown( void ) { clgi.KeyDown( &in_speed ); }
static void IN_SpeedUp( void ) { clgi.KeyUp( &in_speed ); }
static void IN_StrafeDown( void ) { clgi.KeyDown( &in_strafe ); }
static void IN_StrafeUp( void ) { clgi.KeyUp( &in_strafe ); }
static void IN_Impulse( void ) {
    in_impulse = atoi( clgi.Cmd_Argv( 1 ) );
}
static void IN_CenterView( void ) {
    clgi.client->viewangles[ PITCH ] = -/*SHORT2ANGLE*/QM_AngleMod( clgi.client->frame.ps.pmove.delta_angles[ PITCH ] );
}
static void IN_MLookDown( void ) {
    in_mlooking = true;
}
static void IN_MLookUp( void ) {
    in_mlooking = false;

    if ( !freelook->integer && lookspring->integer )
        IN_CenterView();
}
/**
*
*   The following few button callbacks ensure that we instantly send out the usercmd 
*   message to ensure there is no delay. Giving a responsive user experience.
*
**/
/**
*   @brief  Primary Fire Down.
**/
static void IN_PrimaryFireDown( void ) {
    clgi.KeyDown( &in_primary_fire );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}
/**
*   @brief  Primary Fire Up.
**/
static void IN_PrimaryFireUp( void ) {
    clgi.KeyUp( &in_primary_fire );
}

/**
*   @brief  Secondary Fire Down.
**/
static void IN_SecondaryFireDown( void ) {
    clgi.KeyDown( &in_secondary_fire );
    
    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}
/**
*   @brief  Secondary Fire Up.
**/
static void IN_SecondaryFireUp( void ) {
    clgi.KeyUp( &in_secondary_fire );
}

/**
*   @brief  When the 'Use Inventory Item" button goes down.
**/
static void IN_UseItemDown( void ) {
    clgi.KeyDown( &in_use_item );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}
/**
*   @brief  When the 'Use Inventory Item" button goes up.
**/
static void IN_UseItemUp( void ) {
    clgi.KeyUp( &in_use_item );
}

/**
*   @brief  When the 'Use target entity we point at" button goes down.
**/
static void IN_UseTargetDown( void ) {
    clgi.KeyDown( &in_use_target );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}
/**
*   @brief  When the 'Use target entity we point at" button goes up.
**/
static void IN_UseTargetUp( void ) {
    clgi.KeyUp( &in_use_target );
}

/**
*   @brief  Reload Weapon Down.
**/
static void IN_ReloadDown( void ) {
    clgi.KeyDown( &in_reload );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}
/**
*   @brief  Reload Weapon Up.
**/
static void IN_ReloadUp( void ) {
    clgi.KeyUp( &in_reload );
}



//! Called upon when mouse movement is detected.
void CLG_MouseMotionMove( client_mouse_motion_t *mouseMotion ) {
    //if ( ( in_strafe.state & BUTTON_STATE_HELD ) || ( lookstrafe->integer && !in_mlooking ) ) {
    //    clgi.client->mousemove[ 1 ] += m_side->value * moveX;
    //} else {
    //    clgi.client->viewangles[ YAW ] -= m_yaw->value * moveX;
    //}

    //if ( ( in_mlooking || freelook->integer ) && !( in_strafe.state & BUTTON_STATE_HELD ) ) {
    //    clgi.client->viewangles[ PITCH ] += m_pitch->value * moveY * ( m_invert->integer ? -1.f : 1.f );
    //} else {
    //    clgi.client->mousemove[ 0 ] -= m_forward->value * moveY;
    //}
    if ( ( in_strafe.state & BUTTON_STATE_HELD ) || ( lookstrafe->integer && !in_mlooking ) ) {
        clgi.client->mousemove[ 1 ] += m_side->value * mouseMotion->moveX;
    } else {
        clgi.client->viewangles[ YAW ] -= m_yaw->value * mouseMotion->moveX;
    }

    if ( ( in_mlooking || freelook->integer ) && !( in_strafe.state & BUTTON_STATE_HELD ) ) {
        clgi.client->viewangles[ PITCH ] += m_pitch->value * mouseMotion->moveY * ( m_invert->integer ? -1.f : 1.f );
    } else {
        clgi.client->mousemove[ 0 ] -= m_forward->value * mouseMotion->moveY;
    }
}

/**
*   @brief  Moves the local angle positions
**/
static void CLG_AdjustAngles( const int64_t msec ) {
    double speed;

    if ( in_speed.state & BUTTON_STATE_HELD ) {
        speed = msec * cl_anglespeedkey->value * 0.001f;
    } else {
        speed = msec * 0.001f;
    }

    if ( !( in_strafe.state & BUTTON_STATE_HELD ) ) {
        clgi.client->viewangles[ YAW ] -= speed * cl_yawspeed->value * clgi.KeyState( &in_right );
        clgi.client->viewangles[ YAW ] += speed * cl_yawspeed->value * clgi.KeyState( &in_left );
    }
    if ( in_klook.state & BUTTON_STATE_HELD ) {
        clgi.client->viewangles[ PITCH ] -= speed * cl_pitchspeed->value * clgi.KeyState( &in_forward );
        clgi.client->viewangles[ PITCH ] += speed * cl_pitchspeed->value * clgi.KeyState( &in_back );
    }

    clgi.client->viewangles[ PITCH ] -= speed * cl_pitchspeed->value * clgi.KeyState( &in_lookup );
    clgi.client->viewangles[ PITCH ] += speed * cl_pitchspeed->value * clgi.KeyState( &in_lookdown );
}

/**
*   @brief  Build the intended movement vector
**/
static void CLG_BaseMove( Vector3 &move ) {
    if ( in_strafe.state & BUTTON_STATE_HELD ) {
        move[ 1 ] += cl_sidespeed->value * clgi.KeyState( &in_right );
        move[ 1 ] -= cl_sidespeed->value * clgi.KeyState( &in_left );
    }

    move[ 1 ] += cl_sidespeed->value * clgi.KeyState( &in_moveright );
    move[ 1 ] -= cl_sidespeed->value * clgi.KeyState( &in_moveleft );

    move[ 2 ] += cl_upspeed->value * clgi.KeyState( &in_up );
    move[ 2 ] -= cl_upspeed->value * clgi.KeyState( &in_down );

    if ( !( in_klook.state & BUTTON_STATE_HELD ) ) {
        move[ 0 ] += cl_forwardspeed->value * clgi.KeyState( &in_forward );
        move[ 0 ] -= cl_forwardspeed->value * clgi.KeyState( &in_back );
    }

    // adjust for speed key / running
    if ( ( in_speed.state & BUTTON_STATE_HELD ) ^ cl_run->integer ) {
        VectorScale( move, 2, move );
    }
}

/**
*   @brief  
**/
static void CLG_ClampSpeed( Vector3 &move ) {
    // Determine the speed limit value to account for.
    float speed = default_pmoveParams_t::pm_max_speed; // default (maximum) running speed
    // For 'flying' aka noclip or spectating.
    if ( clgi.client->frame.ps.pmove.pm_type == PM_SPECTATOR
        || clgi.client->frame.ps.pmove.pm_type == PM_NOCLIP ) {
        speed = default_pmoveParams_t::pm_fly_speed;
    }
    // For in case we're 'swimming'.
    if ( game.predictedState.liquid.level >= cm_liquid_level_t::LIQUID_WAIST ) {
        speed = default_pmoveParams_t::pm_water_speed;
    }

    move = QM_Vector3Clamp( move,
        {
            -speed,
            -speed,
            -speed,
        },
        {
            speed,
            speed,
            speed,
        }
    );
}

/**
*   @brief  Adds the delta angles and clamps the viewangle's pitch appropriately. 
*/
static void CLG_ClampPitch( void ) {
    //const float pitch = /*SHORT2ANGLE*/( clgi.client->frame.ps.pmove.delta_angles[ PITCH ] );
    const double pitch = /*SHORT2ANGLE*/QM_AngleMod( clgi.client->frame.ps.pmove.delta_angles[ PITCH ] );
    double angle = clgi.client->viewangles[ PITCH ] + pitch;
    // Wrap around.
    if ( angle < -180. ) {
        angle += 360.; // wrapped
    }
    if ( angle > 180. ) {
        angle -= 360.; // wrapped
    }
    // Clamp pitch angle.
    angle = std::clamp( angle, -89., 89. );
    clgi.client->viewangles[ PITCH ] = angle - pitch;
}

/**
*   @brief  Figures the button bits.
**/
static void CLG_FigureButtonBits( client_movecmd_t *moveCommand ) {
    if ( in_primary_fire.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_PRIMARY_FIRE;
    }
    if ( in_secondary_fire.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_SECONDARY_FIRE;
    }

    if ( in_use_target.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_USE_TARGET;
    }
    if ( in_use_item.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_USE_ITEM;
    }
    if ( in_reload.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_RELOAD;
    }

    if ( in_strafe.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_STRAFE;
    }
    if ( in_up.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_JUMP;
    }
    if ( in_down.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_CROUCH;
    }
    if ( in_speed.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_WALK;
    }
}

/**
*   @brief  Clear state_down flags for various in_ keys.
**/
void CLG_ClearStateDownFlags( client_movecmd_t *moveCommand ) {
    in_primary_fire.state = static_cast<keybutton_state_t>( in_primary_fire.state & ~BUTTON_STATE_DOWN );
    in_secondary_fire.state = static_cast<keybutton_state_t>( in_secondary_fire.state & ~BUTTON_STATE_DOWN );
    
    in_use_target.state = static_cast<keybutton_state_t>( in_use_target.state & ~BUTTON_STATE_DOWN );
    in_use_item.state = static_cast<keybutton_state_t>( in_use_item.state & ~BUTTON_STATE_DOWN );
    in_reload.state = static_cast<keybutton_state_t>( in_reload.state & ~BUTTON_STATE_DOWN );

    in_speed.state = static_cast<keybutton_state_t>( in_speed.state & ~BUTTON_STATE_DOWN );
    in_strafe.state = static_cast<keybutton_state_t>( in_strafe.state & ~BUTTON_STATE_DOWN );
    in_up.state = static_cast<keybutton_state_t>( in_up.state & ~BUTTON_STATE_DOWN );
    in_down.state = static_cast<keybutton_state_t>( in_down.state & ~BUTTON_STATE_DOWN );
}

/**
*   @brief  Updates msec, angles and builds the interpolated movement vector for local movement prediction.
*           Doesn't touch command forward/side/upmove, these are filled by CL_FinalizeCommand.
**/
void PF_UpdateMoveCommand( const int64_t msec, client_movecmd_t *moveCommand, client_mouse_motion_t *mouseMotion ) {
    // adjust viewangles
    CLG_AdjustAngles( msec );

    // get basic movement from keyboard, including jump/crouch.
    CLG_BaseMove( clgi.client->localmove );

    //
    // Figure button bits.
    //
    CLG_FigureButtonBits( moveCommand );

    //
    // Clear Various Down States.
    //
    CLG_ClearStateDownFlags( moveCommand );

    // Allow mice to add to the move
    if ( mouseMotion->hasMotion ) {
        CLG_MouseMotionMove( mouseMotion );
    }

    // Add accumulated mouse forward/side movement.
    clgi.client->localmove[ 0 ] += clgi.client->mousemove[ 0 ];
    clgi.client->localmove[ 1 ] += clgi.client->mousemove[ 1 ];

    // clamp to server defined max speed
    CLG_ClampSpeed( clgi.client->localmove );

    CLG_ClampPitch();

    moveCommand->cmd.angles[ 0 ] = /*ANGLE2SHORT*/( clgi.client->viewangles[ 0 ] );
    moveCommand->cmd.angles[ 1 ] = /*ANGLE2SHORT*/( clgi.client->viewangles[ 1 ] );
    moveCommand->cmd.angles[ 2 ] = /*ANGLE2SHORT*/( clgi.client->viewangles[ 2 ] );
}

/**
*   @brief  Builds the actual movement vector for sending to server. Assumes that msec
*           and angles are already set for this frame by CL_UpdateCommand.
**/
void PF_FinalizeMoveCommand( client_movecmd_t *moveCommand ) {

    //
    // Figure button bits.
    //
    CLG_FigureButtonBits( moveCommand );

    //
    // Clear Various Down States.
    //
    CLG_ClearStateDownFlags( moveCommand );

    // Any key down flag.
    if ( clgi.GetKeyEventDestination() == KEY_GAME && clgi.Key_AnyKeyDown() ) {
        moveCommand->cmd.buttons |= BUTTON_ANY;
    }

    if ( moveCommand->cmd.msec > 75 ) { // Was: > 250
        // Time was unreasonable.
        moveCommand->cmd.msec = BASE_FRAMERATE; // Was: 100
        // Debug display.
        #if USE_DEBUG
        clgi.Print( PRINT_DEVELOPER, "%s: moveCommand->cmd.msec(%i) was unreasonable(max is 75)!\n", __func__, moveCommand->cmd.msec );
        #endif
    }

    // rebuild the movement vector
    Vector3 move = QM_Vector3Zero();

    // get basic movement from keyboard
    CLG_BaseMove( move );

    // add mouse forward/side movement
    move.x += clgi.client->mousemove.x;
    move.y += clgi.client->mousemove.y;

    // clamp to server defined max speed
    CLG_ClampSpeed( move );

    // Store the movement vector
    moveCommand->cmd.forwardmove = move.x;
    moveCommand->cmd.sidemove = move.y;
    moveCommand->cmd.upmove = move.z;

    // Store impulse.
    moveCommand->cmd.impulse = in_impulse;
}

/**
*   @brief  
**/
void PF_ClearMoveCommand( client_movecmd_t *moveCommand ) {
    // clear pending cmd
    moveCommand->cmd = {};

    CLG_ClearStateDownFlags( moveCommand );

    clgi.KeyClear( &in_right );
    clgi.KeyClear( &in_left );

    clgi.KeyClear( &in_moveright );
    clgi.KeyClear( &in_moveleft );

    clgi.KeyClear( &in_up );
    clgi.KeyClear( &in_down );

    clgi.KeyClear( &in_forward );
    clgi.KeyClear( &in_back );

    clgi.KeyClear( &in_lookup );
    clgi.KeyClear( &in_lookdown );

    in_impulse = 0;
}

/**
*   @brief    
**/
void PF_RegisterUserInput( void ) {
    clgi.Cmd_AddCommand( "centerview", IN_CenterView );

    clgi.Cmd_AddCommand( "+moveup", IN_UpDown );
    clgi.Cmd_AddCommand( "-moveup", IN_UpUp );
    clgi.Cmd_AddCommand( "+movedown", IN_DownDown );
    clgi.Cmd_AddCommand( "-movedown", IN_DownUp );
    clgi.Cmd_AddCommand( "+left", IN_LeftDown );
    clgi.Cmd_AddCommand( "-left", IN_LeftUp );
    clgi.Cmd_AddCommand( "+right", IN_RightDown );
    clgi.Cmd_AddCommand( "-right", IN_RightUp );
    clgi.Cmd_AddCommand( "+forward", IN_ForwardDown );
    clgi.Cmd_AddCommand( "-forward", IN_ForwardUp );
    clgi.Cmd_AddCommand( "+back", IN_BackDown );
    clgi.Cmd_AddCommand( "-back", IN_BackUp );
    clgi.Cmd_AddCommand( "+lookup", IN_LookupDown );
    clgi.Cmd_AddCommand( "-lookup", IN_LookupUp );
    clgi.Cmd_AddCommand( "+lookdown", IN_LookdownDown );
    clgi.Cmd_AddCommand( "-lookdown", IN_LookdownUp );
    clgi.Cmd_AddCommand( "+strafe", IN_StrafeDown );
    clgi.Cmd_AddCommand( "-strafe", IN_StrafeUp );
    clgi.Cmd_AddCommand( "+moveleft", IN_MoveleftDown );
    clgi.Cmd_AddCommand( "-moveleft", IN_MoveleftUp );
    clgi.Cmd_AddCommand( "+moveright", IN_MoverightDown );
    clgi.Cmd_AddCommand( "-moveright", IN_MoverightUp );
    clgi.Cmd_AddCommand( "+speed", IN_SpeedDown );
    clgi.Cmd_AddCommand( "-speed", IN_SpeedUp );
    clgi.Cmd_AddCommand( "+fire_prim", IN_PrimaryFireDown );
    clgi.Cmd_AddCommand( "-fire_prim", IN_PrimaryFireUp );
    clgi.Cmd_AddCommand( "+fire_sec", IN_SecondaryFireDown );
    clgi.Cmd_AddCommand( "-fire_sec", IN_SecondaryFireUp );
    clgi.Cmd_AddCommand( "+useitem", IN_UseItemDown );
    clgi.Cmd_AddCommand( "-useitem", IN_UseItemUp );
    clgi.Cmd_AddCommand( "+reload", IN_ReloadDown );
    clgi.Cmd_AddCommand( "-reload", IN_ReloadUp );
    clgi.Cmd_AddCommand( "+usetarget", IN_UseTargetDown );
    clgi.Cmd_AddCommand( "-usetarget", IN_UseTargetUp );
    clgi.Cmd_AddCommand( "impulse", IN_Impulse );
    clgi.Cmd_AddCommand( "+klook", IN_KLookDown );
    clgi.Cmd_AddCommand( "-klook", IN_KLookUp );
    clgi.Cmd_AddCommand( "+mlook", IN_MLookDown );
    clgi.Cmd_AddCommand( "-mlook", IN_MLookUp );

    // Get access to cvars.
    // (Due to some being shared by freecam.c in /refresh/vkpt, we are creating them in the client).
    m_pitch = clgi.CVar_Get( "m_pitch", nullptr, 0 );
    m_invert = clgi.CVar_Get( "m_invert", nullptr, 0 );
    m_yaw = clgi.CVar_Get( "m_yaw", nullptr, 0 );
    m_forward = clgi.CVar_Get( "m_forward", nullptr, 0 );
    m_side = clgi.CVar_Get( "m_side", nullptr, 0 );

    // client game cvars:
    cl_instantpacket = clgi.CVar_Get( "cl_instantpacket", "1", 0 );

    cl_upspeed = clgi.CVar_Get( "cl_upspeed", "200", 0 );
    cl_forwardspeed = clgi.CVar_Get( "cl_forwardspeed", "200", 0 );
    cl_sidespeed = clgi.CVar_Get( "cl_sidespeed", "200", 0 );
    cl_yawspeed = clgi.CVar_Get( "cl_yawspeed", "140", 0 );
    cl_pitchspeed = clgi.CVar_Get( "cl_pitchspeed", "150", CVAR_CHEAT );
    cl_anglespeedkey = clgi.CVar_Get( "cl_anglespeedkey", "1.5", CVAR_CHEAT );
    cl_run = clgi.CVar_Get( "cl_run", "1", CVAR_ARCHIVE );

    freelook = clgi.CVar_Get( "freelook", "1", CVAR_ARCHIVE );
    lookspring = clgi.CVar_Get( "lookspring", "0", CVAR_ARCHIVE );
    lookstrafe = clgi.CVar_Get( "lookstrafe", "0", CVAR_ARCHIVE );
}