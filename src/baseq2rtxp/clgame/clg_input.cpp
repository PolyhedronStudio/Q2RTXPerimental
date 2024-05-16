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
#include "clg_local.h"
#include "clg_input.h"

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
static keybutton_t    in_use, in_primary_fire, in_secondary_fire, in_reload;

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

static void IN_PrimaryFireDown( void ) {
    clgi.KeyDown( &in_primary_fire );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}

static void IN_PrimaryFireUp( void ) {
    clgi.KeyUp( &in_primary_fire );
}

static void IN_SecondaryFireDown( void ) {
    clgi.KeyDown( &in_secondary_fire );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}

static void IN_SecondaryFireUp( void ) {
    clgi.KeyUp( &in_secondary_fire );
}


static void IN_UseDown( void ) {
    clgi.KeyDown( &in_use );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}

static void IN_UseUp( void ) {
    clgi.KeyUp( &in_use );
}

static void IN_ReloadDown( void ) {
    clgi.KeyDown( &in_reload );

    if ( cl_instantpacket->integer && clgi.GetConnectionState() == ca_active && !clgi.IsDemoPlayback() ) {
        clgi.client->sendPacketNow = true;
    }
}

static void IN_ReloadUp( void ) {
    clgi.KeyUp( &in_reload );
}

static void IN_Impulse( void ) {
    in_impulse = atoi( clgi.Cmd_Argv( 1 ) );
}

static void IN_CenterView( void ) {
    clgi.client->viewangles[ PITCH ] = -/*SHORT2ANGLE*/( clgi.client->frame.ps.pmove.delta_angles[ PITCH ] );
}

static void IN_MLookDown( void ) {
    in_mlooking = true;
}

static void IN_MLookUp( void ) {
    in_mlooking = false;

    if ( !freelook->integer && lookspring->integer )
        IN_CenterView();
}

//! Called upon when mouse movement is detected.
void PF_MouseMove( const float deltaX, const float deltaY, const float moveX, const float moveY, const float speed ) {
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
    constexpr float speed = 400.f;  // default (maximum) running speed

    move = QM_Vector3ClampValue( move, -speed, speed );
}

/**
*   @brief  Adds the delta angles and clamps the viewangle's pitch appropriately. 
*/
static void CLG_ClampPitch( void ) {
    float pitch, angle;

    pitch = /*SHORT2ANGLE*/( clgi.client->frame.ps.pmove.delta_angles[ PITCH ] );
    angle = clgi.client->viewangles[ PITCH ] + pitch;

    if ( angle < -180 )
        angle += 360; // wrapped
    if ( angle > 180 )
        angle -= 360; // wrapped

    clamp( angle, -89, 89 );
    clgi.client->viewangles[ PITCH ] = angle - pitch;
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

    if ( in_up.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_JUMP;
    }
    if ( in_down.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_CROUCH;
    }
    if ( in_speed.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_WALK;
    }
    if ( in_strafe.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_STRAFE;
    }

    // Allow mice to add to the move
    if ( mouseMotion->hasMotion ) {
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
    Vector3 move;

    //
    // figure button bits
    //
    if ( in_primary_fire.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_PRIMARY_FIRE;
    }
    if ( in_secondary_fire.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_SECONDARY_FIRE;
    }
    if ( in_use.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_USE_ITEM;
    }
    if ( in_reload.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_RELOAD;
    }
    if ( in_up.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_JUMP;
    }
    if ( in_down.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        moveCommand->cmd.buttons |= BUTTON_CROUCH;
    }
    in_primary_fire.state = static_cast<keybutton_state_t>( in_primary_fire.state & ~BUTTON_STATE_DOWN );
    in_secondary_fire.state = static_cast<keybutton_state_t>( in_secondary_fire.state & ~BUTTON_STATE_DOWN );
    in_use.state = static_cast<keybutton_state_t>( in_use.state & ~BUTTON_STATE_DOWN );
    in_reload.state = static_cast<keybutton_state_t>( in_reload.state & ~BUTTON_STATE_DOWN );

    in_up.state = static_cast<keybutton_state_t>( in_up.state & ~BUTTON_STATE_DOWN );
    in_down.state = static_cast<keybutton_state_t>( in_down.state & ~BUTTON_STATE_DOWN );

    if ( clgi.GetKeyEventDestination() == KEY_GAME && clgi.Key_AnyKeyDown() ) {
        moveCommand->cmd.buttons |= BUTTON_ANY;
    }

    if ( moveCommand->cmd.msec > 75 ) { // Was: > 250
        // Time was unreasonable.
        moveCommand->cmd.msec = BASE_FRAMERATE; // Was: 100
    }

    // rebuild the movement vector
    VectorClear( move );

    // get basic movement from keyboard
    CLG_BaseMove( move );

    // add mouse forward/side movement
    move[ 0 ] += clgi.client->mousemove[ 0 ];
    move[ 1 ] += clgi.client->mousemove[ 1 ];

    // clamp to server defined max speed
    CLG_ClampSpeed( move );

    // Store the movement vector
    moveCommand->cmd.forwardmove = move[ 0 ];
    moveCommand->cmd.sidemove = move[ 1 ];
    moveCommand->cmd.upmove = move[ 2 ];

    // Store impulse.
    moveCommand->cmd.impulse = in_impulse;
}

/**
*   @brief  
**/
void PF_ClearMoveCommand( client_movecmd_t *moveCommand ) {
    // clear pending cmd
    moveCommand->cmd = {};

    in_primary_fire.state = static_cast<keybutton_state_t>( in_primary_fire.state & ~BUTTON_STATE_DOWN );
    in_secondary_fire.state = static_cast<keybutton_state_t>( in_secondary_fire.state & ~BUTTON_STATE_DOWN );
    in_use.state = static_cast<keybutton_state_t>( in_use.state & ~BUTTON_STATE_DOWN );
    in_reload.state = static_cast<keybutton_state_t>( in_reload.state & ~BUTTON_STATE_DOWN );

    in_up.state = static_cast<keybutton_state_t>( in_up.state & ~BUTTON_STATE_DOWN );
    in_down.state = static_cast<keybutton_state_t>( in_down.state & ~BUTTON_STATE_DOWN );
    //in_holster.state &= ~BUTTON_STATE_DOWN;

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
    clgi.Cmd_AddCommand( "+use", IN_UseDown );
    clgi.Cmd_AddCommand( "-use", IN_UseUp );
    clgi.Cmd_AddCommand( "+reload", IN_ReloadDown );
    clgi.Cmd_AddCommand( "-reload", IN_ReloadUp );
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