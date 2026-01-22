/********************************************************************
*
*
*	Client Sound System: QAL Dynamic Implementation
*
*	Provides dynamic OpenAL linking for the sound system. This module
*	loads OpenAL functions at runtime from a shared library, allowing
*	the game to run without OpenAL being statically linked. Function
*	pointers are resolved via dlopen/LoadLibrary and GetProcAddress.
*
*
********************************************************************/
/*
Copyright (C) 2010 Andrey Nazarov

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
#include "system/system.h"
#include "common/cvar.h"
#include "common/common.h"
#include "common/files.h"

// Module header.
#include "dynamic.h"

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
*	OpenAL Function Pointer Macros:
*
*
*
**/
//! Macro defining all OpenAL context (ALC) functions that need dynamic loading.
//! This list includes device management, context creation, and capture functions.
#define QALC_IMP \
    QAL( LPALCCREATECONTEXT, alcCreateContext ); \
    QAL( LPALCMAKECONTEXTCURRENT, alcMakeContextCurrent ); \
    QAL( LPALCPROCESSCONTEXT, alcProcessContext ); \
    QAL( LPALCSUSPENDCONTEXT, alcSuspendContext ); \
    QAL( LPALCDESTROYCONTEXT, alcDestroyContext ); \
    QAL( LPALCGETCURRENTCONTEXT, alcGetCurrentContext ); \
    QAL( LPALCGETCONTEXTSDEVICE, alcGetContextsDevice ); \
    QAL( LPALCOPENDEVICE, alcOpenDevice ); \
    QAL( LPALCCLOSEDEVICE, alcCloseDevice ); \
    QAL( LPALCGETERROR, alcGetError ); \
    QAL( LPALCISEXTENSIONPRESENT, alcIsExtensionPresent ); \
    QAL( LPALCGETPROCADDRESS, alcGetProcAddress ); \
    QAL( LPALCGETENUMVALUE, alcGetEnumValue ); \
    QAL( LPALCGETSTRING, alcGetString ); \
    QAL( LPALCGETINTEGERV, alcGetIntegerv ); \
    QAL( LPALCCAPTUREOPENDEVICE, alcCaptureOpenDevice ); \
    QAL( LPALCCAPTURECLOSEDEVICE, alcCaptureCloseDevice ); \
    QAL( LPALCCAPTURESTART, alcCaptureStart ); \
    QAL( LPALCCAPTURESTOP, alcCaptureStop ); \
    QAL( LPALCCAPTURESAMPLES, alcCaptureSamples );



/**
*
*
*
*	Module State:
*
*
*
**/
//! OpenAL driver library path configuration variable.
static cvar_t *al_driver;
//! OpenAL device name configuration variable.
static cvar_t *al_device;

//! Handle to the dynamically loaded OpenAL library.
static void *handle;
//! OpenAL device handle.
static ALCdevice *device;
//! OpenAL context handle.
static ALCcontext *context;

// Declare static function pointers for OpenAL context functions.
#define QAL( type, func )  static type q##func
QALC_IMP
#undef QAL

// Declare global function pointers for OpenAL functions (defined in header).
#define QAL( type, func )  type q##func
QAL_IMP
#undef QAL



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
*	@brief	Shuts down the dynamically loaded OpenAL system.
*	@note	This function destroys the context, closes the device, unloads the
*			OpenAL library, and resets all function pointers to NULL. It also
*			clears the CVAR_SOUND flags from the configuration variables.
**/
void QAL_Shutdown( void ) {
	// Destroy the audio context if it exists.
	if ( context ) {
		qalcMakeContextCurrent( NULL );
		qalcDestroyContext( context );
		context = NULL;
	}

	// Close the audio device if it exists.
	if ( device ) {
		qalcCloseDevice( device );
		device = NULL;
	}

	// Reset all function pointers to NULL.
#define QAL( type, func )  q##func = NULL
	QALC_IMP
	QAL_IMP
#undef QAL

	// Unload the OpenAL library.
	if ( handle ) {
		Sys_FreeLibrary( handle );
		handle = NULL;
	}

	// Clear the sound system flags from the cvars.
	if ( al_driver ) {
		al_driver->flags &= ~CVAR_SOUND;
	}
	if ( al_device ) {
		al_device->flags &= ~CVAR_SOUND;
	}
}

/**
*	@brief	Initializes the dynamically loaded OpenAL system.
*	@return	True if initialization succeeds, false otherwise.
*	@note	This function loads the OpenAL library specified by the "al_driver" cvar,
*			resolves all required function pointers, opens the audio device specified
*			by "al_device" cvar (or default if empty), and creates an OpenAL context.
*			On failure, QAL_Shutdown is called to clean up any partial initialization.
**/
bool QAL_Init( void ) {
	// Get configuration variables for driver and device.
	al_driver = Cvar_Get( "al_driver", LIBAL, 0 );
	al_device = Cvar_Get( "al_device", "", 0 );

	// Don't allow absolute or relative paths in driver name for security.
	FS_SanitizeFilenameVariable( al_driver );

	// Load the OpenAL library dynamically.
	Sys_LoadLibrary( al_driver->string, NULL, &handle );
	if ( !handle ) {
		return false;
	}

	// Resolve all required OpenAL function pointers from the library.
	// If any function cannot be found, abort initialization.
#define QAL( type, func )  if ( ( q##func = (type)Sys_GetProcAddress( handle, #func ) ) == NULL ) goto fail;
	QALC_IMP
	QAL_IMP
#undef QAL

	// Open the OpenAL device. Use default if cvar is empty.
	device = qalcOpenDevice( al_device->string[0] ? al_device->string : NULL );
	if ( !device ) {
		Com_SetLastError( va( "alcOpenDevice(%s) failed", al_device->string ) );
		goto fail;
	}

	// Create an OpenAL context for the device.
	context = qalcCreateContext( device, NULL );
	if ( !context ) {
		Com_SetLastError( "alcCreateContext failed" );
		goto fail;
	}

	// Make the context current for this thread.
	if ( !qalcMakeContextCurrent( context ) ) {
		Com_SetLastError( "alcMakeContextCurrent failed" );
		goto fail;
	}

	// Mark the cvars as being used by the sound system.
	al_driver->flags |= CVAR_SOUND;
	al_device->flags |= CVAR_SOUND;

	return true;

fail:
	// Clean up any partial initialization on failure.
	QAL_Shutdown();
	return false;
}
