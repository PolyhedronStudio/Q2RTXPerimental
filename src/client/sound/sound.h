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

typedef struct {
    float   left;
    float   right;
} samplepair_t;

typedef struct sfxcache_s {
    int         length;
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

typedef struct sfx_s {
    char        name[MAX_QPATH];
    int         registration_sequence;
    sfxcache_t  *cache;
    char        *truename;
    int         error;
} sfx_t;

#define PS_FIRST(list)      LIST_FIRST(playsound_t, list, entry)
#define PS_TERM(ps, list)   LIST_TERM(ps, list, entry)

// a playsound_t will be generated by each call to S_StartSound,
// when the mixer reaches playsound->begin, the playsound will
// be assigned to a channel
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

typedef struct channel_s {
    sfx_t       *sfx;           // sfx number
    float       leftvol;        // 0.0-1.0 volume
    float       rightvol;       // 0.0-1.0 volume
    int         end;            // end time in global paintsamples
    int         pos;            // sample position in sfx
    int         entnum;         // to allow overriding a specific sound
    int         entchannel;     //
    vec3_t      origin;         // only use if fixed_origin is set
    vec_t       dist_mult;      // distance multiplier (attenuation/clipK)
    float       master_vol;     // 0.0-1.0 master volume
    bool        fixed_origin;   // use origin instead of fetching entnum's origin
    bool        autosound;      // from an entity->sound, cleared each frame
#if USE_OPENAL
    unsigned    autoframe;
    unsigned    srcnum;
#endif
} channel_t;

typedef struct {
    char        *name;
    int         format;
    int         channels;
    int         rate;
    int         width;
    int         loopstart;
    int         samples;
    byte        *data;
} wavinfo_t;

/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

typedef struct {
    bool (*init)(void);
    void (*shutdown)(void);
    void (*update)(void);
    void (*activate)(void);
    void (*sound_info)(void);
    sfxcache_t *(*upload_sfx)(sfx_t *s);
    void (*delete_sfx)(sfx_t *s);
    void (*page_in_sfx)(sfx_t *s);
    bool (*raw_samples)(int samples, int rate, int width, int channels, const byte *data, float volume);
    bool (*need_raw_samples)(void);
    void (*drop_raw_samples)(void);
    int (*get_begin_ofs)(float timeofs);
    void (*play_channel)(channel_t *ch);
    void (*stop_channel)(channel_t *ch);
    void (*stop_all_sounds)(void);
} sndapi_t;

#if USE_SNDDMA
extern const sndapi_t   snd_dma;
#endif

#if USE_OPENAL
/* number of buffers in flight (needed for ogg) */
extern int active_buffers;
extern const sndapi_t   snd_openal;
#endif

//====================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME        80

#define SOUND_LOOPATTENUATE     0.003f

extern sndstarted_t s_started;
extern bool         s_active;
extern sndapi_t     s_api;

#define MAX_CHANNELS            32
extern channel_t    s_channels[MAX_CHANNELS];
extern int          s_numchannels;

extern int          s_paintedtime;
extern list_t       s_pendingplays;

extern wavinfo_t    s_info;

extern cvar_t       *s_volume;
extern cvar_t       *s_ambient;
#if USE_DEBUG
extern cvar_t       *s_show;
#endif
extern cvar_t       *s_underwater;
extern cvar_t       *s_underwater_gain_hf;

// clip integer to [-0x8000, 0x7FFF] range (stolen from FFmpeg)
static inline int clip16(int v)
{
    return ((v + 0x8000U) & ~0xFFFF) ? (v >> 31) ^ 0x7FFF : v;
}

static inline const bool S_IsFullVolume( channel_t *ch ) { //
    return ( ( ch )->entnum == -1 || ( ch )->entnum == listener_entnum || ( ch )->dist_mult == 0 );
}

static inline const bool S_IsUnderWater() {
    return ( cls.state == ca_active && ( cl.frame.ps.rdflags | cl.predictedState.view.rdflags ) & RDF_UNDERWATER && s_underwater->integer );
}

#define S_Malloc(x)     Z_TagMalloc(x, TAG_SOUND)
#define S_CopyString(x) Z_TagCopyString(x, TAG_SOUND)

sfx_t *S_SfxForHandle(qhandle_t hSfx);
sfxcache_t *S_LoadSound(sfx_t *s);
channel_t *S_PickChannel(int entnum, int entchannel);
void S_IssuePlaysound(playsound_t *ps);
void S_BuildSoundList(int *sounds);
void S_SetupSpatialListener( const vec3_t viewOrigin, const vec3_t vForward, const vec3_t vRight, const vec3_t vUp );

bool OGG_Load(sizebuf_t *sz);
