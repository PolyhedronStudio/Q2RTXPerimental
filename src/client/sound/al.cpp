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

/********************************************************************
*
*
*	Module Name: OpenAL Sound Backend
*
*	This module implements the OpenAL-based sound backend for Q2RTXPerimental.
*	It provides 3D spatial audio, environmental audio effects (EAX reverb),
*	and streaming audio playback capabilities.
*
*	Architecture:
*	- Manages OpenAL sources for playing sound effects (static and looping)
*	- Implements 3D spatialization with distance attenuation
*	- Provides EAX reverb support with fallback to standard reverb
*	- Handles streaming audio for music and cinematics
*	- Coordinates system: OpenAL uses right-handed Y-up, Quake uses
*	  right-handed Z-up, requiring coordinate transformation
*
*	Key Components:
*	- Source Pool: Fixed array of OpenAL sources for sound channels
*	- Stream Source: Dedicated source for streaming audio (music/cinematics)
*	- EAX Mixer: Environmental audio effects (reverb) processor
*	- Auxiliary Effect Slot: OpenAL effect slot for applying reverb
*	- Spatial Audio: 3D positioning with listener and source transforms
*
*	Performance Characteristics:
*	- Supports minimum MIN_CHANNELS (16) simultaneous sounds
*	- Uses loop point extension for seamless audio loops
*	- Implements source spatialize extension for optimized 2D audio
*	- Stream buffers limited to 16 to prevent excessive latency
*
*
********************************************************************/

#include "sound.h"

#if USE_FIXED_LIBAL
#include "qal/fixed.h"
#else
#include "qal/dynamic.h"
#endif

/**
*
*
*
*	Coordinate System Transformation:
*
*
*
**/

//! Translates from OpenAL coordinate system (right-handed Y-up) to Quake (right-handed Z-up).
#define AL_UnpackVector(v)  -v[1],v[2],-v[0]

//! Copies and transforms a Quake vector to OpenAL coordinate system.
#define AL_CopyVector(a,b)  ((b)[0]=-(a)[1],(b)[1]=(a)[2],(b)[2]=-(a)[0])

//! OpenAL implementation should support at least this number of sources.
#define MIN_CHANNELS    16

/**
*
*
*
*	Module State:
*
*
*
**/

//! Array of OpenAL source IDs for sound channels.
static ALuint       s_srcnums[MAX_CHANNELS];

//! OpenAL source ID for streaming audio (music/cinematics).
static ALuint       s_stream;

//! Number of buffers currently queued in the stream source.
static ALuint       s_stream_buffers;

//! True if AL_SOFT_loop_points extension is available.
static ALboolean    s_loop_points;

//! True if AL_SOFT_source_spatialize extension is available.
static ALboolean    s_source_spatialize;

//! Frame counter for autosound management.
static uint64_t     s_framecount;
/**
*
*
*
*	EAX Environmental Audio Effects:
*
*
*
**/

//! OpenAL ID for the global auxiliary effect slot used for reverb.
static ALuint s_auxiliary_effect_slot;

//! EAX mixer state containing effect resource ID and capability flag.
struct {
	ALuint mixer_resource_id;		//! OpenAL effect ID for reverb effect.
	qboolean is_eax;				//! True if EAX reverb is available, false if using standard reverb.
} s_eax_mixer;

/**
*	@brief	Generate and initialize the EAX reverb effect mixer.
*	@note	Attempts to create EAX reverb effect, falls back to standard reverb if unavailable.
**/
static void _AL_GenerateEAXMixer( void ) {
	// Allocate a unique reverb effect resource ID.
	qalGetError();
	qalGenEffects( 1, &s_eax_mixer.mixer_resource_id );

	// Try EAX reverb for more settings and better quality.
	qalEffecti( s_eax_mixer.mixer_resource_id, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );
	if ( !qalGetError() ) {
		// EAX reverb is available.
		s_eax_mixer.is_eax = true;
	} else {
		// Fall back to standard reverb.
		s_eax_mixer.is_eax = false;
	}
}

/**
*	@brief	Delete the EAX mixer effect resource.
**/
static void _AL_DeleteEAXMixer( void ) {
	qalDeleteEffects( 1, &s_eax_mixer.mixer_resource_id );
}

/**
*	@brief	Allocate the global auxiliary effect slot for reverb effects.
**/
static void _AL_GenerateAuxiliaryEffectsSlot( void ) {
	qalGenAuxiliaryEffectSlots( 1, &s_auxiliary_effect_slot );
}

/**
*	@brief	Deallocate the global auxiliary effect slot.
**/
static void _AL_DeleteAuxiliaryEffectsSlot( void ) {
	qalDeleteAuxiliaryEffectSlots( 1, &s_auxiliary_effect_slot );
}

/**
*	@brief	Apply EAX or standard reverb properties to the global effect mixer.
*	@param	reverb	Pointer to reverb properties structure containing all effect parameters.
*	@return	True if properties were successfully applied, false otherwise.
*	@note	All reverb parameters are clamped to valid OpenAL ranges to prevent exceptions.
*	@note	Uses EAX reverb if available for enhanced quality, otherwise standard reverb.
**/
static const qboolean AL_SetEAXEffectProperties( const sfx_eax_properties_t *reverb ) {
	ALuint effect = s_eax_mixer.mixer_resource_id;

	if ( s_eax_mixer.is_eax ) {
		/**
		*	EAX Reverb is available. Set the EAX effect type then load the reverb properties
		*	and make sure to CLAMP them between sane ratios. This way we prevent OpenAL-Soft
		*	from throwing C++ exceptions should we exceed min/max limits of said property.
		**/
		qalEffecti( effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );

        qalEffectf( effect, AL_EAXREVERB_DENSITY, constclamp( reverb->flDensity, AL_EAXREVERB_MIN_DENSITY, AL_EAXREVERB_MAX_DENSITY ) );
        
        qalEffectf( effect, AL_EAXREVERB_DIFFUSION, constclamp( reverb->flDiffusion, AL_EAXREVERB_MIN_DIFFUSION, AL_EAXREVERB_MAX_DIFFUSION ) );
        
        qalEffectf( effect, AL_EAXREVERB_GAIN, constclamp( reverb->flGain, AL_EAXREVERB_MIN_GAIN, AL_EAXREVERB_MAX_GAIN ) );
        qalEffectf( effect, AL_EAXREVERB_GAINHF, constclamp( reverb->flGainHF, AL_EAXREVERB_MIN_GAINHF, AL_EAXREVERB_MAX_GAINHF ) );
        qalEffectf( effect, AL_EAXREVERB_GAINLF, constclamp( reverb->flGainLF, AL_EAXREVERB_MIN_GAINLF, AL_EAXREVERB_MAX_GAINLF ) );

        qalEffectf( effect, AL_EAXREVERB_DECAY_TIME, constclamp( reverb->flDecayTime, AL_EAXREVERB_MIN_DECAY_TIME, AL_EAXREVERB_MAX_DECAY_TIME ) );
        qalEffectf( effect, AL_EAXREVERB_DECAY_HFRATIO, constclamp( reverb->flDecayHFRatio, AL_EAXREVERB_MIN_DECAY_HFRATIO, AL_EAXREVERB_MAX_DECAY_HFRATIO ) );
        qalEffectf( effect, AL_EAXREVERB_DECAY_LFRATIO, constclamp( reverb->flDecayLFRatio, AL_EAXREVERB_MIN_DECAY_LFRATIO, AL_EAXREVERB_MAX_DECAY_LFRATIO ) );
        
        qalEffectf( effect, AL_EAXREVERB_REFLECTIONS_GAIN, constclamp( reverb->flReflectionsGain, AL_EAXREVERB_MIN_REFLECTIONS_GAIN, AL_EAXREVERB_MAX_REFLECTIONS_GAIN ) );
        qalEffectf( effect, AL_EAXREVERB_REFLECTIONS_DELAY, constclamp( reverb->flReflectionsDelay, AL_EAXREVERB_MIN_REFLECTIONS_DELAY, AL_EAXREVERB_MAX_REFLECTIONS_DELAY ) );
        qalEffectfv( effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan );
        
        qalEffectf( effect, AL_EAXREVERB_LATE_REVERB_GAIN, constclamp( reverb->flLateReverbGain, AL_EAXREVERB_MIN_LATE_REVERB_GAIN, AL_EAXREVERB_MAX_LATE_REVERB_GAIN ) );
        qalEffectf( effect, AL_EAXREVERB_LATE_REVERB_DELAY, constclamp( reverb->flLateReverbDelay, AL_EAXREVERB_MIN_LATE_REVERB_DELAY, AL_EAXREVERB_MAX_LATE_REVERB_DELAY ) );
        qalEffectfv( effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan );
        
        qalEffectf( effect, AL_EAXREVERB_ECHO_TIME, constclamp( reverb->flEchoTime, AL_EAXREVERB_MIN_ECHO_TIME, AL_EAXREVERB_MAX_ECHO_TIME ) );
        qalEffectf( effect, AL_EAXREVERB_ECHO_DEPTH, constclamp( reverb->flEchoDepth, AL_EAXREVERB_MIN_ECHO_DEPTH, AL_EAXREVERB_MAX_ECHO_DEPTH ) );
        
        qalEffectf( effect, AL_EAXREVERB_MODULATION_TIME, constclamp( reverb->flModulationTime, AL_EAXREVERB_MIN_MODULATION_TIME, AL_EAXREVERB_MAX_MODULATION_TIME ) );
        qalEffectf( effect, AL_EAXREVERB_MODULATION_DEPTH, constclamp( reverb->flModulationDepth, AL_EAXREVERB_MIN_MODULATION_DEPTH, AL_EAXREVERB_MAX_MODULATION_DEPTH ) );
        
        qalEffectf( effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, constclamp( reverb->flAirAbsorptionGainHF, AL_EAXREVERB_MIN_AIR_ABSORPTION_GAINHF, AL_EAXREVERB_MAX_AIR_ABSORPTION_GAINHF ) );
        
        qalEffectf( effect, AL_EAXREVERB_HFREFERENCE, constclamp( reverb->flHFReference, AL_EAXREVERB_MIN_HFREFERENCE, AL_EAXREVERB_MAX_HFREFERENCE ) );
        qalEffectf( effect, AL_EAXREVERB_LFREFERENCE, constclamp( reverb->flLFReference, AL_EAXREVERB_MIN_LFREFERENCE, AL_EAXREVERB_MAX_LFREFERENCE ) );
        
        qalEffectf( effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, constclamp( reverb->flRoomRolloffFactor, AL_EAXREVERB_MIN_ROOM_ROLLOFF_FACTOR, AL_EAXREVERB_MAX_ROOM_ROLLOFF_FACTOR ) );
        
        qalEffecti( effect, AL_EAXREVERB_DECAY_HFLIMIT, constclamp( reverb->iDecayHFLimit, AL_EAXREVERB_MIN_DECAY_HFLIMIT, AL_EAXREVERB_MAX_DECAY_HFLIMIT ) );
    } else {
        /* No EAX Reverb. Set the standard reverb effect type then load the
         * available reverb properties. */
        qalEffecti( effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB );

        qalEffectf( effect, AL_REVERB_DENSITY, constclamp( reverb->flDensity, AL_REVERB_MIN_DENSITY, AL_REVERB_MAX_DENSITY ) );

        qalEffectf( effect, AL_REVERB_DIFFUSION, constclamp( reverb->flDiffusion, AL_REVERB_MIN_DIFFUSION, AL_REVERB_MAX_DIFFUSION ) );

        qalEffectf( effect, AL_REVERB_GAIN, constclamp( reverb->flGain, AL_REVERB_MIN_GAIN, AL_REVERB_MAX_GAIN ) );
        qalEffectf( effect, AL_REVERB_GAINHF, constclamp( reverb->flGainHF, AL_REVERB_MIN_GAINHF, AL_REVERB_MAX_GAINHF ) );

        qalEffectf( effect, AL_REVERB_DECAY_TIME, constclamp( reverb->flDecayTime, AL_REVERB_MIN_DECAY_TIME, AL_REVERB_MAX_DECAY_TIME ) );
        qalEffectf( effect, AL_REVERB_DECAY_HFRATIO, constclamp( reverb->flDecayHFRatio, AL_REVERB_MIN_DECAY_HFRATIO, AL_REVERB_MAX_DECAY_HFRATIO ) );

        qalEffectf( effect, AL_REVERB_REFLECTIONS_GAIN, constclamp( reverb->flReflectionsGain, AL_REVERB_MIN_REFLECTIONS_GAIN, AL_REVERB_MAX_REFLECTIONS_GAIN ) );
        qalEffectf( effect, AL_REVERB_REFLECTIONS_DELAY, constclamp( reverb->flReflectionsDelay, AL_REVERB_MIN_REFLECTIONS_DELAY, AL_REVERB_MAX_REFLECTIONS_DELAY ) );

        qalEffectf( effect, AL_REVERB_LATE_REVERB_GAIN, constclamp( reverb->flLateReverbGain, AL_REVERB_MIN_LATE_REVERB_GAIN, AL_REVERB_MAX_LATE_REVERB_GAIN ) );
        qalEffectf( effect, AL_REVERB_LATE_REVERB_DELAY, constclamp( reverb->flLateReverbDelay, AL_REVERB_MIN_LATE_REVERB_DELAY, AL_REVERB_MAX_LATE_REVERB_DELAY ) );

        qalEffectf( effect, AL_REVERB_AIR_ABSORPTION_GAINHF, constclamp( reverb->flAirAbsorptionGainHF, AL_REVERB_MIN_AIR_ABSORPTION_GAINHF, AL_REVERB_MAX_AIR_ABSORPTION_GAINHF ) );

        qalEffectf( effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, constclamp( reverb->flRoomRolloffFactor, AL_REVERB_MIN_ROOM_ROLLOFF_FACTOR, AL_REVERB_MAX_ROOM_ROLLOFF_FACTOR ) );

        qalEffecti( effect, AL_REVERB_DECAY_HFLIMIT, constclamp( reverb->iDecayHFLimit, AL_REVERB_MIN_DECAY_HFLIMIT, AL_REVERB_MAX_DECAY_HFLIMIT ) );
    }

    return true;
}


/**
*
*
*   Other OpenAL
* 
* 
**/
static void AL_StreamStop(void);

static void AL_SoundInfo(void)
{
    Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_Printf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));
    Com_Printf("Number of sources: %d\n", s_numchannels);
}

static bool AL_Init(void)
{
	int i;

	Com_DPrintf( "Initializing OpenAL\n" );

	// Initialize QAL wrapper layer (loads OpenAL library and function pointers).
	if ( !QAL_Init() ) {
		goto fail0;
	}

	Com_DPrintf( "AL_VENDOR: %s\n", qalGetString( AL_VENDOR ) );
	Com_DPrintf( "AL_RENDERER: %s\n", qalGetString( AL_RENDERER ) );
	Com_DPrintf( "AL_VERSION: %s\n", qalGetString( AL_VERSION ) );
	Com_DDPrintf( "AL_EXTENSIONS: %s\n", qalGetString( AL_EXTENSIONS ) );

	// Check for linear distance extension (required for proper distance attenuation).
	if ( !qalIsExtensionPresent( "AL_EXT_LINEAR_DISTANCE" ) ) {
		Com_SetLastError( "AL_EXT_LINEAR_DISTANCE extension is missing" );
		goto fail1;
	}

	// Generate stream source for music/cinematics.
	qalGetError();
	qalGenSources( 1, &s_stream );
	if ( qalGetError() != AL_NO_ERROR ) {
		Com_Printf( "ERROR: Couldn't get a single Source.\n" );
		goto fail1;
	}

	// Generate sound effect sources (allocate as many as possible up to MAX_CHANNELS).
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		qalGenSources( 1, &s_srcnums[i] );
		if ( qalGetError() != AL_NO_ERROR ) {
			break;
		}
	}

	Com_DPrintf( "Got %d AL sources\n", i );

	// Verify minimum source count for acceptable audio quality.
	if ( i < MIN_CHANNELS ) {
		Com_SetLastError( "Insufficient number of AL sources" );
		goto fail1;
	}

	s_numchannels = i;

	// Detect optional extensions for enhanced features.
	s_loop_points = qalIsExtensionPresent( "AL_SOFT_loop_points" );
	s_source_spatialize = qalIsExtensionPresent( "AL_SOFT_source_spatialize" );

	// Initialize stream source (no 3D spatialization for music/cinematics).
	qalSourcef( s_stream, AL_ROLLOFF_FACTOR, 0.0f );
	qalSourcei( s_stream, AL_SOURCE_RELATIVE, AL_TRUE );
	if ( s_source_spatialize )
		qalSourcei( s_stream, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE );

	// Generate the global EAX mixer for reverb effects.
	_AL_GenerateEAXMixer();

	// Generate the global auxiliary effects slot for applying reverb.
	_AL_GenerateAuxiliaryEffectsSlot();

	Com_Printf( "OpenAL initialized.\n" );
	return true;

fail1:
	QAL_Shutdown();
fail0:
	Com_EPrintf( "Failed to initialize OpenAL: %s\n", Com_GetLastError() );
	return false;
}

/**
*	@brief	Shutdown OpenAL sound backend and release all audio resources.
*	@note	Stops all sounds, deletes sources, destroys EAX mixer and effects slots.
**/
static void AL_Shutdown( void ) {
	Com_Printf( "Shutting down OpenAL.\n" );

	// Delete all sound effect sources.
	if ( s_numchannels ) {
		qalDeleteSources( s_numchannels, s_srcnums );
		memset( s_srcnums, 0, sizeof( s_srcnums ) );
		s_numchannels = 0;
	}

	// Stop and delete stream source.
	if ( s_stream ) {
		AL_StreamStop();
		qalDeleteSources( 1, &s_stream );
		s_stream = 0;
	}

	// Delete the EAX mixer.
	_AL_DeleteEAXMixer();

	// Delete the global auxiliary effects slot.
	_AL_DeleteAuxiliaryEffectsSlot();

	// Shutdown QAL wrapper layer.
	QAL_Shutdown();
}

/**
*
*
*
*	Sound Buffer Management:
*
*
*
**/

/**
*	@brief	Upload sound effect data to OpenAL buffer and create sfxcache.
*	@param	s	Sound effect structure containing metadata and raw audio data.
*	@return	Pointer to allocated sfxcache structure, or NULL on error.
*	@note	Creates OpenAL buffer, uploads audio data, sets loop points if available.
*	@note	Supported formats: mono/stereo, 8-bit/16-bit PCM.
**/

static sfxcache_t *AL_UploadSfx( sfx_t *s ) {
	ALsizei size = s_info.samples * s_info.width * s_info.channels;
	ALenum format = AL_FORMAT_MONO8 + ( s_info.channels - 1 ) * 2 + ( s_info.width - 1 );
	ALuint buffer = 0;

	// Generate OpenAL buffer for sound data.
	qalGetError();
	qalGenBuffers( 1, &buffer );
	if ( qalGetError() )
		goto fail;

	// Upload raw PCM audio data to buffer.
	qalBufferData( buffer, format, s_info.data, size, s_info.rate );
	if ( qalGetError() ) {
		qalDeleteBuffers( 1, &buffer );
		goto fail;
	}

	// Specify OpenAL-Soft style loop points for seamless looping.
	if ( s_info.loopstart > 0 && s_loop_points ) {
		ALint points[2] = { s_info.loopstart, s_info.samples };
		qalBufferiv( buffer, AL_LOOP_POINTS_SOFT, points );
	}

	// Allocate sfxcache structure to store buffer metadata.
	sfxcache_t *sc = s->cache = S_Malloc( sizeof( *sc ) );
	sc->length = s_info.samples * 1000LL / s_info.rate; // Convert to milliseconds.
	sc->loopstart = s_info.loopstart;
	sc->width = s_info.width;
	sc->channels = s_info.channels;
	sc->size = size;
	sc->bufnum = buffer;

	return sc;

fail:
	s->error = Q_ERR_LIBRARY_ERROR;
	return nullptr;
}

/**
*	@brief	Delete OpenAL buffer associated with sound effect.
*	@param	s	Sound effect structure containing cached buffer reference.
**/
static void AL_DeleteSfx( sfx_t *s ) {
	sfxcache_t *sc = s->cache;
	if ( sc ) {
		ALuint name = sc->bufnum;
		qalDeleteBuffers( 1, &name );
	}
}

/**
*	@brief	Calculate begin offset time for delayed sound playback.
*	@param	timeofs	Time offset in seconds from current time.
*	@return	Absolute paint time in milliseconds when sound should start.
**/
static int AL_GetBeginofs( float timeofs ) {
	return s_paintedtime + timeofs * 1000;
}

/**
*
*
*
*	3D Spatialization and Channel Management:
*
*
*
**/

/**
*	@brief	Apply 3D spatialization to sound channel source.
*	@param	ch	Channel structure containing source ID, entity, and origin information.
*	@note	Transforms Quake coordinates to OpenAL coordinate system (Y-up to Z-up).
*	@note	Applies EAX reverb effect to auxiliary effect slot for environmental audio.
*	@note	View entity sounds are always full volume (no distance attenuation).
**/

static void AL_Spatialize( channel_t *ch ) {
	vec3_t origin;

	// Anything coming from the view entity will always be full volume (no attenuation/spatialization).
	if ( S_IsFullVolume( ch ) ) {
		VectorCopy( cl.listener_spatialize.origin, origin );
	// Use channel origin in case of a fixed set origin.
	} else if ( ch->fixed_origin ) {
		VectorCopy( ch->origin, origin );
	// Otherwise, retrieve exact interpolated origin for the entity.
	} else {
		CL_GetEntitySoundOrigin( ch->entnum, origin );
	}

	// Apply software spatialization flag if extension is available.
	if ( s_source_spatialize ) {
		qalSourcei( ch->srcnum, AL_SOURCE_SPATIALIZE_SOFT, !S_IsFullVolume( ch ) );
	}

	// Set 3D position for the channel audio source (transform Quake to OpenAL coords).
	qalSource3f( ch->srcnum, AL_POSITION, AL_UnpackVector( origin ) );

	// Apply the current mixed EAX reverb to the auxiliary effect slot.
	qalAuxiliaryEffectSloti( s_auxiliary_effect_slot, AL_EFFECTSLOT_EFFECT, s_eax_mixer.mixer_resource_id );

	// Connect the EAX effect slot to the sound channel source.
	qalSource3i( ch->srcnum, AL_AUXILIARY_SEND_FILTER, s_auxiliary_effect_slot, 0, AL_FILTER_NULL );
}

/**
*	@brief	Stop playback on sound channel and release channel resources.
*	@param	ch	Channel structure to stop.
**/

static void AL_StopChannel( channel_t *ch ) {
	if ( !ch->sfx )
		return;

#if USE_DEBUG
	if ( s_show->integer > 1 )
		Com_Printf( "%s: %s\n", __func__, ch->sfx->name );
#endif

	// Stop source playback and detach buffer.
	qalSourceStop( ch->srcnum );
	qalSourcei( ch->srcnum, AL_BUFFER, AL_NONE );
	memset( ch, 0, sizeof( *ch ) );
}

/**
*	@brief	Start playback on sound channel with appropriate source parameters.
*	@param	ch	Channel structure containing sound effect and playback parameters.
*	@note	Sets buffer, looping, gain, distance attenuation, and 3D spatialization.
**/
static void AL_PlayChannel( channel_t *ch ) {
	sfxcache_t *sc = ch->sfx->cache;

#if USE_DEBUG
	if ( s_show->integer > 1 )
		Com_Printf( "%s: %s\n", __func__, ch->sfx->name );
#endif

	// Assign OpenAL source to channel.
	ch->srcnum = s_srcnums[ch - s_channels];
	qalGetError();

	// Attach sound buffer to source.
	qalSourcei( ch->srcnum, AL_BUFFER, sc->bufnum );

	// Enable looping for autosounds or sounds with loop points.
	qalSourcei( ch->srcnum, AL_LOOPING, ch->autosound || sc->loopstart >= 0 );

	// Set volume gain.
	qalSourcef( ch->srcnum, AL_GAIN, ch->master_vol );

	// Configure distance attenuation (linear distance model).
	qalSourcef( ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME );
	qalSourcef( ch->srcnum, AL_MAX_DISTANCE, CM_MAX_WORLD_SIZE );
	qalSourcef( ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * ( CM_MAX_WORLD_SIZE - SOUND_FULLVOLUME ) );

	// Apply 3D spatialization and reverb effects.
	AL_Spatialize( ch );

	// Start source playback.
	qalSourcePlay( ch->srcnum );
	if ( qalGetError() != AL_NO_ERROR ) {
		AL_StopChannel( ch );
	}
}

/**
*
*
*
*	Playback Management:
*
*
*
**/

/**
*	@brief	Process pending playsounds and issue playback for sounds due to start.
*	@note	Iterates through pending playsound queue and starts sounds at scheduled times.
**/

static void AL_IssuePlaysounds( void ) {
	// Start any playsounds scheduled to begin at or before current time.
	while ( 1 ) {
		playsound_t *ps = PS_FIRST( &s_pendingplays );
		if ( PS_TERM( ps, &s_pendingplays ) )
			break;  // No more pending sounds.
		if ( ps->begin > s_paintedtime )
			break;  // This sound is scheduled for future playback.
		S_IssuePlaysound( ps );
	}
}

/**
*	@brief	Stop all active sound channels immediately.
*	@note	Typically called when changing levels or pausing game.
**/
static void AL_StopAllSounds( void ) {
	int i;
	channel_t *ch;

	for ( i = 0, ch = s_channels; i < s_numchannels; i++, ch++ ) {
		if ( !ch->sfx )
			continue;
		AL_StopChannel( ch );
	}
}

/**
*	@brief	Find existing looping sound channel by entity and sound effect.
*	@param	entnum	Entity number to search for (0 for any entity).
*	@param	sfx		Sound effect to search for.
*	@return	Pointer to channel if found, NULL otherwise.
*	@note	Used to synchronize new looping sounds with existing ones of same type.
**/
static channel_t *AL_FindLoopingSound( int entnum, sfx_t *sfx ) {
	int i;
	channel_t *ch;

	for ( i = 0, ch = s_channels; i < s_numchannels; i++, ch++ ) {
		if ( !ch->autosound )
			continue;
		if ( entnum && ch->entnum != entnum )
			continue;
		if ( ch->sfx != sfx )
			continue;
		return ch;
	}

	return nullptr;
}

/**
*	@brief	Update and add looping ambient sounds for visible entities.
*	@note	Builds sound list from entity states, allocates channels, synchronizes offsets.
*	@note	Looping sounds are automatically removed next frame if not refreshed.
*	@note	Distance attenuation is checked before allocating channel to prevent wasted resources.
**/

static void AL_AddLoopSounds( void ) {
	int i;
	int sounds[MAX_EDICTS];
	channel_t *ch, *ch2;
	sfx_t *sfx;
	sfxcache_t *sc;
	int num;
	entity_state_t *ent;
	vec3_t origin;
	float dist;

	// Skip if not in active game state or if ambient sounds are disabled.
	if ( cls.state != ca_active || sv_paused->integer || !s_ambient->integer )
		return;

	// Build list of looping sounds from entity states.
	S_BuildSoundList( sounds );

	for ( i = 0; i < cl.frame.numEntities; i++ ) {
		if ( !sounds[i] )
			continue;

		// Resolve sound effect from precache index.
		sfx = S_SfxForHandle( cl.sound_precache[sounds[i]] );
		if ( !sfx )
			continue;       // Bad sound effect.
		sc = sfx->cache;
		if ( !sc )
			continue;

		num = ( cl.frame.firstEntity + i ) & PARSE_ENTITIES_MASK;
		ent = &cl.entityStates[num];

		// Check if this entity already has a looping sound channel.
		ch = AL_FindLoopingSound( ent->number, sfx );
		if ( ch ) {
			// Refresh existing channel lifetime.
			ch->autoframe = s_framecount;
			ch->end = s_paintedtime + sc->length;
			continue;
		}

		// Check distance attenuation before allocating a channel.
		CL_GetEntitySoundOrigin( ent->number, origin );
		VectorSubtract( origin, cl.listener_spatialize.origin, origin );
		dist = VectorNormalize( origin );
		dist = ( dist - SOUND_FULLVOLUME ) * SOUND_LOOPATTENUATE;
		if ( dist >= 1.f )
			continue; // Completely attenuated, don't waste a channel.

		// Allocate a new channel for this looping sound.
		ch = S_PickChannel( 0, 0 );
		if ( !ch )
			continue;

		// Attempt to synchronize playback offset with existing sounds of same type.
		ch2 = AL_FindLoopingSound( 0, sfx );
		if ( ch2 ) {
			ALfloat offset = 0;
			qalGetSourcef( ch2->srcnum, AL_SAMPLE_OFFSET, &offset );
			qalSourcef( s_srcnums[ch - s_channels], AL_SAMPLE_OFFSET, offset );
		}

		// Initialize channel parameters.
		ch->autosound = true;   // Mark for automatic removal next frame if not refreshed.
		ch->autoframe = s_framecount;
		ch->sfx = sfx;
		ch->entnum = ent->number;
		ch->master_vol = 1.0f;
		ch->dist_mult = SOUND_LOOPATTENUATE;
		ch->end = s_paintedtime + sc->length;

		// Start playback on channel.
		AL_PlayChannel( ch );
	}
}

/**
*
*
*
*	Stream Audio (Music/Cinematics):
*
*
*
**/

/**
*	@brief	Update stream source by removing processed buffers.
*	@note	Frees processed buffers and updates internal buffer count.
**/

static void AL_StreamUpdate( void ) {
	ALint num_buffers = 0;
	qalGetSourcei( s_stream, AL_BUFFERS_PROCESSED, &num_buffers );
	
	// Unqueue and delete all processed buffers.
	while ( num_buffers-- > 0 ) {
		ALuint buffer = 0;
		qalSourceUnqueueBuffers( s_stream, 1, &buffer );
		qalDeleteBuffers( 1, &buffer );
		s_stream_buffers--;
	}
}

/**
*	@brief	Stop stream playback and cleanup all queued buffers.
*	@note	Verifies buffer count consistency to detect resource leaks.
**/
static void AL_StreamStop( void ) {
	qalSourceStop( s_stream );
	AL_StreamUpdate();

	// Sanity check: all buffers should be released.
	if ( s_stream_buffers )
		Com_Error( ERR_FATAL, "Unbalanced number of AL buffers" );
}

/**
*	@brief	Queue raw audio samples for streaming playback (music/cinematics).
*	@param	samples		Number of samples in buffer.
*	@param	rate		Sample rate in Hz.
*	@param	width		Sample width in bytes (1=8-bit, 2=16-bit).
*	@param	channels	Number of audio channels (1=mono, 2=stereo).
*	@param	data		Pointer to raw PCM audio data.
*	@param	volume		Playback volume (0.0 to 1.0).
*	@return	True if samples were successfully queued, false on error.
*	@note	Limits queued buffers to 16 to prevent excessive latency.
*	@note	Automatically restarts playback if source is not playing.
**/
static bool AL_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume ) {
	ALenum format = AL_FORMAT_MONO8 + ( channels - 1 ) * 2 + ( width - 1 );
	ALuint buffer = 0;

	// Limit number of queued buffers to prevent excessive latency.
	if ( s_stream_buffers < 16 ) {
		qalGetError();
		qalGenBuffers( 1, &buffer );
		if ( qalGetError() )
			return false;

		// Upload raw PCM data to buffer.
		qalBufferData( buffer, format, data, samples * width * channels, rate );
		if ( qalGetError() ) {
			qalDeleteBuffers( 1, &buffer );
			return false;
		}

		// Queue buffer to stream source.
		qalSourceQueueBuffers( s_stream, 1, &buffer );
		if ( qalGetError() ) {
			qalDeleteBuffers( 1, &buffer );
			return false;
		}
		s_stream_buffers++;
	}

	// Update stream volume.
	qalSourcef( s_stream, AL_GAIN, volume );

	// Restart playback if source has stopped (buffer underrun).
	ALint state = AL_PLAYING;
	qalGetSourcei( s_stream, AL_SOURCE_STATE, &state );
	if ( state != AL_PLAYING )
		qalSourcePlay( s_stream );

	return true;
}

/**
*	@brief	Check if stream source needs more raw samples.
*	@return	True if more samples can be queued, false if buffer is full.
**/
static bool AL_NeedRawSamples( void ) {
	return s_stream_buffers < 16;
}

/**
*
*
*
*	Per-Frame Update:
*
*
*
**/

/**
*	@brief	Main per-frame update for OpenAL sound backend.
*	@note	Updates listener position/orientation, spatializes all channels, manages looping sounds.
*	@note	Called once per client frame during audio mixing phase.
**/

static void AL_Update( void ) {
	int i;
	channel_t *ch;
	ALfloat orientation[6];

	if ( !s_active )
		return;

	// Update current paint time from client time.
	s_paintedtime = cl.time;

	// Set listener position (transform Quake to OpenAL coordinates).
	qalListener3f( AL_POSITION, AL_UnpackVector( cl.listener_spatialize.origin ) );

	// Set listener orientation (forward and up vectors).
	AL_CopyVector( cl.listener_spatialize.v_forward, orientation );
	AL_CopyVector( cl.listener_spatialize.v_up, orientation + 3 );
	qalListenerfv( AL_ORIENTATION, orientation );

	// Set master volume gain.
	qalListenerf( AL_GAIN, S_GetLinearVolume( s_volume->value ) );

	// Set distance attenuation model.
	qalDistanceModel( AL_LINEAR_DISTANCE_CLAMPED );

	// Update spatialization for all active channels.
	for ( i = 0, ch = s_channels; i < s_numchannels; i++, ch++ ) {
		if ( !ch->sfx )
			continue;

		if ( ch->autosound ) {
			// Autosounds are regenerated fresh each frame, stop if not refreshed.
			if ( ch->autoframe != s_framecount ) {
				AL_StopChannel( ch );
				continue;
			}
		} else {
			// Check if one-shot sound has finished playing.
			ALenum state = AL_STOPPED;
			qalGetSourcei( ch->srcnum, AL_SOURCE_STATE, &state );
			if ( state == AL_STOPPED ) {
				AL_StopChannel( ch );
				continue;
			}
		}

#if USE_DEBUG
		if ( s_show->integer ) {
			ALfloat offset = 0;
			qalGetSourcef( ch->srcnum, AL_SAMPLE_OFFSET, &offset );
			Com_Printf( "%d %.1f %.1f %s\n", i, ch->master_vol, offset, ch->sfx->name );
		}
#endif

		// Respatialize channel (update 3D position and effects).
		AL_Spatialize( ch );
	}

	// Increment frame counter for autosound management.
	s_framecount++;

	// Add looping ambient sounds for visible entities.
	AL_AddLoopSounds();

	// Update stream source (cleanup processed buffers).
	AL_StreamUpdate();

	// Issue pending playsounds that are due to start.
	AL_IssuePlaysounds();

	// Final stream update (cleanup any buffers processed during frame).
	AL_StreamUpdate();
}

/**
*
*
*
*	API Structure:
*
*
*
**/

//! OpenAL sound backend API structure with function pointers.


const sndapi_t snd_openal = {
	.init = AL_Init,
	.shutdown = AL_Shutdown,
	.update = AL_Update,
	.activate = S_StopAllSounds,
	.sound_info = AL_SoundInfo,
	.upload_sfx = AL_UploadSfx,
	.delete_sfx = AL_DeleteSfx,
	.set_eax_effect_properties = AL_SetEAXEffectProperties,
	.raw_samples = AL_RawSamples,
	.need_raw_samples = AL_NeedRawSamples,
	.drop_raw_samples = AL_StreamStop,
	.get_begin_ofs = AL_GetBeginofs,
	.play_channel = AL_PlayChannel,
	.stop_channel = AL_StopChannel,
	.stop_all_sounds = AL_StopAllSounds,
};
