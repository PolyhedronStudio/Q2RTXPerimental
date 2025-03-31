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
// world.c -- world query functions

#include "server/sv_server.h"
#include "server/sv_world.h"

/*
===============================================================================

ENTITY AREA CHECKING

FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/

typedef struct areanode_s {
    int     axis;       // -1 = leaf node
    float   dist;
    struct areanode_s   *children[2];
    list_t  trigger_edicts;
    list_t  solid_edicts;
} areanode_t;

#define    AREA_DEPTH    4
#define    AREA_NODES    32

static areanode_t   sv_areanodes[AREA_NODES];
static int          sv_numareanodes;

static const vec_t  *area_mins, *area_maxs;
static edict_t      **area_list;
static int          area_count, area_maxcount;
static int          area_type;



/**
*	@brief	Builds a uniformly subdivided tree for the given world size
**/
static areanode_t *SV_CreateAreaNode(int depth, const vec3_t mins, const vec3_t maxs)
{
    areanode_t  *anode;
    vec3_t      size;
    vec3_t      mins1, maxs1, mins2, maxs2;

    anode = &sv_areanodes[sv_numareanodes];
    sv_numareanodes++;

    List_Init(&anode->trigger_edicts);
    List_Init(&anode->solid_edicts);

    if (depth == AREA_DEPTH) {
        anode->axis = -1;
        anode->children[0] = anode->children[1] = NULL;
        return anode;
    }

    VectorSubtract(maxs, mins, size);
    if (size[0] > size[1])
        anode->axis = 0;
    else
        anode->axis = 1;

    anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
    VectorCopy(mins, mins1);
    VectorCopy(mins, mins2);
    VectorCopy(maxs, maxs1);
    VectorCopy(maxs, maxs2);

    maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

    anode->children[0] = SV_CreateAreaNode(depth + 1, mins2, maxs2);
    anode->children[1] = SV_CreateAreaNode(depth + 1, mins1, maxs1);

    return anode;
}

/**
*   @brief  Called after the world model has been loaded, before linking any entities.
**/
void SV_ClearWorld(void)
{
    mmodel_t *cm;
    edict_t *ent;
    int i;

    // Clear area node data.
    memset(sv_areanodes, 0, sizeof(sv_areanodes));
    sv_numareanodes = 0;

    // Recreate a new area node list based on the current precached world model's mins/maxs.
    if (sv.cm.cache) {
        cm = &sv.cm.cache->models[0];
        SV_CreateAreaNode(0, cm->mins, cm->maxs);
    }

    // Make sure all entities are unlinked.
    for (i = 0; i < ge->max_edicts; i++) {
        // Get edict pointer.s
        ent = EDICT_FOR_NUMBER(i);
        // Unlink.
        ent->area.prev = ent->area.next = NULL;
    }
}

/**
*   @brief  Needs to be called any time an entity changes origin, mins, maxs,
*           or solid.  Automatically unlinks if needed.
*           sets ent->v.absmin and ent->v.absmax
*           sets ent->leafnums[] for pvs determination even if the entity.
*           is not solid.
**/
void SV_LinkEdict(cm_t *cm, edict_t *ent)
{
    mleaf_t *leafs[MAX_TOTAL_ENT_LEAFS];
    int32_t clusters[MAX_TOTAL_ENT_LEAFS];
    int32_t num_leafs;
    int32_t i, j;
    int32_t area;
    mnode_t     *topnode;

    // set the size
    VectorSubtract(ent->maxs, ent->mins, ent->size);

    // set the abs box
    if ( ent->solid == SOLID_BSP && !VectorEmpty( ent->s.angles ) ) {
        // expand for rotation
        float   max, v;

        max = 0;
        for ( i = 0; i < 3; i++ ) {
            v = fabsf( ent->mins[ i ] );
            if ( v > max )
                max = v;
            v = fabsf( ent->maxs[ i ] );
            if ( v > max )
                max = v;
        }
        for ( i = 0; i < 3; i++ ) {
            ent->absmin[ i ] = ent->s.origin[ i ] - max;
            ent->absmax[ i ] = ent->s.origin[ i ] + max;
        }
    } else {
        // normal
        VectorAdd( ent->s.origin, ent->mins, ent->absmin );
        VectorAdd( ent->s.origin, ent->maxs, ent->absmax );
    }

    // Because movement is clipped an epsilon away from an actual edge,
    // we must fully check even when bounding boxes don't quite touch.
    ent->absmin[0] -= 1;
    ent->absmin[1] -= 1;
    ent->absmin[2] -= 1;
    ent->absmax[0] += 1;
    ent->absmax[1] += 1;
    ent->absmax[2] += 1;

    // Link to PVS leafs.
    ent->num_clusters = 0;
    ent->areanum = 0;
    ent->areanum2 = 0;

    //get all leafs, including solids
    num_leafs = CM_BoxLeafs(cm, ent->absmin, ent->absmax,
                            leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

    // set areas
    for ( i = 0; i < num_leafs; i++ ) {
        clusters[ i ] = leafs[ i ]->cluster;
        area = leafs[ i ]->area;
        if ( area ) {
            // doors may legally straggle two areas,
            // but nothing should evern need more than that
            if ( ent->areanum && ent->areanum != area ) {
                if ( ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading ) {
                    Com_DPrintf( "Object touching 3 areas at %s\n", vtos( ent->absmin ) );
                }
                ent->areanum2 = area;
            } else
                ent->areanum = area;
        }
    }

    if ( num_leafs >= MAX_TOTAL_ENT_LEAFS ) {
        // Assume we missed some leafs, and mark by headnode.
        ent->num_clusters = -1;
        ent->headnode = CM_NumberForNode( cm, topnode );
    } else {
        ent->num_clusters = 0;
        for ( i = 0; i < num_leafs; i++ ) {
            if ( clusters[ i ] == -1 )
                continue;        // not a visible leaf
            for ( j = 0; j < i; j++ )
                if ( clusters[ j ] == clusters[ i ] )
                    break;
            if ( j == i ) {
                if ( ent->num_clusters == MAX_ENT_CLUSTERS ) {
                    // Assume we missed some leafs, and mark by headnode.
                    ent->num_clusters = -1;
                    ent->headnode = CM_NumberForNode( cm, topnode );
                    break;
                }

                ent->clusternums[ ent->num_clusters++ ] = clusters[ i ];
            }
        }
    }
}

/**
*   @brief  Call before removing an entity, and before trying to move one,
*           so it doesn't clip against itself.
**/
void PF_UnlinkEdict(edict_t *ent)
{
    if (!ent->area.prev)
        return;        // not linked in anywhere
    List_Remove(&ent->area);
    ent->area.prev = ent->area.next = NULL;
}

/**
*   @brief  Needs to be called any time an entity changes origin, mins, maxs,
*           or solid.  Automatically unlinks if needed.
*           sets ent->v.absmin and ent->v.absmax
*           sets ent->leafnums[] for pvs determination even if the entity.
*           is not solid.
**/
void PF_LinkEdict(edict_t *ent)
{
    areanode_t *node;
    server_entity_t *sent;
    int entnum;

    // If it has been linked previously(possibly to an other position), unlink first.
    if ( ent->area.prev ) {
        PF_UnlinkEdict( ent );
    }

    // Do not try and add the world.
    if ( ent == ge->edicts ) {
        return;        // don't add the world
    }

    // Entity has to be in-use.
    if (!ent->inuse) {
        Com_DPrintf("%s: entity %d is not in use\n", __func__, NUMBER_OF_EDICT(ent));
        return;
    }

    // Can't link of no world has been precached yet.
    if (!sv.cm.cache) {
        return;
    }

    // Get entity number.
    entnum = NUMBER_OF_EDICT(ent);
    // Specific server entity data pointer.
    sent = &sv.entities[entnum];

    // encode the size into the entity_state for client prediction
    switch (ent->solid) {
    case SOLID_BOUNDS_BOX:
        if ( ( ent->svflags & SVF_DEADMONSTER ) || VectorCompare( ent->mins, ent->maxs ) ) {
            ent->s.solid = SOLID_NOT;   // 0
            sent->solid32 = SOLID_NOT;  // 0
        } else {
            ent->s.solid = static_cast<solid_t>( sent->solid32 = ent->solid );
            ent->s.bounds = static_cast<uint32_t>( MSG_PackBoundsUint32( ent->mins, ent->maxs ).u );
        }
        break;
    case SOLID_BOUNDS_OCTAGON:
        if ( ( ent->svflags & SVF_DEADMONSTER ) || VectorCompare( ent->mins, ent->maxs ) ) {
            ent->s.solid = SOLID_NOT;   // 0
            sent->solid32 = SOLID_NOT;  // 0
        } else {
            ent->s.solid = static_cast<solid_t>( sent->solid32 = ent->solid );
            ent->s.bounds = static_cast<uint32_t>( MSG_PackBoundsUint32( ent->mins, ent->maxs ).u );
        }
        break;
    case SOLID_BSP:
        ent->s.solid = static_cast<solid_t>(BOUNDS_BRUSHMODEL);      // a SOLID_BOUNDS_BOX will never create this value
        sent->solid32 = BOUNDS_BRUSHMODEL;                           // FIXME: use 255? NOTICE: We do now :-)
        break;
    default:
        ent->s.solid = SOLID_NOT;   // 0
        sent->solid32 = SOLID_NOT;  // 0
        break;
    }

    // Clipmask:
    if ( ent->clipmask ) {
        ent->s.clipmask = ent->clipmask;
    } else {
        ent->s.clipmask = CONTENTS_NONE;
    }

    // Hull Contents: (Further on this gets set to CONTENTS_NONE in case of a SOLID_NOT).
    ent->s.hullContents = ent->hullContents;

    // Owner:
    if ( ent->owner != nullptr ) {
        ent->s.ownerNumber = ent->owner->s.number;
    } else {
        ent->s.ownerNumber = 0;
    }

    // Link edit in.
    SV_LinkEdict(&sv.cm, ent);

    // If its the entity's first time, make sure old_origin is valid, unless a BEAM which handles it by itself.
    if ( !ent->linkcount ) {
        if ( ent->s.entityType != ET_BEAM && !( ent->s.renderfx & RF_BEAM ) ) {
            VectorCopy( ent->s.origin, ent->s.old_origin );
        }
    }

    // Increment link count.
    ent->linkcount++;

    // Solid NOT won't have any contents either.
    if ( ent->solid == SOLID_NOT ) {
        ent->s.hullContents = CONTENTS_NONE;
        return;
    }

    // Find the first node that the ent's box crosses.
    node = sv_areanodes;
    while (1) {
        if (node->axis == -1)
            break;
        if (ent->absmin[node->axis] > node->dist)
            node = node->children[0];
        else if (ent->absmax[node->axis] < node->dist)
            node = node->children[1];
        else
            break;        // crosses the node
    }

    // link it in
    if (ent->solid == SOLID_TRIGGER)
        List_Append(&node->trigger_edicts, &ent->area);
    else
        List_Append(&node->solid_edicts, &ent->area);
}


/**
*	@brief	SV_AreaEdicts_r
**/
static void SV_AreaEdicts_r(areanode_t *node)
{
    list_t      *start;
    edict_t     *check;

    // touch linked edicts
    if (area_type == AREA_SOLID)
        start = &node->solid_edicts;
    else
        start = &node->trigger_edicts;

    LIST_FOR_EACH(edict_t, check, start, area) {
        if (check->solid == SOLID_NOT)
            continue;        // deactivated
        if (check->absmin[0] > area_maxs[0]
            || check->absmin[1] > area_maxs[1]
            || check->absmin[2] > area_maxs[2]
            || check->absmax[0] < area_mins[0]
            || check->absmax[1] < area_mins[1]
            || check->absmax[2] < area_mins[2])
            continue;        // not touching

        if (area_count == area_maxcount) {
            Com_WPrintf("SV_AreaEdicts: MAXCOUNT\n");
            return;
        }

        area_list[area_count] = check;
        area_count++;
    }

    if (node->axis == -1)
        return;        // terminal node

    // recurse down both sides
    if (area_maxs[node->axis] > node->dist)
        SV_AreaEdicts_r(node->children[0]);
    if (area_mins[node->axis] < node->dist)
        SV_AreaEdicts_r(node->children[1]);
}

/**
*   @brief  fills in a table of edict pointers with edicts that have bounding boxes
*           that intersect the given area.  It is possible for a non-axial bmodel
*           to be returned that doesn't actually intersect the area on an exact
*           test.
*   @todo: Does this always return the world?
*   @return The number of pointers filled in.
**/
const int32_t SV_AreaEdicts(const vec3_t mins, const vec3_t maxs,
                  edict_t **list, const int32_t maxcount, const int32_t areatype)
{
    area_mins = mins;
    area_maxs = maxs;
    area_list = list;
    area_count = 0;
    area_maxcount = maxcount;
    area_type = areatype;

    SV_AreaEdicts_r(sv_areanodes);

    return area_count;
}


//===========================================================================

/**
*	@return	A headnode that can be used for testing and/or clipping an
*			object 'hull' of mins/maxs size for the entity's said 'solid'.
**/
static mnode_t *SV_HullForEntity(edict_t *ent, const bool includeSolidTriggers = false ) {
    if ( ent->solid == SOLID_BSP || ( includeSolidTriggers && ent->solid == SOLID_TRIGGER ) ){
        int32_t i = ent->s.modelindex - 1;
        
        //// account for "hole" in configstring namespace
        //if ( i >= MODELINDEX_PLAYER && sv.cm.cache->nummodels >= MODELINDEX_PLAYER )
        //    i--;

        // Explicit hulls in the BSP model.
        if ( i <= 0 || i >= sv.cm.cache->nummodels ) {
            if ( !includeSolidTriggers ) {
                Com_Error( ERR_DROP, "%s: inline model %d out of range", __func__, i );
                return nullptr;
            }
        }

        return sv.cm.cache->models[i].headnode;
    }

    // Create a temp hull from entity bounds and contents clipmask for the specific type of 'solid'.
    if ( ent->solid == SOLID_BOUNDS_OCTAGON ) {
        return CM_HeadnodeForOctagon( &sv.cm, ent->mins, ent->maxs, ent->hullContents );
    } else {
        return CM_HeadnodeForBox( &sv.cm, ent->mins, ent->maxs, ent->hullContents );
    }
}

/**
*	@brief	SV_WorldNodes
**/
static mnode_t *SV_WorldNodes( void ) {
	return sv.cm.cache ? sv.cm.cache->nodes : nullptr;
}

/**
*	@return	The CONTENTS_* value from the world at the given point.
*			Quake 2 extends this to also check entities, to allow moving liquids
**/
const contents_t SV_PointContents( const vec3_t p ) {
	edict_t *touch[ MAX_EDICTS ], *hit;
	int         i, num;
    contents_t  contents;


	if ( !sv.cm.cache ) {
		Com_Error( ERR_DROP, "%s: no map loaded", __func__ );
	}

    if ( !p ) {
        return CONTENTS_NONE;
    }

	// get base contents from world
    contents = ( CM_PointContents( &sv.cm, p, SV_WorldNodes() ) );

	// or in contents from all the other entities
	num = SV_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID );

	for ( i = 0; i < num; i++ ) {
		hit = touch[ i ];

        //// Skip client entiteis if their SVF_PLAYER flag is set.
        //if ( hit->s.number <= sv_maxclients->integer && ( hit->svflags & SVF_PLAYER ) ) {
        //    continue;
        //}

		// Might intersect, so do an exact clip.
		contents = ( contents | CM_TransformedPointContents( &sv.cm, p, SV_HullForEntity(hit),
												hit->s.origin, hit->s.angles ) );
	}

	return contents;
}

/**
*	@brief	SV_ClipMoveToEntities
**/
static void SV_ClipMoveToEntities(const vec3_t start, const vec3_t mins,
                                  const vec3_t maxs, const vec3_t end,
                                  edict_t *passedict, const contents_t contentmask, cm_trace_t *tr)
{
    vec3_t      boxmins, boxmaxs;
    int         i, num;
    edict_t     *touchlist[MAX_EDICTS], *touch;
    cm_trace_t     trace = {};
    // create the bounding box of the entire move
    for (i = 0; i < 3; i++) {
        if (end[i] > start[i]) {
            boxmins[i] = start[i] + mins[i] - 1;
            boxmaxs[i] = end[i] + maxs[i] + 1;
        } else {
            boxmins[i] = end[i] + mins[i] - 1;
            boxmaxs[i] = start[i] + maxs[i] + 1;
        }
    }

    num = SV_AreaEdicts(boxmins, boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0; i < num; i++) {
        touch = touchlist[ i ];
        if ( touch->solid == SOLID_NOT ) {
            continue;
        }
        if ( touch == passedict ) {
            continue;
        }
        if ( tr->allsolid ) {
            return;
        }
        if ( passedict ) {
            if ( touch->owner == passedict )
                continue;    // Don't clip against own missiles.
            if ( passedict->owner == touch )
                continue;    // Don't clip against owner.
        }

        if ( !( contentmask & CONTENTS_DEADMONSTER )
            && ( touch->svflags & SVF_DEADMONSTER ) ) {
            continue;
        }

        if ( !( contentmask & CONTENTS_PROJECTILE )
            && ( touch->svflags & SVF_PROJECTILE ) ) {
            continue;
        }
        if ( !( contentmask & CONTENTS_PLAYER ) 
            && ( touch->svflags & SVF_PLAYER ) ) {
            continue;
        }

        // might intersect, so do an exact clip
        CM_TransformedBoxTrace( &sv.cm, &trace, start, end, mins, maxs,
                               SV_HullForEntity(touch), contentmask,
                               touch->s.origin, touch->s.angles);

        CM_ClipEntity( &sv.cm, tr, &trace, touch);
    }
}

/**
*	@description	mins and maxs are relative
*
*					if the entire move stays in a solid volume, trace.allsolid will be set,
*					trace.startsolid will be set, and trace.fraction will be 0
*
*					if the starting point is in a solid, it will be allowed to move out
*					to an open area
*
*					passedict is explicitly excluded from clipping checks (normally NULL)
**/
const cm_trace_t q_gameabi SV_Trace(const vec3_t start, const vec3_t mins,
                           const vec3_t maxs, const vec3_t end,
                           edict_t *passedict, const contents_t contentmask)
{
    cm_trace_t     trace;

    if (!sv.cm.cache) {
        Com_Error(ERR_DROP, "%s: no map loaded", __func__);
    }

    if (!mins)
        mins = vec3_origin;
    if (!maxs)
        maxs = vec3_origin;

    // clip to world
    CM_BoxTrace( &sv.cm, &trace, start, end, mins, maxs, SV_WorldNodes( ), contentmask );
    trace.ent = ge->edicts;
    if (trace.fraction == 0) {
        return trace;   // blocked by the world
    }

    // clip to other solid entities
    SV_ClipMoveToEntities(start, mins, maxs, end, passedict, contentmask, &trace);

    return trace;
}

/**
*	@brief	Like SV_Trace(), but clip to specified entity only.
*			Can be used to clip to SOLID_TRIGGER by its BSP tree.
**/
const cm_trace_t q_gameabi SV_Clip( edict_t *clip, const vec3_t start, const vec3_t mins,
                            const vec3_t maxs, const vec3_t end,
                            const contents_t contentmask ) {
	cm_trace_t     trace;

    if ( !sv.cm.cache ) {
        Com_Error( ERR_DROP, "%s: no map loaded", __func__ );
    }

	if ( !mins )
		mins = vec3_origin;
	if ( !maxs )
		maxs = vec3_origin;

	if ( clip == ge->edicts )
		CM_BoxTrace( &sv.cm, &trace, start, end, mins, maxs, SV_WorldNodes( ), contentmask );
	else
		CM_TransformedBoxTrace( &sv.cm, &trace, start, end, mins, maxs,
							   SV_HullForEntity( clip, true ), contentmask,
							   clip->s.origin, clip->s.angles );
	trace.ent = clip;
	return trace;
}