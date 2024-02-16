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

static cvar_t    *cl_nodelta;
static cvar_t    *cl_maxpackets;
static cvar_t    *cl_fuzzhack;
#if USE_DEBUG
static cvar_t    *cl_showpackets;
#endif
static cvar_t    *cl_instantpacket;
static cvar_t    *cl_batchcmds;

static cvar_t    *m_filter;


static cvar_t    *cl_upspeed;
static cvar_t    *cl_forwardspeed;
static cvar_t    *cl_sidespeed;
static cvar_t    *cl_yawspeed;
static cvar_t    *cl_pitchspeed;
static cvar_t    *cl_run;
static cvar_t    *cl_anglespeedkey;

static cvar_t    *freelook;
static cvar_t    *lookspring;
static cvar_t    *lookstrafe;

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

static cvar_t    *in_enable;
static cvar_t    *in_grab;

static bool IN_GetCurrentGrab(void)
{
    if (cls.active != ACT_ACTIVATED)
        return false;  // main window doesn't have focus

    if (r_config.flags & QVF_FULLSCREEN)
        return true;   // full screen

    if (cls.key_dest & (KEY_MENU | KEY_CONSOLE))
        return false;  // menu or console is up

    if ( sv_paused->integer )
        return false;   // game paused

    if (cls.state != ca_active && cls.state != ca_cinematic)
        return false;  // not connected

    if (in_grab->integer >= 2) {
        if (cls.demo.playback && !Key_IsDown(K_SHIFT))
            return false;  // playing a demo (and not using freelook)

        if (cl.frame.ps.pmove.pm_type == PM_FREEZE)
            return false;  // spectator mode
    }

    if (in_grab->integer >= 1)
        return true;   // regular playing mode

    return false;
}

/*
============
IN_Activate
============
*/
void IN_Activate(void)
{
    if (vid.grab_mouse) {
        vid.grab_mouse(IN_GetCurrentGrab());
    }
}

/*
============
IN_Restart_f
============
*/
static void IN_Restart_f(void)
{
    IN_Shutdown();
    IN_Init();
}

/*
============
IN_Frame
============
*/
void IN_Frame(void)
{
    if (input.modified) {
        IN_Restart_f();
    }
}

/*
================
IN_WarpMouse
================
*/
void IN_WarpMouse(int x, int y)
{
    if (vid.warp_mouse) {
        vid.warp_mouse(x, y);
    }
}

/*
============
IN_Shutdown
============
*/
void IN_Shutdown(void)
{
    if (in_grab) {
        in_grab->changed = NULL;
    }

    if (vid.shutdown_mouse) {
        vid.shutdown_mouse();
    }

    memset(&input, 0, sizeof(input));
}

static void in_changed_hard(cvar_t *self)
{
    input.modified = true;
}

static void in_changed_soft(cvar_t *self)
{
    IN_Activate();
}

/*
============
IN_Init
============
*/
void IN_Init(void)
{
    in_enable = Cvar_Get("in_enable", "1", 0);
    in_enable->changed = in_changed_hard;
    if (!in_enable->integer) {
        Com_Printf("Mouse input disabled.\n");
        return;
    }

    if (!vid.init_mouse()) {
        Cvar_Set("in_enable", "0");
        return;
    }

    in_grab = Cvar_Get("in_grab", "1", 0);
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

static keybutton_t    in_klook;
static keybutton_t    in_left, in_right, in_forward, in_back;
static keybutton_t    in_lookup, in_lookdown, in_moveleft, in_moveright;
static keybutton_t    in_strafe, in_speed, in_use, in_attack;
static keybutton_t    in_up, in_down;

//static keybutton_t    in_holster;

static int          in_impulse;
static bool         in_mlooking;

static void KeyDown(keybutton_t *b)
{
    int k;
    const char *c; // WID: C++20: Added const.

    c = Cmd_Argv(1);
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
        Com_WPrintf("Three keys down for a button!\n");
        return;
    }

    if ( b->state & BUTTON_STATE_HELD ) {
        return;        // still down
    }

    // save timestamp
    c = Cmd_Argv(2);
    b->downtime = atoi(c);
    if (!b->downtime) {
        b->downtime = com_eventTime - BASE_FRAMETIME;
    }

    b->state = static_cast<keybutton_state_t>( b->state | BUTTON_STATE_HELD | BUTTON_STATE_DOWN );//1 + 2;    // down + impulse down
}

static void KeyUp(keybutton_t *b)
{
    int k;
    const char *c; // WID: C++20: Added const.
    uint64_t uptime;

    c = Cmd_Argv(1);
    if ( c[ 0 ] ) {
        k = atoi( c );
    } else {
        // Typed manually at the console, assume for unsticking, so clear all.
        b->down[0] = b->down[1] = 0;
        b->state = static_cast<keybutton_state_t>( 0 );    // impulse up
        return;
    }

    if (b->down[0] == k) {
        b->down[0] = 0;
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
    c = Cmd_Argv(2);
    uptime = atoi(c);
    if (!uptime) {
        b->msec += BASE_FRAMETIME; // WID: 40hz: Used to be 10;
    } else if (uptime > b->downtime) {
        b->msec += uptime - b->downtime;
    }

    b->state = static_cast<keybutton_state_t>( b->state & ~BUTTON_STATE_HELD );        // now up
}

static void KeyClear(keybutton_t *b)
{
    b->msec = 0;
    b->state = static_cast<keybutton_state_t>( b->state & ~BUTTON_STATE_DOWN );        // clear impulses
    if (b->state & BUTTON_STATE_HELD ) {
        b->downtime = com_eventTime; // still down
    }
}

static void IN_KLookDown(void) { KeyDown(&in_klook); }
static void IN_KLookUp(void) { KeyUp(&in_klook); }
static void IN_UpDown(void) { KeyDown(&in_up); }
static void IN_UpUp(void) { KeyUp(&in_up); }
static void IN_DownDown(void) { KeyDown(&in_down); }
static void IN_DownUp(void) { KeyUp(&in_down); }
static void IN_LeftDown(void) { KeyDown(&in_left); }
static void IN_LeftUp(void) { KeyUp(&in_left); }
static void IN_RightDown(void) { KeyDown(&in_right); }
static void IN_RightUp(void) { KeyUp(&in_right); }
static void IN_ForwardDown(void) { KeyDown(&in_forward); }
static void IN_ForwardUp(void) { KeyUp(&in_forward); }
static void IN_BackDown(void) { KeyDown(&in_back); }
static void IN_BackUp(void) { KeyUp(&in_back); }
static void IN_LookupDown(void) { KeyDown(&in_lookup); }
static void IN_LookupUp(void) { KeyUp(&in_lookup); }
static void IN_LookdownDown(void) { KeyDown(&in_lookdown); }
static void IN_LookdownUp(void) { KeyUp(&in_lookdown); }
static void IN_MoveleftDown(void) { KeyDown(&in_moveleft); }
static void IN_MoveleftUp(void) { KeyUp(&in_moveleft); }
static void IN_MoverightDown(void) { KeyDown(&in_moveright); }
static void IN_MoverightUp(void) { KeyUp(&in_moveright); }
static void IN_SpeedDown(void) { KeyDown(&in_speed); }
static void IN_SpeedUp(void) { KeyUp(&in_speed); }
static void IN_StrafeDown(void) { KeyDown(&in_strafe); }
static void IN_StrafeUp(void) { KeyUp(&in_strafe); }

static void IN_AttackDown(void)
{
    KeyDown(&in_attack);

    if (cl_instantpacket->integer && cls.state == ca_active && !cls.demo.playback) {
        cl.sendPacketNow = true;
    }
}

static void IN_AttackUp(void)
{
    KeyUp(&in_attack);
}

static void IN_UseDown(void)
{
    KeyDown(&in_use);

    if (cl_instantpacket->integer && cls.state == ca_active && !cls.demo.playback) {
        cl.sendPacketNow = true;
    }
}

static void IN_UseUp(void)
{
    KeyUp(&in_use);
}

static void IN_Impulse(void)
{
    in_impulse = atoi(Cmd_Argv(1));
}

static void IN_CenterView(void)
{
    cl.viewangles[PITCH] = -/*SHORT2ANGLE*/(cl.frame.ps.pmove.delta_angles[PITCH]);
}

static void IN_MLookDown(void)
{
    in_mlooking = true;
}

static void IN_MLookUp(void)
{
    in_mlooking = false;

    if (!freelook->integer && lookspring->integer)
        IN_CenterView();
}

//static void IN_HolsterDown( void ) { KeyDown( &in_holster ); }
//static void IN_HolsterUp( void ) { KeyUp( &in_holster ); }

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
static double CL_KeyState(keybutton_t *key)
{
	uint64_t msec = key->msec;
    double val;

    if (key->state & BUTTON_STATE_HELD) {
        // still down
        if (com_eventTime > key->downtime) {
            msec += com_eventTime - key->downtime;
        }
    }

    // special case for instant packet
    if (!cl.moveCommand.cmd.msec) {
        return (double)(key->state & BUTTON_STATE_HELD );
    }

    val = (double)msec / cl.moveCommand.cmd.msec;

    return clamp(val, 0, 1);
}

//==========================================================================

// WID: C++20: Linkage
extern "C" {
	float autosens_x;
	float autosens_y;
};
/*
================
CL_MouseMove
================
*/
static void CL_MouseMove(void)
{
    int dx, dy;
    float mx, my;
    float speed;

    if (!vid.get_mouse_motion) {
        return;
    }
    if (cls.key_dest & (KEY_MENU | KEY_CONSOLE)) {
        return;
    }
    if (!vid.get_mouse_motion(&dx, &dy)) {
        return;
    }

    if (m_filter->integer) {
        mx = (dx + input.old_dx) * 0.5f;
        my = (dy + input.old_dy) * 0.5f;
    } else {
        mx = dx;
        my = dy;
    }

    input.old_dx = dx;
    input.old_dy = dy;

    if (!mx && !my) {
        return;
    }

    Cvar_ClampValue(m_accel, 0, 1);

    speed = sqrtf(mx * mx + my * my);
    speed = sensitivity->value + speed * m_accel->value;

    mx *= speed;
    my *= speed;

    if (m_autosens->integer) {
        mx *= cl.fov_x * autosens_x;
        my *= cl.fov_y * autosens_y;
    }

// add mouse X/Y movement
    if ((in_strafe.state & BUTTON_STATE_HELD ) || (lookstrafe->integer && !in_mlooking)) {
        cl.mousemove[1] += m_side->value * mx;
    } else {
        cl.viewangles[YAW] -= m_yaw->value * mx;
    }

    if ((in_mlooking || freelook->integer) && !(in_strafe.state & BUTTON_STATE_HELD )) {
        cl.viewangles[PITCH] += m_pitch->value * my * (m_invert->integer ? -1.f : 1.f);
    } else {
        cl.mousemove[0] -= m_forward->value * my;
    }
}


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
static void CL_AdjustAngles(int msec)
{
    double speed;

    if (in_speed.state & BUTTON_STATE_HELD )
        speed = msec * cl_anglespeedkey->value * 0.001f;
    else
        speed = msec * 0.001f;

    if (!(in_strafe.state & BUTTON_STATE_HELD )) {
        cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState(&in_right);
        cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState(&in_left);
    }
    if (in_klook.state & BUTTON_STATE_HELD ) {
        cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState(&in_forward);
        cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState(&in_back);
    }

    cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState(&in_lookup);
    cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState(&in_lookdown);
}

/*
================
CL_BaseMove

Build the intended movement vector
================
*/
static void CL_BaseMove( Vector3 &move ) {
    if ( in_strafe.state & BUTTON_STATE_HELD ) {
        move[1] += cl_sidespeed->value * CL_KeyState(&in_right);
        move[1] -= cl_sidespeed->value * CL_KeyState(&in_left);
    }

    move[1] += cl_sidespeed->value * CL_KeyState(&in_moveright);
    move[1] -= cl_sidespeed->value * CL_KeyState(&in_moveleft);

    move[2] += cl_upspeed->value * CL_KeyState(&in_up);
    move[2] -= cl_upspeed->value * CL_KeyState(&in_down);

    if ( !( in_klook.state & BUTTON_STATE_HELD ) ) {
        move[0] += cl_forwardspeed->value * CL_KeyState(&in_forward);
        move[0] -= cl_forwardspeed->value * CL_KeyState(&in_back);
    }

// adjust for speed key / running
    if ( ( in_speed.state & BUTTON_STATE_HELD ) ^ cl_run->integer ) {
        VectorScale(move, 2, move);
    }
}

static void CL_ClampSpeed( Vector3 &move ) {
    const float speed = 400;  // default (maximum) running speed
    move = QM_Vector3ClampValue( move, -speed, speed );
    //clamp(move[0], -speed, speed);
    //clamp(move[1], -speed, speed);
    //clamp(move[2], -speed, speed);
}

static void CL_ClampPitch(void)
{
    float pitch, angle;

    pitch = /*SHORT2ANGLE*/(cl.frame.ps.pmove.delta_angles[PITCH]);
    angle = cl.viewangles[PITCH] + pitch;

    if (angle < -180)
        angle += 360; // wrapped
    if (angle > 180)
        angle -= 360; // wrapped

    clamp(angle, -89, 89);
    cl.viewangles[PITCH] = angle - pitch;
}

/*
=================
CL_UpdateCmd

Updates msec, angles and builds interpolated movement vector for local prediction.
Doesn't touch command forward/side/upmove, these are filled by CL_FinalizeCmd.
=================
*/
void CL_UpdateCmd(int msec)
{
    VectorClear(cl.localmove);

    if (sv_paused->integer) {
        return;
    }

    // add to milliseconds of time to apply the move
    cl.moveCommand.cmd.msec += msec;

    // Store framenumber.
    cl.moveCommand.cmd.frameNumber = cl.frame.number;

    // Store times.
    cl.moveCommand.simulationTime = cl.time;
    cl.moveCommand.systemTime = cls.realtime;

    // adjust viewangles
    CL_AdjustAngles(msec);

    // get basic movement from keyboard, including jump/crouch.
    CL_BaseMove(cl.localmove);
    if ( in_up.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        cl.moveCommand.cmd.buttons |= BUTTON_JUMP;
    }
    if ( in_down.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        cl.moveCommand.cmd.buttons |= BUTTON_CROUCH;
    }

    // allow mice to add to the move
    CL_MouseMove();

    // add accumulated mouse forward/side movement
    cl.localmove[0] += cl.mousemove[0];
    cl.localmove[1] += cl.mousemove[1];

    // clamp to server defined max speed
    CL_ClampSpeed(cl.localmove);

    CL_ClampPitch();

    cl.moveCommand.cmd.angles[0] = /*ANGLE2SHORT*/(cl.viewangles[0]);
    cl.moveCommand.cmd.angles[1] = /*ANGLE2SHORT*/(cl.viewangles[1]);
    cl.moveCommand.cmd.angles[2] = /*ANGLE2SHORT*/(cl.viewangles[2]);
}

static void m_autosens_changed(cvar_t *self)
{
    float fov;

    if ( self->value > 90.0f && self->value <= 179.0f ) {
        fov = self->value;
    } else {
        fov = 90.0f;
    }

    autosens_x = 1.0f / fov;
    autosens_y = 1.0f / V_CalcFov(fov, 4, 3);
}

/*
============
CL_RegisterInput
============
*/
void CL_RegisterInput(void)
{
    Cmd_AddCommand("centerview", IN_CenterView);

    Cmd_AddCommand("+moveup", IN_UpDown);
    Cmd_AddCommand("-moveup", IN_UpUp);
    Cmd_AddCommand("+movedown", IN_DownDown);
    Cmd_AddCommand("-movedown", IN_DownUp);
    Cmd_AddCommand("+left", IN_LeftDown);
    Cmd_AddCommand("-left", IN_LeftUp);
    Cmd_AddCommand("+right", IN_RightDown);
    Cmd_AddCommand("-right", IN_RightUp);
    Cmd_AddCommand("+forward", IN_ForwardDown);
    Cmd_AddCommand("-forward", IN_ForwardUp);
    Cmd_AddCommand("+back", IN_BackDown);
    Cmd_AddCommand("-back", IN_BackUp);
    Cmd_AddCommand("+lookup", IN_LookupDown);
    Cmd_AddCommand("-lookup", IN_LookupUp);
    Cmd_AddCommand("+lookdown", IN_LookdownDown);
    Cmd_AddCommand("-lookdown", IN_LookdownUp);
    Cmd_AddCommand("+strafe", IN_StrafeDown);
    Cmd_AddCommand("-strafe", IN_StrafeUp);
    Cmd_AddCommand("+moveleft", IN_MoveleftDown);
    Cmd_AddCommand("-moveleft", IN_MoveleftUp);
    Cmd_AddCommand("+moveright", IN_MoverightDown);
    Cmd_AddCommand("-moveright", IN_MoverightUp);
    Cmd_AddCommand("+speed", IN_SpeedDown);
    Cmd_AddCommand("-speed", IN_SpeedUp);
    Cmd_AddCommand("+attack", IN_AttackDown);
    Cmd_AddCommand("-attack", IN_AttackUp);
    Cmd_AddCommand("+use", IN_UseDown);
    Cmd_AddCommand("-use", IN_UseUp);
    Cmd_AddCommand("impulse", IN_Impulse);
    Cmd_AddCommand("+klook", IN_KLookDown);
    Cmd_AddCommand("-klook", IN_KLookUp);
    Cmd_AddCommand("+mlook", IN_MLookDown);
    Cmd_AddCommand("-mlook", IN_MLookUp);

    Cmd_AddCommand("in_restart", IN_Restart_f);

    cl_nodelta = Cvar_Get("cl_nodelta", "0", 0);
	// WID: netstuff: Changed from 30, to actually, 40.
    cl_maxpackets = Cvar_Get("cl_maxpackets", "40", 0);
    cl_fuzzhack = Cvar_Get("cl_fuzzhack", "0", 0);
#if USE_DEBUG
    cl_showpackets = Cvar_Get("cl_showpackets", "0", 0);
#endif
    cl_instantpacket = Cvar_Get("cl_instantpacket", "1", 0);
    cl_batchcmds = Cvar_Get("cl_batchcmds", "1", 0);

    cl_upspeed = Cvar_Get("cl_upspeed", "200", 0);
    cl_forwardspeed = Cvar_Get("cl_forwardspeed", "200", 0);
    cl_sidespeed = Cvar_Get("cl_sidespeed", "200", 0);
    cl_yawspeed = Cvar_Get("cl_yawspeed", "140", 0);
    cl_pitchspeed = Cvar_Get("cl_pitchspeed", "150", CVAR_CHEAT);
    cl_anglespeedkey = Cvar_Get("cl_anglespeedkey", "1.5", CVAR_CHEAT);
    cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE);

    freelook = Cvar_Get("freelook", "1", CVAR_ARCHIVE);
    lookspring = Cvar_Get("lookspring", "0", CVAR_ARCHIVE);
    lookstrafe = Cvar_Get("lookstrafe", "0", CVAR_ARCHIVE);
    sensitivity = Cvar_Get("sensitivity", "3", CVAR_ARCHIVE);

	m_pitch = Cvar_Get("m_pitch", "0.022", CVAR_ARCHIVE);
	m_invert = Cvar_Get("m_invert", "0", CVAR_ARCHIVE);
    m_yaw = Cvar_Get("m_yaw", "0.022", 0);
    m_forward = Cvar_Get("m_forward", "1", 0);
    m_side = Cvar_Get("m_side", "1", 0);
    m_filter = Cvar_Get("m_filter", "0", 0);
    m_accel = Cvar_Get("m_accel", "0", 0);
    m_autosens = Cvar_Get("m_autosens", "0", 0);
    m_autosens->changed = m_autosens_changed;
    m_autosens_changed(m_autosens);
}

/*
=================
CL_FinalizeCmd

Builds the actual movement vector for sending to server. Assumes that msec
and angles are already set for this frame by CL_UpdateCmd.
=================
*/
void CL_FinalizeCmd(void)
{
    Vector3 move;

    // command buffer ticks in sync with cl_maxfps
    Cbuf_Frame( &cmd_buffer );
    Cbuf_Frame( &cl_cmdbuf );

    if (cls.state != ca_active) {
        goto clear; // not talking to a server
    }

    if (sv_paused->integer) {
        goto clear;
    }

//
// figure button bits
//
    if ( in_attack.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        cl.moveCommand.cmd.buttons |= BUTTON_ATTACK;
    }
    if ( in_use.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        cl.moveCommand.cmd.buttons |= BUTTON_USE;
    }
    //if ( in_use.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) )
    //    cl.moveCommand.cmd.buttons |= BUTTON_HOLSTER;
    if ( in_up.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        cl.moveCommand.cmd.buttons |= BUTTON_JUMP;
    }
    if ( in_down.state & ( BUTTON_STATE_HELD | BUTTON_STATE_DOWN ) ) {
        cl.moveCommand.cmd.buttons |= BUTTON_CROUCH;
    }

    //in_attack.state &= ~BUTTON_STATE_DOWN;
    //in_use.state &= ~BUTTON_STATE_DOWN;

    if (cls.key_dest == KEY_GAME && Key_AnyKeyDown()) {
        cl.moveCommand.cmd.buttons |= BUTTON_ANY;
    }

	// WID: 64-bit-frame: Should we messabout with this?
    if (cl.moveCommand.cmd.msec > 75) { // Was: > 250
        cl.moveCommand.cmd.msec = BASE_FRAMERATE;        // time was unreasonable
    }

    // rebuild the movement vector
    VectorClear(move);

    // get basic movement from keyboard
    CL_BaseMove(move);

    // add mouse forward/side movement
    move[0] += cl.mousemove[0];
    move[1] += cl.mousemove[1];

    // clamp to server defined max speed
    CL_ClampSpeed(move);

    // Store the movement vector
    cl.moveCommand.cmd.forwardmove = move[0];
    cl.moveCommand.cmd.sidemove = move[1];
    cl.moveCommand.cmd.upmove = move[2];

    // Store impulse.
    cl.moveCommand.cmd.impulse = in_impulse;

    // Store the frame number this was fired at.
    cl.moveCommand.cmd.frameNumber = cl.frame.number;

    // Store times.
    cl.moveCommand.simulationTime = cl.time;
    cl.moveCommand.systemTime = cls.realtime;

    // save this command off for prediction
    cl.currentUserCommandNumber++;
    cl.moveCommands[ cl.currentUserCommandNumber & CMD_MASK ] = cl.moveCommand;

clear:
    // clear pending cmd
    cl.moveCommand.cmd = {};

    // clear all states
    cl.mousemove[0] = 0;
    cl.mousemove[1] = 0;

    in_attack.state = static_cast<keybutton_state_t>( in_attack.state & ~BUTTON_STATE_DOWN );
    in_use.state = static_cast<keybutton_state_t>( in_use.state & ~BUTTON_STATE_DOWN );
    //in_holster.state &= ~BUTTON_STATE_DOWN;

    KeyClear(&in_right);
    KeyClear(&in_left);

    KeyClear(&in_moveright);
    KeyClear(&in_moveleft);

    KeyClear(&in_up);
    KeyClear(&in_down);

    KeyClear(&in_forward);
    KeyClear(&in_back);

    KeyClear(&in_lookup);
    KeyClear(&in_lookdown);

    in_impulse = 0;

}

static inline bool ready_to_send(void)
{
    uint64_t msec;

    if (cl.sendPacketNow) {
		//Com_DPrintf( "%s\n", "client_ready_to_send(cl.sendPacketNow): true" );
        return true;
    }
    if (cls.netchan.message.cursize || cls.netchan.reliable_ack_pending) {
        return true;
    }
    if (!cl_maxpackets->integer) {
		//Com_DPrintf( "%s\n", "client_ready_to_send (!cl_maxpackets->integer): true" );
        return true;
    }

	// WID: netstuff: Changed from 10, to actually, 40.
    if (cl_maxpackets->integer < BASE_FRAMERATE ) {
        Cvar_Set("cl_maxpackets", std::to_string( BASE_FRAMERATE ).c_str() );
    }

    msec = 1000 / cl_maxpackets->integer;
    if (msec) {
        msec = BASE_FRAMETIME / ( BASE_FRAMETIME / msec );
    }
    if (cls.realtime - cl.lastTransmitTime < msec) {
		//Com_DPrintf( "client_ready_to_send (cls.realtime - cl.lastTransmitTime < msec): false\n" );
		//Com_DPrintf( "cls.realtime(%i), cl.lastTransmitTime(%i), msec(%i)\n", cls.realtime, cl.lastTransmitTime, msec );
        return false;
    }

    return true;
}

static inline bool ready_to_send_hacked(void)
{
    if (!cl_fuzzhack->integer) {
        return true; // packet drop hack disabled
    }

    if (cl.currentUserCommandNumber - cl.lastTransmitCmdNumberReal > 2) {
        return true; // can't drop more than 2 cmds
    }

    return ready_to_send();
}

/*
=================
CL_SendDefaultCmd
=================
*/
static void CL_SendDefaultCmd(void)
{
    size_t cursize q_unused, checksumIndex;
    usercmd_t *cmd, *oldcmd;
    client_usercmd_history_t *history;

    // archive this packet
    history = &cl.history[cls.netchan.outgoing_sequence & CMD_MASK];
    history->commandNumber = cl.currentUserCommandNumber;
    history->timeSent = cls.realtime;    // for ping calculation
    history->timeReceived = 0;

    cl.lastTransmitCmdNumber = cl.currentUserCommandNumber;

    // see if we are ready to send this packet
    if (!ready_to_send_hacked()) {
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
        SZ_GetSpace(&msg_write, 1);
    //} else if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2) {
    //    version = cls.protocolVersion;
    //}

    // let the server know what the last frame we
    // got was, so the next message can be delta compressed
    if (cl_nodelta->integer || !cl.frame.valid /*|| cls.demowaiting*/) {
		MSG_WriteIntBase128(-1);   // no compression
    } else {
        MSG_WriteIntBase128(cl.frame.number);
    }

    // send this and the previous cmds in the message, so
    // if the last packet was dropped, it can be recovered
    cmd = &cl.moveCommands[(cl.currentUserCommandNumber - 2) & CMD_MASK].cmd;
    MSG_WriteDeltaUserCommand(NULL, cmd, cls.protocolVersion);
    oldcmd = cmd;

	cmd = &cl.moveCommands[ ( cl.currentUserCommandNumber - 1 ) & CMD_MASK ].cmd;
    MSG_WriteDeltaUserCommand(oldcmd, cmd, cls.protocolVersion);
    oldcmd = cmd;

	cmd = &cl.moveCommands[ ( cl.currentUserCommandNumber ) & CMD_MASK ].cmd;
    MSG_WriteDeltaUserCommand(oldcmd, cmd, cls.protocolVersion);

    if (cls.serverProtocol <= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
        // calculate a checksum over the move commands
        msg_write.data[checksumIndex] = COM_BlockSequenceCRCByte(
            msg_write.data + checksumIndex + 1,
            msg_write.cursize - checksumIndex - 1,
            cls.netchan.outgoing_sequence);
    }

    P_FRAMES++;

    //
    // deliver the message
    //
    cursize = NetchanQ2RTXPerimental_Transmit(&cls.netchan, msg_write.cursize, msg_write.data );
#if USE_DEBUG
    if (cl_showpackets->integer) {
        Com_Printf("%zu ", cursize);
    }
#endif

    SZ_Clear(&msg_write);
}

/*
=================
CL_SendBatchedCmd
=================
*/
static void CL_SendBatchedCmd( void ) {
	int i, j, seq, bits q_unused;
	int numCmds, numDups;
	int totalCmds, totalMsec;
	size_t cursize q_unused;
	usercmd_t *cmd, *oldcmd;
	client_usercmd_history_t *history, *oldest;
	//byte *patch;

	// see if we are ready to send this packet
	if ( !ready_to_send( ) ) {
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

	MSG_BeginWriting( );
	//Cvar_ClampInteger( cl_packetdup, 0, MAX_PACKET_FRAMES - 1 );
	//numDups = cl_packetdup->integer;
	numDups = MAX_PACKET_FRAMES - 1;

	// Check whether cl_nodelta is wished for, or in case of an invalid frame, so we can
	// let the server know what the last frame we got was, which in return allows
	// the next message to be delta compressed
	if ( cl_nodelta->integer || !cl.frame.valid /*|| cls.demowaiting*/ ) {
		MSG_WriteUint8( clc_move_nodelta );
		MSG_WriteUint8( numDups );
	} else {
		MSG_WriteUint8( clc_move_batched );
		MSG_WriteUint8( numDups );
		MSG_WriteIntBase128( cl.frame.number );
	}


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
	//MSG_FlushBits( );

	P_FRAMES++;

	//
	// deliver the message
	//
	cursize = NetchanQ2RTXPerimental_Transmit( &cls.netchan, msg_write.cursize, msg_write.data );
	#if USE_DEBUG
	if ( cl_showpackets->integer == 1 ) {
		Com_Printf( "%zu(%i) ", cursize, totalCmds );
	} else if ( cl_showpackets->integer == 2 ) {
		Com_Printf( "%zu(%i) ", cursize, totalMsec );
	} else if ( cl_showpackets->integer == 3 ) {
		Com_Printf( " | " );
	}
	#endif

	SZ_Clear( &msg_write );
}

static void CL_SendKeepAlive(void)
{
    client_usercmd_history_t *history;
    size_t cursize q_unused;

    // archive this packet
    history = &cl.history[cls.netchan.outgoing_sequence & CMD_MASK];
    history->commandNumber = cl.currentUserCommandNumber;
    history->timeSent = cls.realtime;    // for ping calculation
    history->timeReceived = 0;

    cl.lastTransmitTime = cls.realtime;
    cl.lastTransmitCmdNumber = cl.currentUserCommandNumber;
    cl.lastTransmitCmdNumberReal = cl.currentUserCommandNumber;

    cursize = NetchanQ2RTXPerimental_Transmit(&cls.netchan, 0, "" );
#if USE_DEBUG
    if (cl_showpackets->integer) {
        Com_Printf("%zu ", cursize);
    }
#endif
}

static void CL_SendUserinfo(void)
{
    char userinfo[MAX_INFO_STRING];
    cvar_t *var;
    int i;

    if (cls.userinfo_modified == MAX_PACKET_USERINFOS) {
        size_t len = Cvar_BitInfo(userinfo, CVAR_USERINFO);
        Com_DDPrintf("%s: %u: full update\n", __func__, com_framenum);
        MSG_WriteUint8(clc_userinfo);
        MSG_WriteData(userinfo, len + 1);
        MSG_FlushTo(&cls.netchan.message);
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

static void CL_SendReliable(void)
{
    if (cls.userinfo_modified) {
        CL_SendUserinfo();
        cls.userinfo_modified = 0;
    }

    if (cls.netchan.message.overflowed) {
        SZ_Clear(&cls.netchan.message);
        Com_Error(ERR_DROP, "Reliable message overflowed");
    }
}

void CL_SendCmd(void)
{
    if (cls.state < ca_connected) {
        return; // not talking to a server
    }

    // generate usercmds while playing a demo, but do not send them
    if (cls.demo.playback) {
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
    if (cl.lastTransmitCmdNumber == cl.currentUserCommandNumber) {
        return; // nothing to send
    }

    // send a userinfo update if needed
    CL_SendReliable();

	// Check whether to send batched cmd packets, or just a single regular default command packet.
	if ( cl_batchcmds->integer ) {
		CL_SendBatchedCmd( );
	} else {
		CL_SendDefaultCmd();
	}

    cl.sendPacketNow = false;
}

