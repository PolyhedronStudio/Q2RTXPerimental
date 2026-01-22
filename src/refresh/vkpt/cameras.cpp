/********************************************************************
*
*
*	VKPT Renderer: Camera Management System
*
*	Manages camera positions and view directions loaded from external
*	camera definition files. Used for cinematic camera paths, replay
*	systems, and debugging camera positions in levels. Provides console
*	commands for capturing current camera state.
*
*
********************************************************************/
/*
Copyright (C) 2021, NVIDIA CORPORATION. All rights reserved.

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

// Module headers.
#include "cameras.h"
#include "vkpt.h"

// Standard C library.
#include <string.h>



/**
*
*
*
*	Camera Loading:
*
*
*
**/
/**
*	@brief	Loads camera positions and directions from a text file for the specified map.
*	@param	wm			BSP mesh structure to store camera data in.
*	@param	map_name	Name of the map (used to construct the camera file path).
*	@note	Camera files are located at maps/cameras/<mapname>.txt and contain one camera
*			per line in the format: (x, y, z) (dx, dy, dz) where the first triplet is
*			the position and the second is the direction vector. Lines starting with '#'
*			are treated as comments and ignored.
**/
void vkpt_cameras_load( bsp_mesh_t* wm, const char* map_name ) {
	// Reset camera count.
	wm->num_cameras = 0;

	char* filebuf = NULL;

	// Construct the camera definition filename.
	char filename[MAX_QPATH];
	Q_snprintf( filename, sizeof( filename ), "maps/cameras/%s.txt", map_name );

	// Attempt to load the camera file.
	FS_LoadFile( filename, (void**)&filebuf );

	if ( !filebuf ) {
		Com_DPrintf( "Couldn't read %s\n", filename );
		return;
	}

	// Parse the camera file line by line.
	char const* ptr = (char const*)filebuf;
	char linebuf[1024];

	while ( sgets( linebuf, sizeof( linebuf ), &ptr ) ) {
		// Remove comments (everything after '#').
		{ char* t = strchr( linebuf, '#' ); if ( t ) *t = 0; }
		
		// Remove newline characters.
		{ char* t = strchr( linebuf, '\n' ); if ( t ) *t = 0; }

		vec3_t pos, dir;

		// Parse camera position and direction in format: (x, y, z) (dx, dy, dz)
		if ( sscanf( linebuf, "(%f, %f, %f) (%f, %f, %f)",
				&pos[0], &pos[1], &pos[2], &dir[0], &dir[1], &dir[2] ) == 6 ) {
			
			// Check if we have room for another camera.
			if ( wm->num_cameras < MAX_CAMERAS ) {
				// Store camera position and direction.
				VectorCopy( pos, wm->cameras[wm->num_cameras].pos );
				VectorCopy( dir, wm->cameras[wm->num_cameras].dir );
				wm->num_cameras++;
			} else {
				Com_WPrintf( "Map has too many cameras (max: %i)\n", MAX_CAMERAS );
				break;
			}
		}
	}

	// Free the file buffer.
	Z_Free( filebuf );
}



/**
*
*
*
*	Console Commands:
*
*
*
**/
/**
*	@brief	Console command that prints the current camera position and forward direction.
*	@note	Output format matches the camera file format, making it easy to capture
*			camera positions during gameplay for later use in camera path files.
**/
static void Camera_Cmd_f( void ) {
	vec3_t forward;

	// Get the forward direction vector from view angles.
	AngleVectors( vkpt_refdef.fd->viewangles, forward, NULL, NULL );

	// Print in format: (x, y, z) (dx, dy, dz) - same as camera file format.
	Com_Printf( "(%f, %f, %f) (%f, %f, %f)\n",
		vkpt_refdef.fd->vieworg[0], vkpt_refdef.fd->vieworg[1], vkpt_refdef.fd->vieworg[2],
		forward[0], forward[1], forward[2] );
}

//! Console command registration table.
static const cmdreg_t cmds[] = {
	{ "camera", &Camera_Cmd_f, NULL },
	{ NULL, NULL, NULL }
};



/**
*
*
*
*	Module Initialization & Shutdown:
*
*
*
**/
/**
*	@brief	Initializes the camera system and registers console commands.
**/
void vkpt_cameras_init( void ) {
	Cmd_Register( cmds );
}

/**
*	@brief	Shuts down the camera system and unregisters console commands.
**/
void vkpt_cameras_shutdown( void ) {
	Cmd_RemoveCommand( "camera" );
}
