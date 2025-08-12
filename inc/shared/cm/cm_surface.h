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
//! <Q2RTXP>: WID: TODO: Implement for VKPT?
#define CM_SURFACE_N64_UV           (1U << 28)
#define CM_SURFACE_N64_SCROLL_X     (1U << 29)
#define CM_SURFACE_N64_SCROLL_Y     (1U << 30)
#define CM_SURFACE_N64_SCROLL_FLIP  (1U << 31)



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