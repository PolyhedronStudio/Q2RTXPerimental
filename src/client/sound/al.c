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

#include "sound.h"

#if USE_FIXED_LIBAL
#include "qal/fixed.h"
#else
#include "qal/dynamic.h"
#endif

// translates from AL coordinate system to quake
#define AL_UnpackVector(v)  -v[1],v[2],-v[0]
#define AL_CopyVector(a,b)  ((b)[0]=-(a)[1],(b)[1]=(a)[2],(b)[2]=-(a)[0])

// OpenAL implementation should support at least this number of sources
#define MIN_CHANNELS    16

static ALuint       s_srcnums[MAX_CHANNELS];
static ALuint       s_stream;
static ALuint       s_stream_buffers;
static ALboolean    s_loop_points;
static ALboolean    s_source_spatialize;
static uint64_t     s_framecount;

static ALuint       s_underwater_filter;
static bool         s_underwater_flag;


/**
*
*
*   EAX Reverb:
*
*
**/
static ALuint s_auxiliary_effect_slot;

/**
*   @brief
**/
static void _AL_GenerateAuxiliaryEffectsSlot(void) {
    qalGenAuxiliaryEffectSlots( 1, &s_auxiliary_effect_slot );
}
/**
*   @brief
**/
static void _AL_DeleteAuxiliaryEffectsSlot( void ) {
    qalDeleteAuxiliaryEffectSlots( 1, &s_auxiliary_effect_slot );
}

/**
*   @brief  Will update the reverb effect ID with the given properties.
**/
static const qboolean AL_UpdateReverbEffect( const qhandle_t effect, sfx_reverb_properties_t *reverb ) {
#ifdef AL_EFFECT_EAXREVERB
    //try eax reverb for more settings
    qalEffecti( effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );
    if ( !qalGetError() ) {
        /* EAX Reverb is available. Set the EAX effect type then load the
         * reverb properties. */
        qalEffectf( effect, AL_EAXREVERB_DENSITY, reverb->flDensity );
        qalEffectf( effect, AL_EAXREVERB_DIFFUSION, reverb->flDiffusion );
        qalEffectf( effect, AL_EAXREVERB_GAIN, reverb->flGain );
        qalEffectf( effect, AL_EAXREVERB_GAINHF, reverb->flGainHF );
        qalEffectf( effect, AL_EAXREVERB_GAINLF, reverb->flGainLF );
        qalEffectf( effect, AL_EAXREVERB_DECAY_TIME, reverb->flDecayTime );
        qalEffectf( effect, AL_EAXREVERB_DECAY_HFRATIO, reverb->flDecayHFRatio );
        qalEffectf( effect, AL_EAXREVERB_DECAY_LFRATIO, reverb->flDecayLFRatio );
        qalEffectf( effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain );
        qalEffectf( effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay );
        qalEffectfv( effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan );
        qalEffectf( effect, AL_EAXREVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain );
        qalEffectf( effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay );
        qalEffectfv( effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan );
        qalEffectf( effect, AL_EAXREVERB_ECHO_TIME, reverb->flEchoTime );
        qalEffectf( effect, AL_EAXREVERB_ECHO_DEPTH, reverb->flEchoDepth );
        qalEffectf( effect, AL_EAXREVERB_MODULATION_TIME, reverb->flModulationTime );
        qalEffectf( effect, AL_EAXREVERB_MODULATION_DEPTH, reverb->flModulationDepth );
        qalEffectf( effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF );
        qalEffectf( effect, AL_EAXREVERB_HFREFERENCE, reverb->flHFReference );
        qalEffectf( effect, AL_EAXREVERB_LFREFERENCE, reverb->flLFReference );
        qalEffectf( effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor );
        qalEffecti( effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit );
    } else
#endif // #ifdef AL_EFFECT_EAXREVERB
    {
#ifdef AL_EFFECT_REVERB
        /* No EAX Reverb. Set the standard reverb effect type then load the
         * available reverb properties. */
        qalEffecti( effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB );

        qalEffectf( effect, AL_REVERB_DENSITY, reverb->flDensity );
        qalEffectf( effect, AL_REVERB_DIFFUSION, reverb->flDiffusion );
        qalEffectf( effect, AL_REVERB_GAIN, reverb->flGain );
        qalEffectf( effect, AL_REVERB_GAINHF, reverb->flGainHF );
        qalEffectf( effect, AL_REVERB_DECAY_TIME, reverb->flDecayTime );
        qalEffectf( effect, AL_REVERB_DECAY_HFRATIO, reverb->flDecayHFRatio );
        qalEffectf( effect, AL_REVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain );
        qalEffectf( effect, AL_REVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay );
        qalEffectf( effect, AL_REVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain );
        qalEffectf( effect, AL_REVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay );
        qalEffectf( effect, AL_REVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF );
        qalEffectf( effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor );
        qalEffecti( effect, AL_REVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit );
#endif // #ifdef AL_EFFECT_REVERB
    }

    return true;
}

/**
*   @brief  Returns if succesful, the newly allocated reverb effect ID.
**/
static qhandle_t AL_UploadReverbEffect( sfx_reverb_properties_t *reverb ) {
    ALuint effect = 0;

    // Allocate a unique reverb effect resource ID.
    qalGetError();
    qalGenEffects( 1, &effect );

    // Update the reverb effect properties.
    AL_UpdateReverbEffect( effect, reverb );
//    //try eax reverb for more settings
//    qalEffecti( effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );
//    if ( !qalGetError() ) {
//        /* EAX Reverb is available. Set the EAX effect type then load the
//         * reverb properties. */
//        qalEffectf( effect, AL_EAXREVERB_DENSITY, reverb->flDensity );
//        qalEffectf( effect, AL_EAXREVERB_DIFFUSION, reverb->flDiffusion );
//        qalEffectf( effect, AL_EAXREVERB_GAIN, reverb->flGain );
//        qalEffectf( effect, AL_EAXREVERB_GAINHF, reverb->flGainHF );
//        qalEffectf( effect, AL_EAXREVERB_GAINLF, reverb->flGainLF );
//        qalEffectf( effect, AL_EAXREVERB_DECAY_TIME, reverb->flDecayTime );
//        qalEffectf( effect, AL_EAXREVERB_DECAY_HFRATIO, reverb->flDecayHFRatio );
//        qalEffectf( effect, AL_EAXREVERB_DECAY_LFRATIO, reverb->flDecayLFRatio );
//        qalEffectf( effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain );
//        qalEffectf( effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay );
//        qalEffectfv( effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan );
//        qalEffectf( effect, AL_EAXREVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain );
//        qalEffectf( effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay );
//        qalEffectfv( effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan );
//        qalEffectf( effect, AL_EAXREVERB_ECHO_TIME, reverb->flEchoTime );
//        qalEffectf( effect, AL_EAXREVERB_ECHO_DEPTH, reverb->flEchoDepth );
//        qalEffectf( effect, AL_EAXREVERB_MODULATION_TIME, reverb->flModulationTime );
//        qalEffectf( effect, AL_EAXREVERB_MODULATION_DEPTH, reverb->flModulationDepth );
//        qalEffectf( effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF );
//        qalEffectf( effect, AL_EAXREVERB_HFREFERENCE, reverb->flHFReference );
//        qalEffectf( effect, AL_EAXREVERB_LFREFERENCE, reverb->flLFReference );
//        qalEffectf( effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor );
//        qalEffecti( effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit );
//    } else
//#endif // #ifdef AL_EFFECT_EAXREVERB
//    {
//#ifdef AL_EFFECT_REVERB
//        /* No EAX Reverb. Set the standard reverb effect type then load the
//         * available reverb properties. */
//        qalEffecti( effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB );
//
//        qalEffectf( effect, AL_REVERB_DENSITY, reverb->flDensity );
//        qalEffectf( effect, AL_REVERB_DIFFUSION, reverb->flDiffusion );
//        qalEffectf( effect, AL_REVERB_GAIN, reverb->flGain );
//        qalEffectf( effect, AL_REVERB_GAINHF, reverb->flGainHF );
//        qalEffectf( effect, AL_REVERB_DECAY_TIME, reverb->flDecayTime );
//        qalEffectf( effect, AL_REVERB_DECAY_HFRATIO, reverb->flDecayHFRatio );
//        qalEffectf( effect, AL_REVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain );
//        qalEffectf( effect, AL_REVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay );
//        qalEffectf( effect, AL_REVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain );
//        qalEffectf( effect, AL_REVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay );
//        qalEffectf( effect, AL_REVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF );
//        qalEffectf( effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor );
//        qalEffecti( effect, AL_REVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit );
//#endif // #ifdef AL_EFFECT_REVERB
//    }
    return effect;
}

/**
*   @brief  Deletes the reverb resource IDs data.
**/
static void AL_DeleteReverbEffect( const qhandle_t resourceID ) {
    // Delete the reverb effect from memory.
    qalDeleteEffects( 1, &resourceID );
}
/**
*   @brief  Apply the reverb resource IDs data.
**/
static void AL_SetReverbEffect( const qhandle_t resourceID ) {
    qalAuxiliaryEffectSloti( s_auxiliary_effect_slot, AL_EFFECTSLOT_EFFECT, resourceID );
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

    Com_DPrintf("Initializing OpenAL\n");

    if (!QAL_Init()) {
        goto fail0;
    }

    Com_DPrintf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    Com_DPrintf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    Com_DPrintf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    Com_DDPrintf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));

    // check for linear distance extension
    if (!qalIsExtensionPresent("AL_EXT_LINEAR_DISTANCE")) {
        Com_SetLastError("AL_EXT_LINEAR_DISTANCE extension is missing");
        goto fail1;
    }

    // generate source names
    qalGetError();
    qalGenSources(1, &s_stream);
	if (qalGetError() != AL_NO_ERROR)
	{
		Com_Printf("ERROR: Couldn't get a single Source.\n");
		goto fail1;
	}
    for (i = 0; i < MAX_CHANNELS; i++) {
        qalGenSources(1, &s_srcnums[i]);
        if (qalGetError() != AL_NO_ERROR) {
            break;
        }
    }

    Com_DPrintf("Got %d AL sources\n", i);

    if (i < MIN_CHANNELS) {
        Com_SetLastError("Insufficient number of AL sources");
        goto fail1;
    }

    s_numchannels = i;

    s_loop_points = qalIsExtensionPresent("AL_SOFT_loop_points");
    s_source_spatialize = qalIsExtensionPresent("AL_SOFT_source_spatialize");

    // init stream source
    qalSourcef(s_stream, AL_ROLLOFF_FACTOR, 0.0f);
    qalSourcei(s_stream, AL_SOURCE_RELATIVE, AL_TRUE);
    if (s_source_spatialize)
        qalSourcei(s_stream, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);

    // Generate the global auxiliary effects slot, we use this to apply reverb effects on.
    _AL_GenerateAuxiliaryEffectsSlot();

    Com_Printf("OpenAL initialized.\n");
    return true;

fail1:
    QAL_Shutdown();
fail0:
    Com_EPrintf("Failed to initialize OpenAL: %s\n", Com_GetLastError());
    return false;
}

static void AL_Shutdown(void)
{
    Com_Printf("Shutting down OpenAL.\n");

    if (s_numchannels) {
        // delete source names
        qalDeleteSources(s_numchannels, s_srcnums);
        memset(s_srcnums, 0, sizeof(s_srcnums));
        s_numchannels = 0;
    }

    if (s_stream) {
        AL_StreamStop();
        qalDeleteSources(1, &s_stream);
        s_stream = 0;
    }

    // Delete the global auxiliary effects slot.
    _AL_DeleteAuxiliaryEffectsSlot();

    QAL_Shutdown();
}

static sfxcache_t *AL_UploadSfx(sfx_t *s)
{
    ALsizei size = s_info.samples * s_info.width * s_info.channels;
    ALenum format = AL_FORMAT_MONO8 + (s_info.channels - 1) * 2 + (s_info.width - 1);
    ALuint buffer = 0;

    qalGetError();
    qalGenBuffers(1, &buffer);
    if (qalGetError())
        goto fail;

    qalBufferData(buffer, format, s_info.data, size, s_info.rate);
    if (qalGetError()) {
        qalDeleteBuffers(1, &buffer);
        goto fail;
    }

    // specify OpenAL-Soft style loop points
    if (s_info.loopstart > 0 && s_loop_points) {
        ALint points[2] = { s_info.loopstart, s_info.samples };
        qalBufferiv(buffer, AL_LOOP_POINTS_SOFT, points);
    }

    // allocate placeholder sfxcache
    sfxcache_t *sc = s->cache = S_Malloc(sizeof(*sc));
    sc->length = s_info.samples * 1000LL / s_info.rate; // in msec
    sc->loopstart = s_info.loopstart;
    sc->width = s_info.width;
    sc->channels = s_info.channels;
    sc->size = size;
    sc->bufnum = buffer;

    return sc;

fail:
    s->error = Q_ERR_LIBRARY_ERROR;
    return NULL;
}

static void AL_DeleteSfx(sfx_t *s)
{
    sfxcache_t *sc = s->cache;
    if (sc) {
        ALuint name = sc->bufnum;
        qalDeleteBuffers(1, &name);
    }
}

static int AL_GetBeginofs( float timeofs ) {
    return s_paintedtime + timeofs * 1000;
}

static void AL_Spatialize( channel_t *ch ) {
    // Actual origin to apply for AL_POSITION.
    vec3_t      origin;

    //qhandle_t reverb_resource_id = 0;

    // Anything coming from the view entity will always be full volume:
    // 'NO' Attenuation == 'NO' Spatialization.
    if ( S_IsFullVolume(ch) ) {
        VectorCopy( cl.listener_spatialize.origin, origin);
    // Used channel origin in case of a fixed set origin:
    } else if ( ch->fixed_origin ) {
        VectorCopy( ch->origin, origin );
    // Otherwise, retreive exact interpolated origin for the entity:
    } else {
        CL_GetEntitySoundOrigin( ch->entnum, origin );
        
        // Apply the current reverb properties.
        //reverb_resource_id = CL_GetEAXBySoundOrigin( ch->entnum, origin );
    }

    // 'Software' spatialization:
    if (s_source_spatialize) {
        qalSourcei(ch->srcnum, AL_SOURCE_SPATIALIZE_SOFT, !S_IsFullVolume(ch) );
    }

    // Spatialize(Set position) for the channel audio effect to play from.
    qalSource3f( ch->srcnum, AL_POSITION, AL_UnpackVector(origin));

    // Make sure to apply the reverb effect slot to the sound channel.
    qalSource3i( ch->srcnum, AL_AUXILIARY_SEND_FILTER, s_auxiliary_effect_slot, 0, AL_FILTER_NULL );
}

static void AL_StopChannel(channel_t *ch)
{
    if (!ch->sfx)
        return;

#if USE_DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name);
#endif

    // stop it
    qalSourceStop(ch->srcnum);
    qalSourcei(ch->srcnum, AL_BUFFER, AL_NONE);
    memset(ch, 0, sizeof(*ch));
}

static void AL_PlayChannel(channel_t *ch)
{
    sfxcache_t *sc = ch->sfx->cache;

#if USE_DEBUG
    if (s_show->integer > 1)
        Com_Printf("%s: %s\n", __func__, ch->sfx->name);
#endif

    ch->srcnum = s_srcnums[ch - s_channels];
    qalGetError();
    qalSourcei(ch->srcnum, AL_BUFFER, sc->bufnum);
    qalSourcei(ch->srcnum, AL_LOOPING, ch->autosound || sc->loopstart >= 0);
    qalSourcef(ch->srcnum, AL_GAIN, ch->master_vol);
    qalSourcef(ch->srcnum, AL_REFERENCE_DISTANCE, SOUND_FULLVOLUME);
    qalSourcef(ch->srcnum, AL_MAX_DISTANCE, CM_MAX_WORLD_SIZE );
    qalSourcef(ch->srcnum, AL_ROLLOFF_FACTOR, ch->dist_mult * ( CM_MAX_WORLD_SIZE - SOUND_FULLVOLUME ) );

    AL_Spatialize(ch);

    // play it
    qalSourcePlay(ch->srcnum);
    if (qalGetError() != AL_NO_ERROR) {
        AL_StopChannel(ch);
    }
}

static void AL_IssuePlaysounds(void)
{
    // start any playsounds
    while (1) {
        playsound_t *ps = PS_FIRST(&s_pendingplays);
        if (PS_TERM(ps, &s_pendingplays))
            break;  // no more pending sounds
        if (ps->begin > s_paintedtime)
            break;
        S_IssuePlaysound(ps);
    }
}

static void AL_StopAllSounds(void)
{
    int         i;
    channel_t   *ch;

    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->sfx)
            continue;
        AL_StopChannel(ch);
    }
}

static channel_t *AL_FindLoopingSound(int entnum, sfx_t *sfx)
{
    int         i;
    channel_t   *ch;

    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->autosound)
            continue;
        if (entnum && ch->entnum != entnum)
            continue;
        if (ch->sfx != sfx)
            continue;
        return ch;
    }

    return NULL;
}

static void AL_AddLoopSounds(void)
{
    int         i;
    int         sounds[MAX_EDICTS];
    channel_t   *ch, *ch2;
    sfx_t       *sfx;
    sfxcache_t  *sc;
    int         num;
    entity_state_t  *ent;
    vec3_t      origin;
    float       dist;

    if (cls.state != ca_active || sv_paused->integer || !s_ambient->integer)
        return;

    S_BuildSoundList(sounds);

    for (i = 0; i < cl.frame.numEntities; i++) {
        if (!sounds[i])
            continue;

        sfx = S_SfxForHandle(cl.sound_precache[sounds[i]]);
        if (!sfx)
            continue;       // bad sound effect
        sc = sfx->cache;
        if (!sc)
            continue;

        num = (cl.frame.firstEntity + i) & PARSE_ENTITIES_MASK;
        ent = &cl.entityStates[num];

        ch = AL_FindLoopingSound(ent->number, sfx);
        if (ch) {
            ch->autoframe = s_framecount;
            ch->end = s_paintedtime + sc->length;
            continue;
        }

        // check attenuation before playing the sound
        CL_GetEntitySoundOrigin(ent->number, origin);
        VectorSubtract(origin, cl.listener_spatialize.origin, origin);
        dist = VectorNormalize(origin);
        dist = (dist - SOUND_FULLVOLUME) * SOUND_LOOPATTENUATE;
        if(dist >= 1.f)
            continue; // completely attenuated
        
        // allocate a channel
        ch = S_PickChannel(0, 0);
        if (!ch)
            continue;

        // attempt to synchronize with existing sounds of the same type
        ch2 = AL_FindLoopingSound(0, sfx);
        if (ch2) {
            ALfloat offset = 0;
            qalGetSourcef(ch2->srcnum, AL_SAMPLE_OFFSET, &offset);
            qalSourcef(s_srcnums[ch - s_channels], AL_SAMPLE_OFFSET, offset);
        }

        ch->autosound = true;   // remove next frame
        ch->autoframe = s_framecount;
        ch->sfx = sfx;
        ch->entnum = ent->number;
        ch->master_vol = 1.0f;
        ch->dist_mult = SOUND_LOOPATTENUATE;
        ch->end = s_paintedtime + sc->length;

        AL_PlayChannel(ch);
    }
}

static void AL_StreamUpdate(void)
{
    ALint num_buffers = 0;
    qalGetSourcei(s_stream, AL_BUFFERS_PROCESSED, &num_buffers);
    while (num_buffers-- > 0) {
        ALuint buffer = 0;
        qalSourceUnqueueBuffers(s_stream, 1, &buffer);
        qalDeleteBuffers(1, &buffer);
        s_stream_buffers--;
    }
}

static void AL_StreamStop(void)
{
    qalSourceStop(s_stream);
    AL_StreamUpdate();

    if (s_stream_buffers)
        Com_Error(ERR_FATAL, "Unbalanced number of AL buffers");
}

static bool AL_RawSamples(int samples, int rate, int width, int channels, const byte *data, float volume)
{
    ALenum format = AL_FORMAT_MONO8 + (channels - 1) * 2 + (width - 1);
    ALuint buffer = 0;

    if (s_stream_buffers < 16) {
        qalGetError();
        qalGenBuffers(1, &buffer);
        if (qalGetError())
            return false;

        qalBufferData(buffer, format, data, samples * width * channels, rate);
        if (qalGetError()) {
            qalDeleteBuffers(1, &buffer);
            return false;
        }

        qalSourceQueueBuffers(s_stream, 1, &buffer);
        if (qalGetError()) {
            qalDeleteBuffers(1, &buffer);
            return false;
        }
        s_stream_buffers++;
    }

    qalSourcef(s_stream, AL_GAIN, volume);

    ALint state = AL_PLAYING;
    qalGetSourcei(s_stream, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING)
        qalSourcePlay(s_stream);
    return true;
}

static bool AL_NeedRawSamples(void)
{
    return s_stream_buffers < 16;
}

static void AL_Update(void)
{
    int         i;
    channel_t   *ch;
    ALfloat     orientation[6];

    if (!s_active)
        return;

    s_paintedtime = cl.time;

    // set listener parameters
    qalListener3f( AL_POSITION, AL_UnpackVector( cl.listener_spatialize.origin ) );
    AL_CopyVector( cl.listener_spatialize.v_forward, orientation );
    AL_CopyVector( cl.listener_spatialize.v_up, orientation + 3 );
    qalListenerfv( AL_ORIENTATION, orientation );
    qalListenerf( AL_GAIN, S_GetLinearVolume( s_volume->value ) );
    qalDistanceModel( AL_LINEAR_DISTANCE_CLAMPED );

    // update spatialization for dynamic sounds
    for (i = 0, ch = s_channels; i < s_numchannels; i++, ch++) {
        if (!ch->sfx)
            continue;

        if (ch->autosound) {
            // autosounds are regenerated fresh each frame
            if (ch->autoframe != s_framecount) {
                AL_StopChannel(ch);
                continue;
            }
        } else {
            ALenum state = AL_STOPPED;
            qalGetSourcei(ch->srcnum, AL_SOURCE_STATE, &state);
            if (state == AL_STOPPED) {
                AL_StopChannel(ch);
                continue;
            }
        }

#if USE_DEBUG
        if (s_show->integer) {
            ALfloat offset = 0;
            qalGetSourcef(ch->srcnum, AL_SAMPLE_OFFSET, &offset);
            Com_Printf("%d %.1f %.1f %s\n", i, ch->master_vol, offset, ch->sfx->name);
        }
#endif

        AL_Spatialize(ch);         // respatialize channel
    }

    s_framecount++;

    // add loopsounds
    AL_AddLoopSounds();

	AL_StreamUpdate();
    AL_IssuePlaysounds();

    AL_StreamUpdate();
}

const sndapi_t snd_openal = {
    .init = AL_Init,
    .shutdown = AL_Shutdown,
    .update = AL_Update,
    .activate = S_StopAllSounds,
    .sound_info = AL_SoundInfo,
    .upload_sfx = AL_UploadSfx,
    .delete_sfx = AL_DeleteSfx,
    .upload_reverb_effect = AL_UploadReverbEffect,
    .delete_reverb_effect = AL_DeleteReverbEffect,
    .set_active_reverb_effect = AL_SetReverbEffect,
    .raw_samples = AL_RawSamples,
    .need_raw_samples = AL_NeedRawSamples,
    .drop_raw_samples = AL_StreamStop,
    .get_begin_ofs = AL_GetBeginofs,
    .play_channel = AL_PlayChannel,
    .stop_channel = AL_StopChannel,
    .stop_all_sounds = AL_StopAllSounds,
};
