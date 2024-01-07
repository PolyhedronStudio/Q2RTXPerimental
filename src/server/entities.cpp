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

#include "server.h"

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
                                  client_frame_t   *from,
                                  client_frame_t   *to,
                                  int              clientEntityNum)
{
    entity_packed_t *newent;
    const entity_packed_t *oldent;
    int i, oldnum, newnum, oldindex, newindex, from_num_entities;
    msgEsFlags_t flags;

    if (!from)
        from_num_entities = 0;
    else
        from_num_entities = from->num_entities;

    newindex = 0;
    oldindex = 0;
    oldent = newent = NULL;
    while (newindex < to->num_entities || oldindex < from_num_entities) {
        if (newindex >= to->num_entities) {
            newnum = 9999;
        } else {
            i = (to->first_entity + newindex) % svs.num_entities;
            newent = &svs.entities[i];
            newnum = newent->number;
        }

        if (oldindex >= from_num_entities) {
            oldnum = 9999;
        } else {
            i = (from->first_entity + oldindex) % svs.num_entities;
            oldent = &svs.entities[i];
            oldnum = oldent->number;
        }

        if (newnum == oldnum) {
            // Delta update from old position. Because the force parm is false,
            // this will not result in any bytes being emitted if the entity has
            // not changed at all. Note that players are always 'newentities',
            // this updates their old_origin always and prevents warping in case
            // of packet loss.
            flags = client->esFlags;
            if (newnum <= client->maxclients) {
                // WID: C++20:
				//flags |= MSG_ES_NEWENTITY;
				flags = static_cast<msgEsFlags_t>( flags | MSG_ES_NEWENTITY );
            }
            if (newnum == clientEntityNum) {
                //flags |= MSG_ES_FIRSTPERSON;
				// WID: C++20:
				//flags |= MSG_ES_NEWENTITY;
				flags = static_cast<msgEsFlags_t>( flags | MSG_ES_FIRSTPERSON );
                VectorCopy(oldent->origin, newent->origin);
                VectorCopy(oldent->angles, newent->angles);
            }
            if (Q2PRO_SHORTANGLES(client, newnum)) {
                //flags |= MSG_ES_SHORTANGLES;
				// WID: C++20:
				//flags |= MSG_ES_NEWENTITY;
				flags = static_cast<msgEsFlags_t>( flags | MSG_ES_SHORTANGLES );
			}
            MSG_WriteDeltaEntity(oldent, newent, flags);
            oldindex++;
            newindex++;
            continue;
        }

        if (newnum < oldnum) {
            // this is a new entity, send it from the baseline
			//flags = client->esFlags | MSG_ES_FORCE | MSG_ES_NEWENTITY;
			// WID: C++20:
			//flags |= MSG_ES_NEWENTITY;
			flags = static_cast<msgEsFlags_t>( client->esFlags | MSG_ES_FORCE | MSG_ES_NEWENTITY );
            oldent = client->baselines[newnum >> SV_BASELINES_SHIFT];
            if (oldent) {
                oldent += (newnum & SV_BASELINES_MASK);
            } else {
                oldent = &nullEntityState;
            }
            if (newnum == clientEntityNum) {
                //flags |= MSG_ES_FIRSTPERSON;
				// WID: C++20:
				//flags |= MSG_ES_NEWENTITY;
				flags = static_cast<msgEsFlags_t>( flags | MSG_ES_FIRSTPERSON );
                VectorCopy(oldent->origin, newent->origin);
                VectorCopy(oldent->angles, newent->angles);
            }
            if (Q2PRO_SHORTANGLES(client, newnum)) {
                //flags |= MSG_ES_SHORTANGLES;
				// WID: C++20:
				//flags |= MSG_ES_NEWENTITY;
				flags = static_cast<msgEsFlags_t>( flags | MSG_ES_SHORTANGLES );
            }
            MSG_WriteDeltaEntity(oldent, newent, flags);
            newindex++;
            continue;
        }

        if (newnum > oldnum) {
            // the old entity isn't present in the new message
            MSG_WriteDeltaEntity(oldent, NULL, MSG_ES_FORCE);
            oldindex++;
            continue;
        }
    }

    MSG_WriteInt16(0);      // end of packetentities
}

static client_frame_t *get_last_frame(client_t *client)
{
    client_frame_t *frame;

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
	client_frame_t *frame, *oldframe;
	player_packed_t *oldstate;
	int             lastframe;

	// this is the frame we are creating
	frame = &client->frames[ client->framenum & UPDATE_MASK ];

	// this is the frame we are delta'ing from
	oldframe = get_last_frame( client );
	if ( oldframe ) {
		oldstate = &oldframe->ps;
		lastframe = client->lastframe;
	} else {
		oldstate = NULL;
		lastframe = -1;
	}

	MSG_WriteUint8( svc_frame );
	MSG_WriteIntBase128( client->framenum ); // WID: 64-bit-frame MSG_WriteInt32( client->framenum );
	MSG_WriteIntBase128( lastframe ); // WID: 64-bit-frame MSG_WriteInt32( lastframe );   // what we are delta'ing from
	MSG_WriteUint8( client->suppress_count );  // rate dropped packets
	client->suppress_count = 0;
	client->frameflags = 0;

	// send over the areabits
	MSG_WriteUint8( frame->areabytes );
	MSG_WriteData( frame->areabits, frame->areabytes );

	// delta encode the playerstate
	MSG_WriteUint8( svc_playerinfo );
	MSG_WriteDeltaPlayerstate( oldstate, &frame->ps );

	// delta encode the entities
	MSG_WriteUint8( svc_packetentities );
	SV_EmitPacketEntities( client, oldframe, frame, 0 );
}


/*
=============================================================================

Build a client frame structure

=============================================================================
*/

/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame(client_t *client)
{
    int         e, i;
    vec3_t      org;
    edict_t     *ent;
    edict_t     *clent;
    client_frame_t  *frame;
    entity_packed_t *state;
    player_state_t  *ps;
	entity_state_t  es;
    int         clientarea, clientcluster;
    mleaf_t     *leaf;
    byte        clientphs[VIS_MAX_BYTES];
    byte        clientpvs[VIS_MAX_BYTES];
    bool    ent_visible;
    int cull_nonvisible_entities = Cvar_Get("sv_cull_nonvisible_entities", "1", CVAR_CHEAT)->integer;
    bool        need_clientnum_fix;

    clent = client->edict;
    if (!clent->client)
        return;        // not in game yet

    // this is the frame we are creating
    frame = &client->frames[client->framenum & UPDATE_MASK];
    frame->number = client->framenum;
    frame->sentTime = com_eventTime; // save it for ping calc later
    frame->latency = -1; // not yet acked

    client->frames_sent++;

    // find the client's PVS
    ps = &clent->client->ps;
	VectorAdd( ps->viewoffset, ps->pmove.origin, org );
    // Add the actual viewoffset to the origin.
    org[ 2 ] += ps->pmove.viewheight;
    //VectorMA(ps->viewoffset, 1, ps->pmove.origin, org); // WID: floating-point

    leaf = CM_PointLeaf(client->cm, org);
    clientarea = leaf->area;
    clientcluster = leaf->cluster;

    // calculate the visible areas
    frame->areabytes = CM_WriteAreaBits(client->cm, frame->areabits, clientarea);
    if (!frame->areabytes/* && client->protocol != PROTOCOL_VERSION_Q2PRO*/) {
        frame->areabits[0] = 255;
        frame->areabytes = 1;
    }

    // grab the current player_state_t
    MSG_PackPlayer(&frame->ps, ps);

    // grab the current clientNum
    //if (g_features->integer & GMF_CLIENTNUM) {
    //    frame->clientNum = clent->client->clientNum;
    //    if (!VALIDATE_CLIENTNUM(frame->clientNum)) {
    //        Com_WPrintf("%s: bad clientNum %d for client %d\n",
    //                    __func__, frame->clientNum, client->number);
    //        frame->clientNum = client->number;
    //    }
    //} else {
        frame->clientNum = client->number;
    //}

    // fix clientNum if out of range for older version of Q2PRO protocol
    //need_clientnum_fix = client->protocol == PROTOCOL_VERSION_Q2PRO
    //    && client->version < PROTOCOL_VERSION_Q2PRO_CLIENTNUM_SHORT
    //    && frame->clientNum >= CLIENTNUM_NONE;
	need_clientnum_fix = false;

	if (clientcluster >= 0)
	{
		CM_FatPVS(client->cm, clientpvs, org, DVIS_PVS2);
		client->last_valid_cluster = clientcluster;
	}
	else
	{
		BSP_ClusterVis(client->cm->cache, clientpvs, client->last_valid_cluster, DVIS_PVS2);
	}

    BSP_ClusterVis(client->cm->cache, clientphs, clientcluster, DVIS_PHS);

    // build up the list of visible entities
    frame->num_entities = 0;
    frame->first_entity = svs.next_entity;

    for (e = 1; e < client->pool->num_edicts; e++) {
        ent = EDICT_POOL(client, e);

        // ignore entities not in use
        if (!ent->inuse && (g_features->integer & GMF_PROPERINUSE)) {
            continue;
        }

        // ignore ents without visible models
        if (ent->svflags & SVF_NOCLIENT)
            continue;

        // ignore ents without visible models unless they have an effect
        if (!ent->s.modelindex && !ent->s.effects && !ent->s.sound) {
            if (!ent->s.event) {
                continue;
            }
        }

        ent_visible = true;

        // ignore if not touching a PV leaf
        if (ent != clent) {
            // check area
			if (clientcluster >= 0 && !CM_AreasConnected(client->cm, clientarea, ent->areanum)) {
                // doors can legally straddle two areas, so
                // we may need to check another one
                if (!CM_AreasConnected(client->cm, clientarea, ent->areanum2)) {
                    ent_visible = false;        // blocked by a door
                }
            }

            if (ent_visible)
            {
                // beams just check one point for PHS
                if (ent->s.renderfx & RF_BEAM) {
                    if (!Q_IsBitSet(clientphs, ent->clusternums[0]))
                        ent_visible = false;
                }
                else {
                    if (cull_nonvisible_entities) {
                        if (ent->num_clusters == -1) {
                            // too many leafs for individual check, go by headnode
                            if (!CM_HeadnodeVisible(CM_NodeNum(client->cm, ent->headnode), clientpvs))
                                ent_visible = false;
                        } else {
                            // check individual leafs
                            for (i = 0; i < ent->num_clusters; i++)
                                if (Q_IsBitSet(clientpvs, ent->clusternums[i]))
                                    break;
                            if (i == ent->num_clusters)
                                ent_visible = false;       // not visible
                        }
                    }

                    if ( !ent->s.modelindex && !( ent->s.effects & EF_SPOTLIGHT ) ) {
                        // don't send sounds if they will be attenuated away
                        vec3_t    delta;
                        float    len;

                        VectorSubtract(org, ent->s.origin, delta);
                        len = VectorLength(delta);
                        if (len > 400)
                            ent_visible = false;
                    }
                }
            }
        }

        if(!ent_visible && (!sv_novis->integer || !ent->s.modelindex))
            continue;
        
		if (ent->s.number != e) {
			Com_WPrintf("%s: fixing ent->s.number: %d to %d\n",
				__func__, ent->s.number, e);
			ent->s.number = e;
		}

		memcpy(&es, &ent->s, sizeof(entity_state_t));

		if (!ent_visible) {
			// if the entity is invisible, kill its sound
			es.sound = 0;
		}

        // add it to the circular client_entities array
        state = &svs.entities[svs.next_entity % svs.num_entities];
        MSG_PackEntity(state, &es, Q2PRO_SHORTANGLES(client, e));

        // clear footsteps
        //if (state->event == EV_FOOTSTEP && client->settings[CLS_NOFOOTSTEPS]) {
        //    state->event = 0;
        //}

        // hide POV entity from renderer, unless this is player's own entity
        if (e == frame->clientNum + 1 && ent != clent &&
            (/*!Q2PRO_OPTIMIZE(client) || */need_clientnum_fix)) {
            state->modelindex = 0;
        }

		// don't mark players missiles as solid
        if (ent->owner == clent) {
            state->solid.u = 0;
        }
		// WID: netstuff: longsolid
		// else if (client->esFlags & MSG_ES_LONGSOLID) {
            state->solid.u = sv.entities[e].solid32;
        //}

        svs.next_entity++;

        if (++frame->num_entities == MAX_PACKET_ENTITIES) {
            break;
        }
    }

    if (need_clientnum_fix)
        frame->clientNum = client->slot;
}

