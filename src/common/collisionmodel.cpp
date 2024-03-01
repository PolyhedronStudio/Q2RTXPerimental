/**
*
*
*   Collision Model: General Functions
*
* 
**/
#include "shared/shared.h"
#include "common/bsp.h"
#include "common/cmd.h"
#include "common/collisionmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/math.h"
#include "common/sizebuf.h"
#include "common/zone.h"
#include "system/hunk.h"

extern "C" {
    mtexinfo_t nulltexinfo;
};

cvar_t       *map_noareas;
cvar_t       *map_allsolid_bug;


//=======================================================================


/**
*   @brief
**/
void CM_Init() {
    map_noareas = Cvar_Get( "map_noareas", "0", 0 );
    map_allsolid_bug = Cvar_Get( "map_allsolid_bug", "1", 0 );
}
/**
*   @brief
**/
void CM_InitCollisionModel( cm_t *cm ) {
    // Initialize box hull for the specified collision model.
    CM_InitBoxHull( cm );

    // Set null leaf cluster to -1.
    cm->nullLeaf.cluster = -1;
}

/**
*   @brief  Loads in the map, all of its submodels, and the entity key/value dictionaries.
**/
const int32_t CM_LoadMap( cm_t *cm, const char *name ) {
    int ret;

    // Prepare the collision model, including the BSP 'hull' for bounding box entities.
    CM_InitCollisionModel( cm );

    // Load in the actual BSP file.
    ret = BSP_Load( name, &cm->cache );
    if ( !cm->cache ) {
        return ret;
    }

    // Set map file checksum.
    cm->checksum = cm->cache->checksum;

    // We don't really want this.
    //if (!(cm->override_bits & OVERRIDE_ENTS)) {
    cm->entitystring = cm->cache->entitystring;
    //} We really don't.

    // Parse the entity string, creating the entity key/value dictionary list.
    CM_ParseEntityString( cm );

    // Floodnums and Portals:
    cm->floodnums = static_cast<int *>( Z_TagMallocz( sizeof( cm->floodnums[ 0 ] ) * cm->cache->numareas, TAG_CMODEL ) );
    cm->portalopen = static_cast<qboolean *>( Z_TagMallocz( sizeof( cm->portalopen[ 0 ] ) * cm->cache->numportals, TAG_CMODEL ) );
    FloodAreaConnections( cm );

    // Successfully loaded the map.
    return Q_ERR_SUCCESS;
}

/**
*   @brief  Frees up all of the collision model's data.
**/
void CM_FreeMap( cm_t *cm ) {
    if ( !cm ) {
        return;
    }

    Z_Free( cm->portalopen );
    Z_Free( cm->floodnums );

    //// Clear entity data, not part of the bsp memory hunk.
    if ( cm->entities ) {
        Z_Freep( (void **)( cm->entities ) );
    }
    //Z_FreeTags( memtag_t::TAG_CMODEL );

    //if (cm->override_bits & OVERRIDE_ENTS)
    //    Z_Free(cm->entitystring);
    
    // Free hull bounding box.
    Z_Free( cm->hull_boundingbox );

    // Free BSP models.
    BSP_Free( cm->cache );

    // Reset collision model.
    memset( cm, 0, sizeof( *cm ) );
}


/**
*   @return A pointer to nullleaf if there is no BSP model cached up, or the number is out of bounds.
*           A pointer to the 'special case for solid leaf' if number == -1
*           A pointer to the BSP Node that matched with 'number'.
**/
mnode_t *CM_NodeForNumber( cm_t *cm, const int32_t number ) {
    if ( !cm->cache ) {
        return (mnode_t *)&cm->nullLeaf;
    }
    if ( number == -1 ) {
        return (mnode_t *)cm->cache->leafs;   // special case for solid leaf
    }
    if ( number < 0 || number >= cm->cache->numnodes ) {
        Com_EPrintf( "%s: bad number: %d\n", __func__, number );
        return (mnode_t *)&cm->nullLeaf;
    }
    return cm->cache->nodes + number;
}

/**
*   @return A pointer to nullleaf if there is no BSP model cached up.
*           A pointer to the BSP Leaf that matched with 'number'.
**/
mleaf_t *CM_LeafForNumber( cm_t *cm, const int32_t number ) {
    if ( !cm->cache ) {
        return &cm->nullLeaf;
    }
    if ( number < 0 || number >= cm->cache->numleafs ) {
        Com_EPrintf( "%s: bad number: %d\n", __func__, number );
        return &cm->nullLeaf;
    }
    return cm->cache->leafs + number;
}