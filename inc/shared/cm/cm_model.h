/********************************************************************
*
*
*   Collision Model:
*
*
********************************************************************/
#pragma once


/**
*   @brief  Contains the 'Box' hull BSP structure.
**/
typedef struct hull_boundingbox_s {
    cm_plane_t planes[ 12 ];
    mnode_t  nodes[ 6 ];
    mnode_t *headnode;
    mbrush_t brush;
    mbrush_t *leafbrush;
    mbrushside_t brushsides[ 6 ];
    mleaf_t  leaf;
    mleaf_t  emptyleaf;
} hull_boundingbox_t;

/**
*   @brief  Contains the 'Octagon' hull BSP structure.
**/
typedef struct hull_octagonbox_s {
    cm_plane_t planes[ 20 ];
    mnode_t  nodes[ 10 ];
    mnode_t *headnode;
    mbrush_t brush;
    mbrush_t *leafbrush;
    mbrushside_t brushsides[ 10 ];
    mleaf_t  leaf;
    mleaf_t  emptyleaf;

    //! Used for transformed trace.
    vec3_t cylinder_offset;
} hull_octagonbox_t;

//#define MAX_MATERIAL_NAME CM_MAX_TEXNAME
//typedef uint32_t cm_material_physical_type;
//typedef struct cm_material_s {
//    /**
//    *   Texture Info:
//    **/
//    //! The unique material index corresponding to its matching texture into bsp->texinfos.
//    uint32_t texinfoID;
//    //! The name of the material( identical to that of mtexinfo, but stored separately for now. ).
//    char name[ MAX_MATERIAL_NAME ];
//    //! A pointer to the matching texinfo_t.
//    mtexinfo_t *mtexinfo;
//    //! A pointer to the matching textures cm_surface_t
//    cm_surface_t *csurface;
//
//    /**
//    *   Physical Properties:
//    **/
//    struct cm_material_physical_properties_s {
//        //! Material.
//        cm_material_physical_type type;   // Can match cm_material_type enum values.
//        //! Friction.
//        float friction;
//    } physical;
//} cm_material_t;
typedef struct cm_material_s cm_material_t;

/**
*   @brief  The actual 'collision model', storing the current BSP model(s),
*           portal states as well as the actual entity key/value definitions.
**/
typedef struct cm_s {
    bsp_t *cache;
    int32_t *floodnums;     // if two areas have equal floodnums,
                            // they are connected
	int32_t *portalopen;
    int32_t checksum;

    //! Entity String:
    char *entitystring;

    //! WID: 'Collision Model' Entities: contain all the key/value definitions of the entity 'dictionaries' that are parsed from the entity string.
    int32_t numentities;
    const cm_entity_t **entities;

    // Valid floods and check count:
    int32_t floodValid;
    int32_t checkCount;

    // Null Leaf, as well as null texture, returned in case the query had invalid results
    mleaf_t nullLeaf;
    //mtexinfo_t nullTextureInfo;

    //! Collision Model's bounding box hull.
    hull_boundingbox_t *hull_boundingbox;
    //! Collision Model's octagon box hull.
    hull_octagonbox_t *hull_octagonbox;

    //! Material types, array index equals their typeID. The zero index(0) is used as the default material type.
    cm_material_t *materials;
    int32_t num_materials;
} cm_t;