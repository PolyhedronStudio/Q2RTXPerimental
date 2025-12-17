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
static void SV_EmitPacketEntities( client_t *client,
    sv_client_frame_t *from,
    sv_client_frame_t *to,
    const int32_t     clientEntityNumber ) {
    // Defines the new state indices.
    int32_t newindex = 0;
    int32_t oldindex = 0;

    // Old entity numbers.
    int32_t oldnum = 0;
    int32_t newnum = 0;

    // Pointer to old and new states.
    entity_state_t *newent = nullptr;
    const entity_state_t *oldent = nullptr;

    // Flags.
    msgEsFlags_t flags = MSG_ES_NONE;
    // Iterator.
    int32_t i = 0;
    // Fetch where in the circular buffer we start from.
    const int32_t from_num_entities = ( !from ? 0 : from->num_entities );

    // one-time developer log guard for missing/out-of-range baseline usage
    static bool baseline_oob_logged = false;

    /**
    *   Write the 'svc_packetentities' command.
    **/
    while ( newindex < to->num_entities || oldindex < from_num_entities ) {
        // Reset per-entity flags at start of each iteration to avoid propagation
        // of flags from previous entities.
        flags = MSG_ES_NONE;//

        /**
        *   New Entity Index:
        **/
        // Fetch the new entity number.
        if ( newindex >= to->num_entities ) {
            newnum = 9999;
        } else {
            // Fetch the new entity index into the entities array..
            i = ( to->first_entity + newindex ) % svs.num_entities;
            // Fetch the new entity.
            newent = &svs.entities[ i ];
            // Fetch the new entity its number and back it up.
            newnum = newent->number;
        }
        /**
        *   Old Entity Index:
        **/
        // Fetch the old entity number.
        if ( oldindex >= from_num_entities ) {
            oldnum = 9999;
        } else {
            // Fetch the old entity index into the entities array..
            i = ( from->first_entity + oldindex ) % svs.num_entities;
            // Fetch the old entity.
            oldent = &svs.entities[ i ];
            // Fetch the old entity its number and back it up.
            oldnum = oldent->number;
        }

        // Compare the new and old entity numbers.
        if ( newnum == oldnum ) {
            // Delta update from old position. Because the force parm is false,
            // this will not result in any bytes being emitted if the entity has
            // not changed at all. Note that players are always 'newentities',
            // this updates their old_origin always and prevents warping in case
            // of packet loss.
            // In case of a Player Entity, force the new entity state flag to be set.
            flags = client->esFlags;
            if ( newnum <= client->maxclients ) {
                flags |= MSG_ES_NEWENTITY;
            }
            // In case this is our own client entity, update the new ent's origin and angles.
            if ( newnum == clientEntityNumber ) {
                // Indicate first-person entity.
                flags |= MSG_ES_FIRSTPERSON;
                // Backup origin and angles.
                newent->origin = oldent->origin; //VectorCopy(oldent->origin, newent->origin);
                newent->angles = oldent->angles; //VectorCopy(oldent->angles, newent->angles);
            }
            // Write the delta entity.
            MSG_WriteDeltaEntity( oldent, newent, flags );
            // Advance both indices.
            oldindex++;
            newindex++;
            // Skip the remainder to the next iteration.
            continue;
        }

        /**
        *   The old entity is present in the current frame.
        **/
        if ( newnum < oldnum ) {
            // start from client's base flags for this entity
            // (flags was already reset at top of loop)
            flags = client->esFlags | MSG_ES_FORCE | MSG_ES_NEWENTITY;
            const uint32_t baselineIndex = (uint32_t)( newnum >> SV_BASELINES_SHIFT );

            // Default fallback
            const entity_state_t *fallback_oldent = &nullEntityState;

            if ( baselineIndex < (uint32_t)SV_BASELINES_CHUNKS ) {
                entity_state_t *chunk = client->baselines[ baselineIndex ];
                if ( chunk ) {
                    const uint32_t baselineOffset = (uint32_t)( newnum & SV_BASELINES_MASK );
                    if ( baselineOffset < (uint32_t)SV_BASELINES_PER_CHUNK ) {
                        fallback_oldent = chunk + baselineOffset;
                    } else {
                        // out-of-range offset: log once
                        if ( !baseline_oob_logged ) {
                            Com_DPrintf( "%s: baseline offset OOB newnum=%d baselineIndex=%u offset=%u (falling back to nullEntityState)\n",
                                client->name[ 0 ] ? client->name : "<unknown>", newnum, baselineIndex, baselineOffset );
                            baseline_oob_logged = true;
                        }
                        fallback_oldent = &nullEntityState;
                    }
                } else {
                    // Missing chunk pointer: log once
                    if ( !baseline_oob_logged ) {
                        Com_DPrintf( "%s: missing baseline chunk for newnum=%d baselineIndex=%u (falling back to nullEntityState)\n",
                            client->name[ 0 ] ? client->name : "<unknown>", newnum, baselineIndex );
                        baseline_oob_logged = true;
                    }
                    fallback_oldent = &nullEntityState;
                }
            } else {
                // baselineIndex out-of-range: log once
                if ( !baseline_oob_logged ) {
                    Com_DPrintf( "%s: baselineIndex OOB newnum=%d baselineIndex=%u (SV_BASELINES_CHUNKS=%d) (falling back to nullEntityState)\n",
                        client->name[ 0 ] ? client->name : "<unknown>", newnum, baselineIndex, SV_BASELINES_CHUNKS );
                    baseline_oob_logged = true;
                }
                fallback_oldent = &nullEntityState;
            }

            oldent = fallback_oldent;
            // In case this is our own client entity, update the new ent's origin and angles.
            if ( newnum == clientEntityNumber ) {
                // Indicate first-person entity.
                flags |= MSG_ES_FIRSTPERSON;
                // Backup origin and angles.
                newent->origin = oldent->origin; //VectorCopy(oldent->origin, newent->origin);
                newent->angles = oldent->angles; //VectorCopy(oldent->angles, newent->angles);
            }
            // Write the delta entity.
            MSG_WriteDeltaEntity( oldent, newent, flags );
            // Advance the new index.
            newindex++;
            // Skip the remainder to the next iteration.
            continue;
        }

        // The old entity isn't present in the new frame.
        if ( newnum > oldnum ) {
            // The old entity isn't present in the new message.
            MSG_WriteDeltaEntity( oldent, NULL, MSG_ES_FORCE );
            // Advance the old index.
            oldindex++;
            // Skip the remainder to the next iteration.
            continue;
        }
    }

    /**
    *   End Of 'svc_packetentities' command.
    **/
    MSG_WriteInt16( 0 );      // End Of 'svc_packetentities'.
}

/**
*   @brief  Retrieves the last frame to delta from for the given client.
**/
static sv_client_frame_t *get_last_frame( client_t *client ) {
    // Client is asking for a retransmit.
    if ( client->lastframe <= 0 ) {
        // Count no-delta frames for stats.
        client->frames_nodelta++;
        return NULL;
    }

    // Reset no-delta counter
    client->frames_nodelta = 0;

    // Check if the frame is too old
    if ( client->framenum - client->lastframe >= UPDATE_BACKUP ) {
        // Client hasn't gotten a good message through in a long time.
        Com_DPrintf( "%s: delta request from out-of-date packet.\n", client->name );
        return NULL;
    }

    // We have a valid message to delta from
    sv_client_frame_t *frame = &client->frames[ client->lastframe & UPDATE_MASK ];
    if ( frame->number != client->lastframe ) {
        // But it got never sent.
        Com_DPrintf( "%s: delta request from dropped frame.\n", client->name );
        return NULL;
    }

    // Check if the entities are still valid.
    if ( svs.next_entity - frame->first_entity > svs.num_entities ) {
        // But entities are too old.
        Com_DPrintf( "%s: delta request from out-of-date entities.\n", client->name );
        return NULL;
    }

    return frame;
}

/**
*   @brief  Write the portal bits for the current server state to the message.
**/
static void WriteCurrentPortalBits() {
    // Static buffer for portal bits.
    static byte portalBits[ MAX_MAP_PORTAL_BYTES ] = { };
    // Clean zeroed memory portal bits buffer for writing.
    memset( portalBits, 0, MAX_MAP_PORTAL_BYTES );

    // Write the current portal bit states to the portalBits buffer.
    const int32_t numPortalBits = CM_WritePortalBits( &sv.cm, portalBits );

    // PortalBits frame message.
    MSG_WriteUint8( svc_portalbits );
    // Write data for message.
    MSG_WriteUint8( numPortalBits );
    MSG_WriteData( portalBits, numPortalBits );
}
/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient( client_t *client ) {
    // Defaults to nullptr, gets set when we found an actual old frame to retreive player state from.
    player_state_t *oldPlayerState = nullptr;
    // Defaults to -1, gets set when we found an actual old frame to delta from.
    int64_t          lastFrameNumber = -1;

    // This is the frame we are creating, and writing out to the client.
    sv_client_frame_t *newFrame = &client->frames[ client->framenum & UPDATE_MASK ];

    // This is the frame we are delta'ing from.
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
        WriteCurrentPortalBits();
    }

    #ifndef _NO_PM_TYPE_AFTER_DEATH
    // Default.
    int32_t clientEntityNumber = 0;
    // All pm_types < PM_DEAD are predicted, so we can send the client entity number for prediction.
    if ( newFrame->ps.pmove.pm_type < PM_DEAD ) {
        clientEntityNumber = newFrame->ps.clientNumber + 1;
    }
    #else
    const int32_t clientEntityNumber = newFrame->ps.clientNumber + 1;
    #endif
    // Delta encode the playerstate.
    MSG_WriteUint8( svc_playerinfo );
    //MSG_WriteUint8( newFrame->ps.clientNumber );
    MSG_WriteDeltaPlayerstate( oldPlayerState, &newFrame->ps );

    // Delta encode the entities.
    MSG_WriteUint8( svc_packetentities );
    SV_EmitPacketEntities( client, oldFrame, newFrame, clientEntityNumber );
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
    *   Sanity check :
    *   We can't have an entity that has SVF_NOCLIENT as well as SVF_NO_CULL or SVF_SENDCLIENT_* flags set.
    **/
    if ( ( ent->svFlags & SVF_NOCLIENT ) &&
        ( ent->svFlags & (
            SVF_NO_CULL
            | SVF_SENDCLIENT_EXCLUDE_ID
            | SVF_SENDCLIENT_SEND_TO_ID
            | SVF_SENDCLIENT_BITMASK_IDS ) )
        ) {
        //Com_Error( ERR_DROP, "SV_SendClientIDSkipCheck: Entity(#%d) has both SVF_NOCLIENT + SVF_SENDCLIENT_* and/or SVF_NO_CULL flags set.\n", ent->s.number );
        if ( developer->integer ) {
            Com_DPrintf( "SV_SendClientIDSkipCheck: Entity(#%d) has both SVF_NOCLIENT + SVF_SENDCLIENT_* and/or SVF_NO_CULL flags set.\n", ent->s.number );
        } else {
            Com_Error( ERR_DROP, "SV_SendClientIDSkipCheck: Entity(#%d) has both SVF_NOCLIENT + SVF_SENDCLIENT_* and/or SVF_NO_CULL flags set.\n", ent->s.number );
        }
        return true;
    }

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
        if ( ent->sendClientID == frameClientNumber ) {
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
*   @brief  Determines if the entity is to be 'attenuated' away.
**/
static inline const bool SV_CheckEntitySoundDistance( sv_edict_t *ent, const Vector3 &viewOrg ) {
    // We only cull non-model, non-spotlight entities by distance.
    if ( !ent->s.modelindex && !( ent->s.entityFlags & EF_SPOTLIGHT ) ) {
        // Calculate the distance from the entity to the client's view origin.
        const Vector3 delta = viewOrg - ent->s.origin;
        // Calculate the length of the delta.
        const double len = QM_Vector3LengthDP( delta );
        // Distance for culling.
        constexpr double soundCullDistance = 400.;
        // Test against cull distance.
        if ( len > soundCullDistance ) {
            // Culled by sound distance.
            return false;
        }
    }
    // Default: not culled.
    return true;
}

/**
*   @brief  Determines if an entity is hearable/visible to the client based on PVS/PHS.
**/
static inline const bool SV_CheckEntityInFrame( sv_edict_t *ent, const Vector3 &viewOrigin, cm_t *cm, const int32_t clientCluster, const int32_t clientArea, byte *clientPVS, byte *clientPHS, const int32_t cullNonVisibleEntities ) {
    /**
    *   Check area visibility:
    **/
    if ( clientCluster >= 0 && !CM_AreasConnected( cm, clientArea, ent->areaNumber0 ) ) {
        // Doors can legally straddle two areas, so we may need to check another one.
        if ( !CM_AreasConnected( cm, clientArea, ent->areaNumber1 ) ) {
            // Blocked by a door.
            return false;
        }
        /**
        *   If visible by area:
        **/
    } else {
        // beams just check one point for PHS
        if ( ent->s.renderfx & RF_BEAM ) {
            if ( !Q_IsBitSet( clientPHS, ent->clusterNumbers[ 0 ] ) ) {
                return false;
            }
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
            if ( !SV_CheckEntitySoundDistance( ent, viewOrigin ) ) {
                return false;
            }
        }
        ///**
        //*   Don't send sounds if they will be attenuated away.
        //**/
        //if ( !SV_CheckEntitySoundDistance( ent, viewOrigin ) ) {
        //  return false;
        //}
    }

    return true;
}

/**
*   @brief  Decides which entities are going to be visible to the client, and
*           copies off the playerstat and areabits.
**/
void SV_BuildClientFrame( client_t *client ) {
    // Static PHS/PVS buffers.
    static byte        clientPHS[ VIS_MAX_BYTES ];
    static byte        clientPVS[ VIS_MAX_BYTES ];
    // Clear the PHS/PVS buffers.
    memset( clientPHS, 0, sizeof( clientPHS ) );
    memset( clientPVS, 0, sizeof( clientPVS ) );

    // Cvar to cull non-visible entities.
    int32_t cullNonVisibleEntities = sv_cull_nonvisible_entities->integer;

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
    if ( !frame->areabytes/* && client->protocol != PROTOCOL_VERSION_Q2PRO*/ ) {
        frame->areabits[ 0 ] = 255;
        frame->areabytes = 1;
    }

    /**
    *   Setup playerstate, ensure the frame has the correct clientNum:
    **/
    // Assign the clientNum from the entity's client structure.
    // <Q2RTXP>: WID: This is commented out, because it'll be set to that of other clients in case of
	// spectating. We need to keep the actual client number of the client here.
    //ps->clientNumber = clent->client->clientNum;


    // If the clientNum is invalid, fix it.
    if ( !VALIDATE_CLIENTNUM( ps->clientNumber ) ) {
        // Warn.
        Com_WPrintf( "%s: bad clientNum " PRId32 " for client " PRId32 "\n",
            __func__, ps->clientNumber, client->number );

        // Fix the clientNum by resetting it to our own default received at connection time.
        ps->clientNumber = client->number;
    }
    // Set the frame's playerstate to the current player_state_t
    frame->ps = *ps;

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
    // Iterate all server entities, starting at 1. (World == 0 )."
    int32_t entityNumber = 1;
    // Iterate all entities.
    for ( entityNumber = 1; entityNumber < client->pool->num_edicts; entityNumber++ ) {
        // Get the entity.
        sv_edict_t *ent = EDICT_POOL( client, entityNumber );

        /**
        *   Ignore entities not in use.
        **/
        if ( ent == nullptr || ( ent->inUse == false && ( g_features->integer & GMF_PROPERINUSE ) ) ) {
            if ( ent == nullptr ) {
                Com_WPrintf( "%s: WARNING: ent == nullptr for entityNumber: %d\n", __func__, entityNumber );
            }
            continue;
        }

        /**
        *   Determine whether the entity should be sent to this client.
        **/
        if ( SV_SendClientIDSkipCheck( ent, frame->ps.clientNumber ) ) {
            continue;
        }

        /**
        *   Skip PHS/PVS Culling if the entity has the SVF_NO_CULL flag set.
        **/
        // Default to visible.
        bool entityVisibleForFrame = true;

		// Subtract the temp event entity type offset to determine if this is a temp event entity.
		// A value of < 0 indicates this is not a temp event entity, as ET_TEMP_EVENT_ENTITY always comes last in the entityType enum.
		const bool isTempEventEntity = ( ent->s.entityType - ge->GetTempEventEntityTypeOffset() > 0 );

        // Cull non-visible entities unless this entity is requested not to.
        if ( !( ent->svFlags & SVF_NO_CULL ) ) {
            /**
			*	Unless this is an actual Temp Event Entity:
            *		- Ignore ents without visible models if they have no effects, sound or events.
            **/
			// A value of < 0 indicates this is not a temp event entity, as ET_TEMP_EVENT_ENTITY 
			// always comes last in the entityType enum.
			if ( !isTempEventEntity ) {
				if ( !ent->s.modelindex && !ent->s.entityFlags && !ent->s.sound ) {
					if ( !EV_GetEntityEventValue( ent->s.event ) ) {
						continue;
					}
				}
			}

            /**
            *   If we are not the entity's own client:
            *   Check PVS/PHS for visibility, and ignore if not touching a PV leaf.
            **/
            if ( ent != clent ) {
                // Check PVS/PHS visibility.
                entityVisibleForFrame = SV_CheckEntityInFrame(
                    ent,
                    viewOrigin,

                    client->cm,

                    clientCluster, clientArea,
                    clientPVS, clientPHS,

                    cullNonVisibleEntities
                );
            }
        }

        /**
        *   Skip if the entity is not visible, and sv_novis is set, or the entity has no model.
        **/
		// A value of < 0 indicates this is not a temp event entity, as ET_TEMP_EVENT_ENTITY always comes last in the entityType enum.
        if ( !entityVisibleForFrame && ( !sv_novis->integer || ( !isTempEventEntity && !ent->s.modelindex ) ) ) {
            continue;
        }

        /**
        *   If the entity number is different from the current iteration, fix it.
        **/
        if ( ent->s.number != entityNumber ) {
            Com_WPrintf( "%s: fixing ent->s.number: (#%d) to (#%d)!\n", __func__, ent->s.number, entityNumber );
            ent->s.number = entityNumber;
        }

        /**
        *   Copy the state of the entity. (So we can modify if needed.)
        **/
        entity_state_t entityStateCopy = ent->s;

        /**
        *   Ensure entity state has the correct clientNumber set (if applicable).
        **/
        // Decode the skinnum to modify the clientNumber.
        encoded_skinnum_t skinnum = static_cast<encoded_skinnum_t>( entityStateCopy.skinnum );
        if ( ( ent->s.number > 0 && ent->s.number <= sv_maxclients->integer )
            && ent == clent
            && skinnum.clientNumber != ps->clientNumber
            ) {
            // Fix the skinnum to be the client's own number.
            skinnum.clientNumber = ps->clientNumber;
            // Set the skinnum back to the entity state.
            entityStateCopy.skinnum = skinnum.skinnum;
        }

        /**
        *   If the entity is invisible, kill its sound. (Unless SVF_NO_CULL is set.)
        **/
        if ( entityVisibleForFrame == false && !( ent->svFlags & SVF_NO_CULL ) ) {
            entityStateCopy.sound = 0;
        }

        /**
        *   Keep POV entity if this is player's own entity. Otherwise, remove it from rendering.
        **/
        if ( entityNumber == frame->ps.clientNumber + 1 && ent != clent ) {
            entityStateCopy.modelindex = 0;
        }

        /**
        *   Don't mark players missiles( or other owner entities ) as solid.
        **/
        if ( ent->owner == clent ) {
            entityStateCopy.solid = SOLID_NOT;
            // WID: netstuff: longsolid
            // else if (client->esFlags & MSG_ES_LONGSOLID) {
        } else {
            entityStateCopy.solid = sv.entities[ entityNumber ].solid32;
        }

        /**
        *   Add it to the circular client_entities array
        **/
        // Get a pointer to the next entity state in the circular buffer.
        entity_state_t *bufferEntityState = &svs.entities[ svs.next_entity % svs.num_entities ];
        // Copy over the (modifyable) entity state.
        *bufferEntityState = entityStateCopy;

        /**
        *   Continue or break if we reached max entities for this frame.
        **/
        // Increment the circular buffer's 'next_entity' number.
        svs.next_entity++;
        // Increment number of entities in this frame. And break if we reached max.
        if ( ++frame->num_entities == sv_max_packet_entities->integer ) {
            break;
        }
    }

    // <Q2RTXP>: WID: Do we still need this clientNum fix for older protocol versions?
    // If needed, fix the clientNum for older protocol versions.
    //if ( need_clientnum_fix ) {
    //    frame->ps.clientNumber = client->slot;
    //}
}

