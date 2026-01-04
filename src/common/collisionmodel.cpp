/**
*
*
*   Collision Model: General Functions
*
* 
**/
#include "shared/shared.h"
#include "common/bsp.h"
#include "common/collisionmodel.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/zone.h"

// C Linkage, required for GL/VKPT C refresh codes.
QEXTERN_C_OPEN
    mtexinfo_t nulltexinfo = {
        .c = {
            .name = "nulltexinfo",
        },
        .name = "nulltexinfo",
        .texInfoID = 0
    };
QEXTERN_C_CLOSE

cvar_t       *map_noareas;


//=======================================================================


/**
*   @brief
**/
void CM_Init() {
    map_noareas = Cvar_Get( "map_noareas", "0", 0 );
}
/**
*   @brief
**/
static void CM_InitCollisionModelHulls( cm_t *cm ) {
    if ( !cm ) {
        return;
    }

    // Initialize box hull for the specified collision model.
    CM_InitBoxHull( cm );
    // Initialize octagon hull for the specified collision model.
    CM_InitOctagonHull( cm );

    // Set null leaf cluster to -1.
    cm->nullLeaf.cluster = -1;
}

/**
*   @brief  Loads in the map, all of its submodels, and the entity key/value dictionaries.
**/
const int32_t CM_LoadMap( cm_t *cm, const char *name ) {
    int ret;

    // Load in the actual BSP file.
    ret = BSP_Load( name, &cm->cache );
    if ( !cm->cache ) {
        return ret;
    }

    // Iterate all BSP texinfos and load in their matching corresponding material file equivelants.
    CM_LoadMaterials( cm );

    // Generate the collision model hulls for entity clipping.
    CM_InitCollisionModelHulls( cm );

    // Set map file checksum.
    cm->checksum = cm->cache->checksum;

    // Set entity string to use.
    cm->entitystring = cm->cache->entitystring;
    // Parse the entity string, creating the entity key/value dictionary list.
    CM_ParseEntityString( cm );

    // Floodnums and Portals:
    cm->floodnums = static_cast<int *>( Z_TagMallocz( sizeof( cm->floodnums[ 0 ] ) * cm->cache->numareas, TAG_CMODEL ) );
    cm->portalopen = static_cast<bool *>( Z_TagMallocz( sizeof( cm->portalopen[ 0 ] ) * cm->cache->numportals, TAG_CMODEL ) );

    // Default Quake II behavior: portals start open (unless closed by game logic).
    for ( int32_t i = 0; i < cm->cache->numportals; i++ ) {
        cm->portalopen[ i ] = true;
    }

    CM_FloodAreaConnections( cm );

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

    // Clear portal/area -state related stack memory.
    Z_Free( cm->portalopen );
    Z_Free( cm->floodnums );

    // Clear entity data, not part of the bsp memory hunk.
    if ( cm->entities ) {
        Z_Freep( (void **)( cm->entities ) );
    }

    // Clear material data.
    Z_Free( cm->materials );
    
    // Free hull type BSPs.
    Z_Free( cm->hull_boundingbox );
    Z_Free( cm->hull_octagonbox );

    // Free BSP World and its Models.
    BSP_Free( cm->cache );

    // Zero out all collision model memory for an optional new re-use.
    //memset( cm, 0, sizeof( *cm ) );
    *cm = {};
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