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
// cl_parse.c  -- parse a message received from the server

#include "cl_client.h"
#include "client/ui/ui.h"


/*
=====================================================================

  DELTA FRAME PARSING

=====================================================================
*/

static inline void CL_ParseDeltaEntity(server_frame_t  *frame,
                                       int32_t         newnum,
                                       entity_state_t  *old,
                                       uint64_t        bits)
{
    entity_state_t    *state;

    // suck up to MAX_EDICTS for servers that don't cap at MAX_PACKET_ENTITIES
    if (frame->numEntities >= MAX_EDICTS) {
        Com_Error(ERR_DROP, "%s: MAX_EDICTS exceeded", __func__);
    }

    state = &cl.entityStates[cl.numEntityStates & PARSE_ENTITIES_MASK];
    cl.numEntityStates++;
    frame->numEntities++;

#if USE_DEBUG
    if (cl_shownet->integer > 2 && bits) {
        MSG_ShowDeltaEntityBits(bits);
        Com_LPrintf(PRINT_DEVELOPER, "\n");
    }
#endif

    MSG_ParseDeltaEntity(old, state, newnum, bits, cl.esFlags);

    // shuffle previous origin to old
    if ( !( bits & U_OLDORIGIN ) && ( !( state->renderfx & RF_BEAM ) && state->entityType != ET_BEAM ) ) {
        VectorCopy( old->origin, state->old_origin );
    }
}

static void CL_ParsePacketEntities(server_frame_t *oldframe,
                                   server_frame_t *frame)
{
    int32_t newnum = 0;
    uint64_t bits = 0;
    entity_state_t *oldstate = nullptr;
    int oldindex, oldnum;
    int i;

    frame->firstEntity = cl.numEntityStates;
    frame->numEntities = 0;

    // delta from the entities present in oldframe
    oldindex = 0;
    oldstate = NULL;
    if (!oldframe) {
        oldnum = 99999;
    } else {
        if (oldindex >= oldframe->numEntities) {
            oldnum = 99999;
        } else {
            i = oldframe->firstEntity + oldindex;
            oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
            oldnum = oldstate->number;
        }
    }

    while (1) {
		bool removeEntity = false;

		// Read out entity number, whether to remove it or not, and its byteMask.
		newnum = MSG_ReadEntityNumber( &removeEntity, &bits );

        if (newnum < 0 || newnum >= MAX_EDICTS) {
            Com_Error(ERR_DROP, "%s: bad number: %d", __func__, newnum);
        }

        if (msg_read.readcount > msg_read.cursize) {
            Com_Error(ERR_DROP, "%s: read past end of message", __func__);
        }

        if (!newnum) {
            break;
        }

        while (oldnum < newnum) {
            // one or more entities from the old packet are unchanged
            SHOWNET(3, "   unchanged: %i\n", oldnum);
            CL_ParseDeltaEntity(frame, oldnum, oldstate, 0);

            oldindex++;

            if (oldindex >= oldframe->numEntities) {
                oldnum = 99999;
            } else {
                i = oldframe->firstEntity + oldindex;
                oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
                oldnum = oldstate->number;
            }
        }

        if ( removeEntity ) {
            // the entity present in oldframe is not in the current frame
            SHOWNET(2, "   remove: %i\n", newnum);
            if (oldnum != newnum) {
                Com_DPrintf("RemoveEntity Bit Set: oldnum != newnum\n");
            }
            if (!oldframe) {
                Com_Error(ERR_DROP, "%s: RemoveEntity Bit Set with NULL oldframe", __func__);
            }

            oldindex++;

            if (oldindex >= oldframe->numEntities) {
                oldnum = 99999;
            } else {
                i = oldframe->firstEntity + oldindex;
                oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
                oldnum = oldstate->number;
            }
            continue;
        }

        if (oldnum == newnum) {
            // delta from previous state
            SHOWNET(2, "   delta: %i ", newnum);
            CL_ParseDeltaEntity(frame, newnum, oldstate, bits);
            if (!bits) {
                SHOWNET(2, "\n");
            }

            oldindex++;

            if (oldindex >= oldframe->numEntities) {
                oldnum = 99999;
            } else {
                i = oldframe->firstEntity + oldindex;
                oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
                oldnum = oldstate->number;
            }
            continue;
        }

        if (oldnum > newnum) {
            // delta from baseline
            SHOWNET(2, "   baseline: %i ", newnum);
            CL_ParseDeltaEntity(frame, newnum, &cl.baselines[newnum], bits);
            if (!bits) {
                SHOWNET(2, "\n");
            }
            continue;
        }

    }

    // any remaining entities in the old frame are copied over
    while (oldnum != 99999) {
        // one or more entities from the old packet are unchanged
        SHOWNET(3, "   unchanged: %i\n", oldnum);
        CL_ParseDeltaEntity(frame, oldnum, oldstate, 0);

        oldindex++;

        if (oldindex >= oldframe->numEntities) {
            oldnum = 99999;
        } else {
            i = oldframe->firstEntity + oldindex;
            oldstate = &cl.entityStates[i & PARSE_ENTITIES_MASK];
            oldnum = oldstate->number;
        }
    }
}

static void CL_ParseFrame()
{
    uint64_t bits = 0;
    // Current frame being parsed.
    server_frame_t  frame = { };
    // Previous old frame.
    server_frame_t *oldframe = nullptr;
    // Player state to interpolate from.
    player_state_t *from;

    // Reset current frame flags.
    cl.frameflags = FF_NONE;

    // Current frame.
    int64_t currentframe = frame.number = MSG_ReadIntBase128( );// WID: 64-bit-frame MSG_ReadInt32();
    // Frame to delta from.
    int64_t deltaframe = frame.delta = MSG_ReadIntBase128( );// WID: 64-bit-frame MSG_ReadInt32(); 

    // WID: net-protocol2: Removed the 'BIG HACK' to let old demos continue to work.
    //if (cls.serverProtocol != PROTOCOL_VERSION_OLD) {
    // Read frame supressed count.
    int64_t suppressed = MSG_ReadUint8();
    if (suppressed) {
        cl.frameflags |= FF_SUPPRESSED;
    }
    //}
    // Determine if the frame was dropped.
    if (cls.netchan.dropped) {
        cl.frameflags |= FF_SERVERDROP;
    }

    // If the frame is delta compressed from data that we no longer have
    // available, we must suck up the rest of the frame, but not use it, then
    // ask for a non-compressed message.
    //
    // Delta Compressed Frame:
    if ( deltaframe > 0 ) {
        oldframe = &cl.frames[ deltaframe & UPDATE_MASK ];
        from = &oldframe->ps;
        // Old servers may cause this on map change:
        if ( deltaframe == currentframe ) {
            Com_DPrintf( "%s: delta from current frame\n", __func__ );
            cl.frameflags |= FF_BADFRAME;
            // Should never happen.
        } else if ( !oldframe->valid ) {
            Com_DPrintf( "%s: delta from invalid frame\n", __func__ );
            cl.frameflags |= FF_BADFRAME;
        // The frame that the server did the delta from is too old, so we can't reconstruct it properly.
        } else if ( oldframe->number != deltaframe ) {
            Com_DPrintf( "%s: delta frame was never received or too old\n", __func__ );
            cl.frameflags |= FF_OLDFRAME;
        // The entities are outdated:
        } else if ( ( cl.numEntityStates - oldframe->firstEntity ) > ( MAX_PARSE_ENTITIES - MAX_PACKET_ENTITIES ) ) {
            Com_DPrintf( "%s: delta entities too old\n", __func__ );
            cl.frameflags |= FF_OLDENT;
        // A valid delta frame has been parsed.
        } else {
            frame.valid = true;
        }

        // For demo playback, support invalid frames either way.
        if ( !frame.valid && cl.frame.valid && cls.demo.playback ) {
            Com_DPrintf( "%s: recovering broken demo\n", __func__ );
            oldframe = &cl.frame;
            from = &oldframe->ps;
            frame.valid = true;
        }
    // Unompressed Frame:
    } else {
        oldframe = nullptr;
        from = nullptr;
        frame.valid = true; // uncompressed frame
        cl.frameflags |= FF_NODELTA;
    }

    // Read areabits
    int32_t length = MSG_ReadUint8();
    if (length) {
        if (length < 0 || msg_read.readcount + length > msg_read.cursize) {
            Com_Error(ERR_DROP, "%s: read past end of message", __func__);
        }
        if (length > sizeof(frame.areabits)) {
            Com_Error(ERR_DROP, "%s: invalid areabits length", __func__);
        }
        memcpy(frame.areabits, msg_read.data + msg_read.readcount, length);
        msg_read.readcount += length;
        frame.areabytes = length;
    } else {
        frame.areabytes = 0;
    }

    // Expect svc_portalbits or bail out.
    if ( ( cl.frameflags & FF_NODELTA ) ) {
        if ( MSG_ReadUint8() != svc_portalbits ) {
            Com_Error( ERR_DROP, "%s: not svc_portalbits", __func__ );
        }

        // Acquire length of portal bits.
        uint8_t lengthPortalBits = MSG_ReadUint8();
        // Ensure it is sane, otherwise bail out.
        if ( lengthPortalBits > MAX_MAP_PORTAL_BYTES ) {
            Com_Error( ERR_DROP, "%s: svc_frame -> svc_portalbits: Portalbits number too high(%d), must be below(%d)\n",
                __func__, lengthPortalBits, MAX_MAP_PORTAL_BYTES );
            return;
        }
        // Clean zerod out portal bits buffer.
        byte portalBits[ MAX_MAP_PORTAL_BYTES ];
        memset( portalBits, 0, MAX_MAP_PORTAL_BYTES );
        // Pointer to the read buffer's portal bit part.
        byte *receivedPortalBits = MSG_ReadData( lengthPortalBits );
        // Copy over the received portal bits into the zeroed out portal bits buffer.
        //memcpy( portalBits, receivedPortalBits, lengthPortalBits );
        for ( int32_t i = 0; i < lengthPortalBits; i++ ) {
            portalBits[ i ] = receivedPortalBits[ i ];
        }
        // Set the entire of the portal states with the newly received portal state bit data.
        CM_SetPortalStates( &cl.collisionModel, portalBits, lengthPortalBits );
        CM_FloodAreaConnections( &cl.collisionModel );
    }

    //if (cls.serverProtocol <= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
        if (MSG_ReadUint8() != svc_playerinfo) {
            Com_Error(ERR_DROP, "%s: not playerinfo", __func__);
        }
    //}

    SHOWNET(2, "%3zu:playerinfo\n", msg_read.readcount - 1);

    // parse playerstate
    bits = MSG_ReadUintBase128();
	MSG_ParseDeltaPlayerstate(from, &frame.ps, bits);
#if USE_DEBUG
        if (cl_shownet->integer > 2 && bits) {
            MSG_ShowDeltaPlayerstateBits(bits);
            Com_LPrintf(PRINT_DEVELOPER, "\n");
        }
#endif
	frame.clientNum = cl.clientNumber;

    // parse packetentities
    if (cls.serverProtocol <= PROTOCOL_VERSION_Q2RTXPERIMENTAL) {
        if (MSG_ReadUint8() != svc_packetentities) {
            Com_Error(ERR_DROP, "%s: not packetentities", __func__);
        }
    }

    SHOWNET(2, "%3zu:packetentities\n", msg_read.readcount - 1);

    CL_ParsePacketEntities(oldframe, &frame);

    // Save the frame off in the backup array for later delta comparisons
    cl.frames[currentframe & UPDATE_MASK] = frame;

#if USE_DEBUG
    if (cl_shownet->integer > 2) {
        int64_t seq = cls.netchan.incoming_acknowledged & CMD_MASK;
        int64_t rtt = cls.demo.playback ? 0 : cls.realtime - cl.history[seq].timeSent;
        Com_LPrintf(PRINT_DEVELOPER, "%3zu:frame:%d  delta:%d  rtt:%d\n",
                    msg_read.readcount - 1, frame.number, frame.delta, rtt);
    }
#endif

    // Ignore the data if the parsed frame isn't valid.
    if (!frame.valid) {
        cl.frame.valid = false;
        return; // Do not change anything.
    }

    // Fail out early to prevent spurious errors later in case of a bad FOV.
    if (!frame.ps.fov) {
        Com_Error(ERR_DROP, "%s: bad fov", __func__);
    }

    // Do NOT actually start swapping our active client frames if we're not precached yet.
    if ( cls.state < ca_precached ) {
        return;
    }

    // Swap frames.
    cl.oldframe = cl.frame;
    cl.frame = frame;

    // Increase demo frames read.
    cls.demo.frames_read++;

    // Start to perform the delta frame lerping if we're not demo seeking,
    // this will also move our cls.state into ca_active if it is our first valid received frame.
    if ( !cls.demo.seeking ) {
        CL_DeltaFrame();
    }
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

static void CL_ParseConfigstring(int index)
{
    size_t  len, maxlen;
    char    *s;

    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);
    }

    s = cl.configstrings[index];
    maxlen = CS_SIZE(index);
    len = MSG_ReadString(s, maxlen);

    SHOWNET(2, "    %d \"%s\"\n", index, s);

    if (len >= maxlen) {
        Com_WPrintf(
            "%s: index %d overflowed: %zu > %zu\n",
            __func__, index, len, maxlen - 1);
    }

    if (cls.demo.seeking) {
        Q_SetBit(cl.dcs, index);
        return;
    }

    if (cls.demo.recording && cls.demo.paused) {
        Q_SetBit(cl.dcs, index);
    }

    // do something apropriate
    CL_UpdateConfigstring(index);
}

static void CL_ParseBaseline(int index, uint64_t bits)
{
    if (index < 1 || index >= MAX_EDICTS) {
        Com_Error(ERR_DROP, "%s: bad index: %d", __func__, index);
    }
#if USE_DEBUG
    if (cl_shownet->integer > 2) {
        MSG_ShowDeltaEntityBits(bits);
        Com_LPrintf(PRINT_DEVELOPER, "\n");
    }
#endif
    MSG_ParseDeltaEntity(NULL, &cl.baselines[index], index, bits, cl.esFlags);
}

// instead of wasting space for svc_configstring and svc_spawnbaseline
// bytes, entire game state is compressed into a single stream.
static void CL_ParseGamestate( const int32_t cmd ) {
    /**
    *   ConfigStrings:
    **/
    if ( cmd == svc_gamestate || cmd == svc_configstringstream ) {
		while ( msg_read.readcount < msg_read.cursize ) {
			const int32_t index = MSG_ReadInt16( );
			if ( index == MAX_CONFIGSTRINGS ) {
				break;
			}
			CL_ParseConfigstring( index );
		}
	}

    /**
    *   Entity States:
    **/
	if ( cmd == svc_gamestate || cmd == svc_baselinestream ) {
		while ( msg_read.readcount < msg_read.cursize ) {
			bool remove = false; // Unused here.
			uint64_t byteMask = 0;
			const int32_t index = MSG_ReadEntityNumber( &remove, &byteMask );
			if ( !index ) {
				break;
			}

			CL_ParseBaseline( index, byteMask );
		}
	}
}

static void CL_ParseServerData(void)
{
    char    levelname[MAX_QPATH];
    bool    cinematic;

    Cbuf_Execute(&cl_cmdbuf);          // make sure any stuffed commands are done

    // wipe the client_state_t struct
    CL_ClearState();

    // parse protocol version number
    int32_t protocol = MSG_ReadInt32();
    cl.servercount = MSG_ReadInt32();
    int32_t attractloop = MSG_ReadUint8();
	int32_t gamemode = MSG_ReadUint8();

	// TEMP
	std::string strGamemode = clge->GetGamemodeName( gamemode );
	// EOF TEMP

    Com_DPrintf("Serverdata packet received "
                "(protocol=%d, gamemode=%s, servercount=%d, attractloop=%d)\n",
                protocol, strGamemode.c_str( ), cl.servercount, attractloop );

    // check protocol
    if (cls.serverProtocol != protocol) {
		if ( !cls.demo.playback ) {
			Com_Error( ERR_DROP, "Requested protocol version %d, but server returned %d.",
					  cls.serverProtocol, protocol );
		}
		// Ensure it is our protocol.
		if ( protocol != PROTOCOL_VERSION_Q2RTXPERIMENTAL ) {
			Com_Error( ERR_DROP, "Demo uses unsupported protocol version %d.", protocol );
		}
        cls.serverProtocol = protocol;
    }

    // game directory
    if (MSG_ReadString(cl.gamedir, sizeof(cl.gamedir)) >= sizeof(cl.gamedir)) {
        Com_Error(ERR_DROP, "Oversize gamedir string");
    }

    // never allow demos to change gamedir
    // do not change gamedir if connected to local sever either,
    // as it was already done by SV_InitGame, and changing it
    // here will not work since server is now running
    if (!cls.demo.playback && !sv_running->integer) {
        // pretend it has been set by user, so that 'changed' hook
        // gets called and filesystem is restarted
        Cvar_UserSet("game", cl.gamedir);

        // protect it from modifications while we are connected
        fs_game->flags |= CVAR_ROM;
    }

    // parse player entity number
    cl.clientNumber = MSG_ReadInt16();

    // get the full level name
    MSG_ReadString(levelname, sizeof(levelname));

    // setup default pmove parameters
    //clge->ConfigurePlayerMoveParameters( &cl.pmp );

// WID: 40hz - For proper frame lerping for 10hz models.
	cl.sv_frametime = BASE_FRAMETIME;
	cl.sv_frametime_inv = 1.0f / cl.sv_frametime;
	cl.sv_framediv = 1;
// WID: 40hz
// 
    // setup default server state
    cl.serverstate = ss_game;
    cinematic = ( cl.clientNumber == -1 );

    if (cinematic) {
        SCR_PlayCinematic(levelname);
    } else {
        // seperate the printfs so the server message can have a color
        Con_Printf(
            "\n\n"
            "\35\36\36\36\36\36\36\36\36\36\36\36"
            "\36\36\36\36\36\36\36\36\36\36\36\36"
            "\36\36\36\36\36\36\36\36\36\36\36\37"
            "\n\n");

        Com_SetColor(COLOR_ALT);
        Com_Printf("%s\n", levelname);
        Com_SetColor(COLOR_NONE);

        // make sure clientNum is in range
        if (!VALIDATE_CLIENTNUM(cl.clientNumber)) {
            Com_WPrintf("Serverdata has invalid playernum %d\n", cl.clientNumber);
            cl.clientNumber = -1;
        }
    }
}

/**
*   @brief  
**/
static void CL_ParseStartSoundPacket(void)
{
	int flags, channel, entity;

	flags = MSG_ReadUint8( );

	if ( flags & SND_INDEX16 )
		cl.snd.index = MSG_ReadUint16( );
	else
		cl.snd.index = MSG_ReadUint8( );

	if ( cl.snd.index >= MAX_SOUNDS )
		Com_Error( ERR_DROP, "%s: bad index: %d", __func__, cl.snd.index );

	if ( flags & SND_VOLUME )
		cl.snd.volume = MSG_ReadUint8( ) / 255.0f;
	else
		cl.snd.volume = DEFAULT_SOUND_PACKET_VOLUME;

	if ( flags & SND_ATTENUATION )
		cl.snd.attenuation = MSG_ReadUint8( ) / 64.0f;
	else
		cl.snd.attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	if ( flags & SND_OFFSET )
		cl.snd.timeofs = MSG_ReadUint8( ) / 1000.0f;
	else
		cl.snd.timeofs = 0;

	if ( flags & SND_ENT ) {
		// entity relative
		channel = MSG_ReadUint16( );
		entity = channel >> 3;
		if ( entity < 0 || entity >= MAX_EDICTS )
			Com_Error( ERR_DROP, "%s: bad entity: %d", __func__, entity );
		cl.snd.entity = entity;
		cl.snd.channel = channel & 7;
	} else {
		cl.snd.entity = 0;
		cl.snd.channel = 0;
	}

	// positioned in space
	if ( flags & SND_POS )
		MSG_ReadPos( cl.snd.pos, MSG_POSITION_ENCODING_NONE );

	cl.snd.flags = flags;

	SHOWNET( 2, "    %s\n", cl.configstrings[ CS_SOUNDS + cl.snd.index ] );
}

static void CL_ParseReconnect(void)
{
    if (cls.demo.playback) {
        Com_Error(ERR_DISCONNECT, "Server disconnected");
    }

    Com_Printf("Server disconnected, reconnecting\n");

    // close netchan now to prevent `disconnect'
    // message from being sent to server
    Netchan_Close(&cls.netchan);

    CL_Disconnect(ERR_RECONNECT);

    cls.state = ca_challenging;
    cls.connect_time -= CONNECT_FAST;
    cls.connect_count = 0;

    CL_CheckForResend();
}

#if USE_AUTOREPLY
void CL_CheckForVersion(const char *s)
{
    const char *p; // WID: C++20: used to be non const.

    p = strstr(s, ": ");
    if (!p) {
        return;
    }

    if (strncmp(p + 2, "!version", 8)) {
        return;
    }

    if (cl.reply_time && cls.realtime - cl.reply_time < 120000) {
        return;
    }

    cl.reply_time = cls.realtime;
    cl.reply_delta = 1024 + (Q_rand() & 1023);
}
#else
void CL_CheckForVersion( const char *s ) {
    // PlaceHolder.
}
#endif

// attempt to scan out an IP address in dotted-quad notation and
// add it into circular array of recent addresses
void CL_CheckForIP(const char *s)
{
    unsigned b1, b2, b3, b4, port;
    netadr_t *a;
    const char *p; // WID: C++20: Used to be non const.

    while (*s) {
        if (sscanf(s, "%3u.%3u.%3u.%3u", &b1, &b2, &b3, &b4) == 4 &&
            b1 < 256 && b2 < 256 && b3 < 256 && b4 < 256) {
            p = strchr(s, ':');
            if (p) {
                port = strtoul(p + 1, NULL, 10);
                if (port < 1024 || port > 65535) {
                    break; // privileged or invalid port
                }
            } else {
                port = PORT_SERVER;
            }

            a = &cls.recent_addr[cls.recent_head++ & RECENT_MASK];
            a->type = NA_IP;
            a->ip.u8[0] = b1;
            a->ip.u8[1] = b2;
            a->ip.u8[2] = b3;
            a->ip.u8[3] = b4;
            a->port = BigShort(port);
            break;
        }

        s++;
    }
}

static void CL_ParseStuffText(void)
{
    char s[MAX_STRING_CHARS];

    MSG_ReadString(s, sizeof(s));
    SHOWNET(2, "    \"%s\"\n", s);
    Cbuf_AddText(&cl_cmdbuf, s);
}

static void CL_ParseDownload(int cmd)
{
    int size, percent, decompressed_size;
    byte *data;

    if (!cls.download.temp[0]) {
        Com_Error(ERR_DROP, "%s: no download requested", __func__);
    }

    // read the data
    size = MSG_ReadInt16();
    percent = MSG_ReadUint8();
    if (size == -1) {
        CL_HandleDownload(NULL, size, percent, 0);
        return;
    }

    // read optional decompressed packet size
    if (cmd == svc_zdownload) {
#if USE_ZLIB
        //if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2) {
            decompressed_size = MSG_ReadInt16();
        //} else {
        //   decompressed_size = -1;
        //
#else
        Com_Error(ERR_DROP, "Compressed server packet received, "
                  "but no zlib support linked in.");
#endif
    } else {
        decompressed_size = 0;
    }

    if (size < 0) {
        Com_Error(ERR_DROP, "%s: bad size: %d", __func__, size);
    }

    if (msg_read.readcount + size > msg_read.cursize) {
        Com_Error(ERR_DROP, "%s: read past end of message", __func__);
    }

    data = msg_read.data + msg_read.readcount;
    msg_read.readcount += size;

    CL_HandleDownload(data, size, percent, decompressed_size);
}

static void CL_ParseZPacket(void)
{
#if USE_ZLIB
    sizebuf_t   temp;
    byte        buffer[MAX_MSGLEN];
    int         ret, inlen, outlen;

    if (msg_read.data != msg_read_buffer) {
        Com_Error(ERR_DROP, "%s: recursively entered", __func__);
    }

    inlen = MSG_ReadUint16();
    outlen = MSG_ReadUint16();

    if (inlen == -1 || outlen == -1 || msg_read.readcount + inlen > msg_read.cursize) {
        Com_Error(ERR_DROP, "%s: read past end of message", __func__);
    }

    if (outlen > MAX_MSGLEN) {
        Com_Error(ERR_DROP, "%s: invalid output length", __func__);
    }

    inflateReset(&cls.z);

    cls.z.next_in = msg_read.data + msg_read.readcount;
    cls.z.avail_in = (uInt)inlen;
    cls.z.next_out = buffer;
    cls.z.avail_out = (uInt)outlen;
    ret = inflate(&cls.z, Z_FINISH);
    if (ret != Z_STREAM_END) {
        Com_Error(ERR_DROP, "%s: inflate() failed with error %d", __func__, ret);
    }

    msg_read.readcount += inlen;

    temp = msg_read;
    SZ_Init(&msg_read, buffer, outlen);
    msg_read.cursize = outlen;
    msg_read.bitposition = msg_read.cursize << 3;

    CL_ParseServerMessage();

    msg_read = temp;
#else
    Com_Error(ERR_DROP, "Compressed server packet received, "
              "but no zlib support linked in.");
#endif
}

//void CL_Scoreboard_ClearFrame( void );
//void CL_Scoreboard_AddEntry( const int64_t clientNumber, const int64_t clientTime, const int64_t clientScore, const int64_t clientPing );
//void CL_Scoreboard_RebuildFrame( const uint8_t totalClients );
//static int64_t scoreBoardFrame = 0;
//
//static void CL_ParseScoreboard( void ) {
//    // Add it to the client scoreboard list.
//    CL_Scoreboard_ClearFrame();
//
//    // Read in amount of clients to enlist.
//    int32_t numClients = MSG_ReadUint8();
//
//    // Iterate over all clients and add their entry.
//    for ( int32_t i = 0; i < numClients; i++ ) {
//        // Read in data.
//        int32_t clientNumber = MSG_ReadUint8();
//        int64_t clientTime = MSG_ReadIntBase128();
//        int64_t clientScore = MSG_ReadIntBase128();
//        uint16_t clientPing = MSG_ReadUint16();
//
//        // Add in the entry.
//        CL_Scoreboard_AddEntry( clientNumber, clientTime, clientScore, clientPing );
//    }
//
//    // Regenerate scoreboard.
//    CL_Scoreboard_RebuildFrame( numClients );
//
//    Com_LPrintf( PRINT_DEVELOPER, "Parsing scoreboard(clients: %i) and frame(%i)\n",
//        numClients, cl.frame.number );
//}

/**
*   @brief
**/
static void CL_ParseSetAreaPortalBit() {
    // Read in the specific portal number to set the bit of.
    int32_t portalNumber = MSG_ReadInt32();
    // Read its state
    bool stateIsOpen = MSG_ReadUint8();

    // Set the area portal bit.
    CM_SetAreaPortalState( &cl.collisionModel, portalNumber, stateIsOpen );

    // Flood update area connections.
    CM_FloodAreaConnections( &cl.collisionModel );

    // Developer print.
    Com_LPrintf( PRINT_DEVELOPER, "%s: svc_set_portalbit: portalNumber(#%d), isOpen(%s)\n",
        __func__, portalNumber, stateIsOpen ? "true" : "false" );
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage(void)
{
    int         cmd;
    size_t      readcount;
    int         index;

#if USE_DEBUG
    if (cl_shownet->integer == 1) {
        Com_LPrintf(PRINT_DEVELOPER, "%zu ", msg_read.cursize);
    } else if (cl_shownet->integer > 1) {
        Com_LPrintf(PRINT_DEVELOPER, "------------------\n");
    }
#endif

//
// parse the message
//
    while (1) {
        if (msg_read.readcount > msg_read.cursize) {
            Com_Error(ERR_DROP, "%s: read past end of server message", __func__);
        }

        readcount = msg_read.readcount;

        if ((cmd = MSG_ReadUint8()) == -1) {
            SHOWNET(1, "%3zu:END OF MESSAGE\n", msg_read.readcount - 1);
            break;
        }

#if USE_DEBUG
        if (cl_shownet->integer > 1) {
            MSG_ShowSVC(cmd);
        }
#endif

        // other commands
        switch (cmd) {
        default:
            // Didn't recognize the command, pass control over the client game to give it a shot
            // at handling the command.
            clge->StartServerMessage( false );
            if ( !clge->ParseServerMessage( cmd ) ) {
                Com_Error( ERR_DROP, "%s: illegible server message: %d", __func__, cmd );
            }
            clge->EndServerMessage( false );
//badbyte:
//            Com_Error(ERR_DROP, "%s: illegible server message: %d", __func__, cmd);
            break;

        case svc_nop:
            break;

        case svc_disconnect:
            Com_Error(ERR_DISCONNECT, "Server disconnected");
            break;

        case svc_reconnect:
            CL_ParseReconnect();
            return;

        // Moved to Client Game.
        //case svc_print:
        //    CL_ParsePrint();
        //    break;
        // Moved to Client Game.
        //case svc_centerprint:
        //    CL_ParseCenterPrint();
        //    break;

        case svc_stufftext:
            CL_ParseStuffText();
            break;

        case svc_serverdata:
            CL_ParseServerData();
            continue;

        case svc_configstring:
            index = MSG_ReadInt16();
            CL_ParseConfigstring(index);
            break;

        case svc_sound:
            CL_ParseStartSoundPacket();
            S_ParseStartSound();
            break;

        case svc_spawnbaseline: {
			uint64_t byteMask = 0;
			bool removeEntity = false;
			index = MSG_ReadEntityNumber( &removeEntity, &byteMask );
			CL_ParseBaseline( index, byteMask );
            break;
		}

        // WID: Moved to client game.
        //case svc_temp_entity:
        //    CL_ParseTEntPacket();
        //    CL_ParseTEnt();
        //    break;
        // WID: Moved to client game.
        //case svc_muzzleflash:
        //    CL_ParseMuzzleFlashPacket(MZ_SILENCED);
        //    CL_MuzzleFlash();
        //    break;
        // WID: Moved to client game.
        //case svc_muzzleflash2:
        //    CL_ParseMuzzleFlashPacket(0);
        //    CL_MuzzleFlash2();
        //    break;

        case svc_download:
            CL_ParseDownload(cmd);
            continue;

        case svc_frame:
            CL_ParseFrame();
            continue;
        case svc_set_portalbit:
            CL_ParseSetAreaPortalBit();
            continue;


        //case svc_scoreboard:
        //    CL_ParseScoreboard();
        //    continue;
        //case svc_inventory:
        //    CL_ParseInventory();
        //    break;

        //case svc_layout:
        //    CL_ParseLayout();
        //    break;

        case svc_zpacket:
            //if (cls.serverProtocol < PROTOCOL_VERSION_R1Q2) {
            //    goto badbyte;
            //}
            CL_ParseZPacket();
            continue;

        case svc_zdownload:
            //if (cls.serverProtocol < PROTOCOL_VERSION_R1Q2) {
            //    goto badbyte;
            //}
            CL_ParseDownload(cmd);
            continue;

        case svc_gamestate:
		case svc_baselinestream:
		case svc_configstringstream:
			//if (cls.serverProtocol != PROTOCOL_VERSION_Q2PRO) {
            //    goto badbyte;
            //}
            CL_ParseGamestate( cmd );
			continue;
        }

        // if recording demos, copy off protocol invariant stuff
        if (cls.demo.recording && !cls.demo.paused) {
            size_t len = msg_read.readcount - readcount;

            // it is very easy to overflow standard 1390 bytes
            // demo frame with modern servers... attempt to preserve
            // reliable messages at least, assuming they come first
            if (cls.demo.buffer.cursize + len < cls.demo.buffer.maxsize) {
                SZ_WriteData(&cls.demo.buffer, msg_read.data + readcount, len);
            } else {
                cls.demo.others_dropped++;
            }
        }
    }
}

/*
=====================
CL_SeekDemoMessage

A variant of ParseServerMessage that skips over non-important action messages,
used for seeking in demos.
=====================
*/
//// WID: So stair smoothing and crouching works.
//static void read_predicted_state() {
//    static cplane_t last_predicted_plane = cl.predictedState.groundPlane;
//
//    // Emit the 'predicted state'.
//    MSG_ReadUint8(); //MSG_ReadUint8( svc_demo_predicted_state );
//    // Error:
//    //MSG_ReadFloat( cl.predictedState.error[ 0 ] );
//    //MSG_ReadFloat( cl.predictedState.error[ 1 ] );
//    //MSG_ReadFloat( cl.predictedState.error[ 2 ] );
//
//    // Ground Entity ( Needed for client side test with smooth stepping ):
//    // -1 if nullptr.
//    const int32_t groundEntity = MSG_ReadInt32();
//    if ( groundEntity != -1 ) {
//        cl.predictedState.groundEntity = &clge->entities[ groundEntity ];
//    }
//    // Ground Plane ( Needed for client side test with smooth stepping ):
//    const int32_t groundPlaneMessage = MSG_ReadInt8();
//    if ( groundPlaneMessage == -1 ) {
//        cl.predictedState.step = MSG_ReadFloat();
//        cl.predictedState.step_time = MSG_ReadUintBase128();
//    } else if ( groundPlaneMessage == 1 ) {
//        cplane_t *p = &cl.predictedState.groundPlane;
//        p->dist = MSG_ReadFloat( );
//        p->normal[ 0 ] = MSG_ReadFloat();
//        p->normal[ 1 ] = MSG_ReadFloat();
//        p->normal[ 2 ] = MSG_ReadFloat();
//        p->signbits = MSG_ReadInt8();
//        p->type = MSG_ReadInt8();
//
//        cl.predictedState.step = MSG_ReadFloat();
//        cl.predictedState.step_time = MSG_ReadUintBase128();
//
//        cl.predictedState.view_current_height = MSG_ReadFloat();
//        cl.predictedState.view_previous_height = MSG_ReadFloat();
//        cl.predictedState.view_height_time = cls.realtime + MSG_ReadIntBase128();
//    }
//
//    // Keep track of actual ground plane changes so we can write them out.
//    last_predicted_plane = cl.predictedState.groundPlane;
//}

void CL_SeekDemoMessage(void)
{
    int         cmd;
    int         index;

#if USE_DEBUG
    if (cl_shownet->integer == 1) {
        Com_LPrintf(PRINT_DEVELOPER, "%zu ", msg_read.cursize);
    } else if (cl_shownet->integer > 1) {
        Com_LPrintf(PRINT_DEVELOPER, "------------------\n");
    }
#endif

//
// parse the message
//
    while (1) {
        if (msg_read.readcount > msg_read.cursize) {
            Com_Error(ERR_DROP, "%s: read past end of server message", __func__);
        }

        if ((cmd = MSG_ReadUint8()) == -1) {
            SHOWNET(1, "%3zu:END OF MESSAGE\n", msg_read.readcount - 1);
            break;
        }

#if USE_DEBUG
        if (cl_shownet->integer > 1) {
            MSG_ShowSVC(cmd);
        }
#endif

        // other commands
        switch (cmd) {
        default:
            // Didn't recognize the command, pass control over the client game to give it a shot
            // at handling the command.
            clge->StartServerMessage( true );
            if ( !clge->SeekDemoMessage( cmd ) ) {
                Com_Error( ERR_DROP, "%s: illegible server message: %d", __func__, cmd );
            }
            clge->EndServerMessage( true );
            break;

        case svc_nop:
            break;

        case svc_disconnect:
        case svc_reconnect:
            Com_Error(ERR_DISCONNECT, "Server disconnected");
            break;

        case svc_print:
            MSG_ReadUint8();
            // fall through

        case svc_centerprint:
        case svc_stufftext:
            MSG_ReadString(NULL, 0);
            break;

        case svc_configstring:
            index = MSG_ReadInt16();
            CL_ParseConfigstring(index);
            break;

        case svc_sound:
            CL_ParseStartSoundPacket();
            break;

        //case svc_temp_entity:
        //    CL_ParseTEntPacket();
        //    break;

        //case svc_muzzleflash:
        //case svc_muzzleflash2:
        //    CL_ParseMuzzleFlashPacket(0);
        //    break;

        case svc_frame:
            CL_ParseFrame();
            continue;
        case svc_set_portalbit:
            CL_ParseSetAreaPortalBit();
            continue;

        // Moved to Client Game.
        //case svc_inventory:
        //    CL_ParseInventory();
        //    break;
        // Moved to Client Game.
        //case svc_layout:
        //    CL_ParseLayout();
        //    break;

        }
    }
}
