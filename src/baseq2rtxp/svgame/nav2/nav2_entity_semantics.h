/********************************************************************
*
*
*	ServerGame: Nav2 Inline BSP Entity Semantics
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include <vector>


/**
*
*
*	Nav2 Inline BSP Entity Semantics Enumerations:
*
*
**/
/**
*	@brief	Role flags describing how a BSP inline-model entity participates in navigation.
*	@note	These flags are intended for connectors, occupancy, and mover-local nav layers.
**/
enum nav2_inline_bsp_role_bits_t : uint32_t {
    //! No nav role assigned yet.
    NAV2_INLINE_BSP_ROLE_NONE = 0,
    //! Entity should be treated only as a blocking volume.
    NAV2_INLINE_BSP_ROLE_BLOCKER_ONLY = ( 1u << 0 ),
    //! Entity provides a timed connector (door-like).
    NAV2_INLINE_BSP_ROLE_TIMED_CONNECTOR = ( 1u << 1 ),
    //! Entity is a rideable surface.
    NAV2_INLINE_BSP_ROLE_RIDEABLE_SURFACE = ( 1u << 2 ),
    //! Entity is both rideable and traversable as a local nav subspace.
    NAV2_INLINE_BSP_ROLE_RIDEABLE_TRAVERSABLE = ( 1u << 3 ),
    //! Entity can open as a passage.
    NAV2_INLINE_BSP_ROLE_OPENABLE_PASSAGE = ( 1u << 4 ),
    //! Entity is a rotating hazard or obstruction.
    NAV2_INLINE_BSP_ROLE_ROTATING_HAZARD = ( 1u << 5 ),
    //! Entity should be treated as a moving traversal subspace.
    NAV2_INLINE_BSP_ROLE_MOVING_TRAVERSAL_SUBSPACE = ( 1u << 6 )
};


/**
*
*
*	Nav2 Inline BSP Entity Semantics Data Structures:
*
*
**/
/**
*	@brief	Stable nav2-facing metadata describing one inline BSP entity.
*	@note	This keeps classification results pointer-free and safe for snapshot publication.
**/
struct nav2_inline_bsp_entity_info_t {
    //! Owning entity number for this inline model.
    int32_t owner_entnum = -1;
    //! Entity type classification from the sharedgame layer.
    int32_t entity_type = 0;
    //! Entity movetype for traversal semantics.
    svg_movetype_t movetype = MOVETYPE_NONE;
    //! Entity solidity classification.
    int32_t solid = 0;
    //! Brush model index when known.
    int32_t model_index = 0;
   //! Inline-model index derived from the brush model string (numeric part of "*N").
    int32_t inline_model_index = -1;
    //! Bitmask of nav2 inline BSP role flags.
    uint32_t role_flags = NAV2_INLINE_BSP_ROLE_NONE;
    //! True when this entity references inline BSP geometry.
    bool has_inline_model = false;
    //! True when the inline model data could be resolved.
    bool model_data_valid = false;
  //! True when the inline-model headnode could be resolved.
    bool headnode_valid = false;
};

/**
*	@brief	Stable result set for inline BSP entity classification.
*	@note	The list is intentionally pointer-free for future snapshot publication.
**/
struct nav2_inline_bsp_entity_list_t {
    //! Classified inline BSP entity records.
    std::vector<nav2_inline_bsp_entity_info_t> entities = {};
};


/**
*
*
*	Nav2 Inline BSP Entity Semantics Public API:
*
*
**/
/**
*	@brief	Classify one inline BSP entity for navigation roles.
*	@param	ent	Entity to classify.
*	@param	out_info	[out] Classification record receiving role flags and metadata.
*	@return	True when classification produced a valid record.
**/
const bool SVG_Nav2_ClassifyInlineBspEntity( const svg_base_edict_t *ent, nav2_inline_bsp_entity_info_t *out_info );

/**
*	@brief	Collect and classify inline BSP entities within the provided entity set.
*	@param	entities	Entity list to classify.
*	@param	out_list	[out] Classification list receiving inline BSP records.
*	@return	True when at least one inline BSP entity was classified.
**/
const bool SVG_Nav2_ClassifyInlineBspEntities( const std::vector<svg_base_edict_t *> &entities, nav2_inline_bsp_entity_list_t *out_list );

/**
*	@brief	Emit a bounded debug summary for classified inline BSP entities.
*	@param	list	Classification list to report.
*	@param	limit	Maximum number of entities to report.
**/
void SVG_Nav2_DebugPrintInlineBspEntities( const nav2_inline_bsp_entity_list_t &list, const int32_t limit = 16 );

/**
*	@brief	Keep the nav2 inline BSP entity semantics module represented in the build.
**/
void SVG_Nav2_EntitySemantics_ModuleAnchor( void );
