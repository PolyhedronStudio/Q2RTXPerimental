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
// sv_game.c -- interface to the game dll

#include "server/sv_server.h"
#include "server/sv_game.h"
#include "server/sv_models.h"
#include "server/sv_send.h"
#include "server/sv_world.h"

svgame_export_t    *ge;

static void PF_configstring(int index, const char *val);

//! Loaded model handles.
qhandle_t sv_loaded_model_handles[ MAX_MODELS ];

/**
*   @brief  Will return the index if it was already registered, otherwise determine whether to load server sided
*           data for it and pass on the configstring registration change to be broadcasted.
**/
static int PF_FindLoadIndex( const char *name, int start, int max, int skip, const char *func ) {
	char *string;
	int i;

    // Ensure we got a valid name to work with.
    if ( !name || !name[ 0 ] ) {
        return 0;
    }

    // Check if it has been 'indexed' before already.
	for ( i = 1; i < max; i++ ) {
        // Skip:
		if ( i == skip ) {
			continue;
		}
        // Fetch string.
		string = sv.configstrings[ start + i ];
        // Break if nothing.
		if ( !string[ 0 ] ) {
			break;
		}
        // Return index in case of a match(It has been 'indexed' and loaded before.)
		if ( !strcmp( string, name ) ) {
			return i;
		}
	}

    // Error out if we're overloading the configstring cache.
	if ( i == max ) {
		//if ( g_features->integer & GMF_ALLOW_INDEX_OVERFLOW ) {
		//	Com_DPrintf( "%s(%s): overflow\n", func, name );
		//	return 0;
		//}
		Com_Error( ERR_DROP, "%s(%s): overflow", func, name );
	}

    // Determine whether we're dealing with a model:
    if ( start == CS_MODELS ) {
        if ( name ) {
            const size_t strl = strlen( name );
            if ( strl >= 4 && !strcmp( name + ( strl - 4 ), ".iqm" ) ) {
                sv_loaded_model_handles[ i ] = SV_RegisterModel( name );
            }
        }
    }

    // Proceed to apply(and possibly broadcast) the new config string.
	PF_configstring( i + start, name );

    // Return index.
	return i;
}



static int PF_ModelIndex(const char *name)
{
    return PF_FindLoadIndex(name, CS_MODELS, MAX_MODELS, MODELINDEX_PLAYER,  __func__);
}

static int PF_SoundIndex(const char *name)
{
    return PF_FindLoadIndex(name, CS_SOUNDS, MAX_SOUNDS, 0, __func__);
}

static int PF_ImageIndex(const char *name)
{
    return PF_FindLoadIndex(name, CS_IMAGES, MAX_IMAGES, 0, __func__);
}

/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client.
Archived in MVD stream.
===============
*/
static void PF_Unicast(edict_t *ent, bool reliable)
{
	client_t *client;
	int         cmd, flags, clientNum;

	if ( !ent ) {
		goto clear;
	}

	clientNum = NUMBER_OF_EDICT( ent ) - 1;
	if ( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		Com_WPrintf( "%s to a non-client %d\n", __func__, clientNum );
		goto clear;
	}

	client = svs.client_pool + clientNum;
	if ( client->state <= cs_zombie ) {
		Com_WPrintf( "%s to a free/zombie client %d\n", __func__, clientNum );
		goto clear;
	}

	if ( !msg_write.cursize ) {
		Com_DPrintf( "%s with empty data\n", __func__ );
		goto clear;
	}

	cmd = msg_write.data[ 0 ];

	flags = 0;
	if ( reliable ) {
		flags |= MSG_RELIABLE;
	}

	if ( cmd == svc_layout || ( cmd == svc_configstring && RL16( &msg_write.data[ 1 ] ) == CS_STATUSBAR ) ) {
		flags |= MSG_COMPRESS_AUTO;
	}

	SV_ClientAddMessage( client, flags );

	// fix anti-kicking exploit for broken mods
	if ( cmd == svc_disconnect ) {
		client->drop_hack = true;
		goto clear;
	}

clear:
    SZ_Clear(&msg_write);
}

/**
*	@brief	Wrapper around SV_Multicast that always uses false for reliable.
**/
static void PF_Multicast( const vec3_t origin, multicast_t to, bool reliable = false ) {
	return SV_Multicast( origin, to, reliable );
}

/*
=================
PF_bprintf

Sends text to all active clients.
Archived in MVD stream.
=================
*/
static void PF_bprintf(int level, const char *fmt, ...)
{
    va_list     argptr;
    char        string[MAX_STRING_CHARS];
    client_t    *client;
    size_t      len;
    int         i;

    va_start(argptr, fmt);
    len = Q_vsnprintf(string, sizeof(string), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(string)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }

    MSG_WriteUint8(svc_print);
    MSG_WriteUint8(level);
    MSG_WriteData(string, len + 1);
    
    // WID: For Lua and other error type or warning printing...
    #if USE_CLIENT
    if ( !COM_DEDICATED && level >= print_type_t::PRINT_WARNING ) {
        Com_LPrintf( (print_type_t)level, "[SERVER]: %s", string );
        //return;
    } else 
    #endif // } else // if ( COM_DEDICATED ) {
    // echo to console
    if (COM_DEDICATED) {
        // mask off high bits
        for (i = 0; i < len; i++)
            string[i] &= 127;
        Com_Printf("%s", string);
        //Com_LPrintf( (print_type_t)level, "%s", string );
    }

    FOR_EACH_CLIENT(client) {
        if ( client->state != cs_spawned ) {
            continue;
        }
        if (level >= client->messagelevel) {
            SV_ClientAddMessage(client, MSG_RELIABLE);
        }
    }

    SZ_Clear(&msg_write);
}

/*
===============
PF_dprintf

Debug print to server console.
===============
*/
static void PF_dprintf(const char *fmt, ...)
{
    char        msg[MAXPRINTMSG];
    va_list     argptr;

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Com_LPrintf( print_type_t::PRINT_DEVELOPER, "%s", msg );
    //Com_Printf("%s", msg);
}

/*
===============
PF_cprintf

Print to a single client if the level passes.
Archived in MVD stream.
===============
*/
static void PF_cprintf(edict_t *ent, int level, const char *fmt, ...)
{
    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    int         clientNum;
    size_t      len;
    client_t    *client;

    va_start(argptr, fmt);
    len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }

    if (!ent) {
        Com_LPrintf(level == PRINT_CHAT ? PRINT_TALK : PRINT_ALL, "%s", msg);
        return;
    }

    clientNum = NUMBER_OF_EDICT(ent) - 1;
    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP, "%s to a non-client %d", __func__, clientNum);
    }

    client = svs.client_pool + clientNum;
    if (client->state <= cs_zombie) {
        Com_WPrintf("%s to a free/zombie client %d\n", __func__, clientNum);
        return;
    }

    MSG_WriteUint8(svc_print);
    MSG_WriteUint8(level);
    MSG_WriteData(msg, len + 1);

    if (level >= client->messagelevel) {
        SV_ClientAddMessage(client, MSG_RELIABLE);
    }

    SZ_Clear(&msg_write);
}

/*
===============
PF_centerprintf

Centerprint to a single client.
Archived in MVD stream.
===============
*/
static void PF_centerprintf(edict_t *ent, const char *fmt, ...)
{
    char        msg[MAX_STRING_CHARS];
    va_list     argptr;
    int         n;
    size_t      len;

    if (!ent) {
        return;
    }

    n = NUMBER_OF_EDICT(ent);
    if (n < 1 || n > sv_maxclients->integer) {
        Com_WPrintf("%s to a non-client %d\n", __func__, n - 1);
        return;
    }

    va_start(argptr, fmt);
    len = Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (len >= sizeof(msg)) {
        Com_WPrintf("%s: overflow\n", __func__);
        return;
    }

    MSG_WriteUint8(svc_centerprint);
    MSG_WriteData(msg, len + 1);

    PF_Unicast(ent, true);
}

/*
===============
PF_error

Abort the server with a game error
===============
*/
static q_noreturn void PF_error(const char *fmt, ...)
{
    char        msg[MAXERRORMSG];
    va_list     argptr;

    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Com_Error(ERR_DROP, "ServerGame Error: %s", msg);
}

/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
static void PF_setmodel(edict_t *ent, const char *name)
{
    mmodel_t    *mod;

    if (!ent || !name)
        Com_Error(ERR_DROP, "PF_setmodel: NULL");

    ent->s.modelindex = PF_ModelIndex(name);

// if it is an inline model, get the size information for it
    if (name[0] == '*') {
        mod = BSP_InlineModel( sv.cm.cache, name);
        VectorCopy(mod->mins, ent->mins);
        VectorCopy(mod->maxs, ent->maxs);
        PF_LinkEdict(ent);
    }
}

/**
*	@brief	Apply configstring change, and if the game is actively running, broadcasts the configstring change.
**/
static void PF_configstring(int index, const char *val)
{
	size_t len, maxlen;
	client_t *client;
	char *dst;

    if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
        Com_Error( ERR_DROP, "%s: bad index: %d", __func__, index );
    }

    // Commented out so it can be used in the Game's PreInit function.
	//if ( sv.state == ss_dead ) {
	//	Com_WPrintf( "%s: not yet initialized\n", __func__ );
	//	return;
	//}

    if ( !val ) {
        val = "";
    }

	// error out entirely if it exceedes array bounds
	len = strlen( val );
	maxlen = ( MAX_CONFIGSTRINGS - index ) * MAX_CS_STRING_LENGTH;
	if ( len >= maxlen ) {
		Com_Error( ERR_DROP,
				  "%s: index %d overflowed: %zu > %zu",
				  __func__, index, len, maxlen - 1 );
	}

	// print a warning and truncate everything else
	maxlen = CS_SIZE( index );
	if ( len >= maxlen ) {
		Com_WPrintf(
			"%s: index %d overflowed: %zu > %zu\n",
			__func__, index, len, maxlen - 1 );
		len = maxlen - 1;
	}

	dst = sv.configstrings[ index ];
	if ( !strncmp( dst, val, maxlen ) ) {
		return;
	}

	// change the string in sv
	memcpy( dst, val, len );
	dst[ len ] = 0;

	if ( sv.state == ss_loading ) {
		return;
	}

	// send the update to everyone
	MSG_WriteUint8( svc_configstring );
	MSG_WriteInt16( index );
	MSG_WriteData( val, len );
	MSG_WriteUint8( 0 );

	FOR_EACH_CLIENT( client ) {
		if ( client->state < cs_primed ) {
			continue;
		}
		SV_ClientAddMessage( client, MSG_RELIABLE );
	}

	SZ_Clear( &msg_write );
}

/**
*	@brief	Returns the given configstring that sits at index.
**/
static configstring_t *PF_GetConfigString( const int32_t configStringIndex ) {
	if ( configStringIndex < 0 || configStringIndex > MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "%s: bad index: %d", __func__, configStringIndex );
		return nullptr;
	}
	return &sv.configstrings[ configStringIndex ];
}

/**
*   @return True if the points p1 to p2 are within two visible areas of the specified vis type.
*   @note   Also checks portalareas so that doors block sight
**/
static const qboolean PF_inVIS(const vec3_t p1, const vec3_t p2, const int32_t vis)
{
    mleaf_t *leaf1, *leaf2;
    byte mask[VIS_MAX_BYTES];
    bsp_t *bsp = sv.cm.cache;

    if (!bsp) {
        Com_Error(ERR_DROP, "%s: no map loaded", __func__);
    }

    leaf1 = BSP_PointLeaf(bsp->nodes, p1);
    BSP_ClusterVis(bsp, mask, leaf1->cluster, vis);

    leaf2 = BSP_PointLeaf(bsp->nodes, p2);
    if (leaf2->cluster == -1)
        return false;
    if (!Q_IsBitSet(mask, leaf2->cluster))
        return false;
    if (!CM_AreasConnected(&sv.cm, leaf1->area, leaf2->area))
        return false;       // a door blocks it
    return true;
}

/**
*   @brief  Also checks portalareas so that doors block sight
**/
static const qboolean PF_inPVS(const vec3_t p1, const vec3_t p2)
{
    return PF_inVIS(p1, p2, DVIS_PVS);
}

/**
*   @brief  Also checks portalareas so that doors block sound
**/
static const qboolean PF_inPHS(const vec3_t p1, const vec3_t p2)
{
    return PF_inVIS(p1, p2, DVIS_PHS);
}

/**
*   @description    Each entity can have eight independant sound sources, like voice,
*                   weapon, feet, etc.
*
*                   If channel & 8, the sound will be sent to everyone, not just
*                   things in the PHS.
*
*                   FIXME: if entity isn't in PHS, they must be forced to be sent or
*                   have the origin explicitly sent.
*
*                   Channel 0 is an auto-allocate channel, the others override anything
*                   already running on that entity/channel pair.
*
*                   An attenuation of 0 will play full volume everywhere in the level.
*                   Larger attenuations will drop off.  (max 4 attenuation)
*
*                   Timeofs can range from 0.0 to 0.1 to cause sounds to be started
*                   later in the frame than they normally would.
*
*                   If origin is NULL, the origin is determined from the entity origin
*                   or the midpoint of the entity box for bmodels.
**/
static void SV_StartSound(const vec3_t origin, edict_t *edict,
                          int channel, int soundindex, float volume,
                          float attenuation, float timeofs)
{
	int         ent, vol, att, ofs, flags, sendchan;
	vec3_t      origin_v;
	client_t *client;
	byte        mask[ VIS_MAX_BYTES ];
	mleaf_t *leaf1, *leaf2;
	message_packet_t *msg;
	bool        force_pos;

	if ( !edict )
		Com_Error( ERR_DROP, "%s: edict = NULL", __func__ );
	if ( volume < 0 || volume > 1 )
		Com_Error( ERR_DROP, "%s: volume = %f", __func__, volume );
	if ( attenuation < 0 || attenuation > 4 )
		Com_Error( ERR_DROP, "%s: attenuation = %f", __func__, attenuation );
	if ( timeofs < 0 || timeofs > 0.255f )
		Com_Error( ERR_DROP, "%s: timeofs = %f", __func__, timeofs );
	if ( soundindex < 0 || soundindex >= MAX_SOUNDS )
		Com_Error( ERR_DROP, "%s: soundindex = %d", __func__, soundindex );

	vol = volume * 255;
	att = std::min( (int32_t)attenuation * 64, 255 );   // need to clip due to check above
	ofs = timeofs * 1000;

	ent = NUMBER_OF_EDICT( edict );

	sendchan = ( ent << 3 ) | ( channel & 7 );

	// always send the entity number for channel overrides
	flags = SND_ENT;
	if ( vol != 255 )
		flags |= SND_VOLUME;
	if ( att != 64 )
		flags |= SND_ATTENUATION;
	if ( ofs )
		flags |= SND_OFFSET;
	if ( soundindex > 255 )
		flags |= SND_INDEX16;

	// send origin for invisible entities
	// the origin can also be explicitly set
	force_pos = ( edict->svflags & SVF_NOCLIENT ) || origin;

	// use the entity origin unless it is a bmodel or explicitly specified
	if ( !origin ) {
		if ( edict->solid == SOLID_BSP ) {
			VectorAvg( edict->mins, edict->maxs, origin_v );
			VectorAdd( origin_v, edict->s.origin, origin_v );
			origin = origin_v;
		} else {
			origin = edict->s.origin;
		}
	}

	// prepare multicast message
	MSG_WriteUint8( svc_sound );
	MSG_WriteUint8( flags | SND_POS );
	if ( flags & SND_INDEX16 )
		MSG_WriteUint16( soundindex );
	else
		MSG_WriteUint8( soundindex );

	if ( flags & SND_VOLUME )
		MSG_WriteUint8( vol );
	if ( flags & SND_ATTENUATION )
		MSG_WriteUint8( att );
	if ( flags & SND_OFFSET )
		MSG_WriteUint8( ofs );

	MSG_WriteUint16( sendchan );
	MSG_WritePos( origin, MSG_POSITION_ENCODING_NONE );
	
	// if the sound doesn't attenuate, send it to everyone
	// (global radio chatter, voiceovers, etc)
	if ( attenuation == ATTN_NONE )
		channel |= CHAN_NO_PHS_ADD;

	// multicast if force sending origin
	if ( force_pos ) {
		if ( channel & CHAN_NO_PHS_ADD ) {
			SV_Multicast( NULL, MULTICAST_ALL, channel & CHAN_RELIABLE );
		} else {
			SV_Multicast( origin, MULTICAST_PHS, channel & CHAN_RELIABLE );
		}
		return;
	}

	leaf1 = NULL;
	if ( !( channel & CHAN_NO_PHS_ADD ) ) {
		leaf1 = CM_PointLeaf( &sv.cm, origin );
		BSP_ClusterVis( sv.cm.cache, mask, leaf1->cluster, DVIS_PHS );
	}

	// decide per client if origin needs to be sent
	FOR_EACH_CLIENT( client ) {
		// do not send sounds to connecting clients
		if ( !CLIENT_ACTIVE( client ) ) {
			continue;
		}

		// PHS cull this sound
		if ( !( channel & CHAN_NO_PHS_ADD ) ) {
			leaf2 = CM_PointLeaf( &sv.cm, client->edict->s.origin );
			if ( !CM_AreasConnected( &sv.cm, leaf1->area, leaf2->area ) )
				continue;
			if ( leaf2->cluster == -1 )
				continue;
			if ( !Q_IsBitSet( mask, leaf2->cluster ) )
				continue;
		}

		// reliable sounds will always have position explicitly set,
		// as no one guarantees reliables to be delivered in time
		if ( channel & CHAN_RELIABLE ) {
			SV_ClientAddMessage( client, MSG_RELIABLE );
			continue;
		}

		if ( LIST_EMPTY( &client->msg_free_list ) ) {
			Com_WPrintf( "%s: %s: out of message slots\n",
						__func__, client->name );
			continue;
		}

		msg = LIST_FIRST( message_packet_t, &client->msg_free_list, entry );

		msg->cursize = 0;
		msg->flags = flags;
		msg->index = soundindex;
		msg->volume = vol;
		msg->attenuation = att;
		msg->timeofs = ofs;
		msg->sendchan = sendchan;
		VectorCopy( origin, msg->pos );

		List_Remove( &msg->entry );
		List_Append( &client->msg_unreliable_list, &msg->entry );
		client->msg_unreliable_bytes += MAX_SOUND_PACKET;
	}

	// clear multicast buffer
	SZ_Clear( &msg_write );
}
/**
*   @brief
**/
static void PF_StartSound(edict_t *entity, int channel,
                          int soundindex, float volume,
                          float attenuation, float timeofs)
{
    if (!entity)
        return;
    SV_StartSound(NULL, entity, channel, soundindex, volume, attenuation, timeofs);
}

//void PF_Pmove(pmove_t *pm)
//{
//    if (sv_client) {
//        ge->PlayerMove(pm, &sv_client->pmp);
//    } else {
//		ge->PlayerMove(pm, &sv_pmp);
//    }
//}
/**
*   @brief
**/
static cvar_t *PF_cvar(const char *name, const char *value, int flags)
{
    if (flags & CVAR_EXTENDED_MASK) {
        Com_WPrintf("ServerGame attemped to set extended flags on '%s', masked out.\n", name);
        flags &= ~CVAR_EXTENDED_MASK;
    }

    return Cvar_Get(name, value, flags | CVAR_GAME);
}
/**
*   @brief
**/
static void PF_AddCommandString(const char *string)
{
    Cbuf_AddText(&cmd_buffer, string);
}

/**
*   @brief
**/
void SV_SendSetPortalBitMessage( const int32_t portalnum, const bool open ) {
    // Wirte Set Portal Bit command.
    MSG_WriteUint8( svc_set_portalbit );
    // Send a portal number update message
    MSG_WriteInt32( portalnum );
    MSG_WriteUint8( open ? true : false );
    // Multicast to all clients.
    SV_Multicast( NULL, MULTICAST_ALL, CHAN_RELIABLE );
    // Clear multicast buffer
    SZ_Clear( &msg_write );
}
/**
*   @brief
**/
static void PF_SetAreaPortalState( const int32_t portalnum, const bool open) {
    if (!sv.cm.cache) {
        Com_Error(ERR_DROP, "%s: no map loaded", __func__);
    }
    // Set server side, collision model's area portal state.
    CM_SetAreaPortalState(&sv.cm, portalnum, open);
    // Multicast a set portal bit notification to all connected clients.
    SV_SendSetPortalBitMessage( portalnum, open );
}
/**
*   @brief
**/
static const int32_t PF_GetAreaPortalState( const int32_t portalnum ) {
    if ( !sv.cm.cache ) {
        Com_Error( ERR_DROP, "%s: no map loaded", __func__ );
    }
    return CM_GetAreaPortalState( &sv.cm, portalnum );
}
/**
*   @brief   
**/
static const qboolean PF_AreasConnected(const int32_t area1, const int32_t area2)
{
    if (!sv.cm.cache) {
        Com_Error(ERR_DROP, "%s: no map loaded", __func__);
    }
    return CM_AreasConnected(&sv.cm, area1, area2);
}



/**
*
*
*	FileSystem:
*
*
**/
/**
*	@brief	Returns non 0 in case of existance.
**/
static const int32_t PF_FS_FileExistsEx( const char *path, const uint32_t flags ) {
    return FS_FileExistsEx( path, flags | FS_TYPE_ANY | FS_PATH_GAME );
}
/**
*	@brief	Loads file into designated buffer. A nul buffer will return the file length without loading.
*	@return	length < 0 indicates error.
**/
static const int32_t PF_FS_LoadFile( const char *path, void **buffer ) {
    return FS_LoadFileEx( path, buffer, 0 | FS_TYPE_ANY | FS_PATH_GAME, TAG_FILESYSTEM );
}
/**
*	@brief	Frees FS_FILESYSTEM Tag Malloc file buffer.
**/
static void PF_FS_FreeFile( void *buffer ) {
    FS_FreeFile( buffer );
}


/**
*
*
*   Tag Malloc:
* 
* 
**/
static void *PF_TagReMalloc( void *ptr, unsigned newsize ) {
    return Z_Realloc( ptr, newsize );
}
static void *PF_TagMalloc(unsigned size, unsigned tag)
{
    Q_assert(tag <= UINT16_MAX - TAG_MAX);
    return Z_TagMallocz(size, static_cast<memtag_t>( tag + TAG_MAX) );
}

static void PF_FreeTags( unsigned tag)
{
    Q_assert(tag <= UINT16_MAX - TAG_MAX);
    Z_FreeTags(static_cast<memtag_t>( tag + TAG_MAX ) );
}

static void PF_DebugGraph(float value, int color)
{
}

/**
*   @return The realtime of the server since boot time.
**/
const uint64_t PF_GetRealTime( void ) {
    return svs.realtime;
}
/**
*   @return Server Frame Number..
**/
static const int64_t PF_GetServerFrameNumber() {
    return sv.framenum;
}



/**
*
*
*   BSP Inline-Models:
*
*
**/
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
static const mmodel_t *PF_GetInlineModelDataForName( const char *name ) {
    // Ensure cache is valid.
    if ( !sv.cm.cache ) {
        return nullptr;
    }

    // Has to be an inline model name.
    if ( name[ 0 ] == '*' ) {
        // Convert number part into an integer.
        int32_t inlineIndex = atoi( name + 1 );
        // If it is within bounds...
        if ( inlineIndex >= 0 && inlineIndex < sv.cm.cache->nummodels ) {
            return &sv.cm.cache->models[ inlineIndex ];
        }
    }

    // Not found.
    return nullptr;
}
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
static const mmodel_t *PF_GetInlineModelDataForHandle( const qhandle_t handle ) {
    // Ensure cache is valid.
    if ( !sv.cm.cache ) {
        return nullptr;
    }
    if ( handle < sv.cm.cache->nummodels ) {
        return &sv.cm.cache->models[ handle ];
    } else {
        // TODO: Warn?
        return nullptr;
    }
}



/**
*
*
*   (Skeletal though-) Alias Models:
*
*
**/
/**
*   @brief  Pointer to model data matching the name, otherwise a (nullptr) on failure.
**/
static const model_t *PF_GetModelDataForName( const char *name ) {
    return SV_Models_Find( name );
}
/**
*   @return Pointer to model data matching the resource handle, otherwise a (nullptr) on failure.
**/
static const model_t *PF_GetModelDataForHandle( const qhandle_t handle ) {
    return SV_Models_ForHandle( sv_loaded_model_handles[ handle ] );
}



/**
*
* 
* 
*   Progs Loading:
* 
* 
* 
**/
//! Pointer to loaded .dll/.so file.
static void *game_library;

// WID: C++20: Typedef this for casting
typedef svgame_export_t*(GameEntryFunctionPointer(svgame_import_t*));

/**
*   @brief  Called when either the entire server is being killed, or
*           it is changing to a different game directory.
**/
void SV_ShutdownGameProgs(void)
{
    if (ge) {
        ge->Shutdown();
        ge = NULL;
    }
    if (game_library) {
        Sys_FreeLibrary(game_library);
        game_library = NULL;
    }
    Cvar_Set("g_features", "0");

    //Z_LeakTest(TAG_FREE);
}
/**
*   @brief
**/
static GameEntryFunctionPointer *_SV_LoadGameLibrary(const char *path)
{
    void *entry;

    entry = Sys_LoadLibrary(path, "GetGameAPI", &game_library);
    if (!entry)
        Com_EPrintf("Failed to load ServerGame library: %s\n", Com_GetLastError());
    else
        Com_Printf("Loaded ServerGame library from %s\n", path);

    return ( GameEntryFunctionPointer* )( entry ); // WID: GCC Linux hates static cast here.
}
/**
*   @brief  
**/
static GameEntryFunctionPointer *SV_LoadGameLibrary(const char *game, const char *prefix)
{
    char path[MAX_OSPATH];

    // The actual module name to load.
    #if _DEBUG 
    if (Q_concat(path, sizeof(path), sys_libdir->string,
                 PATH_SEP_STRING, game, PATH_SEP_STRING,
                 prefix, "svgame" CPUSTRING "_d" LIBSUFFIX) >= sizeof(path) ) {
        Com_EPrintf("ServerGame library path length exceeded\n");
        return NULL;
    }
    #else
    if ( Q_concat( path, sizeof( path ), sys_libdir->string,
        PATH_SEP_STRING, game, PATH_SEP_STRING,
        prefix, "svgame" CPUSTRING LIBSUFFIX) >= sizeof( path )) {
        Com_EPrintf( "ServerGame library path length exceeded\n" );
        return NULL;
    }
    #endif

    if (os_access(path, F_OK)) {
        if (!*prefix)
            Com_Printf("Can't access %s: %s\n", path, strerror(errno));
        return NULL;
    }

    return _SV_LoadGameLibrary(path);
}

/**
*   @brief  Init the game subsystem for a new map
**/
void SV_InitGameProgs(void) {
    svgame_import_t   imports;
	GameEntryFunctionPointer *entry = NULL;

    // unload anything we have now
    SV_ShutdownGameProgs();

    // for debugging or `proxy' mods
    if ( sys_forcesvgamelib->string[0] ) {
        entry = _SV_LoadGameLibrary( sys_forcesvgamelib->string ); // WID: C++20: Added cast.
    }
    // try game first ( DEFAULT_GAME unless cvar has been modified. )
    if ( !entry && fs_game->string[0] ) {
		entry = SV_LoadGameLibrary( fs_game->string, "" ); // WID: C++20: Added cast.
    }
    // then try baseq2
    if ( !entry ) {
		entry = SV_LoadGameLibrary( BASEGAME, "" ); // WID: C++20: Added cast.
    }

    // All paths failed.
    if ( !entry ) {
        Com_Error( ERR_DROP, "Failed to load game library" );
        return;
    }

	// Setup import frametime related values so the GameDLL knows about it.
	imports.tick_rate = BASE_FRAMERATE;
	imports.frame_time_s = BASE_FRAMETIME_1000;
	imports.frame_time_ms = BASE_FRAMETIME;

    imports.GetRealTime = PF_GetRealTime;
    imports.GetServerFrameNumber = PF_GetServerFrameNumber;

    // load a new game dll
    imports.multicast = PF_Multicast;
    imports.unicast = PF_Unicast;

    imports.bprintf = PF_bprintf;
    imports.dprintf = PF_dprintf;
    imports.cprintf = PF_cprintf;
    imports.centerprintf = PF_centerprintf;
    imports.error = PF_error;

    imports.CM_EntityKeyValue = CM_EntityKeyValue;
    imports.CM_GetNullEntity = CM_GetNullEntity;

    imports.FS_FileExistsEx = PF_FS_FileExistsEx;
    imports.FS_LoadFile = PF_FS_LoadFile;
    imports.FS_FreeFile = PF_FS_FreeFile;

    imports.BoxEdicts = SV_AreaEdicts;
    imports.trace = SV_Trace;
	imports.clip = SV_Clip;
    imports.pointcontents = SV_PointContents;
    imports.linkentity = PF_LinkEdict;
    imports.unlinkentity = PF_UnlinkEdict;

    imports.SetAreaPortalState = PF_SetAreaPortalState;
    imports.GetAreaPortalState = PF_GetAreaPortalState;
    imports.AreasConnected = PF_AreasConnected;
    imports.inPVS = PF_inPVS;
    imports.inPHS = PF_inPHS;
    imports.setmodel = PF_setmodel;

    imports.GetModelDataForName = PF_GetModelDataForName;
    imports.GetModelDataForHandle = PF_GetModelDataForHandle;

    imports.GetInlineModelDataForHandle = PF_GetInlineModelDataForHandle;
    imports.GetInlineModelDataForName = PF_GetInlineModelDataForName;

    imports.modelindex = PF_ModelIndex;
    imports.soundindex = PF_SoundIndex;
    imports.imageindex = PF_ImageIndex;

	imports.GetConfigString = PF_GetConfigString;
    imports.configstring = PF_configstring;

    imports.sound = PF_StartSound;
    imports.positioned_sound = SV_StartSound;

    imports.WriteInt8 = MSG_WriteInt8;
    imports.WriteUint8 = MSG_WriteUint8;
    imports.WriteInt16 = MSG_WriteInt16;
	imports.WriteUint16 = MSG_WriteUint16;
    imports.WriteInt32 = MSG_WriteInt32;
	imports.WriteInt64 = MSG_WriteInt64;
	imports.WriteUintBase128 = MSG_WriteUintBase128;
	imports.WriteIntBase128 = MSG_WriteIntBase128;
    imports.WriteFloat = MSG_WriteFloat;
    imports.WriteString = MSG_WriteString;
    imports.WritePosition = MSG_WritePos;
    imports.WriteDir8 = MSG_WriteDir8;
    imports.WriteAngle8 = MSG_WriteAngle8;
	imports.WriteAngle16 = MSG_WriteAngle16;

    imports.TagReMalloc = PF_TagReMalloc;
    imports.TagMalloc = PF_TagMalloc;
    imports.TagFree = Z_Free;
    imports.FreeTags = PF_FreeTags;

    imports.cvar = PF_cvar;
    imports.cvar_set = Cvar_UserSet;
    imports.cvar_forceset = Cvar_Set;

    imports.argc = Cmd_Argc;
    imports.argv = Cmd_Argv;
    // original Cmd_Args() did actually return raw arguments
    imports.args = Cmd_RawArgs;
    imports.AddCommandString = PF_AddCommandString;

    /**
    *
    *   Other:
    *
    **/
    imports.Q_ErrorNumber = Q_ErrorNumber;
    imports.Q_ErrorString = Q_ErrorString;
    imports.DebugGraph = PF_DebugGraph;

    // Load up module, error out on failure.
    ge = entry(&imports);
    if (!ge) {
        Com_Error(ERR_DROP, "ServerGame library returned NULL exports");
    }

    // Error out on version mismatch.
    if (ge && ge->apiversion != SVGAME_API_VERSION) {
        Com_Error( ERR_DROP, "ServerGame library is version %d, expected %d",
                  ge->apiversion, SVGAME_API_VERSION );
    }
}

