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

// sound.h -- private sound functions

#include "../cl_client.h"
#include "shared/util_list.h"

#if USE_SNDDMA
#include "client/sound/dma.h"
#endif

// =======================================================================
// EAX Reverb Effects( OpenAL only ), stub functions for DMA.
// =======================================================================

/**
*   @brief
**/
typedef struct {
    float   left;
    float   right;
} samplepair_t;

/**
*   @brief
**/
typedef struct sfxcache_s {
    int64_t     length;
    int         loopstart;
    int         width;
    int         channels;
    int         size;
#if USE_OPENAL
    unsigned    bufnum;
#endif
#if USE_SNDDMA
    byte        data[1];        // variable sized
#endif
} sfxcache_t;

/**
*   @brief
**/
typedef struct sfx_s {
    char        name[MAX_QPATH];
    int         registration_sequence;
    sfxcache_t  *cache;
    char        *truename;
    int         error;
} sfx_t;

#define PS_FIRST(list)      LIST_FIRST(playsound_t, list, entry)
#define PS_TERM(ps, list)   LIST_TERM(ps, list, entry)

/**
*   @brief  A 'playsound_t' will be generated by each call to S_StartSound,
*           when the mixer reaches playsound->begin, the playsound will
*           be assigned to a channel
**/
typedef struct playsound_s {
    list_t      entry;
    sfx_t       *sfx;
    float       volume;
    float       attenuation;
    int         entnum;
    int         entchannel;
    bool        fixed_origin;   // use origin field instead of entnum's origin
    vec3_t      origin;
    int         begin;          // begin on this sample
} playsound_t;

/**
*   @brief  Sound Channel.
**/
typedef struct channel_s {
    sfx_t       *sfx;           // sfx number
    float       leftvol;        // 0.0-1.0 volume
    float       rightvol;       // 0.0-1.0 volume
    int64_t     end;            // end time in global paintsamples
    int32_t     pos;            // sample position in sfx
    int32_t     entnum;         // to allow overriding a specific sound
    int32_t     entchannel;     //
    vec3_t      origin;         // only use if fixed_origin is set
    vec_t       dist_mult;      // distance multiplier (attenuation/clipK)
    float       master_vol;     // 0.0-1.0 master volume
    bool        fixed_origin;   // use origin instead of fetching entnum's origin
    bool        autosound;      // from an entity->sound, cleared each frame
#if USE_OPENAL
    uint64_t    autoframe;
    uint32_t    srcnum;
#endif
} channel_t;

/**
*   @brief  Wav file info.
**/
typedef struct {
    char        *name;
    int32_t     format;
    int32_t     channels;
    int32_t     rate;
    int32_t     width;
    int32_t     loopstart;
    int32_t     samples;
    byte        *data;
} wavinfo_t;

/**
* 
* 
* System Specific Functions:
* 
* 
**/
/**
*   @brief  The bridge between the sound API in use, utilized by the general sound code.
**/
typedef struct {
    bool (*init)(void);
    void (*shutdown)(void);
    void (*update)(void);
    void (*activate)(void);
    void (*sound_info)(void);
    sfxcache_t *(*upload_sfx)(sfx_t *s);
    void (*delete_sfx)(sfx_t *s);
    void (*page_in_sfx)(sfx_t *s);
    const qboolean ( *set_eax_effect_properties )( const sfx_eax_properties_t *eax_properties );
    bool (*raw_samples)(int samples, int rate, int width, int channels, const byte *data, float volume);
    bool (*need_raw_samples)(void);
    void (*drop_raw_samples)(void);
    int (*get_begin_ofs)(float timeofs);
    void (*play_channel)(channel_t *ch);
    void (*stop_channel)(channel_t *ch);
    void (*stop_all_sounds)(void);
} sndapi_t;

//! API for 'DMA' sound.
#if USE_SNDDMA
extern const sndapi_t   snd_dma;
#endif

//! API for 'OpenAL' sound.
#if USE_OPENAL
/* number of buffers in flight (needed for ogg) */
extern int active_buffers;
extern const sndapi_t   snd_openal;
#endif

//====================================================================

/**
*   Sound Configuration:
**/
// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME        80
#define SOUND_LOOPATTENUATE     0.003f

/**
*   Sound State:
**/
extern sndstarted_t s_started;
extern bool         s_active;
extern sndapi_t     s_api;

/**
*   Sound Channels:
**/
#define MAX_CHANNELS            32
extern channel_t    s_channels[MAX_CHANNELS];
extern int          s_numchannels;

/**
*   Sound Painting:
**/
extern int64_t      s_paintedtime;
extern list_t       s_pendingplays;

extern wavinfo_t    s_info;

/**
*   Sound CVars:
**/
extern cvar_t       *s_volume;
extern cvar_t       *s_ambient;
#if USE_DEBUG
extern cvar_t       *s_show;
#endif
extern cvar_t       *s_underwater;
extern cvar_t       *s_underwater_gain_hf;

/**
*   Sound Utility Functions:
**/
// clip integer to [-0x8000, 0x7FFF] range (stolen from FFmpeg)
static inline int clip16(int v)
{
    return ((v + 0x8000U) & ~0xFFFF) ? (v >> 31) ^ 0x7FFF : v;
}

static inline const bool S_IsFullVolume( channel_t *ch ) { //
    return ( ( ch )->entnum == -1 || ( ch )->entnum == cl.listener_spatialize.entnum || ( ch )->dist_mult == 0 );
}

static inline const bool S_IsUnderWater() {
    return ( ( cls.state == ca_active && ( cl.frame.ps.rdflags | cl.predictedState.playerState.rdflags ) & RDF_UNDERWATER ) && s_underwater->integer ) ? true : false;
}

#define S_Malloc(x)     Z_TagMalloc(x, TAG_SOUND)
#define S_CopyString(x) Z_TagCopyString(x, TAG_SOUND)

/**
*   Client Sound API:
**/
sfx_t *S_SfxForHandle(qhandle_t hSfx);
sfxcache_t *S_LoadSound(sfx_t *s);
channel_t *S_PickChannel(int entnum, int entchannel);
void S_IssuePlaysound(playsound_t *ps);
void S_BuildSoundList(int *sounds);
void S_SetupSpatialListener( const vec3_t viewOrigin, const vec3_t vForward, const vec3_t vRight, const vec3_t vUp );

// EAX Reverb Funcs:
const qboolean S_SetEAXEnvironmentProperties( const sfx_eax_properties_t *properties );

bool OGG_Load(sizebuf_t *sz);
