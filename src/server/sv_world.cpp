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

typedef struct worldSector_s {
    int     axis;       // -1 = leaf node
    float   dist;
    struct worldSector_s    *children[2];
    list_t  trigger_edicts;
    list_t  solid_edicts;
} worldSector_t;

#define    SECTOR_DEPTH    4
#define    SECTOR_NODES    32

static worldSector_t    sv_sectorNodes[SECTOR_NODES];
static int              sv_numSectorNodes;

static Vector3  sector_mins, sector_maxs;
static sv_edict_t      **sector_list;
static int          sector_count, sector_maxcount;
static int          sector_type;



/**
*	@brief	Builds a uniformly subdivided tree for the given world size
**/
static worldSector_t *SV_CreateSectorNode(int depth, const vec3_t mins, const vec3_t maxs)
{
    worldSector_t  *anode;
    vec3_t      size;
    vec3_t      mins1, maxs1, mins2, maxs2;

    anode = &sv_sectorNodes[sv_numSectorNodes];
    sv_numSectorNodes++;

    List_Init(&anode->trigger_edicts);
    List_Init(&anode->solid_edicts);

    if (depth == SECTOR_DEPTH) {
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

    anode->children[0] = SV_CreateSectorNode(depth + 1, mins2, maxs2);
    anode->children[1] = SV_CreateSectorNode(depth + 1, mins1, maxs1);

    return anode;
}

/**
*   @brief  Called after the world model has been loaded, before linking any entities.
**/
void SV_ClearWorld( void ) {
	// Clear area node data.
	std::memset( sv_sectorNodes, 0, sizeof( sv_sectorNodes ) );
	sv_numSectorNodes = 0;

	// Recreate a new area node list based on the current precached world model's mins/maxs.
	if ( sv.cm.cache ) {
		mmodel_t *cm = &sv.cm.cache->models[ 0 ];
		SV_CreateSectorNode( 0, cm->mins, cm->maxs );
	}

	// Make sure all entities are unlinked.
	for ( int32_t i = 0; i < ge->edictPool->max_edicts; i++ ) {
		// Get edict pointer.s
		sv_edict_t *ent = EDICT_FOR_NUMBER( i );
		if ( ent != nullptr ) {
			// Unlink.
			ent->isLinked = false;
			//ent->area.prev = ent->area.next = NULL;
			ent->area = {};
		}
	}
}



/**
*
*
*
*   Entity World Area Linking:
*
*
*
**/
/**
*	@brief	Will encode/pack the mins/maxs bounds into the solid_packet_t uint32_t.
**/
static inline const bounds_packed_t _MSG_PackBoundsUint32( const Vector3 &mins, const Vector3 &maxs ) {
    return MSG_PackBoundsUint32( &mins.x, &maxs.x );
}

/**
*   @brief  Needs to be called any time an entity changes origin, mins, maxs,
*           or solid, model.
*			Automatically unlinks if needed.
*           
*			sets ent->v.absMin and ent->v.absMax
*           sets ent->leafnums[] for pvs determination even if the entity is not solid.
*			(So we know where it is at.)
**/
void SV_LinkEdict(cm_t *cm, sv_edict_t *ent)
{
    mleaf_t *leafs[MAX_TOTAL_ENT_LEAFS];
    int32_t clusters[MAX_TOTAL_ENT_LEAFS];
    int32_t num_leafs;
    int32_t i, j;
    int32_t area;
    mnode_t     *topnode;

    // set the size
    //VectorSubtract(ent->maxs, ent->mins, ent->size);
	// <Q2RTXP>: PATCH: currentOrigin/currentAngles.
	ent->size = ent->maxs - ent->mins;

    // set the abs box
	if ( ent->solid == SOLID_BSP && !VectorEmpty( ent->currentAngles ) ) {
		// Expand for rotation
		double max = 0;
		for ( int32_t i = 0; i < 3; i++ ) {
			// Mins:
			double v = fabsf( ent->mins[ i ] );
			if ( v > max ) {
				max = v;
			}
			// Maxs:
			v = fabsf( ent->maxs[ i ] );
			if ( v > max ) {
				max = v;
			}
		}
		// <Q2RTXP>: PATCH: currentOrigin/currentAngles.
		//for ( i = 0; i < 3; i++ ) {
		//	ent->absMin[ i ] = ent->s.origin[ i ] - max;
		//	ent->absMax[ i ] = ent->s.origin[ i ] + max;
		//}
		ent->absMin = ent->currentOrigin - Vector3{ max, max, max };
		ent->absMax = ent->currentOrigin + Vector3{ max, max, max };
    } else {
		// <Q2RTXP>: PATCH: currentOrigin/currentAngles.
        // normal
        //VectorAdd( ent->s.origin, ent->mins, ent->absMin );
        //VectorAdd( ent->s.origin, ent->maxs, ent->absMax );
		ent->absMin = ent->currentOrigin + ent->mins;
		ent->absMax = ent->currentOrigin + ent->maxs;
    }

    // Because movement is clipped an epsilon away from an actual edge,
    // we must fully check even when bounding boxes don't quite touch.
    ent->absMin[0] -= 1;
    ent->absMin[1] -= 1;
    ent->absMin[2] -= 1;
    ent->absMax[0] += 1;
    ent->absMax[1] += 1;
    ent->absMax[2] += 1;

    // Link to PVS leafs.
    ent->numberOfClusters = 0;
    ent->areaNumber0 = 0;
    ent->areaNumber1 = 0;

	// Get all leafs, including solids, that the ent's bbox touches,
	// and retrieve the topnode as well.
    num_leafs = CM_BoxLeafs(cm, ent->absMin, ent->absMax, leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

    // Set the areas.
    for ( i = 0; i < num_leafs; i++ ) {
        clusters[ i ] = leafs[ i ]->cluster;
        area = leafs[ i ]->area;
        if ( area ) {
            // doors may legally straggle two areas,
            // but nothing should evern need more than that
            if ( ent->areaNumber0 && ent->areaNumber0 != area ) {
                if ( ent->areaNumber1 && ent->areaNumber1 != area && sv.state == ss_loading ) {
                    Com_DPrintf( "Object touching 3 areas at %s\n", vtos( ent->absMin ) );
                }
                ent->areaNumber1 = area;
            } else {
                ent->areaNumber0 = area;
            }
        }
    }

    if ( num_leafs >= MAX_TOTAL_ENT_LEAFS ) {
        // Assume we missed some leafs, and mark by headNode.
        ent->numberOfClusters = -1;
        ent->headNode = CM_NumberForNode( cm, topnode );
    } else {
        ent->numberOfClusters = 0;
        for ( i = 0; i < num_leafs; i++ ) {
            if ( clusters[ i ] == -1 )
                continue;        // not a visible leaf
            for ( j = 0; j < i; j++ )
                if ( clusters[ j ] == clusters[ i ] )
                    break;
            if ( j == i ) {
                if ( ent->numberOfClusters == MAX_ENT_CLUSTERS ) {
                    // Assume we missed some leafs, and mark by headNode.
                    ent->numberOfClusters = -1;
                    ent->headNode = CM_NumberForNode( cm, topnode );
                    break;
                }

                ent->clusterNumbers[ ent->numberOfClusters++ ] = clusters[ i ];
            }
        }
    }
}

/**
*   @brief  Call before removing an entity, and before trying to move one,
*           so it doesn't clip against itself.
**/
void PF_UnlinkEdict(edict_ptr_t *ent)
{
	// Not linked in anywhere
	if ( !ent->area.prev ) {
		return;
	}

	// Mark linked status as false.
	ent->isLinked = false;
	// Remove from area.
    List_Remove(&ent->area);
	// Clear area links.
    ent->area.prev = ent->area.next = NULL;
}

/**
*   @brief  Needs to be called any time an entity changes origin, mins, maxs,
*			clipMask, model, hullContents, owner, 
*           or solid.  Automatically unlinks if needed.
*           sets ent->v.absMin and ent->v.absMax
*           sets ent->leafnums[] for pvs determination even if the entity.
*           is not solid.
**/
void PF_LinkEdict(edict_ptr_t *ent)
{
    worldSector_t *node;
    server_entity_t *sent;
    int entnum;

    if ( !ent ) {
        Com_Error( ERR_DROP, "%s: (nullptr) edict_t pointer\n", __func__ );
    }
    // If it has been linked previously(possibly to an other position), unlink first.
    if ( ent->area.prev ) {
        PF_UnlinkEdict( ent );
    }

    // Do not try and add the world.
    if ( ent == ge->edictPool->edicts[ 0 ] /* worldspawn */ ) {
        return;        // don't add the world
    }

	// Entity has to be in-use.
	if ( !ent->inUse ) {
		Com_DPrintf( "%s: entity %d is not in use\n", __func__, NUMBER_OF_EDICT( ent ) );
		return;
	}

	// Can't link of no world has been precached yet.
	if ( !sv.cm.cache ) {
		return;
	}

	// Get entity number.
	entnum = NUMBER_OF_EDICT( ent );
	// Specific server entity data pointer.
	sent = &sv.entities[ entnum ];

    // encode the size into the entity_state for client prediction
	switch ( ent->solid ) {
	case SOLID_BOUNDS_BOX:
        if ( ( ent->svFlags & SVF_DEADENTITY ) || VectorCompare( ent->mins, ent->maxs ) ) {
            ent->s.solid = SOLID_NOT;   // 0
            sent->solid32 = SOLID_NOT;  // 0
        } else {
            ent->s.solid = static_cast<cm_solid_t>( sent->solid32 = ent->solid );
            ent->s.bounds = static_cast<uint32_t>( _MSG_PackBoundsUint32( ent->mins, ent->maxs ).u );
        }
        break;
    case SOLID_BOUNDS_OCTAGON:
        if ( ( ent->svFlags & SVF_DEADENTITY ) || VectorCompare( ent->mins, ent->maxs ) ) {
            ent->s.solid = SOLID_NOT;   // 0
            sent->solid32 = SOLID_NOT;  // 0
        } else {
            ent->s.solid = static_cast<cm_solid_t>( sent->solid32 = ent->solid );
            ent->s.bounds = static_cast<uint32_t>( _MSG_PackBoundsUint32( ent->mins, ent->maxs ).u );
        }
        break;
    case SOLID_BSP:
        ent->s.solid = static_cast<cm_solid_t>(BOUNDS_BRUSHMODEL);      // a SOLID_BOUNDS_BOX will never create this value
        sent->solid32 = BOUNDS_BRUSHMODEL;                           // FIXME: use 255? NOTICE: We do now :-)
        break;
    default:
        ent->s.solid = SOLID_NOT;   // 0
        sent->solid32 = SOLID_NOT;  // 0
        break;
    }

	// Current Origin/Angles to Entity State Origin/Angles:
	//ent->s.origin = ent->currentOrigin;
	//ent->s.angles = ent->currentAngles;

    // Clipmask:
    if ( ent->clipMask ) {
        ent->s.clipMask = ent->clipMask;
    } else {
        ent->s.clipMask = CONTENTS_NONE;
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
    if ( !ent->linkCount ) {
        if ( ent->s.entityType != ET_BEAM && !( ent->s.renderfx & RF_BEAM ) ) {
            VectorCopy( ent->s.origin, ent->s.old_origin );
        }
    }

    // Increment link count.
    ent->linkCount++;

	// Mark linked status as true.
	ent->isLinked = true;

    // Solid NOT won't have any contents either.
    if ( ent->solid == SOLID_NOT ) {
        ent->s.hullContents = CONTENTS_NONE;
        return;
    }

    // Find the first node that the ent's box crosses.
    node = sv_sectorNodes;
    while (1) {
        if (node->axis == -1)
            break;
        if (ent->absMin[node->axis] > node->dist)
            node = node->children[0];
        else if (ent->absMax[node->axis] < node->dist)
            node = node->children[1];
        else
            break;        // crosses the node
    }

    // link it in
	if ( ent->solid == SOLID_TRIGGER ) {
		List_Append( &node->trigger_edicts, &ent->area );
	} else {
		List_Append( &node->solid_edicts, &ent->area );
	}
}


/**
*	@brief	SV_AreaEdicts_r
**/
static void SV_AreaEdicts_r(worldSector_t *node)
{
    list_t		*start = nullptr;
    sv_edict_t	*check = nullptr;

    // touch linked edicts
	if ( sector_type == AREA_SOLID ) {
		start = &node->solid_edicts;
	} else {
		start = &node->trigger_edicts;
	}

    LIST_FOR_EACH(sv_edict_t, check, start, area) {
		if ( check->solid == SOLID_NOT ) {
			continue;        // deactivated
		}
		if ( check->absMin[ 0 ] > sector_maxs[ 0 ]
			|| check->absMin[ 1 ] > sector_maxs[ 1 ]
			|| check->absMin[ 2 ] > sector_maxs[ 2 ]
			|| check->absMax[ 0 ] < sector_mins[ 0 ]
			|| check->absMax[ 1 ] < sector_mins[ 1 ]
			|| check->absMax[ 2 ] < sector_mins[ 2 ] ) {
			continue;        // not touching
		}

		if ( sector_count == sector_maxcount ) {
			Com_WPrintf( "SV_AreaEdicts: MAXCOUNT\n" );
			return;
		}

		sector_list[ sector_count ] = check;
		sector_count++;
    }

	// Terminal node!
	if ( node->axis == -1 ) {
		return;
	}

    // recurse down both sides
	if ( sector_maxs[ node->axis ] > node->dist ) {
		SV_AreaEdicts_r( node->children[ 0 ] );
	}
	if ( sector_mins[ node->axis ] < node->dist ) {
		SV_AreaEdicts_r( node->children[ 1 ] );
	}
}

/**
*   @brief  fills in a table of edict pointers with edicts that have bounding boxes
*           that intersect the given area.  It is possible for a non-axial bmodel
*           to be returned that doesn't actually intersect the area on an exact
*           test.
*   @todo: Does this always return the world?
*   @return The number of pointers filled in.
**/
const int32_t SV_AreaEdicts(const Vector3 *mins, const Vector3 *maxs,
                  sv_edict_t **list, const int32_t maxcount, const int32_t areatype)
{
    sector_mins = *mins;
    sector_maxs = *maxs;
    sector_list = list;
    sector_count = 0;
    sector_maxcount = maxcount;
    sector_type = areatype;

    SV_AreaEdicts_r(sv_sectorNodes);

    return sector_count;
}


//===========================================================================

/**
*	@return	A headNode that can be used for testing and/or clipping an
*			object 'hull' of mins/maxs size for the entity's said 'solid'.
**/
static mnode_t *SV_HullForEntity( const sv_edict_t *ent, const bool includeSolidTriggers = false ) {
    if ( ent->solid == SOLID_BSP || ( includeSolidTriggers && ent->solid == SOLID_TRIGGER ) ){
		// Subtract 1 to get the modelindex into a 0-based array.
		// ( Index 0 is reserved for no model )
        const int32_t i = ent->s.modelindex - 1;
        
        //// account for "hole" in configstring namespace
        //if ( i >= MODELINDEX_PLAYER && sv.cm.cache->nummodels >= MODELINDEX_PLAYER )
        //    i--;

        // Explicit hulls in the BSP model only.
        if ( i <= 0 || i >= sv.cm.cache->nummodels ) {
            if ( !includeSolidTriggers ) {
                Com_Error( ERR_DROP, "%s: inline model %d out of range", __func__, i );
                return nullptr;
            }
        }

		// Return the headnode for the model.
        return sv.cm.cache->models[i].headnode;
    }

    // Create a temp hull from entity bounds and contents clipMask for the specific type of 'solid'.
    if ( ent->solid == SOLID_BOUNDS_OCTAGON ) {
        return CM_HeadnodeForOctagon( &sv.cm, &ent->mins.x, &ent->maxs.x, ent->hullContents );
    } else {
        return CM_HeadnodeForBox( &sv.cm, &ent->mins.x, &ent->maxs.x, ent->hullContents );
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
const cm_contents_t SV_PointContents( const Vector3 *p ) {
    sv_edict_t *hit = nullptr;

    // Use static thread_local to avoid large stack allocation and ensure thread safety.
    static /*thread_local*/ sv_edict_t *touchedEdicts[MAX_EDICTS] = {};
    std::fill( std::begin( touchedEdicts ), std::end( touchedEdicts ), nullptr );

	if ( !sv.cm.cache ) {
		Com_Error( ERR_DROP, "%s: no map loaded", __func__ );
	}

	if ( !p ) {
		return CONTENTS_NONE;
	}

	// get base contents from world
	cm_contents_t contents = ( CM_PointContents( &sv.cm, &p->x, SV_WorldNodes() ) );

	// or in contents from all the other entities
	const int32_t num = SV_AreaEdicts( p, p, touchedEdicts, MAX_EDICTS, AREA_SOLID );

	for ( int32_t i = 0; i < num; i++ ) {
		// Get edict pointer.
        sv_edict_t *hit = touchedEdicts[ i ];

        // Skip if nullptr;
        if ( hit == nullptr ) {
            continue;
		}

        // Skip client entities if their SVF_PLAYER flag is set.
        if ( hit->s.number <= sv_maxclients->integer && ( hit->svFlags & SVF_PLAYER ) ) {
            continue;
        }

		// Might intersect, so do an exact clip.
		contents = ( contents | CM_TransformedPointContents( &sv.cm, &p->x, SV_HullForEntity(  hit ),
												&hit->currentOrigin.x, &hit->currentAngles.x ) );
	}

	return contents;
}

/**
*	@brief	SV_ClipMoveToEntities
**/
static void SV_ClipMoveToEntities(const Vector3 &start, const Vector3 *mins,
                                  const Vector3 *maxs, const Vector3 &end,
                                  const Vector3 &moveMins, const Vector3 &moveMaxs,
                                  const sv_edict_t *passedict, const cm_contents_t contentmask, cm_trace_t *dst )
{
    sv_edict_t *touch = nullptr;

		// Use static thread_local to avoid large stack allocation and ensure thread safety.
	static /*thread_local*/ sv_edict_t *touchlist[ MAX_EDICTS ] = {};
	std::fill( std::begin( touchlist ), std::end( touchlist ), nullptr );

    // Query potentially touching entities using the overall move bounds
	const int32_t num = SV_AreaEdicts( &moveMins, &moveMaxs, touchlist, MAX_EDICTS, AREA_SOLID );

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for ( int32_t i = 0; i < num; i++ ) {
        // Use a fresh trace per-entity to avoid stale results influencing others
        cm_trace_t etrace = {
            .entityNumber = ENTITYNUM_NONE,
            .fraction = 1.0,
            .endpos = end,
            .plane = {
                .normal = { 0.0f, 0.0f, 0.0f },
                .dist = 0.0f,
                .type = PLANE_NON_AXIAL,
                .signbits = 0,
            },

            .surface = &nulltexinfo.c,
            .material = &cm_default_material,

            .plane2 = {
                .normal = { 0.0f, 0.0f, 0.0f },
                .dist = 0.0f,
                .type = PLANE_NON_AXIAL,
                .signbits = 0,
            },
        };
        // early out if we already know everything is solid from previous world or entity hits
        if ( dst->allsolid ) {
            return;
        }

        touch = touchlist[ i ];
        if ( touch == nullptr ) {
            continue;
        }
        if ( touch->solid == SOLID_NOT ) {
            continue;
        }
        if ( touch == passedict ) {
            continue;
        }
        //		if ( dst->allsolid ) {
        //		    return;
        //		}
        if ( passedict ) {
            if ( touch->owner == passedict )
                continue;    // Don't clip against own missiles.
            if ( passedict->owner == touch )
                continue;    // Don't clip against owner.
        }

		//      if ( !(contentmask & touch->hullContents ) ) {
		//          continue;
		//		}

        if ( !( contentmask & CONTENTS_DEADMONSTER )
            && ( touch->svFlags & SVF_DEADENTITY ) ) {
            continue;
        }

        if ( !( contentmask & CONTENTS_PROJECTILE )
            && ( touch->svFlags & SVF_PROJECTILE ) ) {
            continue;
        }
        if ( !( contentmask & CONTENTS_PLAYER ) 
            && ( touch->svFlags & SVF_PLAYER ) ) {
            continue;
        }

        // might intersect, so do an exact clip
        CM_TransformedBoxTrace( &sv.cm, &etrace, start, end, mins, maxs,
                               SV_HullForEntity(touch), contentmask,
                               &touch->currentOrigin.x, &touch->currentAngles.x);

        //CM_ClipEntity( &sv.cm, dst, &trace, touch->s.number );

        if ( etrace.allsolid ) {
            dst->allsolid = true;
            etrace.entityNumber = touch->s.number;
        } else if ( etrace.startsolid ) {
            dst->startsolid = true;
            etrace.entityNumber = touch->s.number;
        }

        if ( etrace.fraction < dst->fraction ) {
            // make sure we keep a startsolid from a previous trace
            const int32_t oldStartSolid = dst->startsolid;
            etrace.entityNumber = touch->s.number;
            *dst = etrace;
            // jmarshall
            const int32_t startsolid = (int32_t)dst->startsolid | oldStartSolid;
            dst->startsolid = (bool)startsolid;
        }
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
const cm_trace_t q_gameabi SV_Trace( const Vector3 &start, const Vector3 *mins,
                           const Vector3 *maxs, const Vector3 &end,
                           const edict_ptr_t *passEdict, const cm_contents_t contentmask)
{
	// Initialize to no collision for the initial trace.
    cm_trace_t trace = {
        .entityNumber = ENTITYNUM_NONE,
        .fraction = 1.0,
        .plane = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },

        .surface = &nulltexinfo.c,
        .material = &cm_default_material,

        .plane2 = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },
    };

    if (!sv.cm.cache) {
        Com_Error(ERR_DROP, "%s: no map loaded", __func__);
    }

    // Validate mins and maxs first, otherwise assign zero extents
    if ( !mins ) {
        mins = &qm_vector3_null;
    }
    if ( !maxs ) {
        maxs = &qm_vector3_null;
    }

    // First Clip to world.
    CM_BoxTrace( 
        &sv.cm, &trace, 
        start, end, mins, maxs,
        SV_WorldNodes( ), 
        contentmask 
    );

    // Mark world hit if applicable but do not early-return; we still need to consider entities
    trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

    // create the bounding box of the entire move
    Vector3 moveMins = {};
    Vector3 moveMaxs = {};
    for ( int32_t i = 0; i < 3; i++ ) {
        if ( end[ i ] > start[ i ] ) {
            moveMins[ i ] = start[ i ] + ( *mins )[ i ] - 1;
            moveMaxs[ i ] = end[ i ] + ( *maxs )[ i ] + 1;
        } else {
            moveMins[ i ] = end[ i ] + ( *mins )[ i ] - 1;
            moveMaxs[ i ] = start[ i ] + ( *maxs )[ i ] + 1;
        }
    }

	// If we are not clipping to the world, and the trace fraction is 1.0,
    // test and clip to other solid entities.
    SV_ClipMoveToEntities(
        start, mins, maxs, end, 
        moveMins, moveMaxs,
        passEdict,
        contentmask, 
        &trace
    );

    return trace;
}

/**
*	@brief	Like SV_Trace(), but clip to specified entity only.
*			Can be used to clip to SOLID_TRIGGER by its BSP tree.
**/
const cm_trace_t q_gameabi SV_Clip( const edict_ptr_t *clipEntity, const Vector3 &start, const Vector3 *mins,
                            const Vector3 *maxs, const Vector3 &end,
                            const cm_contents_t contentmask ) {
    // Initialize to no collision for the initial trace.
    cm_trace_t trace = {
        .entityNumber = ENTITYNUM_NONE,
        .fraction = 1.0,
        .plane = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },

        .surface = &nulltexinfo.c,
        .material = &cm_default_material,

        .plane2 = {
            .normal = { 0.0f, 0.0f, 0.0f },
            .dist = 0.0f,
            .type = PLANE_NON_AXIAL,
            .signbits = 0,
        },
    };

    if ( sv.cm.cache ) {
        // Clip against World:
        if ( clipEntity == nullptr || ( clipEntity && clipEntity->s.number == ENTITYNUM_WORLD ) ) {
            CM_BoxTrace( &sv.cm, &trace, start, end, mins, maxs, sv.cm.cache->nodes, contentmask );
            // Clip against clipEntity.
        } else {
            mnode_t *headNode = SV_HullForEntity( clipEntity );

            // Perform clip.
            if ( headNode != nullptr ) {
                CM_TransformedBoxTrace( &sv.cm, &trace, start, end, mins, maxs, headNode, contentmask,
                    &clipEntity->currentOrigin.x, &clipEntity->currentAngles.x );

                if ( trace.fraction < 1. ) {
                    trace.entityNumber = clipEntity->s.number;
                }
            }
        }
    }
	return trace;
}
