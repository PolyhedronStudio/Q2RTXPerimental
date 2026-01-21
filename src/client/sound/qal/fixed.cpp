/********************************************************************
*
*
*	Client Sound System: QAL Fixed Implementation
*
*	Provides fixed OpenAL linking for the sound system. This module
*	initializes the OpenAL device and context without dynamic loading,
*	using statically linked OpenAL functions.
*
*
********************************************************************/
/*
Copyright (C) 2013 Andrey Nazarov

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

// Shared includes.
#include "shared/shared.h"
#include "common/cvar.h"
#include "common/common.h"

// Module header.
#include "fixed.h"

// Platform-specific OpenAL headers.
#ifdef __APPLE__
#include <OpenAL/alc.h>
#else
#include <AL/alc.h>
#endif



/**
*
*
*
*	Module State:
*
*
*
**/
//! OpenAL device name configuration variable.
static cvar_t *al_device;

//! OpenAL device handle.
static ALCdevice *device;
//! OpenAL context handle.
static ALCcontext *context;



/**
*
*
*
*	QAL Shutdown & Initialization:
*
*
*
**/
/**
*	@brief	Shuts down the OpenAL system, destroying the context and closing the device.
*	@note	This function cleans up all OpenAL resources and resets the state to allow
*			re-initialization. It also clears the CVAR_SOUND flag from the al_device cvar.
**/
void QAL_Shutdown( void ) {
	// Destroy the audio context if it exists.
	if ( context ) {
		alcMakeContextCurrent( NULL );
		alcDestroyContext( context );
		context = NULL;
	}

	// Close the audio device if it exists.
	if ( device ) {
		alcCloseDevice( device );
		device = NULL;
	}

	// Clear the sound system flag from the device cvar.
	if ( al_device ) {
		al_device->flags &= ~CVAR_SOUND;
	}
}

/**
*	@brief	Initializes the OpenAL system by opening a device and creating a context.
*	@return	True if initialization succeeds, false otherwise.
*	@note	The device name is read from the "al_device" cvar. If empty, the default
*			device is used. On failure, QAL_Shutdown is called to clean up any partial
*			initialization, and an error message is set via Com_SetLastError.
**/
bool QAL_Init( void ) {
	// Get the device name configuration variable.
	al_device = Cvar_Get( "al_device", "", 0 );

	// Open the OpenAL device. Use default if cvar is empty.
	device = alcOpenDevice( al_device->string[0] ? al_device->string : NULL );
	if ( !device ) {
		Com_SetLastError( va( "alcOpenDevice(%s) failed", al_device->string ) );
		goto fail;
	}

	// Create an OpenAL context for the device.
	context = alcCreateContext( device, NULL );
	if ( !context ) {
		Com_SetLastError( "alcCreateContext failed" );
		goto fail;
	}

	// Make the context current for this thread.
	if ( !alcMakeContextCurrent( context ) ) {
		Com_SetLastError( "alcMakeContextCurrent failed" );
		goto fail;
	}

	// Mark the device cvar as being used by the sound system.
	al_device->flags |= CVAR_SOUND;

	return true;

fail:
	// Clean up any partial initialization on failure.
	QAL_Shutdown();
	return false;
}
