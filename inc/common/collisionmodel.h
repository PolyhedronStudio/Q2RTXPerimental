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

#ifndef CMODEL_H
#define CMODEL_H


#include "shared/format_bsp.h"



//
//  common/collisionmodel.cpp
//
QEXTERN_C_ENCLOSE( extern mtexinfo_t nulltexinfo; );
QEXTERN_C_ENCLOSE( extern cm_material_t cm_default_material; );

/**
*   @brief
**/
void CM_Init(void);
/**
*   @brief  Loads in the map, all of its submodels, and the entity key/value dictionaries.
**/
const int32_t CM_LoadMap( cm_t *cm, const char *name );
/**
*   @brief  Frees up all of the collision model's data.
**/
void CM_FreeMap( cm_t *cm );

/**
*   @return A pointer to nullleaf if there is no BSP model cached up, or the number is out of bounds.
*           A pointer to the 'special case for solid leaf' if number == -1
*           A pointer to the BSP Node that matched with 'number'.
**/
mnode_t *CM_NodeForNumber( cm_t *cm, const int32_t number );
/**
*   @return The number that matched the node's pointer. -1 if node was a nullptr.
**/
static inline const int32_t CM_NumberForNode( cm_t *cm, mnode_t *node ) {
    return ( ( node ) ? ( (node)-( cm )->cache->nodes ) : -1 );
}

/**
*   @return A pointer to nullleaf if there is no BSP model cached up.
*           A pointer to the BSP Leaf that matched with 'number'.
**/
mleaf_t *CM_LeafForNumber( cm_t *cm, const int32_t number );

/**
*   @return The number that matched the leaf's pointer. 0 if leaf was a nullptr.
**/
static inline const int32_t CM_NumberForLeaf( cm_t *cm, mleaf_t *leaf ) {
    return ( ( cm )->cache ? ( (leaf)-( cm )->cache->leafs ) : 0 );
}



//
//  collisionmodel/cm_areaportals.cpp
//
/**
*   @brief
**/
void CM_FloodAreaConnections( cm_t *cm );
/**
*   @brief  Set the portal nums matching portal to open/closed state.
**/
void CM_SetAreaPortalState( cm_t *cm, const int32_t portalnum, const bool open );
/**
*   @return False(0) if the portal nums matching portal is closed, true(1) otherwise.
**/
const int32_t CM_GetAreaPortalState( cm_t *cm, const int32_t portalnum );
/**
*   @return True if the two areas are connected, false if not(or possibly blocked by a door for example.)
**/
const bool CM_AreasConnected( cm_t *cm, const int32_t area1, const int32_t area2 );
/**
*   @brief  Writes a length byte followed by a bit vector of all the areas
*           that area in the same flood as the area parameter
*
*           This is used by the client refreshes to cull visibility
**/
const int32_t CM_WriteAreaBits( cm_t *cm, byte *buffer, const int32_t area );
/**
*   @brief
**/
const int32_t CM_WritePortalBits( cm_t *cm, byte *buffer );
/**
*   @brief
**/
void CM_SetPortalStates( cm_t *cm, byte *buffer, const int32_t bytes );

/**
*   @Return True if any leaf under headnode has a cluster that
*           is potentially visible
**/
const bool CM_HeadnodeVisible( mnode_t *headnode, byte *visbits );
/**
*   @brief  The client will interpolate the view position,
*           so we can't use a single PVS point
**/
byte *CM_FatPVS( cm_t *cm, byte *mask, const vec3_t org, const int32_t vis );



//
// common/collisionmodel/cm_boxleafs.cpp
//
/**
*   @brief  Populates the list of leafs which the specified bounding box touches. If top_node is not
*           set to NULL, it will contain a value copy of the the top node of the BSP tree that fully
*           contains the box.
**/
const int32_t CM_BoxLeafs( cm_t *cm, const vec3_t mins, const vec3_t maxs, mleaf_t **list, const int32_t listsize, mnode_t **topnode );
/**
*   @brief  Recurse the BSP tree from the specified node, accumulating leafs the
*           given box occupies in the data structure.
**/
const int32_t CM_BoxLeafs_headnode( cm_t *cm, const vec3_t mins, const vec3_t maxs, mleaf_t **list, int listsize, mnode_t *headnode, mnode_t **topnode );
/**
*   @return The contents mask of all leafs within the absolute bounds.
**/
const contents_t CM_BoxContents( cm_t *cm, const vec3_t mins, const vec3_t maxs, mnode_t *headnode );


//
//  collisionmodel/cm_entities.cpo
//
/**
*   @brief  Parses the collision model its entity string into collision model entities
*           that contain a list of key/value pairs containing the key's value as well as
*           describing the types it got validly parsed for.
**/
void CM_ParseEntityString( cm_t *cm );
/**
*   @brief  Used to check whether CM_EntityValue was able/unable to find a matching key in the cm_entity_t.
*   @return Pointer to the collision model system's 'null' entity key/pair.
**/
const cm_entity_t *CM_GetNullEntity( void );

/**
*   @brief  Looks up the key/value cm_entity_t pair in the list for the cm_entity_t entity.
*   @return If found, a pointer to the key/value pair, otherwise a pointer to the 'cm_null_entity'.
**/
const cm_entity_t *CM_EntityKeyValue( const cm_entity_t *entity, const char *key );


//
// collisionmodel/cm_hull_boundingbox.cpp
//
/**
*   Set up the planes and nodes so that the six floats of a bounding box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitBoxHull( cm_t *cm );
/**
*   @brief  To keep everything totally uniform, bounding boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
mnode_t *CM_HeadnodeForBox( cm_t *cm, const vec3_t mins, const vec3_t maxs, const contents_t contents );



//
// collisionmodel/cm_hull_octagonbox.cpp
//
/**
*   Set up the planes and nodes so that the ten floats of a Bounding 'Octagon' Box
*   can just be stored out and get a proper BSP clipping hull structure.
**/
void CM_InitOctagonHull( cm_t *cm );
/**
*   @brief  To keep everything totally uniform, Bounding 'Octagon' Boxes are turned into small
*           BSP trees instead of being compared directly.
*
*           The BSP trees' octagon box will match with the bounds(mins, maxs) and have appointed
*           the specified contents. If contents == CONTENTS_NONE(0) then it'll default to CONTENTS_MONSTER.
**/
mnode_t *CM_HeadnodeForOctagon( cm_t *cm, const vec3_t mins, const vec3_t maxs, const contents_t contents );



//
//  common/collisionmodel/cm_pointcontents.cpp
//
/**
*   @brief  Iterate all BSP texinfos and loads in their matching corresponding material file equivelants
*           into the 'collision model' data.
*
*   @return The number of materials loaded for the BSP's texinfos. In case of any errors
*           it'll return 0 instead.
**/
const int32_t CM_LoadMaterials( cm_t *cm );



//
//  common/collisionmodel/cm_pointcontents.cpp
//
/**
*   @return Pointer to the leaf matching coordinate 'p'.
**/
mleaf_t *CM_PointLeaf( cm_t *cm, const vec3_t p );
/**
*   @return An ORed contents mask
**/
const contents_t CM_PointContents( cm_t *cm, const vec3_t p, mnode_t *headnode );
/**
*   @brief  Handles offseting and rotation of the end points for moving and
*           rotating entities
**/
const contents_t CM_TransformedPointContents( cm_t *cm, const vec3_t p, mnode_t *headnode,
                                                const vec3_t origin, const vec3_t angles );



//
//  common/collisionmodel/cm_trace.cpp
//
/**
*   @brief
**/
void        CM_BoxTrace( cm_t *cm, cm_trace_t *trace,
                        const vec3_t start, const vec3_t end,
                        const vec3_t mins, const vec3_t maxs,
                        mnode_t *headnode, const contents_t brushmask );
/**
*   @brief  Handles offseting and rotation of the end points for moving and
*           rotating entities.
**/
void        CM_TransformedBoxTrace( cm_t *cm, cm_trace_t *trace,
                                    const vec3_t start, const vec3_t end,
                                    const vec3_t mins, const vec3_t maxs,
                                    mnode_t *headnode, const contents_t brushmask,
                                    const vec3_t origin, const vec3_t angles );
/**
*   @brief
**/
void        CM_ClipEntity( cm_t *cm, cm_trace_t *dst, const cm_trace_t *src, struct edict_s *ent );


#endif // CMODEL_H
