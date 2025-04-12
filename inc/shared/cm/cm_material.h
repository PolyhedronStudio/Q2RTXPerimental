/********************************************************************
*
*
*   Collision Model Material Definitions:
*
*
********************************************************************/
#pragma once

//! Maximum material name will always equal maximum texture name, otherwise it'd be a mismatch.
#define MAX_MATERIAL_NAME CM_MAX_TEXNAME
#define MAX_MATERIAL_KIND_STR_LENGTH MAX_QPATH

//! The physical property type, such as BRICK, CONCRETE, GLASS, GRASS, SNOW, PLYWOOD etc.
typedef uint32_t cm_material_physical_type;

//! The default material type.
#define MAT_TYPE_DEFAULT    0
//! Game specific defines start from here.
#define MAT_TYPE_GAME       1
// TODO: Move these to shared game code and add game API for parsing the specific
// material type information.
#define MAT_TYPE_CONCRETE   1
#define MAT_TYPE_GRASS      2
#define MAT_TYPE_SNOW       3
#define MAT_TYPE_WOOD       4

/**
*   @brief  For each mtexinfo_t entry in the BSP, a seek and load is executed
*           for a matching material [texturename].wal_json file.
*
*           Whether the json was found and loaded or not is irrelevant to the creation
*           of the materials themselves. However it'll consist of the default material
*           properties instead.
* 
*           If a json file was found, loaded and parsed properly, the material will have
*           its custom properties applied.
**/
typedef struct cm_material_s {
    /**
    *   TexInfo Bound Information:
    **/
    //! The unique material index corresponding to its index inside of the materials array.
    int32_t materialID;
    //! The name of the material( identical to that of all the mtexinfos that it applies to. )
    char name[ MAX_MATERIAL_NAME ];

    /**
    *   Physical Properties:
    **/
    struct cm_material_physical_properties_s {
        //! Material.
        //cm_material_physical_type type;
        char kind[ MAX_MATERIAL_KIND_STR_LENGTH ];

        //! Friction.
        float friction;
    } physical;
} cm_material_t;