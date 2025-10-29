/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

#include "server/sv_server.h"
#include "server/sv_entities.h"
#include "shared/server/sv_game.h"
/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/

// some protocol optimizations are disabled when recording a demo
#define Q2PRO_OPTIMIZE(c) \
    ((c)->protocol == PROTOCOL_VERSION_Q2PRO && !(c)->settings[CLS_RECORDING])

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_packed_t list to the message.
=============
*/
static void SV_EmitPacketEntities(client_t         *client,
                                  sv_client_frame_t   *from,
                                  sv_client_frame_t   *to,
                                  const int32_t     clientEntityNumber) {
    // Defines the new state indices.
    int32_t newindex = 0;
    int32_t oldindex = 0;

    // Old entity numbers.
    int32_t oldnum = 0;
    int32_t newnum = 0;

    // Pointer to old and new states.
    entity_packed_t *newent = nullptr;
    const entity_packed_t *oldent = nullptr;
    
    // Flags.
    msgEsFlags_t flags = MSG_ES_NONE;
    // Iterator.
    int32_t i = 0;
    // Fetch where in the circular buffer we start from.
    const int32_t from_num_entities = ( !from ? 0 : from->num_entities );

	// Write the 'svc_packetentities' command.
    while (newindex < to->num_entities || oldindex < from_num_entities) {
		// Fetch the new entity number.
        if (newindex >= to->num_entities) {
            newnum = 9999;
        } else {
			// Fetch the new entity number.
            i = (to->first_entity + newindex) % svs.num_entities;
			// Fetch the new entity.
            newent = &svs.entities[i];
			// Fetch the new entity its number as the 'new entity' number.
            newnum = newent->number;
        }
		// Fetch the old entity number.
        if (oldindex >= from_num_entities) {
            oldnum = 9999;
        } else {
			// Fetch the old entity number.
            i = (from->first_entity + oldindex) % svs.num_entities;
			// Fetch the old entity.
            oldent = &svs.entities[i];
			// Fetch the old entity its number as the 'old entity' number.
            oldnum = oldent->number;
        }

		// Compare the new and old entity numbers.
        if (newnum == oldnum) {
            // Delta update from old position. Because the force parm is false,
            // this will not result in any bytes being emitted if the entity has
            // not changed at all. Note that players are always 'newentities',
            // this updates their old_origin always and prevents warping in case
            // of packet loss.
            flags = client->esFlags;
            // In case of a Player Entity:
            if (newnum <= client->maxclients) {
                // WID: C++20:
				flags |= MSG_ES_NEWENTITY;
            }
            // In case this is our own client entity, update the new ent's origin and angles.
            if (newnum == clientEntityNumber) {
                flags |= MSG_ES_FIRSTPERSON;
				VectorCopy(oldent->origin, newent->origin);
                VectorCopy(oldent->angles, newent->angles);
            }
            MSG_WriteDeltaEntity(oldent, newent, flags);
            oldindex++;
            newindex++;
            continue;
        }

        // The old entity is present in the current frame.
        if ( newnum < oldnum ) {
            flags |= MSG_ES_FORCE | MSG_ES_NEWENTITY;
            oldent = client->baselines[ newnum >> SV_BASELINES_SHIFT ];
            if ( oldent ) {
                oldent += ( newnum & SV_BASELINES_MASK );
            } else {
                oldent = &nullEntityState;
            }
            // In case this is our own client entity, update the new ent's origin and angles.
            if ( newnum == clientEntityNumber ) {
                flags |= MSG_ES_FIRSTPERSON;
                VectorCopy( oldent->origin, newent->origin );
                VectorCopy( oldent->angles, newent->angles );
            }
            MSG_WriteDeltaEntity( oldent, newent, flags );
            newindex++;
            continue;
        }

        // The old entity isn't present in the new frame.
        if (newnum > oldnum) {
            // The old entity isn't present in the new message.
            MSG_WriteDeltaEntity( oldent, NULL, MSG_ES_FORCE );
            oldindex++;
            continue;
        }
    }

    MSG_WriteInt16(0);      // End Of 'svc_packetentities'.
}

static sv_client_frame_t *get_last_frame(client_t *client)
{
    sv_client_frame_t *frame;

    if (client->lastframe <= 0) {
        // client is asking for a retransmit
        client->frames_nodelta++;
        return NULL;
    }

    client->frames_nodelta = 0;

    if (client->framenum - client->lastframe >= UPDATE_BACKUP) {
        // client hasn't gotten a good message through in a long time
        Com_DPrintf("%s: delta request from out-of-date packet.\n", client->name);
        return NULL;
    }

    // we have a valid message to delta from
    frame = &client->frames[client->lastframe & UPDATE_MASK];
    if (frame->number != client->lastframe) {
        // but it got never sent
        Com_DPrintf("%s: delta request from dropped frame.\n", client->name);
        return NULL;
    }

    if (svs.next_entity - frame->first_entity > svs.num_entities) {
        // but entities are too old
        Com_DPrintf("%s: delta request from out-of-date entities.\n", client->name);
        return NULL;
    }

    return frame;
}

/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient( client_t *client ) {
    // Defaults to nullptr, gets set when we found an actual old frame to retreive player state from.
	player_packed_t *oldPlayerState = nullptr;
    // Defaults to -1, gets set when we found an actual old frame to delta from.
	int64_t          lastFrameNumber = -1;

	// this is the frame we are creating
	sv_client_frame_t *newFrame = &client->frames[ client->framenum & UPDATE_MASK ];

	// this is the frame we are delta'ing from
	sv_client_frame_t *oldFrame = get_last_frame( client );
	if ( oldFrame ) {
		oldPlayerState = &oldFrame->ps;
		lastFrameNumber = client->lastframe;
	} else {
		oldPlayerState = nullptr;
        lastFrameNumber = -1;
	}

	MSG_WriteUint8( svc_frame );
	MSG_WriteIntBase128( client->framenum ); // WID: 64-bit-frame MSG_WriteInt32( client->framenum );
	MSG_WriteIntBase128( lastFrameNumber ); // WID: 64-bit-frame MSG_WriteInt32( lastframe );   // what we are delta'ing from
	MSG_WriteUint8( client->suppress_count );  // Rate dropped packets
	client->suppress_count = 0;
	client->frameflags = 0;

	// Send over the areabits.
	MSG_WriteUint8( newFrame->areabytes );
	MSG_WriteData( newFrame->areabits, newFrame->areabytes );

    // Send over entire frame's portal bits if this is the very first frame
    // or the client required a full on retransmit.
    if ( oldFrame == nullptr ) {
        // PortalBits frame message.
        MSG_WriteUint8( svc_portalbits );
        // Clean zeroed memory portal bits buffer for writing.
        static byte portalBits[ MAX_MAP_PORTAL_BYTES ] = { };
        memset( portalBits, 0, MAX_MAP_PORTAL_BYTES );
        // Write the current portal bit states to the portalBits buffer.
        int32_t numPortalBits = CM_WritePortalBits( &sv.cm, portalBits );
        // Write data to message.
        MSG_WriteUint8( numPortalBits );
        MSG_WriteData( portalBits, numPortalBits );
    }

	// delta encode the playerstate
	MSG_WriteUint8( svc_playerinfo );
    MSG_WriteDeltaPlayerstate( oldPlayerState, &newFrame->ps );

	// delta encode the entities
	MSG_WriteUint8( svc_packetentities );
	SV_EmitPacketEntities( client, oldFrame, newFrame, 0/*newFrame->clientNum*/);
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/
/**
*   @brief  True if the entity should be skipped from being sent the given entity.
**/
static inline const bool SV_SendClientIDSkipCheck( sv_edict_t *ent, const int32_t frameClientNumber ) {
    /**
    *   Entities can be flagged to explicitly not be sent to the client.
    **/
    if ( ent->svFlags & SVF_NOCLIENT ) {
        return true;
    }
    /**
    *   Entities can be flagged to be sent to only one client.
    **/
    if ( ent->svFlags & SVF_SENDCLIENT_SEND_TO_ID ) {
		// Don't skip if this is the client to send to.
		if ( ent->sendClientID == frameClientNumber ) {
            return false;
        }
    }
    /**
    *   Entities can be flagged to be sent to everyone but one client.
    **/
    if ( ent->svFlags & SVF_SENDCLIENT_EXCLUDE_ID ) {
		// Skip if this is the excluded client.
        if ( ent->sendClientID != frameClientNumber ) {
            return true;
        }
    }
    /**
    *   Entities can be flagged to be sent to a given mask of clients.
    **/
    if ( ent->svFlags & SVF_SENDCLIENT_BITMASK_IDS ) {
        if ( frameClientNumber == SENDCLIENT_TO_ALL ) {
			Com_Error( ERR_DROP, "SVF_CLIENTMASK: SENDCLIENT_TO_ALL used with BITMASK\n" );
            return false;
        }
        if ( frameClientNumber >= 32 ) {
            Com_Error( ERR_DROP, "SVF_CLIENTMASK: cientNum >= 32\n" );
            return false;
        }
		// Skip if the bit is not set for this client.
        if ( !( ent->sendClientID & ( 1ULL << frameClientNumber ) ) ) {
            return true;
		}
    }

	// Default: do not skip.
    return false;
}

/**
*   @brief  Determines if the entity is hearable to the client based on PHS.
**/
static inline const bool SV_CheckEntityHearableInFrame( sv_edict_t *ent, const Vector3 &viewOrg ) {
    if ( !ent->s.modelindex && !( ent->s.effects & EF_SPOTLIGHT ) ) {
        vec3_t delta;
        double len;

        VectorSubtract( viewOrg, ent->s.origin, delta );
        len = VectorLength( delta );
        if ( len > 400. ) {
            return false;
        }
    }

    return true;
}

/**
*   @brief  Determines if an entity is visible to the client based on PVS/PHS.
**/
static inline const bool SV_CheckEntityInFrame( sv_edict_t *ent, const Vector3 &viewOrigin, cm_t *cm, const int32_t clientCluster, const int32_t clientArea, byte *clientPVS, byte *clientPHS, const int32_t cullNonVisibleEntities ) {
    /**
    *   Check area visibility:
    **/
    if ( clientCluster >= 0 && !CM_AreasConnected( cm, clientArea, ent->areaNumber0 ) ) {
        // Doors can legally straddle two areas, so we may need to check another one.
        if ( !CM_AreasConnected( cm, clientArea, ent->areaNumber0 ) ) {
            // Blocked by a door.
            return false;
        }
    /**
    *   If visible by area:
    **/ 
    } else { 
        if ( cullNonVisibleEntities ) {
            // Too many leafs for individual check, go by headNode:
            if ( ent->numberOfClusters == -1 ) {
                // Does the client's PVS touch the ent's headnode?
                if ( !CM_HeadnodeVisible( CM_NodeForNumber( cm, ent->headNode ), clientPVS ) ) {
                    // Not visible
                    return false;
                }
            } else {
                // Check individual leafs.
                int32_t i = 0;
                for ( i = 0; i < ent->numberOfClusters; i++ ) {
                    // Visible in one leaf?
                    if ( Q_IsBitSet( clientPVS, ent->clusterNumbers[ i ] ) ) {
                        break;
                    }
                }
                // Not visible in any leaf.
                if ( i == ent->numberOfClusters ) {
                    // Not visible.
                    return false;
                }
            }
        }

        /**
        *   Don't send sounds if they will be attenuated away.
        **/
        if ( !SV_CheckEntityHearableInFrame( ent, viewOrigin ) ) {
            return false;
		}
    }

    return true;
}

/**
*   @brief  Decides which entities are going to be visible to the client, and
*           copies off the playerstat and areabits.
**/
void SV_BuildClientFrame(client_t *client)
{
    int32_t i = 0;
    sv_edict_t     *ent;
    entity_packed_t *state;
	entity_state_t  es;
    static byte        clientPHS[VIS_MAX_BYTES];
    memset( clientPHS, 0, sizeof( clientPHS ) );
    static byte        clientPVS[VIS_MAX_BYTES];
    memset( clientPHS, 0, sizeof( clientPVS ) );
    bool    entityVisibleForFrame;
    int cullNonVisibleEntities = Cvar_Get("sv_cull_nonvisible_entities", "1", CVAR_CHEAT)->integer;
    bool        need_clientnum_fix;

	/**
    *   Get the client's edict.
    **/
    sv_edict_t *clent = EDICT_FOR_NUMBER( client->number + 1 ); //client->edict;
    // Not in game yet.
    if ( !clent->client ) {
        return;        
    }

    /**
    *   This is the frame we are creating
    **/
	// Get the frame for this client.
    sv_client_frame_t *frame = &client->frames[ client->framenum & UPDATE_MASK ];
	// Set the frame number that we're at for this specific client.
    frame->number = client->framenum;
    // Save it for ping calc later.
    frame->sentTime = com_eventTime;
	// Not yet 'acked', (will be set later).
    frame->latency = -1;

	// Make sure to increment the count of sent frames
    client->frames_sent++;

    /**
	*   Find the client's PVS to determine potentially visible entities
    **/
    player_state_t *ps = &clent->client->ps;
	// Add the viewoffset to the pmove origin to get the full view position.
    Vector3 viewOrigin = ps->pmove.origin + ps->viewoffset; //VectorAdd( ps->viewoffset, ps->pmove.origin, org );
    // Also make sure to add the actual viewheight to the origin.
    viewOrigin.z += ps->pmove.viewheight;

    /**
	*   Determine the leaf and area/cluster the client is currently in.
    **/
    mleaf_t *leaf = CM_PointLeaf( client->cm, &viewOrigin.x );
    const int32_t clientArea = leaf->area;
    const int32_t clientCluster = leaf->cluster;

    /**
	*   Calculate the visible areas for the client and write them to the frame.
    **/
    // Calculate the visible areas.
    frame->areabytes = CM_WriteAreaBits( client->cm, frame->areabits, clientArea );
	// If no area bits were calculated, make sure at least one byte is set.
	// (All areas visible).
    if (!frame->areabytes/* && client->protocol != PROTOCOL_VERSION_Q2PRO*/) {
        frame->areabits[0] = 255;
        frame->areabytes = 1;
    }

    /**
	*   Pack up the current player_state_t, and ensure the frame has the correct clientNum.
    **/
    // grab the current player_state_t
    MSG_PackPlayer(&frame->ps, ps);

    // Grab the entity's client's current clientNum
    if (g_features->integer & GMF_CLIENTNUM) {
		// Assign the clientNum from the entity's client structure.
        frame->clientNum = clent->client->clientNum;
		// If the clientNum is invalid, fix it.
        if (!VALIDATE_CLIENTNUM(frame->clientNum)) {
            Com_WPrintf("%s: bad clientNum " PRId32 " for client " PRId32 "\n",
                        __func__, frame->clientNum, client->number );
			// Fix the clientNum.
            frame->clientNum = client->number;
        }
    // Otherwise, hard-assign it from the client number.
    } else {
        // <Q2RTXP>: WID: TODO: Do we need this? Probably best to test on dedicated server then.
        frame->clientNum = client->number;
    }
    // Remember to fix clientNum if out of range for older version of Q2PRO protocol.
    if ( frame->clientNum >= CLIENTNUM_NONE ) {
        need_clientnum_fix = true;
    } else {
        need_clientnum_fix = false;
    }

    /**
	*   Determine the PVS and PHS for the client.
    **/
	// If the client is in a valid cluster, calc full PVS.
	if ( clientCluster >= 0 ) {
		CM_FatPVS( client->cm, clientPVS, &viewOrigin.x, DVIS_PVS2 );
		client->last_valid_cluster = clientCluster;
    // PVS:
	} else {
		BSP_ClusterVis( client->cm->cache, clientPVS, client->last_valid_cluster, DVIS_PVS2 );
	}
    // PHS:
    BSP_ClusterVis( client->cm->cache, clientPHS, clientCluster, DVIS_PHS );

    /**
	*   Now build the list of visible entities for this client frame.
    **/
	// Start with no entities.
    frame->num_entities = 0;
	// Set the first entity index for this frame.
    frame->first_entity = svs.next_entity;
	// Iterate all server entities, starting at 1. (World == 0 ).
    int32_t entityNumber = 1;
    for ( entityNumber = 1; entityNumber < client->pool->num_edicts; entityNumber++ ) {
		// Get the entity.
        ent = EDICT_POOL( client, entityNumber );
        // ignore entities not in use
        if ( !ent->inUse && ( g_features->integer & GMF_PROPERINUSE ) ) {
            continue;
        }

        /**
        *   Determine whether the entity should be sent to this client.
        **/
        if ( SV_SendClientIDSkipCheck( ent, frame->clientNum ) ) {
            continue;
		}
        /**
		*   Ignore ents without visible models if they have no effects, sound or events.
        **/
        if ( !ent->s.modelindex && !ent->s.effects && !ent->s.sound ) {
            if ( !ent->s.event ) {
                continue;
            }
        }
        /**
        *   Determine whether the entity is visible at all, or not:
        **/
        entityVisibleForFrame = true;

        /**
		*   If we are not the entity's own client:
        *   Check PVS/PHS for visibility, and ignore if not touching a PV leaf.
        **/
        if ( ent != clent ) {

            entityVisibleForFrame = SV_CheckEntityInFrame( 
                ent, viewOrigin, 
                client->cm, clientCluster, clientArea,
                clientPVS, clientPHS, 
                cullNonVisibleEntities 
            );


        }

        /**
		*   Skip if the entity is not visible, and sv_novis is set, or the entity has no model.
        **/
        if ( !entityVisibleForFrame && ( !sv_novis->integer || !ent->s.modelindex ) ) {
            continue;
        }

        /**
		*   If the entity number is different from the current iteration, fix it.
        **/
		if ( ent->s.number != entityNumber ) {
			Com_WPrintf( "%s: fixing ent->s.number: %d to %d\n", __func__, ent->s.number, entityNumber );
			ent->s.number = entityNumber;
		}

        /**
        *   Copy the of the entity. (So we can modify if needed.)
        **/
        es = ent->s;

        /**
        *   If the entity is invisible, kill its sound.
        **/
		if ( entityVisibleForFrame == false ) {
			es.sound = 0;
		}
        /**
        *   Add it to the circular client_entities array
        **/
        // 
        state = &svs.entities[ svs.next_entity % svs.num_entities ];
        MSG_PackEntity( state, &es );

        /**
        *   Hide POV entity from renderer, unless this is player's own entity.
        **/
        if ( entityNumber == frame->clientNum + 1 && ent != clent &&
            (/*!Q2PRO_OPTIMIZE(client) || */need_clientnum_fix)) {
            state->modelindex = 0;
        }

        /**
		*   Don't mark players missiles( or other owner entities ) as solid.
        **/
        if (ent->owner == clent) {
            state->solid = SOLID_NOT;
        // WID: netstuff: longsolid
		// else if (client->esFlags & MSG_ES_LONGSOLID) {
        } else {
            state->solid = sv.entities[ entityNumber ].solid32;
        }

        /**
		*   Continue or break if we reached max entities for this frame.
        **/
		// Increment the circular buffer's 'next_entity' number.
        svs.next_entity++;
		// Increment number of entities in this frame. And break if we reached max.
        if (++frame->num_entities == sv_max_packet_entities->integer ) {
            break;
        }
    }

	// <Q2RTXP>: WID: Do we still need this clientNum fix for older protocol versions?
	// If needed, fix the clientNum for older protocol versions.
    if ( need_clientnum_fix ) {
        frame->clientNum = client->slot;
    }
}

