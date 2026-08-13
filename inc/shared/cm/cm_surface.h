/********************************************************************
*
*
*   Shared Collision Detection: BSP Brush side surface struct.
*
*
********************************************************************/
#pragma once



/**
*   Surface Types:
**/
//! 'value' will hold the light strength.
#define CM_SURFACE_FLAG_LIGHT   0x1     
//! Affects game physics.
#define CM_SURFACE_FLAG_SLICK   0x2
//! Don't draw, but add to skybox.
#define CM_SURFACE_FLAG_SKY     0x4
//! Turbulent water warp.
#define CM_SURFACE_FLAG_WARP    0x8
//! 33% Transparent.
#define CM_SURF_TRANSLUCENT_33  0x10
//! 66% Transparent.
#define CM_SURF_TRANSLUCENT_66  0x20
//! Scroll towards angle.
#define CM_SURFACE_FLOWING      0x40
//! Don't bother referencing the texture.
#define CM_SURFACE_NODRAW       0x80
//! Used by kmquake2.
#define CM_SURFACE_ALPHATEST    0x02000000
//! Used for the Navigation Mesh system to indicate that this is an area which does not need to be involved
//! for pathfinding. This is used to mark areas which are not walkable, such as (possibly) water or lava, 
//! or areas which are not relevant to the navmesh generation, such as sky and decorative surfaces. 
//! This flag is used to optimize the navmesh generation process by excluding these areas from consideration.
#define CM_SURFACE_NO_NAVMESH	BIT( 11 )
// Came from Q2RE but y
//#define CM_SURFACE_N64_UV           BIT( 28 )
//#define CM_SURFACE_N64_SCROLL_X     BIT( 29 )
//#define CM_SURFACE_N64_SCROLL_Y     BIT( 30 )
//#define CM_SURFACE_N64_SCROLL_FLIP  BIT( 31 )



/**
*   @brief  BSP Brush side surface.
*
*   Stores material/texture name, flags as well as an
*   integral 'value' which was commonly used for light flagged surfaces.
**/
typedef struct cmsurface_s {
    //! Texture/Material name for the surface.
    char name[ CM_MAX_TEXNAME ]; // WID: materials: Was 16, but what for?
    //! Special specific surface flags such as transparent etc.
    int32_t flags;
    //! BSP surface value(Usually set for 'light' flagged surfaces.)
    int32_t value;

    // WID: materials: Index into the cm_materials_t array.
    int32_t materialID;
    // WID: materials: Pointer into the cm_materials_t array.
    struct cm_material_s *material;
} cm_surface_t;