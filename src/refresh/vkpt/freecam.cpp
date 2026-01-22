/*
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

/********************************************************************
*
*
*	Module Name: FreeCam System
*
*	The FreeCam system provides a free-floating camera mode that activates
*	during game pause (cl_paused == 2 && sv_paused). It intercepts all game
*	input and redirects it to camera control, allowing cinematic screenshots
*	and scene inspection without gameplay interference.
*
*	Camera Controls:
*	  - W/S/A/D/Q/E: Move forward/back/left/right/up/down
*	  - Mouse1 (drag): Rotate camera pitch/yaw
*	  - Mouse1 + Mouse2: Roll camera
*	  - Mouse2 (drag): Adjust camera zoom (FOV)
*	  - Shift: Increase movement speed (5x)
*	  - Ctrl: Decrease movement speed (0.1x)
*
*	Depth of Field (when pt_dof enabled):
*	  - Mouse wheel: Adjust focus distance
*	  - Shift + Mouse wheel: Adjust aperture size
*	  - Ctrl: Fine adjustment mode (1.01x vs 1.1x factor)
*
*	Visual Changes:
*	  - First camera movement removes gun from view
*	  - Player model switches to third-person perspective
*	  - Accumulation buffer resets on any camera or zoom change
*
*	State Management:
*	  - Saves original camera state on activation
*	  - Restores player model setting on deactivation
*	  - Automatically resets when exiting pause mode
*
*
********************************************************************/

#include "vkpt.h"
#include "../../client/cl_client.h"
/**
*
*
*
*	FreeCam State and External Variables:
*
*
*
**/

static vec3_t freecam_vieworg = { 0.f };
static vec3_t freecam_viewangles = { 0.f };
static float freecam_zoom = 1.f;
static bool freecam_keystate[6] = { 0 };	//! Movement keys: forward, back, right, left, up, down
static bool freecam_active = false;
static int freecam_player_model = 0;		//! Saved player model setting

extern float autosens_x;
extern float autosens_y;
extern cvar_t *m_accel;
extern cvar_t *m_autosens;
extern cvar_t *m_pitch;
extern cvar_t *m_invert;
extern cvar_t *m_yaw;
extern cvar_t *sensitivity;
extern cvar_t *cvar_pt_dof;
extern cvar_t *cvar_pt_aperture;
extern cvar_t *cvar_pt_focus;
extern cvar_t *cvar_pt_freecam;

/**
*
*
*
*	FreeCam Control Functions:
*
*
*
**/

/**
*	@brief	Reset freecam mode and restore player model setting.
*
*	Called when exiting pause mode or disabling freecam. Restores the
*	original player model cvar value that was saved on freecam activation.
**/
void vkpt_freecam_reset()
{
	if( !freecam_active )
		return;

	Cvar_SetByVar( cl_player_model, va( "%d", freecam_player_model ), FROM_CODE );
	freecam_active = false;
}

/**
*	@brief	Process mouse movement for freecam camera control.
*
*	Handles three mouse modes:
*	  - Mouse1: Rotate camera (pitch/yaw)
*	  - Mouse1 + Mouse2: Roll camera
*	  - Mouse2: Adjust zoom level (0.5x to 20x)
*
*	Mouse sensitivity respects game sensitivity settings including acceleration,
*	auto-sensitivity for FOV, and pitch inversion. Zoom affects rotation speed.
**/
static void vkpt_freecam_mousemove( void )
{
	int dx, dy;
	float mx, my;
	float speed;
	float fovX = 0;
	float fovY = 0;

	if( !vid.get_mouse_motion )
		return;

	if( !vid.get_mouse_motion( &dx, &dy ) )
		return;

	mx = dx;
	my = dy;

	if( !mx && !my ) {
		return;
	}

	if( Key_IsDown( K_MOUSE1 ) && Key_IsDown( K_MOUSE2 ) ) {
		// Roll camera
		mx *= sensitivity->value;
		freecam_viewangles[ROLL] += m_yaw->value * mx;
	} else if( Key_IsDown( K_MOUSE1 ) ) {
		// Rotate camera with acceleration and sensitivity
		Cvar_ClampValue( m_accel, 0, 1 );

		speed = sqrtf( mx * mx + my * my );
		speed = sensitivity->value + speed * m_accel->value;

		mx *= speed;
		my *= speed;

		// Apply FOV-based auto-sensitivity scaling
		if( m_autosens->integer ) {
			CL_RefExport_GetFieldOfViewXY( &fovX, &fovY );
			mx *= fovX * autosens_x;
			my *= fovY * autosens_y;
		}

		// Scale by current zoom level
		mx /= freecam_zoom;
		my /= freecam_zoom;

		freecam_viewangles[YAW] -= m_yaw->value * mx;
		freecam_viewangles[PITCH] += m_pitch->value * my * ( m_invert->integer ? -1.f : 1.f );

		// Clamp pitch to valid range
		freecam_viewangles[PITCH] = max( -90.f, min( 90.f, freecam_viewangles[PITCH] ) );
	} else if( Key_IsDown( K_MOUSE2 ) ) {
		// Adjust zoom level (0.5x to 20x)
		freecam_zoom *= powf( 0.5f, my * m_pitch->value * 0.1f );
		freecam_zoom = max( 0.5f, min( 20.f, freecam_zoom ) );
	}
}

/**
*	@brief	Update freecam state and apply camera transformations.
*
*	Called once per frame. Processes movement input, applies mouse control,
*	and updates the refdef with new camera position and orientation. Handles
*	activation/deactivation based on pause state and pt_freecam cvar.
*
*	@param	frame_time	Time delta in seconds for movement scaling.
*
*	@note	Switches player model to third-person on first movement.
*	@note	Resets accumulation buffer when camera or zoom changes.
**/
void vkpt_freecam_update( float frame_time )
{
	// Check if freecam should be active
	if( cl_paused->integer != 2 || !sv_paused->integer || !cvar_pt_freecam->integer ) {
		vkpt_freecam_reset();
		return;
	}

	// Initialize freecam on first activation
	if( !freecam_active ) {
		VectorCopy( vkpt_refdef.fd->vieworg, freecam_vieworg );
		VectorCopy( vkpt_refdef.fd->viewangles, freecam_viewangles );
		freecam_zoom = 1.f;
		freecam_player_model = cl_player_model->integer;
		freecam_active = true;
	}

	// Save previous state for change detection
	vec3_t prev_vieworg;
	vec3_t prev_viewangles;
	VectorCopy( freecam_vieworg, prev_vieworg );
	VectorCopy( freecam_viewangles, prev_viewangles );
	float prev_zoom = freecam_zoom;

	// Build velocity vector from keystate
	vec3_t velocity = { 0.f };
	if( freecam_keystate[0] ) velocity[0] += 1.f;	// Forward
	if( freecam_keystate[1] ) velocity[0] -= 1.f;	// Back
	if( freecam_keystate[2] ) velocity[1] += 1.f;	// Right
	if( freecam_keystate[3] ) velocity[1] -= 1.f;	// Left
	if( freecam_keystate[4] ) velocity[2] += 1.f;	// Up
	if( freecam_keystate[5] ) velocity[2] -= 1.f;	// Down

	// Apply speed modifiers
	if( Key_IsDown( K_SHIFT ) )
		VectorScale( velocity, 5.f, velocity );
	else if( Key_IsDown( K_CTRL ) )
		VectorScale( velocity, 0.1f, velocity );

	// Apply movement in camera-relative space
	vec3_t forward, right, up;
	AngleVectors( freecam_viewangles, forward, right, up );
	float speed = 100.f;
	VectorMA( freecam_vieworg, velocity[0] * frame_time * speed, forward, freecam_vieworg );
	VectorMA( freecam_vieworg, velocity[1] * frame_time * speed, right, freecam_vieworg );
	VectorMA( freecam_vieworg, velocity[2] * frame_time * speed, up, freecam_vieworg );

	vkpt_freecam_mousemove();

	// Update refdef with new camera state
	VectorCopy( freecam_vieworg, vkpt_refdef.fd->vieworg );
	VectorCopy( freecam_viewangles, vkpt_refdef.fd->viewangles );
	
	// Apply zoom to FOV
	vkpt_refdef.fd->fov_x = RAD2DEG( atanf( tanf( DEG2RAD( vkpt_refdef.fd->fov_x ) * 0.5f ) / freecam_zoom ) ) * 2.f;
	vkpt_refdef.fd->fov_y = RAD2DEG( atanf( tanf( DEG2RAD( vkpt_refdef.fd->fov_y ) * 0.5f ) / freecam_zoom ) ) * 2.f;

	// Handle player model and accumulation on camera change
	if( !VectorCompare( freecam_vieworg, prev_vieworg ) || !VectorCompare( freecam_viewangles, prev_viewangles ) ) {
		// Switch to third-person on first movement
		if( freecam_player_model != CL_PLAYER_MODEL_DISABLED && cl_player_model->integer != CL_PLAYER_MODEL_THIRD_PERSON )
			Cvar_SetByVar( cl_player_model, va( "%d", CL_PLAYER_MODEL_THIRD_PERSON ), FROM_CODE );

		vkpt_reset_accumulation();
	}

	if( freecam_zoom != prev_zoom ) {
		// Zoom changes require accumulation reset but not player model change
		vkpt_reset_accumulation();
	}
}

/**
*	@brief	Intercept and handle key input for freecam and DOF controls.
*
*	Called for all key events during pause. Returns true if the key was
*	handled by freecam/DOF system, preventing it from affecting gameplay.
*
*	@param	key		Key code (K_* constants).
*	@param	down	True if key pressed, false if released.
*	@return	True if key was handled, false to pass through to game.
*
*	@note	Always allows "pause" binding to pass through for unpause.
**/
bool R_InterceptKey_RTX( unsigned key, bool down )
{
	if( cl_paused->integer != 2 || !sv_paused->integer )
		return false;

	// Allow pause key to pass through for unpause
	const char* kb = Key_GetBindingForKey( key );
	if( kb && strstr( kb, "pause" ) )
		return false;

	// Handle depth-of-field controls with mouse wheel
	if( cvar_pt_dof->integer != 0 && down && ( key == K_MWHEELUP || key == K_MWHEELDOWN ) ) {
		cvar_t* var;
		float minvalue;
		float maxvalue;

		// Shift adjusts aperture, no modifier adjusts focus distance
		if( Key_IsDown( K_SHIFT ) ) {
			var = cvar_pt_aperture;
			minvalue = 0.01f;
			maxvalue = 10.f;
		} else {
			var = cvar_pt_focus;
			minvalue = 1.f;
			maxvalue = 10000.f;
		}

		// Ctrl enables fine adjustment mode
		float factor = Key_IsDown( K_CTRL ) ? 1.01f : 1.1f;

		if( key == K_MWHEELDOWN )
			factor = 1.f / factor;

		float value = var->value;
		value *= factor;
		value = max( minvalue, min( maxvalue, value ) );
		Cvar_SetByVar( var, va( "%f", value ), FROM_CONSOLE );

		return true;
	}

	// Handle freecam movement keys
	switch( key ) {
	case 'w': freecam_keystate[0] = down; return true;
	case 's': freecam_keystate[1] = down; return true;
	case 'd': freecam_keystate[2] = down; return true;
	case 'a': freecam_keystate[3] = down; return true;
	case 'e': freecam_keystate[4] = down; return true;
	case 'q': freecam_keystate[5] = down; return true;

	// Intercept freecam control keys to prevent game actions
	case K_CTRL:
	case K_SHIFT:
	case K_MWHEELDOWN:
	case K_MWHEELUP:
	case K_MOUSE1:
	case K_MOUSE2:
		return true;
	}

	return false;
}
