/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// cl.input.c  -- builds an intended movement command to send to the server

#include "cl_client.h"

static cvar_t *cl_nodelta;
static cvar_t *cl_maxpackets;
static cvar_t *cl_fuzzhack;
#if USE_DEBUG
static cvar_t *cl_showpackets;
#endif
//static cvar_t *cl_instantpacket;
static cvar_t *cl_batchcmds;

static cvar_t *m_filter;


//static cvar_t *cl_upspeed;
//static cvar_t *cl_forwardspeed;
//static cvar_t *cl_sidespeed;
//static cvar_t *cl_yawspeed;
//static cvar_t *cl_pitchspeed;
//static cvar_t *cl_run;
//static cvar_t *cl_anglespeedkey;

//static cvar_t *freelook;
//static cvar_t *lookspring;
//static cvar_t *lookstrafe;

// WID: C++20:
extern "C" {
    cvar_t *m_accel;
    cvar_t *m_autosens;
    cvar_t *m_pitch;
    cvar_t *m_invert;
    cvar_t *m_yaw;
};
static cvar_t *m_forward;
static cvar_t *m_side;
extern "C" {
    cvar_t *sensitivity;
};
/*
===============================================================================

INPUT SUBSYSTEM

===============================================================================
*/

typedef struct {
    bool        modified;
    int         old_dx;
    int         old_dy;
} in_state_t;

static in_state_t   input;

static cvar_t *in_enable;
static cvar_t *in_grab;

static bool IN_GetCurrentGrab( void ) {
    if ( cls.active != ACT_ACTIVATED )
        return false;  // main window doesn't have focus

    if ( r_config.flags & QVF_FULLSCREEN )
        return true;   // full screen

    if ( cls.key_dest & ( KEY_MENU | KEY_CONSOLE ) )
        return false;  // menu or console is up

    if ( sv_paused->integer )
        return false;   // game paused

    if ( cls.state != ca_active && cls.state != ca_cinematic )
        return false;  // not connected

    if ( in_grab->integer >= 2 ) {
        if ( cls.demo.playback && !Key_IsDown( K_SHIFT ) )
            return false;  // playing a demo (and not using freelook)

        if ( cl.frame.ps.pmove.pm_type == PM_FREEZE )
            return false;  // spectator mode
    }

    if ( in_grab->integer >= 1 )
        return true;   // regular playing mode

    return false;
}

/*
============
IN_Activate
============
*/
void IN_Activate( void ) {
    if ( vid.grab_mouse ) {
        vid.grab_mouse( IN_GetCurrentGrab() );
    }
}

/*
============
IN_Restart_f
============
*/
static void IN_Restart_f( void ) {
    IN_Shutdown();
    IN_Init();
}

/*
============
IN_Frame
============
*/
void IN_Frame( void ) {
    if ( input.modified ) {
        IN_Restart_f();
    }
}

/*
================
IN_WarpMouse
================
*/
void IN_WarpMouse( int x, int y ) {
    if ( vid.warp_mouse ) {
        vid.warp_mouse( x, y );
    }
}

/*
============
IN_Shutdown
============
*/
void IN_Shutdown( void ) {
    if ( in_grab ) {
        in_grab->changed = NULL;
    }

    if ( vid.shutdown_mouse ) {
        vid.shutdown_mouse();
    }

    memset( &input, 0, sizeof( input ) );
}

static void in_changed_hard( cvar_t *self ) {
    input.modified = true;
}

static void in_changed_soft( cvar_t *self ) {
    IN_Activate();
}

/*
============
IN_Init
============
*/
void IN_Init( void ) {
    in_enable = Cvar_Get( "in_enable", "1", 0 );
    in_enable->changed = in_changed_hard;
    if ( !in_enable->integer ) {
        Com_Printf( "Mouse input disabled.\n" );
        return;
    }

    if ( !vid.init_mouse() ) {
        Cvar_Set( "in_enable", "0" );
        return;
    }

    in_grab = Cvar_Get( "in_grab", "1", 0 );
    in_grab->changed = in_changed_soft;

    IN_Activate();
}


/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, bool down, unsigned time);

  +mlook src time

===============================================================================
*/

/**
*   @brief  Register a button as being 'held down'.
**/
void CL_KeyDown( keybutton_t *b ) {
    int k;
    const char *c; // WID: C++20: Added const.

    c = Cmd_Argv( 1 );
    if ( c[ 0 ] ) {
        k = atoi( c );
    } else {
        k = -1;        // typed manually at the console for continuous down
    }

    if ( k == b->down[ 0 ] || k == b->down[ 1 ] ) {
        return;        // repeating key
    }

    if ( !b->down[ 0 ] ) {
        b->down[ 0 ] = k;
    } else if ( !b->down[ 1 ] ) {
        b->down[ 1 ] = k;
    } else {
        Com_WPrintf( "Three keys down for a button!\n" );
        return;
    }

    if ( b->state & BUTTON_STATE_HELD ) {
        return;        // still down
    }

    // save timestamp
    c = Cmd_Argv( 2 );
    b->downtime = atoi( c );
    if ( !b->downtime ) {
        b->downtime = com_eventTime - BASE_FRAMETIME;
    }

    b->state = static_cast<keybutton_state_t>( b->state | BUTTON_STATE_HELD | BUTTON_STATE_DOWN );//1 + 2;    // down + impulse down
}

/**
*   @brief  Register a button as being 'released'.
**/
void CL_KeyUp( keybutton_t *b ) {
    int k;
    const char *c; // WID: C++20: Added const.
    uint64_t uptime;

    c = Cmd_Argv( 1 );
    if ( c[ 0 ] ) {
        k = atoi( c );
    } else {
        // Typed manually at the console, assume for unsticking, so clear all.
        b->down[ 0 ] = b->down[ 1 ] = 0;
        b->state = static_cast<keybutton_state_t>( 0 );    // impulse up
        return;
    }

    if ( b->down[ 0 ] == k ) {
        b->down[ 0 ] = 0;
    } else if ( b->down[ 1 ] == k ) {
        b->down[ 1 ] = 0;
    } else {
        return;        // key up without coresponding down (menu pass through)
    }
    if ( b->down[ 0 ] || b->down[ 1 ] ) {
        return;        // some other key is still holding it down
    }
    if ( !( b->state & BUTTON_STATE_HELD ) ) {
        return;        // still up (this should not happen)
    }

    // save timestamp
    c = Cmd_Argv( 2 );
    uptime = atoi( c );
    if ( !uptime ) {
        b->msec += BASE_FRAMETIME; // WID: 40hz: Used to be 10;
    } else if ( uptime > b->downtime ) {
        b->msec += uptime - b->downtime;
    }

    b->state = static_cast<keybutton_state_t>( b->state & ~BUTTON_STATE_HELD );        // now up
}

/**
*   @brief  Clear out a key's down state and msec, but maintain track of whether it is 'held'.
**/
void CL_KeyClear( keybutton_t *b ) {
    b->msec = 0;
    b->state = static_cast<keybutton_state_t>( b->state & ~BUTTON_STATE_DOWN );        // clear impulses
    if ( b->state & BUTTON_STATE_HELD ) {
        b->downtime = com_eventTime; // still down
    }
}

/**
*   @brief  Returns the fraction of the command frame's interval for which the key was 'down'.
**/
const double CL_KeyState( keybutton_t *key ) {
    uint64_t msec = key->msec;
    double val;

    if ( key->state & BUTTON_STATE_HELD ) {
        // still down
        if ( com_eventTime > key->downtime ) {
            msec += com_eventTime - key->downtime;
        }
    }

    // special case for instant packet
    if ( !cl.moveCommand.cmd.msec ) {
        return (double)( key->state & BUTTON_STATE_HELD );
    }

    val = (double)msec / cl.moveCommand.cmd.msec;

    return clamp( val, 0, 1 );
}

//==========================================================================

// WID: C++20: Linkage
extern "C" {
    float autosens_x;
    float autosens_y;
};

/**
*   @brief  A somewhat of a hack, we first read in mouse-movement, to then later on in
*           UpdateCommand actually allow it to be handled and added to our local movement.
**/
static const client_mouse_motion_t CL_MouseMove( void ) {
    // Clear motion struct.
    client_mouse_motion_t motion = { .hasMotion = false };

    if ( !vid.get_mouse_motion ) {
        return motion;
    }
    if ( cls.key_dest & ( KEY_MENU | KEY_CONSOLE ) ) {
        return motion;
    }
    if ( !vid.get_mouse_motion( &motion.deltaX, &motion.deltaY ) ) {
        return motion;
    }

    if ( m_filter->integer ) {
        motion.moveX = ( motion.deltaX + input.old_dx ) * 0.5f;
        motion.moveY = ( motion.deltaY + input.old_dy ) * 0.5f;
    } else {
        motion.moveX = motion.deltaX;
        motion.moveY = motion.deltaY;
    }

    input.old_dx = motion.deltaX;
    input.old_dy = motion.deltaY;

    if ( !motion.moveX && !motion.moveY ) {
        return motion;
    }

    // We got actual mouse motion, so set hasMotion to true.
    motion.hasMotion = true;

    Cvar_ClampValue( m_accel, 0, 1 );

    motion.speed = sqrtf( motion.moveX * motion.moveX + motion.moveY * motion.moveY );
    motion.speed = sensitivity->value + motion.speed * m_accel->value;

    motion.moveX *= motion.speed;
    motion.moveY *= motion.speed;

    if ( m_autosens->integer ) {
        motion.moveX *= cl.fov_x * autosens_x;
        motion.moveY *= cl.fov_y * autosens_y;
    }

    // Return final mouse movement motion result.
    return motion;
}

/**
*   @brief  Updates msec, angles and builds the interpolated movement vector for local movement prediction.
*           Doesn't touch command forward/side/upmove, these are filled by CL_FinalizeCommand.
**/
void CL_UpdateCommand( int64_t msec ) {
    // Always clear out our local move.
    VectorClear( cl.localmove );

    if ( sv_paused->integer ) {
        return;
    }

    // add to milliseconds of time to apply the move
    cl.moveCommand.cmd.msec += msec;

    // Store framenumber.
    cl.moveCommand.cmd.frameNumber = cl.frame.number;

    // Store times.
    cl.moveCommand.prediction.time = cl.time;//cl.moveCommand.simulationTime = cl.time;
    //cl.moveCommand.systemTime = cls.realtime;

    // Catch mouse input data to pass into updateCommand.
    client_mouse_motion_t mouseMotion = CL_MouseMove();

    // Call into UpdateCommand for updating our local prediction move command.
    if ( clge ) {
        clge->UpdateMoveCommand( msec, &cl.moveCommand, &mouseMotion );
    }
}

/**
*   @brief  
**/
static void m_autosens_changed( cvar_t *self ) {
    float fov;

    if ( self->value > 90.0f && self->value <= 179.0f ) {
        fov = self->value;
    } else {
        fov = 90.0f;
    }

    autosens_x = 1.0f / fov;
    autosens_y = 1.0f / V_CalcFov( fov, 4, 3 );
}

/**
*   @brief  Create all move related cvars, and registers all input commands.
*           (Also gives the client game a chance.)
**/
void CL_RegisterInput( void ) {
    Cmd_AddCommand( "in_restart", IN_Restart_f );

    cl_nodelta = Cvar_Get( "cl_nodelta", "0", 0 );
    // WID: netstuff: Changed from 30, to actually, 40.
    cl_maxpackets = Cvar_Get( "cl_maxpackets", "40", 0 );
    cl_fuzzhack = Cvar_Get( "cl_fuzzhack", "0", 0 );
    #if USE_DEBUG
    cl_showpackets = Cvar_Get( "cl_showpackets", "0", 0 );
    #endif
    cl_batchcmds = Cvar_Get( "cl_batchcmds", "1", 0 );

    sensitivity = Cvar_Get( "sensitivity", "3", CVAR_ARCHIVE );

    m_pitch = Cvar_Get( "m_pitch", "0.022", CVAR_ARCHIVE );
    m_invert = Cvar_Get( "m_invert", "0", CVAR_ARCHIVE );
    m_yaw = Cvar_Get( "m_yaw", "0.022", 0 );
    m_forward = Cvar_Get( "m_forward", "1", 0 );
    m_side = Cvar_Get( "m_side", "1", 0 );
    m_filter = Cvar_Get( "m_filter", "0", 0 );
    m_accel = Cvar_Get( "m_accel", "0", 0 );
    m_autosens = Cvar_Get( "m_autosens", "0", 0 );
    m_autosens->changed = m_autosens_changed;
    m_autosens_changed( m_autosens );

    // Call upon the client game module's register user input.
    if ( clge ) {
        clge->RegisterUserInput();
    }
}

/**
*   @brief  Builds the actual movement vector for sending to server. Assumes that msec
*           and angles are already set for this frame by CL_UpdateCommand.
**/
void CL_FinalizeCommand( void ) {
    Vector3 move;

    // Command buffer ticks in sync with cl_maxfps.
    Cbuf_Frame( &cmd_buffer );
    Cbuf_Frame( &cl_cmdbuf );

    if ( cls.state != ca_active ) {
        goto clear; // not talking to a server
    }

    if ( sv_paused->integer ) {
        goto clear;
    }

    // Pass control to client game for finalizing the move command.
    if ( clge ) {
        clge->FinalizeMoveCommand( &cl.moveCommand );
    }

    // Store the frame number this was fired at.
    cl.moveCommand.cmd.frameNumber = cl.frame.number;

    // Store times.
    cl.moveCommand.prediction.time = cl.time;
    //cl.moveCommand.systemTime = cls.realtime;

    // save this command off for prediction
    cl.currentUserCommandNumber++;
    cl.moveCommands[ cl.currentUserCommandNumber & CMD_MASK ] = cl.moveCommand;

clear:
    // Clear mouse move state.
    cl.mousemove[ 0 ] = 0;
    cl.mousemove[ 1 ] = 0;

    // Let the client game handle it from here on.
    if ( clge ) {
        clge->ClearMoveCommand( &cl.moveCommand );
    }
}

/**
*   @brief  Returns true if ready to send.
**/
static inline bool ready_to_send( void ) {
    uint64_t msec;

    if ( cl.sendPacketNow ) {
        //Com_DPrintf( "%s\n", "client_ready_to_send(cl.sendPacketNow): true" );
        return true;
    }
    if ( cls.netchan.message.cursize || cls.netchan.reliable_ack_pending ) {
        return true;
    }
    if ( !cl_maxpackets->integer ) {
        //Com_DPrintf( "%s\n", "client_ready_to_send (!cl_maxpackets->integer): true" );
        return true;
    }

    // WID: Batched Cmds seem to break prediction and I am unsure if it is the prediction
    // itself or the fact that the server might be doing an extra ClienThink(move) for the
    // last use command. However if we allow cl_maxpackets to interfere with cl_batchcmds
    // we end up with pushers that give 'odd' spikes in their move direction.
    //
    // Eventually ended up with a solution, and that is to not care for cl_maxpackets when
    // one is using cl_batchcmds :-) Not content about it, but it works for now.
    if ( cl_batchcmds->integer ) {
        //Com_DPrintf( "%s\n", "client_ready_to_send (cl_batchcmds->integer): true" );
        return true;
    }

    // WID: netstuff: Changed from 10, to actually, 40.
    if ( cl_maxpackets->integer < BASE_FRAMERATE ) {
        Cvar_Set( "cl_maxpackets", std::to_string( BASE_FRAMERATE ).c_str() );
    }

    msec = 1000 / cl_maxpackets->integer;
    if ( msec ) {
        msec = BASE_FRAMETIME / ( BASE_FRAMETIME / msec );
    }
    if ( cls.realtime - cl.lastTransmitTime < msec ) {
        //Com_DPrintf( "client_ready_to_send (cls.realtime - cl.lastTransmitTime < msec): false\n" );
        //Com_DPrintf( "cls.realtime(%i), cl.lastTransmitTime(%i), msec(%i)\n", cls.realtime, cl.lastTransmitTime, msec );
        return false;
    }

    return true;
}

/**
*   @brief  Returns true if ready to send.
**/
static inline bool ready_to_send_hacked( void ) {
    if ( !cl_fuzzhack->integer ) {
        return true; // packet drop hack disabled
    }

    if ( cl.currentUserCommandNumber - cl.lastTransmitCmdNumberReal > 2 ) {
        return true; // can't drop more than 2 cmds
    }

    return ready_to_send();
}

/**
*   @brief  Will send the last 3 move commands available. 
**/
static void CL_SendDefaultCmd( void ) {
    size_t cursize q_unused, checksumIndex;
    usercmd_t *cmd, *oldcmd;
    client_usercmd_history_t *history;

    // archive this packet
    history = &cl.history[ cls.netchan.outgoing_sequence & CMD_MASK ];
    history->commandNumber = cl.currentUserCommandNumber;
    history->timeSent = cls.realtime;    // for ping calculation
    history->timeReceived = 0;

    cl.lastTransmitCmdNumber = cl.currentUserCommandNumber;

    // see if we are ready to send this packet
    if ( !ready_to_send_hacked() ) {
        cls.netchan.outgoing_sequence++; // just drop the packet
        return;
    }

    cl.lastTransmitTime = cls.realtime;
    cl.lastTransmitCmdNumberReal = cl.currentUserCommandNumber;

    // begin a client move command
    MSG_WriteUint8( clc_move );

    // save the position for a checksum byte
    checksumIndex = 0;
    //if (cls.serverProtocol <= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
    checksumIndex = msg_write.cursize;
    SZ_GetSpace( &msg_write, 1 );
    //} else if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2) {
    //    version = cls.protocolVersion;
    //}

    // let the server know what the last frame we
    // got was, so the next message can be delta compressed
    if ( cl_nodelta->integer || !cl.frame.valid /*|| cls.demowaiting*/ ) {
        MSG_WriteIntBase128( -1 );   // no compression
    } else {
        MSG_WriteIntBase128( cl.frame.number );
    }

    // send this and the previous cmds in the message, so
    // if the last packet was dropped, it can be recovered
    cmd = &cl.moveCommands[ ( cl.currentUserCommandNumber - 2 ) & CMD_MASK ].cmd;
    MSG_WriteDeltaUserCommand( NULL, cmd, cls.protocolVersion );
    oldcmd = cmd;

    cmd = &cl.moveCommands[ ( cl.currentUserCommandNumber - 1 ) & CMD_MASK ].cmd;
    MSG_WriteDeltaUserCommand( oldcmd, cmd, cls.protocolVersion );
    oldcmd = cmd;

    cmd = &cl.moveCommands[ ( cl.currentUserCommandNumber ) & CMD_MASK ].cmd;
    MSG_WriteDeltaUserCommand( oldcmd, cmd, cls.protocolVersion );

    if ( cls.serverProtocol <= PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {
        // calculate a checksum over the move commands
        msg_write.data[ checksumIndex ] = COM_BlockSequenceCRCByte(
            msg_write.data + checksumIndex + 1,
            msg_write.cursize - checksumIndex - 1,
            cls.netchan.outgoing_sequence );
    }

    P_FRAMES++;

    //
    // deliver the message
    //
    cursize = NetchanQ2RTXPerimental_Transmit( &cls.netchan, msg_write.cursize, msg_write.data );
    #if USE_DEBUG
    if ( cl_showpackets->integer ) {
        Com_Printf( "%zu ", cursize );
    }
    #endif

    SZ_Clear( &msg_write );
}

/**
*   @brief  Will send up to (MAX_PACKET_FRAMES - 1) of the most recent move commands. 'Batching'.
**/
static void CL_SendBatchedCmd( void ) {
    int i, j, seq, bits q_unused;
    int numCmds, numDups;
    int totalCmds, totalMsec;
    size_t cursize q_unused;
    usercmd_t *cmd, *oldcmd;
    client_usercmd_history_t *history, *oldest;
    //byte *patch;

    // see if we are ready to send this packet
    if ( !ready_to_send() ) {
        return;
    }

    // archive this packet
    seq = cls.netchan.outgoing_sequence;
    history = &cl.history[ seq & CMD_MASK ];
    history->commandNumber = cl.currentUserCommandNumber;
    history->timeSent = cls.realtime;    // for ping calculation
    history->timeReceived = 0;

    cl.lastTransmitTime = cls.realtime;
    cl.lastTransmitCmdNumber = cl.currentUserCommandNumber;
    cl.lastTransmitCmdNumberReal = cl.currentUserCommandNumber;

    MSG_BeginWriting();
    //Cvar_ClampInteger( cl_packetdup, 0, MAX_PACKET_FRAMES - 1 );
    //numDups = cl_packetdup->integer;
    numDups = 2;// MAX_PACKET_FRAMES - 1;

    // Check whether cl_nodelta is wished for, or in case of an invalid frame, so we can
    // let the server know what the last frame we got was, which in return allows
    // the next message to be delta compressed
    if ( cl_nodelta->integer || !cl.frame.valid /*|| cls.demowaiting*/ ) {
        MSG_WriteUint8( clc_move_nodelta );
    } else {
        MSG_WriteUint8( clc_move_batched );
        MSG_WriteIntBase128( cl.frame.number );
    }

    MSG_WriteUint8( numDups );

    // send this and the previous cmds in the message, so
    // if the last packet was dropped, it can be recovered
    oldcmd = NULL;
    totalCmds = 0;
    totalMsec = 0;
    for ( i = seq - numDups; i <= seq; i++ ) {
        oldest = &cl.history[ ( i - 1 ) & CMD_MASK ];
        history = &cl.history[ i & CMD_MASK ];

        numCmds = history->commandNumber - oldest->commandNumber;
        if ( numCmds >= MAX_PACKET_USERCMDS ) {
            Com_WPrintf( "%s: MAX_PACKET_USERCMDS exceeded\n", __func__ );
            SZ_Clear( &msg_write );
            break;
        }
        totalCmds += numCmds;
        //MSG_WriteBits( numCmds, 5 );
        MSG_WriteUint8( numCmds );
        for ( j = oldest->commandNumber + 1; j <= history->commandNumber; j++ ) {
            cmd = &cl.moveCommands[ j & CMD_MASK ].cmd;
            totalMsec += cmd->msec;
            bits = MSG_WriteDeltaUserCommand( oldcmd, cmd, cls.serverProtocol );
            #if USE_DEBUG
            if ( cl_showpackets->integer == 3 ) {
                MSG_ShowDeltaUserCommandBits( bits );
            }
            #endif
            oldcmd = cmd;
        }
    }

    //MSG_FlushTo( &msg_write );
    MSG_FlushBits( );

    P_FRAMES++;

    //
    // deliver the message
    //
    cursize = NetchanQ2RTXPerimental_Transmit( &cls.netchan, msg_write.cursize, msg_write.data );
    #if USE_DEBUG
    if ( cl_showpackets->integer == 1 ) {
        Com_Printf( "%zu(%i) \n", cursize, totalCmds );
    } else if ( cl_showpackets->integer == 2 ) {
        Com_Printf( "%zu(%i) \n", cursize, totalMsec );
    } else if ( cl_showpackets->integer == 3 ) {
        Com_Printf( " | \n" );
    }
    #endif

    SZ_Clear( &msg_write );
}

/**
*   @brief  Sends a 'keep-alive' to prevent us from timing out.
**/
static void CL_SendKeepAlive( void ) {
    client_usercmd_history_t *history;
    size_t cursize q_unused;

    // archive this packet
    history = &cl.history[ cls.netchan.outgoing_sequence & CMD_MASK ];
    history->commandNumber = cl.currentUserCommandNumber;
    history->timeSent = cls.realtime;    // for ping calculation
    history->timeReceived = 0;

    cl.lastTransmitTime = cls.realtime;
    cl.lastTransmitCmdNumber = cl.currentUserCommandNumber;
    cl.lastTransmitCmdNumberReal = cl.currentUserCommandNumber;

    cursize = NetchanQ2RTXPerimental_Transmit( &cls.netchan, 0, "" );
    #if USE_DEBUG
    if ( cl_showpackets->integer ) {
        Com_Printf( "%zu ", cursize );
    }
    #endif
}

/**
*   @brief  Write the userinfo message data to send buffer.
**/
static void CL_SendUserinfo( void ) {
    char userinfo[ MAX_INFO_STRING ];
    cvar_t *var;
    int i;

    if ( cls.userinfo_modified == MAX_PACKET_USERINFOS ) {
        size_t len = Cvar_BitInfo( userinfo, CVAR_USERINFO );
        Com_DDPrintf( "%s: %u: full update\n", __func__, com_framenum );
        MSG_WriteUint8( clc_userinfo );
        MSG_WriteData( userinfo, len + 1 );
        MSG_FlushTo( &cls.netchan.message );
    } else {
        Com_DDPrintf( "%s: %u: %d updates\n", __func__, com_framenum,
            cls.userinfo_modified );
        for ( i = 0; i < cls.userinfo_modified; i++ ) {
            var = cls.userinfo_updates[ i ];
            MSG_WriteUint8( clc_userinfo_delta );
            MSG_WriteString( var->name );
            if ( var->flags & CVAR_USERINFO ) {
                MSG_WriteString( var->string );
            } else {
                // no longer in userinfo
                MSG_WriteString( NULL );
            }
        }
        MSG_FlushTo( &cls.netchan.message );
    }
    //} else {
    //    Com_WPrintf("%s: update count is %d, should never happen.\n",
    //                __func__, cls.userinfo_modified);
    //}
}

/**
*   @brief  Send reliable user data.
**/
static void CL_SendReliable( void ) {
    if ( cls.userinfo_modified ) {
        CL_SendUserinfo();
        cls.userinfo_modified = 0;
    }

    if ( cls.netchan.message.overflowed ) {
        SZ_Clear( &cls.netchan.message );
        Com_Error( ERR_DROP, "Reliable message overflowed" );
    }
}

/**
*   @brief  Sends the current pending command to server.
**/
void CL_SendCommand( void ) {
    if ( cls.state < ca_connected ) {
        return; // not talking to a server
    }

    // generate usercmds while playing a demo, but do not send them
    if ( cls.demo.playback ) {
        return;
    }

    if ( cls.state != ca_active || sv_paused->integer ) {
        // send a userinfo update if needed
        CL_SendReliable();

        // just keepalive and/oor update reliable
        if ( NetchanQ2RTXPerimental_ShouldUpdate( &cls.netchan ) ) {
            CL_SendKeepAlive();
        }

        cl.sendPacketNow = false;
        return;
    }

    // are there any new usercmds to send after all?
    if ( cl.lastTransmitCmdNumber == cl.currentUserCommandNumber ) {
        return; // nothing to send
    }

    // send a userinfo update if needed
    CL_SendReliable();

    // Check whether to send batched cmd packets, or just a single regular default command packet.
    if ( cl_batchcmds->integer ) {
        CL_SendBatchedCmd();
    } else {
        CL_SendDefaultCmd();
    }

    cl.sendPacketNow = false;
}

