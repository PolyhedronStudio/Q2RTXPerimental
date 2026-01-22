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

/********************************************************************
*
*
*	Module Name: Sound System Core
*
*	This module implements the core sound system for Q2RTXPerimental,
*	managing sound effect loading, caching, playback coordination, and
*	backend API abstraction. It serves as the central hub between the
*	game engine and platform-specific audio backends (OpenAL or DMA).
*
*	Architecture:
*	- Sound Effect Cache: LRU cache for loaded sound samples
*	- Channel Management: Allocation and lifecycle of audio channels
*	- Playsound Queue: Deferred playback with timing control
*	- Backend Abstraction: Unified API for OpenAL and DMA backends
*	- Registration System: Level-based sound precaching
*
*	Key Components:
*	- Sound Cache: Hash table of sound effects (sfx_t structures)
*	- Channel Pool: Fixed array of playback channels
*	- Playsound Queue: Linked list of pending playsounds
*	- Backend API: Function pointer table (sndapi_t)
*	- EAX Reverb: Environmental audio effects (OpenAL only)
*
*	Workflow:
*	1. Game precaches sounds during level load (S_RegisterSound)
*	2. Game requests sound playback (S_StartSound)
*	3. Core creates playsound entry with timing info
*	4. Backend issues playsound when due (per-frame update)
*	5. Backend spatializes and mixes channels
*	6. Backend transfers mixed audio to hardware
*
*	Performance Characteristics:
*	- MAX_SFX (MAX_SOUNDS*2) sound effects cached simultaneously
*	- MAX_CHANNELS (32) simultaneous playback channels
*	- MAX_PLAYSOUNDS (128) pending playsounds queue depth
*	- LRU eviction for sound cache when full
*
*
********************************************************************/

#include "sound.h"

/**
*
*
*
*	Module State:
*
*
*
**/

//! Current sound registration sequence number (incremented per level load).
int s_registration_sequence;

//! Array of sound playback channels.
channel_t s_channels[MAX_CHANNELS];

//! Number of available playback channels (backend-dependent).
int s_numchannels;

//! Sound system initialization state.
sndstarted_t s_started;

//! True if sound system is active (window has focus or forced active).
bool s_active;

//! Active sound backend API (OpenAL or DMA).
sndapi_t s_api;

//! True during sound registration phase (allows MAX_SFX cache size).
bool s_registering;

//! Current paint time in sample pairs (for timing synchronization).
int64_t s_paintedtime;

/**
*
*
*
*	Sound Effect Cache:
*
*
*
**/

#define MAX_SFX ( MAX_SOUNDS * 2 )	//! Maximum cached sound effects (double for registration).

//! Array of cached sound effects (hash table).
static sfx_t known_sfx[MAX_SFX];

//! Number of currently cached sound effects.
static int num_sfx;

/**
*
*
*
*	Playsound Queue:
*
*
*
**/

#define MAX_PLAYSOUNDS 128	//! Maximum pending playsounds.

//! Pool of playsound structures.
playsound_t s_playsounds[MAX_PLAYSOUNDS];

//! Free playsound list (linked list of available entries).
list_t s_freeplays;

//! Pending playsound list (linked list of scheduled playsounds).
list_t s_pendingplays;

/**
*
*
*
*	Console Variables:
*
*
*
**/

//! Master volume (0.0 to 1.0).
cvar_t *s_volume;

//! Ambient looping sounds enabled.
cvar_t *s_ambient;

#if USE_DEBUG
//! Debug: show playing sounds.
cvar_t *s_show;
#endif

//! Underwater audio filter state.
cvar_t *s_underwater;

//! Sound system enable/disable.
static cvar_t *s_enable;

//! Auto-focus: continue playing sound when window loses focus.
static cvar_t *s_auto_focus;
//
///**
//*   @return -1 on failure, otherwise a valid ID to the sound(not internal, openal or dma) API reverb effect resource.
//**/
//const qhandle_t S_UploadReverbEffect( const char *name, sfx_eax_properties_t *properties ) {
//    // Return -1 if we exceeded cache size.
//    if ( snd_reverb_cache.num_effects >= MAX_REVERB_EFFECTS ) {
//        return -1;
//    }
//
//    // Get effects slot index.
//    const int32_t reverb_effect_id = snd_reverb_cache.num_effects;
//
//    // Try and register it using the sound API.
//    //const qhandle_t reverb_resource_id = s_api.upload_reverb_effect( properties );
//    // Failed to upload reverb effect data.
//    //if ( reverb_resource_id == -1 ) {
//    //    return -1;
//    //}
//    // Move the reverb resource id into our cache.
//    //snd_reverb_cache.effects[ reverb_effect_id ].resource_id = reverb_resource_id;
//    // Move the properties into our cache.
//    snd_reverb_cache.effects[ reverb_effect_id ].properties = *properties;
//    // Copy the name into names cache.
//    memset( snd_reverb_cache.effects[ reverb_effect_id ].name, 0, MAX_MATERIAL_NAME );
//    Q_strlcpy( snd_reverb_cache.effects[ reverb_effect_id ].name, name, strlen(name) );
//
//    // Increment the number of properties.
//    snd_reverb_cache.num_effects++;
//
//    // Return reverb effect ID.
//    return reverb_effect_id;
//}

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
*	@brief	Display sound system information and backend status.
**/
static void S_SoundInfo_f( void ) {
	if ( !s_started ) {
		Com_Printf( "Sound system not started.\n" );
		return;
	}

	s_api.sound_info();
}

/**
*	@brief	Display list of all loaded sound effects with memory usage.
**/
static void S_SoundList_f( void ) {
    int     i, count;
    sfx_t   *sfx;
    sfxcache_t  *sc;
    size_t  total;

    total = count = 0;
    for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        sc = sfx->cache;
        if (sc) {
            total += sc->size;
            if (sc->loopstart >= 0)
                Com_Printf("L");
            else
                Com_Printf(" ");
            Com_Printf("(%2db) (%dch) %6i : %s\n", sc->width * 8, sc->channels, sc->size, sfx->name);
        } else {
            if (sfx->name[0] == '*')
                Com_Printf("  placeholder : %s\n", sfx->name);
            else
                Com_Printf("  not loaded  : %s (%s)\n",
                           sfx->name, Q_ErrorString(sfx->error));
        }
        count++;
    }
    Com_Printf("Total sounds: %d (out of %d slots)\n", count, num_sfx);
    Com_Printf("Total resident: %zu\n", total);
}

//! Console command registration table.
static const cmdreg_t c_sound[] = {
	{ "stopsound", S_StopAllSounds },
	{ "soundlist", S_SoundList_f },
	{ "soundinfo", S_SoundInfo_f },
	{ nullptr }
};

/**
*
*
*
*	Initialization and Shutdown:
*
*
*
**/

/**
*	@brief	Handle s_auto_focus cvar changes (activate/deactivate sound system).
*	@param	self	Pointer to s_auto_focus cvar.
**/
static void s_auto_focus_changed( cvar_t *self ) {
	S_Activate();
}

/**
*	@brief	Initialize sound system and select backend (OpenAL or DMA).
*	@note	Registers console variables and commands.
*	@note	Attempts to initialize OpenAL first, falls back to DMA if unavailable.
*	@note	Initializes playsound queue, sound cache, and registration sequence.
**/
void S_Init( void ) {
	s_enable = Cvar_Get( "s_enable", "2", CVAR_SOUND );
	if ( s_enable->integer <= SS_NOT ) {
		Com_Printf( "Sound initialization disabled.\n" );
		return;
	}

	Com_Printf( "------- S_Init -------\n" );

	// Register console variables.
	s_volume = Cvar_Get( "s_volume", "0.7", CVAR_ARCHIVE );
	s_ambient = Cvar_Get( "s_ambient", "1", 0 );
#if USE_DEBUG
	s_show = Cvar_Get( "s_show", "0", 0 );
#endif
	s_auto_focus = Cvar_Get( "s_auto_focus", "0", 0 );
	s_underwater = Cvar_Get( "s_underwater", "1", 0 );

	// Attempt to start one of available sound engines.
	s_started = SS_NOT;

#if USE_OPENAL
	// Try OpenAL backend first (preferred for 3D audio and EAX).
	if ( s_started == SS_NOT && s_enable->integer >= SS_OAL && snd_openal.init() ) {
		s_started = SS_OAL;
		s_api = snd_openal;
	}
#endif

#if USE_SNDDMA
	// Fall back to DMA backend if OpenAL unavailable.
	if ( s_started == SS_NOT && s_enable->integer >= SS_DMA && snd_dma.init() ) {
		s_started = SS_DMA;
		s_api = snd_dma;
	}
#endif

	if ( s_started == SS_NOT ) {
		Com_EPrintf( "Sound failed to initialize.\n" );
		goto fail;
	}

	// Register console commands.
	Cmd_Register( c_sound );

	// Initialize playsound queue and clear DMA buffer.
	S_StopAllSounds();

	// Set up auto-focus callback.
	s_auto_focus->changed = s_auto_focus_changed;
	s_auto_focus_changed( s_auto_focus );

	// Initialize sound cache and timing.
	num_sfx = 0;
	s_paintedtime = 0;
	s_registration_sequence = 1;

	// Resume music if we were playing before (level transitions).
	if ( cls.state >= ca_precached ) {
		OGG_RecoverState();
	}

fail:
	Cvar_SetInteger( s_enable, s_started, FROM_CODE );
	Com_Printf( "----------------------\n" );
}


/**
*	@brief	Free single sound effect and associated resources.
*	@param	sfx	Sound effect structure to free.
*	@note	Calls backend delete_sfx, frees cache and truename, zeros structure.
**/
static void S_FreeSound( sfx_t *sfx ) {
	if ( s_api.delete_sfx )
		s_api.delete_sfx( sfx );
	if ( sfx->cache )
		Z_Free( sfx->cache );
	if ( sfx->truename )
		Z_Free( sfx->truename );
	memset( sfx, 0, sizeof( *sfx ) );
}

/**
*	@brief	Free all loaded sound effects from cache.
*	@note	Called during shutdown and sound system restart.
**/
void S_FreeAllSounds( void ) {
	int i;
	sfx_t *sfx;

	// Free all loaded sounds from cache.
	for ( i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++ ) {
		if ( !sfx->name[0] )
			continue;
		S_FreeSound( sfx );
	}

	num_sfx = 0;
}

/**
*	@brief	Shutdown sound system and release all resources.
*	@note	Stops all sounds, frees cache, saves music state, shuts down backend.
**/
void S_Shutdown( void ) {
	if ( !s_started )
		return;

	// Stop all active sounds and playsounds.
	S_StopAllSounds();
	
	// Free sound effect cache.
	S_FreeAllSounds();
	
	// Save music state for next session.
	OGG_SaveState();
	OGG_Stop();

	// Shutdown backend (OpenAL or DMA).
	s_api.shutdown();
	memset( &s_api, 0, sizeof( s_api ) );

	s_started = SS_NOT;
	s_active = false;

	// Remove cvar callback.
	s_auto_focus->changed = nullptr;

	// Unregister console commands.
	Cmd_Deregister( c_sound );

	// Check for memory leaks in sound tag.
	Z_LeakTest( TAG_SOUND );
}

/**
*	@brief	Activate or deactivate sound system based on window focus.
*	@note	Called when window gains/loses focus or s_auto_focus changes.
*	@note	Drops streaming samples when deactivating to prevent stale audio.
**/
void S_Activate( void ) {
	bool active;
	active_t level;

	if ( !s_started )
		return;

	// Determine activation level from s_auto_focus cvar.
	level = Cvar_ClampInteger( s_auto_focus, ACT_MINIMIZED, ACT_ACTIVATED );

	// Check if client activation level meets threshold.
	active = cls.active >= level;

	if ( active == s_active )
		return;

	Com_DDDPrintf( "%s: %d\n", __func__, active );
	s_active = active;

	// Drop streaming samples when deactivating.
	if ( !active )
		s_api.drop_raw_samples();

	s_api.activate();
}

/**
*
*
*
*	Sound Loading and Registration:
*
*
*
**/

/**
*	@brief	Convert sound handle to sfx_t pointer.
*	@param	hSfx	Sound handle (1-based index).
*	@return	Pointer to sfx_t structure, or NULL if handle is 0.
*	@note	Validates handle range and throws error if out of bounds.
**/
sfx_t *S_SfxForHandle( qhandle_t hSfx ) {
	if ( !hSfx ) {
		return nullptr;
	}

	if ( hSfx < 1 || hSfx > num_sfx ) {
		Com_Error( ERR_DROP, "S_SfxForHandle: %d out of range", hSfx );
	}

	return &known_sfx[hSfx - 1];
}

/**
*	@brief	Allocate new sfx_t slot from cache.
*	@return	Pointer to free sfx_t slot, or NULL if cache is full.
*	@note	Searches for free slot (name[0] == 0) or allocates new slot.
**/
static sfx_t *S_AllocSfx( void ) {
	sfx_t *sfx;
	int i;

	// Find first free sfx slot.
	for ( i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++ ) {
		if ( !sfx->name[0] )
			break;
	}

	// Allocate new slot if no free slots found.
	if ( i == num_sfx ) {
		if ( num_sfx == MAX_SFX )
			return nullptr;
		num_sfx++;
	}

	return sfx;
}

/**
*	@brief	Find or allocate sound effect by name.
*	@param	name		Sound file path.
*	@param	namelen		Length of name string.
*	@return	Pointer to existing or newly allocated sfx_t, or NULL if cache full.
*	@note	Updates registration sequence for LRU cache management.
**/
static sfx_t *S_FindName( const char *name, size_t namelen ) {
	int i;
	sfx_t *sfx;

	// Search for existing sound with same name.
	for ( i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++ ) {
		if ( !FS_pathcmp( sfx->name, name ) ) {
			sfx->registration_sequence = s_registration_sequence;
			return sfx;
		}
	}

	// Allocate new sfx slot.
	sfx = S_AllocSfx();
	if ( sfx ) {
		memcpy( sfx->name, name, namelen + 1 );
		sfx->registration_sequence = s_registration_sequence;
	}
	return sfx;
}

/**
*	@brief	Begin sound registration phase for new level.
*	@note	Increments registration sequence for LRU cache management.
*	@note	Sounds not re-registered will be freed during S_EndRegistration.
**/
void S_BeginRegistration( void ) {
	s_registration_sequence++;
	s_registering = true;
}

/**
*	@brief	Register sound effect for precaching.
*	@param	name	Sound file path (relative to game directory or absolute).
*	@return	Sound handle (1-based index), or 0 on error.
*	@note	Accepts three naming conventions:
*			- Empty string: silently returns 0
*			- '*' prefix: placeholder for client-specific sounds
*			- '#' prefix: absolute path (strips '#' prefix)
*			- Normal: prepends "sound/" directory
*	@note	Loads sound immediately if not in registration phase.
**/
qhandle_t S_RegisterSound( const char *name ) {
    char    buffer[MAX_QPATH];
    sfx_t   *sfx;
    size_t  len;

    if (!s_started)
        return 0;

    // empty names are legal, silently ignore them
    if (!*name)
        return 0;

    if (*name == '*') {
        len = Q_strlcpy(buffer, name, MAX_QPATH);
    } else if (*name == '#') {
        len = FS_NormalizePathBuffer(buffer, name + 1, MAX_QPATH);
    } else {
        len = Q_concat(buffer, MAX_QPATH, "sound/", name);
        if (len < MAX_QPATH)
            len = FS_NormalizePath(buffer);
    }

    // this MAY happen after prepending "sound/"
    if (len >= MAX_QPATH) {
        Com_DPrintf("%s: oversize name\n", __func__);
        return 0;
    }

    // normalized to empty name?
    if (len == 0) {
        Com_DPrintf("%s: empty name\n", __func__);
        return 0;
    }

    sfx = S_FindName(buffer, len);
    if (!sfx) {
        Com_DPrintf("%s: out of slots\n", __func__);
        return 0;
    }

    if (!s_registering) {
        S_LoadSound(sfx);
    }

    return (sfx - known_sfx) + 1;
}

/**
*   @brief  Returns the name of the sound matching the resource handle.
**/
const char *S_SoundNameForHandle( const qhandle_t soundResourceHandle ) {
	// Get the sfx.
    sfx_t *sfx = S_SfxForHandle( soundResourceHandle );
	// Null check.
    if ( !sfx ) {
        return "<null>";
    }
	// Return the name.
    return sfx->name;
}

/*
====================
S_RegisterSexedSound
====================
*/
static sfx_t *S_RegisterClientInfoModelSound( int entnum, const char *base ) {
    sfx_t *sfx = NULL;
    const char *model;
    char         buffer[ MAX_QPATH ];
    int          len;

    // Determine which player model to use (baseclientinfo for entnum<=0)
    if ( entnum > 0 && entnum <= MAX_CLIENTS ) {
        model = cl.clientinfo[ entnum - 1 ].model_name;
    } else {
        model = cl.baseclientinfo.model_name;
    }

    // Default to "playerdummy" if model is empty
    if ( !*model ) {
        model = "playerdummy";
    }

    // 1) Try model-specific path: "players/<model>/<sound>"
    len = snprintf( buffer, MAX_QPATH, "players/%s/%s", model, base + 1 );
    if ( len >= MAX_QPATH )
        return NULL;
    len = FS_NormalizePath( buffer );
    sfx = S_FindName( buffer, len );

    // 2) If model-specific not found, fall back to generic player sound "sound/player/<sound>"
    if ( !sfx ) {
        len = snprintf( buffer, MAX_QPATH, "sound/player/%s", base + 1 );
        if ( len >= MAX_QPATH )
            return NULL;
        len = FS_NormalizePath( buffer );
        sfx = S_FindName( buffer, len );
    }

    // If we found an sfx placeholder and it's not registered, preserve legacy behavior:
    // set truename fallback when not currently registering and the sfx can't be loaded.
    if ( sfx && !sfx->truename && !s_registering && !S_LoadSound( sfx ) ) {
        // ensure buffer contains the fallback path; prefer explicit "sound/player/..." truename
        if ( snprintf( buffer, MAX_QPATH, "sound/player/%s", base + 1 ) < MAX_QPATH ) {
            FS_NormalizePath( buffer );
            sfx->error = Q_ERR_SUCCESS;
            sfx->truename = S_CopyString( buffer );
        }
    }

    return sfx;
}

static void S_RegisterClientInfoModelSounds(void)
{
    static int32_t     sounds[MAX_SFX];
    int32_t     i, j, total;
    sfx_t   *sfx;

    // Find player sounds
    total = 0;
    for ( i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++ ) {
        //if ( sfx->name[ 0 ] != '*' ) {
        //    continue;
        //}
        if ( sfx->registration_sequence != s_registration_sequence ) {
            continue;
        }

        sounds[ total++ ] = i;
    }

    // Register sounds for baseclientinfo and other valid clientinfos
    for ( i = 0; i <= MAX_CLIENTS; i++ ) {
        if ( i > 0 && !cl.clientinfo[ i - 1 ].model_name[ 0 ] ) {
            continue;
        }
        for ( j = 0; j < total; j++ ) {
            sfx = &known_sfx[ sounds[ j ] ];
            S_RegisterClientInfoModelSound( i, sfx->name );
        }
    }
}

///**
//*   @brief  Registers a reverb effect, returning a qhandle, which is -1 on failure, >= 0 otherwise.
//**/
//const qhandle_t S_RegisterReverbEffect( const char *name, sfx_eax_properties_t *properties ) {
//    return S_UploadReverbEffect( name, properties );
//}

/**
*   @brief  Updates the EAX Environment settings.
**/
const qboolean S_SetEAXEnvironmentProperties( const sfx_eax_properties_t *properties ) {
    return s_api.set_eax_effect_properties( properties );
}

///**
//*   @brief  Returns a const ptr to the eax effect properties.
//**/
//const sfx_eax_properties_t *S_GetEAXProperties( const qhandle_t id ) {
//    return &snd_reverb_cache.effects[ id ].properties;
//}

/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration(void)
{
    int     i;
    sfx_t   *sfx;

    S_RegisterClientInfoModelSounds();

    // clear playsound list, so we don't free sfx still present there
    S_StopAllSounds();

    // free any sounds not from this registration sequence
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        if (sfx->registration_sequence != s_registration_sequence) {
            // don't need this sound
            S_FreeSound(sfx);
            continue;
        }
        // make sure it is paged in
        if (s_api.page_in_sfx)
            s_api.page_in_sfx(sfx);
    }

    // load everything in
    for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++) {
        if (!sfx->name[0])
            continue;
        S_LoadSound(sfx);
    }

    s_registering = false;
}


//=============================================================================

/*
=================
S_PickChannel

picks a channel based on priorities, empty slots, number of channels
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel)
{
    int         ch_idx;
    int         first_to_die;
    int64_t     life_left;
    channel_t   *ch;

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    // life_left 
    life_left = 0x7fffffff;
    for ( ch_idx = 0; ch_idx < s_numchannels; ch_idx++ ) {
        ch = &s_channels[ ch_idx ];
        // channel 0 never overrides unless out of channels
        if ( ch->entnum == entnum && ch->entchannel == entchannel && entchannel != 0 ) {
            if ( entchannel > 255 && ch->sfx )
                return NULL;    // channels >255 only allow single sfx on that channel
            // always override sound from same entity
            first_to_die = ch_idx;
            break;
        }

        // don't let monster sounds override player sounds
        if ( ch->entnum == cl.listener_spatialize.entnum && entnum != cl.listener_spatialize.entnum && ch->sfx ) {
           continue;
        }
		// find the channel with the least time left
        if (ch->end - s_paintedtime < life_left) {
            life_left = ch->end - s_paintedtime;
            first_to_die = ch_idx;
        }
    }

    if ( first_to_die == -1 ) {
        return NULL;
    }

    ch = &s_channels[first_to_die];
    if (s_api.stop_channel)
        s_api.stop_channel(ch);
    memset(ch, 0, sizeof(*ch));

    return ch;
}

/*
=================
S_AllocPlaysound
=================
*/
static playsound_t *S_AllocPlaysound(void)
{
    playsound_t *ps = PS_FIRST(&s_freeplays);

    if (PS_TERM(ps, &s_freeplays))
        return NULL;        // no free playsounds

    // unlink from freelist
    List_Remove(&ps->entry);

    return ps;
}

/*
=================
S_FreePlaysound
=================
*/
static void S_FreePlaysound(playsound_t *ps)
{
    // unlink from channel
    List_Remove(&ps->entry);

    // add to free list
    List_Insert(&s_freeplays, &ps->entry);
}

/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void S_IssuePlaysound(playsound_t *ps)
{
    channel_t   *ch;
    sfxcache_t  *sc;

#if USE_DEBUG
    if (s_show->integer)
        Com_Printf("Issue %i\n", ps->begin);
#endif
    // pick a channel to play on
    ch = S_PickChannel(ps->entnum, ps->entchannel);
    if (!ch) {
        S_FreePlaysound(ps);
        return;
    }

    sc = S_LoadSound(ps->sfx);
    if (!sc) {
        Com_Printf("S_IssuePlaysound: couldn't load %s\n", ps->sfx->name);
        S_FreePlaysound(ps);
        return;
    }

    // spatialize
    if (ps->attenuation == ATTN_STATIC)
        ch->dist_mult = ps->attenuation * 0.001f;
    else
        ch->dist_mult = ps->attenuation * 0.0005f;
    ch->master_vol = ps->volume;
    ch->entnum = ps->entnum;
    ch->entchannel = ps->entchannel;
    ch->sfx = ps->sfx;
    VectorCopy(ps->origin, ch->origin);
    ch->fixed_origin = ps->fixed_origin;
    ch->pos = 0;
    ch->end = s_paintedtime + sc->length;

    s_api.play_channel(ch);

    // free the playsound
    S_FreePlaysound(ps);
}

// =======================================================================
// Start a sound effect
// =======================================================================
/**
*   @brief  Validates the parms and ques the sound up
*           if pos is NULL, the sound will be dynamically sourced from the entity
*           Entchannel 0 will never override a playing sound
**/
void S_StartSound(const vec3_t origin, const int32_t entnum, const int32_t entchannel, const qhandle_t hSfx, const float vol, const float attenuation, const float timeofs) {
    sfxcache_t  *sc;
    playsound_t *ps, *sort;
    sfx_t       *sfx;

    if ( !s_started ) {
        return;
    }
    if ( !s_active ) {
        return;
    }
    if ( !( sfx = S_SfxForHandle( hSfx ) ) ) {
        return;
    }

    //if (sfx->name[0] == '*') {
    //    sfx = S_RegisterClientInfoModelSound(entnum, sfx->name);
    //    if ( !sfx ) {
    //        return;
    //    }
    //}

    // make sure the sound is loaded
    sc = S_LoadSound(sfx);
    if ( !sc ) {
        return;     // couldn't load the sound's data
    }
    // make the playsound_t
    ps = S_AllocPlaysound();
    if ( !ps ) {
        return;
    }

    if (origin) {
        VectorCopy(origin, ps->origin);
        ps->fixed_origin = true;
    } else {
        ps->fixed_origin = false;
    }

    ps->entnum = entnum;
    ps->entchannel = entchannel;
    ps->attenuation = attenuation;
    ps->volume = vol;
    ps->sfx = sfx;
    ps->begin = s_api.get_begin_ofs(timeofs);

    // sort into the pending sound list
    LIST_FOR_EACH(playsound_t, sort, &s_pendingplays, entry)
        if (sort->begin >= ps->begin)
            break;

    List_Append(&sort->entry, &ps->entry);
}

void S_ParseStartSound(void)
{
    qhandle_t handle = cl.sound_precache[ cl.snd.index ];

    if ( !handle )
        return;

#if USE_DEBUG
    if ( developer->integer && !( cl.snd.flags & SND_POS ) ) {
        CL_CheckEntityPresent( cl.snd.entity, "sound" );
    }
#endif

    S_StartSound( ( cl.snd.flags & SND_POS ) ? cl.snd.pos : NULL,
        cl.snd.entity, cl.snd.channel, handle,
        cl.snd.volume, cl.snd.attenuation, cl.snd.timeofs );
}

/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound(const char *sound)
{
    if (s_started) {
        qhandle_t sfx = S_RegisterSound(sound);
        S_StartSound(NULL, cl.listener_spatialize.entnum, 0, sfx, 1, ATTN_NONE, 0);
    }
}

void S_StartLocalSoundOnce(const char *sound)
{
    if (s_started) {
        qhandle_t sfx = S_RegisterSound(sound);
        S_StartSound(NULL, cl.listener_spatialize.entnum, 256, sfx, 1, ATTN_NONE, 0);
    }
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds(void)
{
    int     i;

    if (!s_started)
        return;

    // clear all the playsounds
    memset(s_playsounds, 0, sizeof(s_playsounds));

    List_Init(&s_freeplays);
    List_Init(&s_pendingplays);

    for (i = 0; i < MAX_PLAYSOUNDS; i++)
        List_Append(&s_freeplays, &s_playsounds[i].entry);

    s_api.stop_all_sounds();

    // clear all the channels
    memset(s_channels, 0, sizeof(s_channels));
}

void S_RawSamples(int samples, int rate, int width, int channels, const byte *data)
{
    if (s_started)
        s_api.raw_samples(samples, rate, width, channels, data, 1.0f);
}

// =======================================================================
// Update sound buffer
// =======================================================================
/**
*   @brief  Iterates all packet entities and builds a sound list
*           according to s_ambient cvar value. When s_ambient is 2,
*           only entities with models will have sounds, when 3,
*           only the listener entity will have sounds.
*           Otherwise, all entities will have their sound member processed.
**/
void S_BuildSoundList(int *sounds)
{
    int         i;
    int         num;
    entity_state_t  *ent;

    for (i = 0; i < cl.frame.numEntities; i++) {
        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];
        if (s_ambient->integer == 2 && !ent->modelindex) {
            sounds[i] = 0;
        } else if (s_ambient->integer == 3 && ent->number != cl.listener_spatialize.entnum) {
            sounds[i] = 0;
        } else {
            sounds[i] = ent->sound;
        }
    }
}

// =======================================================================
// Update Spatial Listener Positioning
// =======================================================================
void S_SetupSpatialListener( const vec3_t viewOrigin, const vec3_t vForward, const vec3_t vRight, const vec3_t vUp ) {
    VectorCopy( viewOrigin, cl.listener_spatialize.origin );
    VectorCopy( vForward, cl.listener_spatialize.v_forward );
    VectorCopy( vRight, cl.listener_spatialize.v_right );
    VectorCopy( vUp, cl.listener_spatialize.v_up );
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(void)
{
    if (cvar_modified & CVAR_SOUND) {
        Cbuf_AddText(&cmd_buffer, "snd_restart\n");
        cvar_modified &= ~CVAR_SOUND;
        return;
    }

    if (!s_started)
        return;

    // if the laoding plaque is up, clear everything
    // out to make sure we aren't looping a dirty
    // dma buffer while loading
    if (cls.state == ca_loading) {
        // S_ClearBuffer should be already done in S_StopAllSounds
        return;
    }

    // set listener entity number
    // other parameters should be already set up by CL_CalcViewValues
    if (cls.state != ca_active) {
        cl.listener_spatialize.entnum = -1;
    } else {
        cl.listener_spatialize.entnum = cl.frame.ps.clientNumber + 1;
    }

    OGG_Update();

    s_api.update();
}

/**
*   @brief   
**/
float S_GetLinearVolume(float perceptual)
{
    float volume = perceptual;
    
    // clamp anything below 1% to zero
    if (volume < 0.01f)
        return 0.f;

    // 50 dB exponential curve
    // more info: https://www.dr-lex.be/info-stuff/volumecontrols.html 
    volume = 0.003161f * expf(perceptual * 5.757f);

    // upper limit
    volume = min(1.f, volume);

    return volume;
}

