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

#include "common/math.h"

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

//! Guards world sector area lists from concurrent readers/writers.
static std::shared_mutex  sv_world_area_mutex;

/**
*   @brief  Checks whether a list node belongs to a known area list.
*   @note   Used to avoid dereferencing obviously corrupted pointers during unlink.
**/
static bool SV_IsKnownAreaListNode( const list_t *node ) {
    if ( !node ) {
        return false;
    }

    for ( int32_t i = 0; i < sv_numSectorNodes; i++ ) {
        if ( node == &sv_sectorNodes[ i ].trigger_edicts || node == &sv_sectorNodes[ i ].solid_edicts ) {
            return true;
        }
    }

    if ( ge && ge->edictPool && ge->edictPool->edicts ) {
        for ( int32_t i = 0; i < ge->edictPool->max_edicts; i++ ) {
            sv_edict_t *edict = ge->edictPool->edicts[ i ];
            if ( edict && node == &edict->area ) {
                return true;
            }
        }
    }

    return false;
}

/**
*   @brief  Per-query scratch state for `SV_AreaEdicts`.
*   @note   Keeping this state on the caller stack avoids the previous shared file-scope
*           area-query scratch that raced between concurrent readers.
**/
struct sv_area_edicts_context_t {
    //! Query-space minimum bounds.
    Vector3 mins = {};
    //! Query-space maximum bounds.
    Vector3 maxs = {};
    //! Caller-provided output list.
    sv_edict_t **list = nullptr;
    //! Current output count.
    int32_t count = 0;
    //! Maximum amount of entities the caller can receive.
    int32_t maxcount = 0;
    //! Requested area list type.
    int32_t type = AREA_SOLID;
};



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
    /**
    *   Serialize world-sector teardown and rebuild so concurrent traces or area queries
    *   never observe partially reset lists or node topology.
    **/
    std::unique_lock<std::shared_mutex> lock( sv_world_area_mutex );

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
        // Reset entity and world area linking data.
        if ( ent != nullptr ) {
            // Unlink.
            ent->isLinked = false;
            //ent->area.prev = ent->area.next = NULL;
            ent->area = { .next = nullptr, .prev = nullptr };
            // Reset entity cluster data.
            ent->numberOfClusters = 0;
            ent->headNode = -1;

            // Reset entity (state-) data for non clients.
            if ( ent->client == nullptr ) {
                ent->Reset();
            }
        }

        // Reset entity world area linking data.
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
void SV_LinkEdict( cm_t *cm, sv_edict_t *ent ) {
	mleaf_t *leafs[ MAX_TOTAL_ENT_LEAFS ];
	int32_t clusters[ MAX_TOTAL_ENT_LEAFS ];
	int32_t num_leafs;
	int32_t i, j;
	int32_t area;
	mnode_t *topnode;

	std::unique_lock<std::shared_mutex> lock( sv_world_area_mutex );

	if ( !cm || !cm->cache ) {
		return;
	}

	if ( !ent ) {
		Com_WPrintf( "(nullptr) entity\n" );
		return;
	}


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

	// <Q2RTXP>: TODO: Review this.
	#if 0
	// If none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if ( !num_leafs ) {
		return;
	}
	#endif

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
	// Lock world area data for the duration of this unlink, so concurrent traces or area queries never observe partially unlinked entities.
	std::unique_lock<std::shared_mutex> lock( sv_world_area_mutex );

	if ( !ent ) {
		Com_WPrintf( "(nullptr) entity\n" );
		return;
	}

	list_t *prev = ent->area.prev;
	list_t *next = ent->area.next;

	// Already unlinked, or reset entity that was never linked.
    if ( !ent->isLinked ) {
		return;
	}

    // Partial or obviously corrupted link state.
    if ( !prev || !next ) {
        Com_WPrintf( "%s: entity %d has incomplete area links\n", __func__, NUMBER_OF_EDICT( ent ) );
        return;
    }
	#if 0
    // Avoid dereferencing pointers that are not part of the known area list topology.
    if ( !SV_IsKnownAreaListNode( prev ) || !SV_IsKnownAreaListNode( next ) ) {
        Com_WPrintf( "%s: entity %d has corrupted area links prev=%p next=%p\n", __func__, NUMBER_OF_EDICT( ent ), (void *)prev, (void *)next );
        return;
    }
	#endif
    // Sanity check the surrounding list topology before unlinking.
    if ( prev->next != &ent->area || next->prev != &ent->area ) {
        Com_WPrintf( "%s: entity %d area list integrity check failed\n", __func__, NUMBER_OF_EDICT( ent ) );
        return;
    }

	// Mark linked status as false.
	ent->isLinked = false;
	// Remove from area.
	List_Remove( &ent->area ); 
	//List_Unlink( prev, next );
	// Clear area links.
    ent->area.prev = ent->area.next = nullptr;
}

/**
*   @brief  Initialize the structural topology of a synthetic bounding-box hull.
*   @param  hull    Hull storage to initialize.
*   @note   This only wires up planes, nodes, and leaf/brush ownership. Per-trace mins/maxs and plane distances are assigned later.
**/
static void InitBoxHullState( hull_boundingbox_t *hull ) {
	/**
	*   Sanity check: require destination storage before wiring the synthetic hull.
	**/
	if ( !hull ) {
		return;
	}

	/**
	*   Reset the hull storage so every plane, node, and leaf starts from a deterministic baseline.
	**/
	*hull = {};

	/**
	*   Initialize the root node, brush, and leaf ownership for the synthetic hull tree.
	**/
	hull->headnode = &hull->nodes[ 0 ];
	hull->brush.numsides = 6;
	hull->brush.firstbrushside = &hull->brushsides[ 0 ];
	hull->brush.contents = CONTENTS_MONSTER;
	hull->leaf.firstleafbrush = &hull->leafbrush;
	hull->leaf.numleafbrushes = 1;
	hull->leaf.contents = CONTENTS_MONSTER;
	hull->leafbrush = &hull->brush;

	/**
	*   Wire the six axial brush sides and their clip nodes into one small BSP chain.
	**/
	for ( int32_t i = 0; i < 6; i++ ) {
		// Determine which side of the split points at the empty leaf.
		const int32_t side = i & 1;

		// Bind the brush side to the corresponding plane and null texture info.
		mbrushside_t *brushSide = &hull->brushsides[ i ];
		brushSide->plane = &hull->planes[ i * 2 + side ];
		brushSide->texinfo = &nulltexinfo;

		// Chain clip nodes together until the final solid leaf is reached.
		mnode_t *clipNode = &hull->nodes[ i ];
		clipNode->plane = &hull->planes[ i * 2 ];
		clipNode->children[ side ] = ( mnode_t * )&hull->emptyleaf;
		if ( i != 5 ) {
			clipNode->children[ side ^ 1 ] = &hull->nodes[ i + 1 ];
		} else {
			clipNode->children[ side ^ 1 ] = ( mnode_t * )&hull->leaf;
		}

		// Initialize the positive-facing axial plane for this axis.
		cm_plane_t *plane = &hull->planes[ i * 2 ];
		plane->normal[ 0 ] = 0.0f;
		plane->normal[ 1 ] = 0.0f;
		plane->normal[ 2 ] = 0.0f;
		plane->normal[ i >> 1 ] = 1.0f;
		SetPlaneType( plane );
		SetPlaneSignbits( plane );

		// Initialize the negative-facing companion axial plane for this axis.
		plane = &hull->planes[ i * 2 + 1 ];
		plane->normal[ 0 ] = 0.0f;
		plane->normal[ 1 ] = 0.0f;
		plane->normal[ 2 ] = 0.0f;
		plane->normal[ i >> 1 ] = -1.0f;
		SetPlaneType( plane );
		SetPlaneSignbits( plane );
	}
}

/**
*   @brief  To keep everything totally uniform, bounding boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
static mnode_t *ResizeEntityHullForBox( cm_t *cm, hull_boundingbox_t *hull, const Vector3 &mins, const Vector3 &maxs, const cm_contents_t contents ) {
	if ( !cm || !cm->cache ) {
		return nullptr;
	}

	if ( !hull->headnode ) {
		InitBoxHullState( hull );
	}
	
	// Setup to CONTENTS_MONSTER in case of no contents being passed in.
	if ( contents == CONTENTS_NONE ) {
		hull->leaf.contents = hull->brush.contents = CONTENTS_MONSTER;
	} else {
		hull->leaf.contents = hull->brush.contents = contents;
	}

	// Setup its bounding boxes.
	VectorCopy( mins, hull->headnode->mins );
	VectorCopy( maxs, hull->headnode->maxs );
	VectorCopy( mins, hull->leaf.mins );
	VectorCopy( maxs, hull->leaf.maxs );

	// Setup planes.
	hull->planes[ 0 ].dist = maxs[ 0 ];
	hull->planes[ 1 ].dist = -maxs[ 0 ];
	hull->planes[ 2 ].dist = mins[ 0 ];
	hull->planes[ 3 ].dist = -mins[ 0 ];
	hull->planes[ 4 ].dist = maxs[ 1 ];
	hull->planes[ 5 ].dist = -maxs[ 1 ];
	hull->planes[ 6 ].dist = mins[ 1 ];
	hull->planes[ 7 ].dist = -mins[ 1 ];
	hull->planes[ 8 ].dist = maxs[ 2 ];
	hull->planes[ 9 ].dist = -maxs[ 2 ];
	hull->planes[ 10 ].dist = mins[ 2 ];
	hull->planes[ 11 ].dist = -mins[ 2 ];

	// Return boundingbox' headnode pointer.
	return hull->headnode;
}


/**
*   @brief  Needs to be called any time an entity changes origin, mins, maxs,
*			clipMask, model, hullContents, owner, 
*           or solid.  Automatically unlinks if needed.
*           sets ent->v.absMin and ent->v.absMax
*           sets ent->leafnums[] for pvs determination even if the entity.
*           is not solid.
**/
void PF_LinkEdict( edict_ptr_t *ent ) {
    if ( !ent ) {
        Com_Error( ERR_DROP, "%s: (nullptr) edict_t pointer\n", __func__ );
    }

    // If it was ever linked, let the unlink path validate and clear any stale topology first.
    // This catches restored entities where `isLinked` survived but the serialized area list did not.
    if ( ent->isLinked || ent->area.prev || ent->area.next ) {
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
	const int32_t entnum = NUMBER_OF_EDICT( ent );
	// The entity number needs to be sanitized.
	if ( entnum < 0 || entnum >= ge->edictPool->max_edicts ) {
		Com_Error( ERR_DROP, "%s: edict_t %d with invalid entity number\n", __func__, entnum );
		return;
	}
	// Specific server entity data pointer.
	server_entity_t *sent = &sv.entities[ entnum ];

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
    case SOLID_CAPSULE:
    case SOLID_CYLINDER:
    case SOLID_SPHERE:
        if ( ( ent->svFlags & SVF_DEADENTITY ) || VectorCompare( ent->mins, ent->maxs ) ) {
            ent->s.solid = SOLID_NOT;
            sent->solid32 = SOLID_NOT;
        } else {
            ent->s.solid = static_cast<cm_solid_t>( sent->solid32 = ent->solid );
            ent->s.bounds = static_cast<uint32_t>( _MSG_PackBoundsUint32( ent->mins, ent->maxs ).u );
        }
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
        ent->s.ownerNumber = ENTITYNUM_NONE;
    }
	// Setup the collision model bounds in a thread-safe lock-free manner during link
    if ( ent->solid == SOLID_BOUNDS_OCTAGON ) {
        CM_SetupOctagonBoxHull( &sent->hullOctagonBox, &ent->mins.x, &ent->maxs.x, ent->hullContents );
        // Give the synthetic hull a stable entity-linked brush ID so collision traces can preserve it.
        sent->hullOctagonBox.brush.brushID = -static_cast< int32_t >( ent->s.number + 1 );
    } else if ( ent->solid == SOLID_BOUNDS_BOX ) {
        CM_SetupBoxHull( &sent->hullBoundingBox, &ent->mins.x, &ent->maxs.x, ent->hullContents );
        // Give the synthetic hull a stable entity-linked brush ID so collision traces can preserve it.
        sent->hullBoundingBox.brush.brushID = -static_cast< int32_t >( ent->s.number + 1 );
    }

    // Link edit in.
    SV_LinkEdict(&sv.cm, ent);

    // If its the entity's first time, make sure old_origin is valid, unless a BEAM which handles it by itself.
    if ( !ent->linkCount ) {
        if ( ent->s.entityType != ET_BEAM && !( ent->s.renderfx & RF_BEAM ) ) {
            VectorCopy( ent->currentOrigin, ent->s.old_origin );
        }
    }

    // Increment link count.
    ent->linkCount++;

    // Solid NOT won't have any contents either.
    if ( ent->solid == SOLID_NOT ) {
        ent->s.hullContents = CONTENTS_NONE;
        ent->isLinked = false;
        return;
    }

	// Mark linked status as true.
	ent->isLinked = true;// ( ent->area.prev != nullptr ? true : false );

    // Find the first node that the ent's box crosses.
	worldSector_t *node = sv_sectorNodes;
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
static void SV_AreaEdicts_r( worldSector_t *node, sv_area_edicts_context_t &context ) {
    list_t      *start = nullptr;
    sv_edict_t  *check = nullptr;

    /**
    *   Sanity check: malformed or not-yet-built sector trees have nothing to enumerate.
    **/
    if ( !node ) {
        return;
    }

    // touch linked edicts
    if ( context.type == AREA_SOLID ) {
        start = &node->solid_edicts;
    } else {
        start = &node->trigger_edicts;
    }

    LIST_FOR_EACH( sv_edict_t, check, start, area ) {
		if ( check == nullptr || check->inUse == false ) {
			continue;
		}
        if ( check->solid == SOLID_NOT ) {
            continue;        // deactivated
        }
        if ( check->absMin[ 0 ] > context.maxs[ 0 ]
            || check->absMin[ 1 ] > context.maxs[ 1 ]
            || check->absMin[ 2 ] > context.maxs[ 2 ]
            || check->absMax[ 0 ] < context.mins[ 0 ]
            || check->absMax[ 1 ] < context.mins[ 1 ]
            || check->absMax[ 2 ] < context.mins[ 2 ] ) {
            continue;        // not touching
        }

        if ( context.count == context.maxcount ) {
            Com_WPrintf( "SV_AreaEdicts: MAXCOUNT\n" );
            return;
        }

        context.list[ context.count ] = check;
        context.count++;
    }

    // Terminal node!
    if ( node->axis == -1 ) {
        return;
    }

    // recurse down both sides
    if ( context.maxs[ node->axis ] > node->dist ) {
        SV_AreaEdicts_r( node->children[ 0 ], context );
    }
    if ( context.mins[ node->axis ] < node->dist ) {
        SV_AreaEdicts_r( node->children[ 1 ], context );
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
	std::shared_lock<std::shared_mutex> read_lock( sv_world_area_mutex );

    /**
    *   Reject invalid caller arguments and the no-world case before touching sector lists.
    **/
    if ( !mins || !maxs || !list || maxcount <= 0 || sv_numSectorNodes <= 0 ) {
        return 0;
    }

    /**
    *   Keep all per-query state local so multiple readers can safely traverse the sector tree concurrently.
    **/
    sv_area_edicts_context_t context = {};
    context.mins = *mins;
    context.maxs = *maxs;
    context.list = list;
    context.count = 0;
    context.maxcount = maxcount;
    context.type = areatype;

    SV_AreaEdicts_r( sv_sectorNodes, context );

    return context.count;
}


//===========================================================================


const mbrush_t *PF_GetBrushByID( const int32_t brushID ) {
	// Brushes with ID `0` are designated spoecifically as "no brush" in the protocol, 
	// so we need to guard against that here to avoid indexing the brush array with an out-of-range ID.
	if ( brushID == 0 ) {
		return nullptr;
	}

	// For world-model and inline-brush based entities, the brush ID is a 1-based index into the brush array of the world model's BSP data, 
	// so we need to subtract 1 to get the correct 0-based index.
	if ( brushID > 0 ) {
		// Brush IDs are 1-based indices into the brush array, so subtract 1 to get the correct 0-based index.
		//return (mbrush_t*)( sv.cm.cache->leafbrushes + ( brushID - 1 ) );

		// Subtract 1 to get the modelindex into a 0-based array.
		// ( Index 0 is reserved for no model )
		return &sv.cm.cache->brushes[ brushID - 1 ];
	} else {
		// For entity-based brushes, the brush ID is a negative value that is used to index into the entity's brush array.
		// Return the pre-computed temp hull from the entity which is thread-safely updated during SV_LinkEdict.
		server_entity_t *sent = &sv.entities[ abs( brushID ) - 1 ];
		if ( sent->solid32 == SOLID_BOUNDS_OCTAGON ) {
			return &sent->hullOctagonBox.brush;
		} else if ( sent->solid32 == SOLID_BOUNDS_BOX ) {
			return &sent->hullBoundingBox.brush;
		} else {
			return nullptr;
		}
	}	
}

/**
*	@return	A headNode that can be used for testing and/or clipping an
*			object 'hull' of mins/maxs size for the entity's said 'solid'.
**/
static mnode_t *SV_HullForEntity( const sv_edict_t *ent, const bool includeSolidTriggers = false ) {
    /**
    *   Sanity checks: a missing entity or unloaded collision model cannot provide a trace hull.
    **/
    if ( !ent || !sv.cm.cache ) {
        return nullptr;
    }

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

            /**
            *   Solid-trigger probes must also reject invalid model indices instead of indexing beyond
            *   the cached inline-model array.
            **/
            return nullptr;
        }

        // Return the headnode for the model.
        return sv.cm.cache->models[i].headnode;
    }

    // Return the pre-computed temp hull from the entity which is thread-safely updated during SV_LinkEdict.
	server_entity_t *sent = &sv.entities[ ent->s.number ];
    if ( ent->solid == SOLID_BOUNDS_OCTAGON ) {
        return sent->hullOctagonBox.headnode;
    } else {
        return sent->hullBoundingBox.headnode;
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
    static thread_local sv_edict_t *touchedEdicts[MAX_EDICTS] = {};
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
static cm_trace_t SV_ClipMoveToEntities(const Vector3 &start, const Vector3 *mins,
                                  const Vector3 *maxs, const Vector3 &end,
                                  const Vector3 &moveMins, const Vector3 &moveMaxs,
                                  const sv_edict_t *passedict, const cm_contents_t contentmask )
{
    sv_edict_t *touch = nullptr;
	cm_trace_t dst = {
		.entityNumber = ENTITYNUM_NONE,
		.fraction = 1.0f,
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

	// Use static thread_local to avoid large stack allocation and ensure thread safety.
	static thread_local sv_edict_t *touchlist[ MAX_EDICTS ] = {};
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
        if ( dst.allsolid ) {
            return dst;
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

        cm_trace_shape_t touchShape = { .type = SHAPE_AABB };
        if ( touch->solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
        else if ( touch->solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
        else if ( touch->solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

        if ( touchShape.type != SHAPE_AABB ) {
            cm_trace_shape_t shapeA = { .type = SHAPE_AABB };
            Vector3 centerStart = start;
            Vector3 centerEnd = end;
            Vector3 offset = { 0.0f, 0.0f, 0.0f };

            if ( mins->x == 0.0f && mins->y == 0.0f && mins->z == 0.0f &&
                 maxs->x == 0.0f && maxs->y == 0.0f && maxs->z == 0.0f ) {
                shapeA.type = SHAPE_POINT;
                shapeA.radius = 0.0f;
                shapeA.halfHeight = 0.0f;
            } else {
                shapeA.extents = Vector3( (maxs->x - mins->x) * 0.5f, (maxs->y - mins->y) * 0.5f, (maxs->z - mins->z) * 0.5f );
                shapeA.radius = std::max(shapeA.extents.x, shapeA.extents.y);
                shapeA.halfHeight = shapeA.extents.z;

                offset.x = (mins->x + maxs->x) * 0.5f;
                offset.y = (mins->y + maxs->y) * 0.5f;
                offset.z = (mins->z + maxs->z) * 0.5f;

                centerStart.x += offset.x; centerStart.y += offset.y; centerStart.z += offset.z;
                centerEnd.x += offset.x; centerEnd.y += offset.y; centerEnd.z += offset.z;
            }

            touchShape.extents = Vector3( (touch->maxs.x - touch->mins.x) * 0.5f, (touch->maxs.y - touch->mins.y) * 0.5f, (touch->maxs.z - touch->mins.z) * 0.5f );
            touchShape.radius = std::max(touchShape.extents.x, touchShape.extents.y);
            touchShape.halfHeight = touchShape.extents.z;

            // SV_AnalyticalShapeSweep expects `touch->currentOrigin` to be the center.
            // We temporarily adjust it, or better yet, we should fix SV_AnalyticalShapeSweep.
            // But since we can't easily change touch, we pass the adjusted touch center as a new argument?
            // Wait, SV_AnalyticalShapeSweep reads `touch->currentOrigin`.
            // We can just temporarily shift touch->currentOrigin!
            Vector3 originalOrigin = touch->currentOrigin;
            touch->currentOrigin.x += (touch->mins.x + touch->maxs.x) * 0.5f;
            touch->currentOrigin.y += (touch->mins.y + touch->maxs.y) * 0.5f;
            touch->currentOrigin.z += (touch->mins.z + touch->maxs.z) * 0.5f;

            etrace = CM_AnalyticalShapeSweep( centerStart, shapeA, centerEnd, touch->currentOrigin, touchShape );
            
            touch->currentOrigin = originalOrigin;

            // Recalculate endpos correctly
            if ( etrace.fraction < 1.0f ) {
                etrace.endpos.x = start.x + (end.x - start.x) * etrace.fraction;
                etrace.endpos.y = start.y + (end.y - start.y) * etrace.fraction;
                etrace.endpos.z = start.z + (end.z - start.z) * etrace.fraction;
                etrace.contents = CONTENTS_SOLID;
            } else {
                etrace.endpos = end;
            }
        } else {
            // might intersect, so do an exact clip
            etrace = CM_TransformedBoxTrace( &sv.cm, start, end, mins, maxs,
                                   SV_HullForEntity(touch), contentmask,
                                   &touch->currentOrigin.x, &touch->currentAngles.x);
        }
        //CM_ClipEntity( &sv.cm, dst, &trace, touch->s.number );

        if ( etrace.allsolid ) {
            dst.allsolid = true;
            etrace.entityNumber = touch->s.number;
            dst.brushID = etrace.brushID;
        } else if ( etrace.startsolid ) {
            dst.startsolid = true;
            etrace.entityNumber = touch->s.number;
            dst.brushID = etrace.brushID;
        }

        if ( etrace.fraction < dst.fraction ) {
            // make sure we keep a startsolid from a previous trace
            const int32_t oldStartSolid = dst.startsolid;
            etrace.entityNumber = touch->s.number;
            dst = etrace;

            const int32_t startsolid = (int32_t)dst.startsolid | oldStartSolid;
            dst.startsolid = (bool)startsolid;
        }
    }

	return dst;
}

/**
*	@brief	Clip a moving shape against nearby entities.
*	@param	start		World-space sweep start.
*	@param	shape		Moving shape at the sweep origin.
*	@param	end			World-space sweep end.
*	@param	moveMins	Expanded area-query mins.
*	@param	moveMaxs	Expanded area-query maxs.
*	@param	passedict	Entity to exclude from clipping.
*	@param	contentmask	Brush contents mask to test against.
*	@return	Best trace result against touched entities.
*	@note	When both shapes are non-AABB, the function performs an analytical sweep
*			in center-space and temporarily shifts the touched entity origin to match.
**/
static cm_trace_t SV_ClipMoveToEntitiesShape( const Vector3 &start, const cm_trace_shape_t &shape,
	const Vector3 &end,
	const Vector3 &moveMins, const Vector3 &moveMaxs,
	const sv_edict_t *passedict, const cm_contents_t contentmask ) {
	sv_edict_t *touch = nullptr;
	cm_trace_t dst = {
		.entityNumber = ENTITYNUM_NONE,
		.fraction = 1.0f,
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

	/**
	*	Reuse a thread-local touch buffer so entity clipping does not allocate
	*	on every trace and can safely collect the nearby solid entities.
	**/
	static thread_local sv_edict_t *touchlist[ MAX_EDICTS ] = {};
	std::fill( std::begin( touchlist ), std::end( touchlist ), nullptr );

	/**
	*	Query the broadphase for all solid entities overlapping the swept bounds.
	*	This reduces the expensive shape-vs-entity tests to only likely contacts.
	**/
	const int32_t num = SV_AreaEdicts( &moveMins, &moveMaxs, touchlist, MAX_EDICTS, AREA_SOLID );

	/**
	*	Test the moving shape against each touched entity and keep the earliest hit.
	**/
	for ( int32_t i = 0; i < num; i++ ) {
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

		/**
		*	Once a guaranteed full-solid result is reached, no later entity can
		*	produce a better trace, so return immediately.
		**/
		if ( dst.allsolid ) {
			return dst;
		}

		// Fetch the current candidate from the broadphase touch list.
		touch = touchlist[ i ];
		// Skip invalid, non-solid, or explicitly excluded entities.
		if ( touch == nullptr || touch->solid == SOLID_NOT || touch == passedict ) {
			continue;
		}

		/**
		*	Respect pass-through ownership relationships so self-owned entities do
		*	not clip against each other during movement traces.
		**/
		if ( passedict ) {
			if ( touch->owner == passedict )
				continue;
			if ( passedict->owner == touch )
				continue;
		}

		/**
		*	Filter special entity categories unless the caller explicitly asked
		*	for them through the contents mask.
		**/
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

		/**
		*	Build the touched entity's collision shape so we can choose the
		*	appropriate sweep path for the entity's solid type.
		**/
		cm_trace_shape_t touchShape = { .type = SHAPE_AABB };
		if ( touch->solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
		else if ( touch->solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
		else if ( touch->solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

		/**
		*	Non-AABB solids use the analytical sweep path so their center-space
		*	shape parameters can be compared directly against the moving shape.
		**/
		if ( touchShape.type != SHAPE_AABB ) {
			touchShape.extents = Vector3( ( touch->maxs.x - touch->mins.x ) * 0.5f, ( touch->maxs.y - touch->mins.y ) * 0.5f, ( touch->maxs.z - touch->mins.z ) * 0.5f );
			touchShape.radius = std::max( touchShape.extents.x, touchShape.extents.y );
			touchShape.halfHeight = touchShape.extents.z;

			// Temporarily shift the entity origin to its brush/shape center for the sweep.
			Vector3 originalOrigin = touch->currentOrigin;
			touch->currentOrigin.x += ( touch->mins.x + touch->maxs.x ) * 0.5f;
			touch->currentOrigin.y += ( touch->mins.y + touch->maxs.y ) * 0.5f;
			touch->currentOrigin.z += ( touch->mins.z + touch->maxs.z ) * 0.5f;

			// Sweep the moving shape against the analytical representation of the target.
			etrace = CM_AnalyticalShapeSweep( start, shape, end, touch->currentOrigin, touchShape );

			// Restore the original entity origin before continuing to the next candidate.
			touch->currentOrigin = originalOrigin;

			/**
			*	Analytical sweeps may not fill all entity metadata, so normalize the
			*	result here before it is compared against the current best trace.
			**/
			if ( etrace.fraction < 1.0f ) {
				// Preserve the touched entity identity for downstream trace consumers.
				etrace.entityNumber = touch->s.number;
				etrace.brushID = -static_cast< int32_t >( touch->s.number + 1 );
				etrace.endpos.x = start.x + ( end.x - start.x ) * etrace.fraction;
				etrace.endpos.y = start.y + ( end.y - start.y ) * etrace.fraction;
				etrace.endpos.z = start.z + ( end.z - start.z ) * etrace.fraction;
				etrace.contents = CONTENTS_SOLID;
			} else {
				// Keep the canonical end position for a non-hit analytical sweep.
				etrace.endpos = end;
			}
		} else {
			/**
			*	AABB entities can use the transformed hull trace directly because
			*	their collision representation already matches the standard trace path.
			**/
			etrace = CM_TransformedShapeTrace( &sv.cm, start, end, shape,
				SV_HullForEntity( touch ), contentmask,
				&touch->currentOrigin.x, &touch->currentAngles.x );
		}

        /**
        *	Stamp entity-backed traces with the touched entity identity so the
        *	caller always sees which entity produced the hit, even when the trace
        *	result is start-solid or all-solid rather than a closer fraction.
        **/
        if ( etrace.allsolid || etrace.startsolid || etrace.fraction < 1.0f ) {
            etrace.entityNumber = touch->s.number;
            etrace.brushID = -static_cast< int32_t >( touch->s.number + 1 );
        }

		/**
		*	Propagate all-solid and start-solid state so the caller can react to
		*	the strongest blocking condition encountered so far.
		**/
		if ( etrace.allsolid ) {
			dst.allsolid = true;
            dst.entityNumber = etrace.entityNumber;
            dst.brushID = etrace.brushID;
		} else if ( etrace.startsolid ) {
			dst.startsolid = true;
            dst.entityNumber = etrace.entityNumber;
            dst.brushID = etrace.brushID;
		}

		/**
		*	Keep the earliest collision fraction while preserving any prior
		*	start-solid state that may have been accumulated from earlier hits.
		**/
		if ( etrace.fraction < dst.fraction ) {
			const int32_t oldStartSolid = dst.startsolid;
			etrace.entityNumber = touch->s.number;
			dst = etrace;

			const int32_t startsolid = ( int32_t )dst.startsolid | oldStartSolid;
			dst.startsolid = ( bool )startsolid;
		}
	}

	return dst;
}

/**
*	@brief	Trace a moving convex shape against the world and active entities.
*	@details	Performs the world trace first, then expands the move bounds and
*			queries entity collisions for the same shape so the final trace can
*			report the earliest blocking hit while preserving start-solid state.
*	@param	start		World-space start position for the sweep.
*	@param	shape		Shape definition to trace with.
*	@param	end			World-space end position for the sweep.
*	@param	passEdict	Optional entity to exclude from clipping checks.
*	@param	contentmask	Contents mask used to filter collision candidates.
*	@return	Combined collision trace against world geometry and entities.
*	@note	Requires an active map; aborts with ERR_DROP if collision data is
*			not loaded.
**/
static const cm_trace_t SV_ShapeTraceInternal( const Vector3 &start, const cm_trace_shape_t &shape,
	const Vector3 &end,
	const edict_ptr_t *passEdict, const cm_contents_t contentmask ) {
/**
*	Initialize the trace to a clean non-hit state so the world trace can
*	populate collision details when a blocking surface is found.
**/
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

	/**
	*	Sanity check: tracing without loaded collision data is a fatal map-state
	*	error, so fail immediately.
	**/
	if ( !sv.cm.cache ) {
		Com_Error( ERR_DROP, "%s: no map loaded", __func__ );
	}

	/**
	*	Trace against the world first so the final result always includes the
	*	static collision model before any entity-level refinement.
	**/
	trace = CM_ShapeTrace(
		&sv.cm,
		start, end, shape,
		SV_WorldNodes(),
		contentmask
	);

	/**
	*	Mark whether the world trace actually touched world geometry.
	**/
	trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

	/**
	*	Build conservative move bounds for entity clipping from the traced shape
	*	extents so nearby entities are considered even when the sweep is diagonal.
	**/
	Vector3 shapeExtents = { shape.radius, shape.radius, shape.halfHeight };
	if ( shape.type == SHAPE_CAPSULE || shape.type == SHAPE_SPHERE ) {
		// Spheres and capsules extend by radius above and below their center.
		shapeExtents.z += shape.radius;
	}

	/**
	*	Compute swept bounds between start and end so entity collision tests can
	*	safely reject out-of-range candidates.
	**/
	Vector3 moveMins = {};
	Vector3 moveMaxs = {};
	for ( int32_t i = 0; i < 3; i++ ) {
		if ( end[ i ] > start[ i ] ) {
			moveMins[ i ] = start[ i ] - shapeExtents[ i ] - 1;
			moveMaxs[ i ] = end[ i ] + shapeExtents[ i ] + 1;
		} else {
			moveMins[ i ] = end[ i ] - shapeExtents[ i ] - 1;
			moveMaxs[ i ] = start[ i ] + shapeExtents[ i ] + 1;
		}
	}

	/**
	*	Trace the same shape against active entities using the conservative move
	*	bounds computed above.
	**/
	const cm_trace_t entityTrace = SV_ClipMoveToEntitiesShape(
		start, shape, end,
		moveMins, moveMaxs,
		passEdict,
		contentmask
	);

	/**
	*	Merge entity results into the world trace while preserving any prior
	*	start-solid/all-solid state accumulated by the world pass.
	**/
	if ( entityTrace.allsolid || entityTrace.fraction < trace.fraction ) {
		const int32_t oldStartSolid = trace.startsolid;
		const int32_t oldAllSolid = trace.allsolid;
		trace = entityTrace;
		trace.startsolid = ( bool )( ( int32_t )trace.startsolid | oldStartSolid );
		trace.allsolid = ( bool )( ( int32_t )trace.allsolid | oldAllSolid );
	} else if ( entityTrace.startsolid ) {
		// Preserve the start-solid condition even when the entity hit is later
		// than the world hit, because the caller still needs that state.
		trace.startsolid = true;
		if ( entityTrace.allsolid ) {
			trace.allsolid = true;
		}
	}

	return trace;
}

/**
*	@brief	Trace a sphere against the world and active entities.
*	@param	start		World-space start position for the sweep.
*	@param	radius		Sphere radius.
*	@param	end			World-space end position for the sweep.
*	@param	passEdict	Optional entity to exclude from clipping checks.
*	@param	contentmask	Contents mask used to filter collision candidates.
*	@return	Combined collision trace for the sphere sweep.
**/
const cm_trace_t q_gameabi SV_TraceSphere( const Vector3 &start, float radius, const Vector3 &end, const edict_ptr_t *passEdict, const cm_contents_t contentmask ) {
	/**
	*	Configure a sphere trace shape and forward to the shared shape tracing
	*	implementation so world/entity handling stays centralized.
	**/
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_SPHERE;
	shape.radius = radius;
	return SV_ShapeTraceInternal( start, shape, end, passEdict, contentmask );
}

/**
*	@brief	Trace a capsule against the world and active entities.
*	@param	start		World-space start position for the sweep.
*	@param	radius		Capsule radius.
*	@param	halfHeight	Capsule half-height excluding the spherical ends.
*	@param	end			World-space end position for the sweep.
*	@param	passEdict	Optional entity to exclude from clipping checks.
*	@param	contentmask	Contents mask used to filter collision candidates.
*	@return	Combined collision trace for the capsule sweep.
**/
const cm_trace_t q_gameabi SV_TraceCapsule( const Vector3 &start, float radius, float halfHeight, const Vector3 &end, const edict_ptr_t *passEdict, const cm_contents_t contentmask ) {
	/**
	*	Configure a capsule trace shape and reuse the shared shape tracing path
	*	so the wrapper stays thin and consistent.
	**/
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CAPSULE;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return SV_ShapeTraceInternal( start, shape, end, passEdict, contentmask );
}

/**
*	@brief	Trace a cylinder against the world and active entities.
*	@param	start		World-space start position for the sweep.
*	@param	radius		Cylinder radius.
*	@param	halfHeight	Cylinder half-height.
*	@param	end			World-space end position for the sweep.
*	@param	passEdict	Optional entity to exclude from clipping checks.
*	@param	contentmask	Contents mask used to filter collision candidates.
*	@return	Combined collision trace for the cylinder sweep.
**/
const cm_trace_t q_gameabi SV_TraceCylinder( const Vector3 &start, float radius, float halfHeight, const Vector3 &end, const edict_ptr_t *passEdict, const cm_contents_t contentmask ) {
	/**
	*	Configure a cylinder trace shape and delegate to the shared helper so
	*	all shape traces follow the same world/entity merge behavior.
	**/
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CYLINDER;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return SV_ShapeTraceInternal( start, shape, end, passEdict, contentmask );
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
    trace = CM_BoxTrace( 
        &sv.cm, 
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

	/**
    *  Refine the already-computed world trace against dynamic entities without discarding the
    *  original BSP result.
    *      The world trace above is authoritative for floors, walls, and other BSP geometry.
    *      Replacing it outright with an entity-only trace can erase a valid floor hit entirely,
    *      which in turn makes movement and ground checks think nothing was beneath the mover.
    **/
    const cm_trace_t entityTrace = SV_ClipMoveToEntities(
        start, mins, maxs, end, 
        moveMins, moveMaxs,
        passEdict,
        contentmask
    );

    /**
    *  Keep the nearest valid hit across the world and entity passes while preserving solid-start
    *  information from either source.
    **/
    if ( entityTrace.allsolid || entityTrace.fraction < trace.fraction ) {
        const int32_t oldStartSolid = trace.startsolid;
        const int32_t oldAllSolid = trace.allsolid;
        trace = entityTrace;
        trace.startsolid = (bool)( ( int32_t )trace.startsolid | oldStartSolid );
        trace.allsolid = (bool)( ( int32_t )trace.allsolid | oldAllSolid );
    } else if ( entityTrace.startsolid ) {
        trace.startsolid = true;
        if ( entityTrace.allsolid ) {
            trace.allsolid = true;
        }
    }

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
            trace = CM_BoxTrace( &sv.cm, start, end, mins, maxs, sv.cm.cache->nodes, contentmask );
            // Clip against clipEntity.
        } else {
            cm_trace_shape_t touchShape = { .type = SHAPE_AABB };
            if ( clipEntity->solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
            else if ( clipEntity->solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
            else if ( clipEntity->solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

            if ( touchShape.type != SHAPE_AABB ) {
                cm_trace_shape_t shapeA = { .type = SHAPE_AABB };
                Vector3 centerStart = start;
                Vector3 centerEnd = end;
                Vector3 offset = { 0.0f, 0.0f, 0.0f };

                if ( mins->x == 0.0f && mins->y == 0.0f && mins->z == 0.0f &&
                     maxs->x == 0.0f && maxs->y == 0.0f && maxs->z == 0.0f ) {
                    shapeA.type = SHAPE_POINT;
                    shapeA.radius = 0.0f;
                    shapeA.halfHeight = 0.0f;
                } else {
                    shapeA.extents = Vector3( (maxs->x - mins->x) * 0.5f, (maxs->y - mins->y) * 0.5f, (maxs->z - mins->z) * 0.5f );
                    shapeA.radius = std::max(shapeA.extents.x, shapeA.extents.y);
                    shapeA.halfHeight = shapeA.extents.z;

                    offset.x = (mins->x + maxs->x) * 0.5f;
                    offset.y = (mins->y + maxs->y) * 0.5f;
                    offset.z = (mins->z + maxs->z) * 0.5f;

                    centerStart.x += offset.x; centerStart.y += offset.y; centerStart.z += offset.z;
                    centerEnd.x += offset.x; centerEnd.y += offset.y; centerEnd.z += offset.z;
                }

                touchShape.extents = Vector3( (clipEntity->maxs.x - clipEntity->mins.x) * 0.5f, (clipEntity->maxs.y - clipEntity->mins.y) * 0.5f, (clipEntity->maxs.z - clipEntity->mins.z) * 0.5f );
                touchShape.radius = std::max(touchShape.extents.x, touchShape.extents.y);
                touchShape.halfHeight = touchShape.extents.z;

                Vector3 originalOrigin = clipEntity->currentOrigin;
                // const_cast because clipEntity is const edict_ptr_t*, but we're temporarily shifting it for math.
                sv_edict_t *tempClip = const_cast<sv_edict_t*>( clipEntity );
                tempClip->currentOrigin.x += (clipEntity->mins.x + clipEntity->maxs.x) * 0.5f;
                tempClip->currentOrigin.y += (clipEntity->mins.y + clipEntity->maxs.y) * 0.5f;
                tempClip->currentOrigin.z += (clipEntity->mins.z + clipEntity->maxs.z) * 0.5f;

                trace = CM_AnalyticalShapeSweep( centerStart, shapeA, centerEnd, clipEntity->currentOrigin, touchShape );

                tempClip->currentOrigin = originalOrigin;

                if ( trace.fraction < 1.0f ) {
                    trace.endpos.x = start.x + (end.x - start.x) * trace.fraction;
                    trace.endpos.y = start.y + (end.y - start.y) * trace.fraction;
                    trace.endpos.z = start.z + (end.z - start.z) * trace.fraction;
                    trace.contents = CONTENTS_SOLID;
                } else {
                    trace.endpos = end;
                }
            } else {
                mnode_t *headNode = SV_HullForEntity( clipEntity );

                // Perform clip.
                if ( headNode != nullptr ) {
                    trace = CM_TransformedBoxTrace( &sv.cm, start, end, mins, maxs, headNode, contentmask,
                        &clipEntity->currentOrigin.x, &clipEntity->currentAngles.x );

                    if ( trace.fraction < 1.0f ) {
                        trace.entityNumber = clipEntity->s.number;
                    }
                }
            }
        }
    }
	return trace;
}

static const cm_trace_t SV_ShapeClipInternal( const edict_ptr_t *clipEntity, const Vector3 &start, const cm_trace_shape_t &shape,
                            const Vector3 &end,
                            const cm_contents_t contentmask ) {
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
        if ( clipEntity == nullptr || ( clipEntity && clipEntity->s.number == ENTITYNUM_WORLD ) ) {
            trace = CM_ShapeTrace( &sv.cm, start, end, shape, sv.cm.cache->nodes, contentmask );
        } else {
            mnode_t *headNode = SV_HullForEntity( clipEntity );
            if ( headNode != nullptr ) {
                trace = CM_TransformedShapeTrace( &sv.cm, start, end, shape, headNode, contentmask,
                    &clipEntity->currentOrigin.x, &clipEntity->currentAngles.x );

                if ( trace.fraction < 1. ) {
                    trace.entityNumber = clipEntity->s.number;
                }
            }
        }
    }
	return trace;
}

const cm_trace_t q_gameabi SV_ClipSphere( const edict_ptr_t *clip, const Vector3 &start, float radius, const Vector3 &end, const cm_contents_t contentmask ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_SPHERE;
	shape.radius = radius;
	return SV_ShapeClipInternal( clip, start, shape, end, contentmask );
}

const cm_trace_t q_gameabi SV_ClipCapsule( const edict_ptr_t *clip, const Vector3 &start, float radius, float halfHeight, const Vector3 &end, const cm_contents_t contentmask ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CAPSULE;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return SV_ShapeClipInternal( clip, start, shape, end, contentmask );
}

const cm_trace_t q_gameabi SV_ClipCylinder( const edict_ptr_t *clip, const Vector3 &start, float radius, float halfHeight, const Vector3 &end, const cm_contents_t contentmask ) {
	cm_trace_shape_t shape = {};
	shape.type = SHAPE_CYLINDER;
	shape.radius = radius;
	shape.halfHeight = halfHeight;
	return SV_ShapeClipInternal( clip, start, shape, end, contentmask );
}

