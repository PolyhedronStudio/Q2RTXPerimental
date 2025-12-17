/********************************************************************
*
*
*   Ground & Liquid Information:
*
*
********************************************************************/
#pragma once



/**
*   @brief  Stores the final ground information results.
**/
typedef struct ground_info_s {
    //! Number to the actual ground entity we are on.(-1 if none).
    int32_t entityNumber;
    //! Number to the actual ground entity we are on.(-1 if none).
    int32_t entityLinkCount;

    //! A copy of the plane data from the ground entity.
    cm_plane_t        plane;
    //! A copy of the ground plane's surface data. (May be none, in which case, it has a 0 name.)
    cm_surface_t      surface;
    //! A copy of the contents data from the ground entity's brush.
    cm_contents_t      contents;

    //! A pointer to the material data of the ground brush' surface we are standing on. (nullptr if none).
    const struct cm_material_s *material;
} ground_info_t;

/**
*   @brief  Stores the final 'liquid' information results. This can be lava, slime, or water, or none.
**/
typedef struct contents_info_s {
    //! The actual BSP liquid 'contents' type we're residing in.
    cm_contents_t      type;
    //! The depth of the player in the actual liquid.
    cm_liquid_level_t	level;
} liquid_info_t;