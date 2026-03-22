/********************************************************************
*
*
*	ServerGame: Nav2 Inline BSP Entity Semantics - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_entity_semantics.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>


/**
*
*
*	Nav2 Inline BSP Entity Semantics Local Helpers:
*
*
**/
/**
*	@brief	Return whether the entity exposes inline BSP geometry.
*	@param	ent	Entity to inspect.
*	@return	True when the entity uses a brush model and solid BSP.
**/
static const bool SVG_Nav2_EntityHasInlineModel( const svg_base_edict_t *ent ) {
	/**
	*    Sanity check: require an entity before inspecting fields.
	**/
	// Bail out if the caller did not supply a valid entity.
	if ( !ent ) {
		// Report that no inline model is available.
		return false;
	}

	/**
	*    Inline BSP entities use brush models and solid BSP collision.
	**/
	// Check for brush-model solidity and a valid model index.
	return ent->solid == SOLID_BSP && ent->s.modelindex > 0;
}

/**
*	@brief	Resolve inline-model headnode metadata for an entity.
*	@param	ent	Entity to inspect.
*	@param	out_headnode	[out] Headnode pointer when resolved.
*	@return	True when a headnode was resolved.
**/
static const bool SVG_Nav2_ResolveInlineModelHeadnode( const svg_base_edict_t *ent, mnode_t **out_headnode ) {
	/**
	*    Sanity check: require output storage and the entity pointer.
	**/
	// Reject missing output storage.
	if ( !out_headnode ) {
		// Abort when there is no output pointer.
		return false;
	}
	// Reset the output headnode to the sentinel.
	*out_headnode = nullptr;

	// Abort if the entity pointer is missing.
	if ( !ent ) {
		return false;
	}

	/**
	*    Require the collision-model imports before touching BSP headnodes.
	**/
	// Reject when the engine helper is not available.
     if ( !gi.CM_InlineModelHeadnode ) {
		return false;
	}

	/**
	*    Resolve the inline model index before requesting the headnode.
	**/
	// Require a valid model string to derive the inline model index.
	if ( !ent->model || !ent->model.ptr ) {
		return false;
	}
	// Require the brush-model prefix to parse the inline model index.
	if ( ent->model.ptr[ 0 ] != '*' ) {
		return false;
	}

	// Convert the numeric portion of the brush model string into an index.
	const int32_t inlineModelIndex = std::atoi( ent->model.ptr + 1 );
	// Reject invalid inline model indices.
	if ( inlineModelIndex <= 0 ) {
		return false;
	}

	/**
	*    Query the inline-model headnode using the resolved index.
	**/
	// Ask the collision system for the inline headnode associated with the model index.
	mnode_t *headnode = gi.CM_InlineModelHeadnode( inlineModelIndex );
	// Reject invalid headnode pointers.
	if ( !headnode ) {
		return false;
	}

	// Publish the resolved headnode.
	*out_headnode = headnode;
	// Report success.
	return true;
}

/**
*	@brief	Assign role flags based on entity type and class traits.
*	@param	ent	Entity to classify.
*	@param	out_flags	[out] Role flags to assign.
**/
static void SVG_Nav2_AssignInlineModelRoles( const svg_base_edict_t *ent, uint32_t *out_flags ) {
	/**
	*    Sanity check: require output storage before writing flags.
	**/
	// Ensure the output mask is available.
	if ( !out_flags ) {
		// Abort when no output pointer is provided.
		return;
	}
	// Reset the output flags before classification.
	*out_flags = NAV2_INLINE_BSP_ROLE_NONE;

	/**
	*    Guard against missing entity data.
	**/
	// Bail out if the caller provided no entity.
	if ( !ent ) {
		return;
	}

	/**
	*    Default classification: treat pushers as traversal-capable surfaces.
	**/
	// Prefer mover semantics for pushers.
	if ( ent->movetype == MOVETYPE_PUSH || ent->movetype == MOVETYPE_STOP ) {
		// Mark as rideable and moving traversal by default.
		*out_flags |= NAV2_INLINE_BSP_ROLE_RIDEABLE_SURFACE | NAV2_INLINE_BSP_ROLE_MOVING_TRAVERSAL_SUBSPACE;
	}

	/**
	*    Apply class-specific role hints using map classnames.
	**/
	// Classify known door types as timed connectors and openable passages.
	if ( ent->classname && ent->classname.ptr
		&& ( strcmp( ent->classname.ptr, "func_door" ) == 0 || strcmp( ent->classname.ptr, "func_door_rotating" ) == 0 ) ) {
		// Door-like entities provide connector and passage semantics.
		*out_flags |= NAV2_INLINE_BSP_ROLE_TIMED_CONNECTOR | NAV2_INLINE_BSP_ROLE_OPENABLE_PASSAGE;
	}

	// Platforms are rideable traversal spaces.
    if ( ent->classname && ent->classname.ptr && strcmp( ent->classname.ptr, "func_plat" ) == 0 ) {
		// Plat surfaces should be treated as rideable traversal subspaces.
		*out_flags |= NAV2_INLINE_BSP_ROLE_RIDEABLE_TRAVERSABLE;
	}

	// Rotating brushes can become hazards when moving.
	if ( ent->classname && ent->classname.ptr && strcmp( ent->classname.ptr, "func_rotating" ) == 0 ) {
		// Rotating entities often damage or block while in motion.
		*out_flags |= NAV2_INLINE_BSP_ROLE_ROTATING_HAZARD;
	}

	/**
	*    If no mover-specific roles were assigned, fall back to blocker-only.
	**/
	// Use blocker-only when no other roles were detected.
	if ( *out_flags == NAV2_INLINE_BSP_ROLE_NONE ) {
		// Mark as a pure blocker.
		*out_flags = NAV2_INLINE_BSP_ROLE_BLOCKER_ONLY;
	}
}


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
const bool SVG_Nav2_ClassifyInlineBspEntity( const svg_base_edict_t *ent, nav2_inline_bsp_entity_info_t *out_info ) {
	/**
	*    Sanity checks: require inputs and ensure the entity uses inline BSP data.
	**/
	// Reject null pointers up front.
	if ( !ent || !out_info ) {
		// Return false when inputs are missing.
		return false;
	}

	// Reject entities without inline BSP geometry.
	if ( !SVG_Nav2_EntityHasInlineModel( ent ) ) {
		return false;
	}

	/**
	*    Populate baseline metadata used for snapshot publication and serialization.
	**/
	// Reset the output record before filling fields.
	*out_info = {};
	// Copy stable entity identifiers and state.
	out_info->owner_entnum = ent->s.number;
	out_info->entity_type = ent->s.entityType;
  out_info->movetype = ( svg_movetype_t )ent->movetype;
	out_info->solid = ent->solid;
	out_info->model_index = ent->s.modelindex;
	out_info->has_inline_model = true;

	/**
	*    Resolve inline-model metadata to inform connector and mover-local navigation.
	**/
	// Attempt to resolve the inline model headnode.
	mnode_t *headnode = nullptr;
	out_info->headnode_valid = SVG_Nav2_ResolveInlineModelHeadnode( ent, &headnode );

	// Record the inline model index for persistence-friendly metadata.
	if ( ent->model && ent->model.ptr && ent->model.ptr[ 0 ] == '*' ) {
		out_info->inline_model_index = std::atoi( ent->model.ptr + 1 );
	}
	// Tag the model data as valid when we resolved a headnode.
	out_info->model_data_valid = out_info->headnode_valid;

	/**
	*    Assign role flags based on entity type and class traits.
	**/
	// Evaluate role flags using the entity's class and runtime state.
	SVG_Nav2_AssignInlineModelRoles( ent, &out_info->role_flags );

	// Report success once classification completes.
	return true;
}

/**
*	@brief	Collect and classify inline BSP entities within the provided entity set.
*	@param	entities	Entity list to classify.
*	@param	out_list	[out] Classification list receiving inline BSP records.
*	@return	True when at least one inline BSP entity was classified.
**/
const bool SVG_Nav2_ClassifyInlineBspEntities( const std::vector<svg_base_edict_t *> &entities, nav2_inline_bsp_entity_list_t *out_list ) {
	/**
	*    Sanity checks: require output storage before iterating.
	**/
	// Guard against missing output storage.
	if ( !out_list ) {
		// Return false when no output list is available.
		return false;
	}

	// Reset the output list so callers never observe stale entries.
	out_list->entities.clear();

	/**
	*    Iterate the provided entity list and classify inline BSP entries.
	**/
	// Walk each entity pointer supplied by the caller.
	for ( svg_base_edict_t *ent : entities ) {
		// Skip null entries in the provided list.
		if ( !ent ) {
			continue;
		}

		// Attempt to classify the entity and record successful results.
		nav2_inline_bsp_entity_info_t info = {};
		if ( SVG_Nav2_ClassifyInlineBspEntity( ent, &info ) ) {
			out_list->entities.push_back( info );
		}
	}

	// Report success when at least one entity was classified.
	return !out_list->entities.empty();
}

/**
*	@brief	Emit a bounded debug summary for classified inline BSP entities.
*	@param	list	Classification list to report.
*	@param	limit	Maximum number of entities to report.
**/
void SVG_Nav2_DebugPrintInlineBspEntities( const nav2_inline_bsp_entity_list_t &list, const int32_t limit ) {
	/**
	*    Early out if the caller provided a non-positive limit.
	**/
	// Skip reporting when the limit is zero or negative.
	if ( limit <= 0 ) {
		return;
	}

	/**
	*    Emit bounded diagnostics for the supplied entity list.
	**/
	// Determine the number of entries to report.
	const int32_t reportCount = std::min( ( int32_t )list.entities.size(), limit );
	// Print a summary header.
	gi.dprintf( "[NAV2][EntitySemantics] inline_bsp_count=%d report=%d\n", ( int32_t )list.entities.size(), reportCount );

	// Iterate the bounded list and print each entry.
	for ( int32_t index = 0; index < reportCount; index++ ) {
		// Reference the entry once for readability.
		const nav2_inline_bsp_entity_info_t &info = list.entities[ ( size_t )index ];
		// Print a single-line summary for each inline BSP entity.
		gi.dprintf( "[NAV2][EntitySemantics] ent=%d type=%d movetype=%d solid=%d model=%d inline_idx=%d headnode_valid=%d roles=0x%08x inline=%d valid=%d\n",
			info.owner_entnum,
			info.entity_type,
			( int32_t )info.movetype,
			info.solid,
			info.model_index,
			info.inline_model_index,
			info.headnode_valid ? 1 : 0,
			info.role_flags,
			info.has_inline_model ? 1 : 0,
			info.model_data_valid ? 1 : 0 );
	}
}

/**
*	@brief	Keep the nav2 inline BSP entity semantics module represented in the build.
**/
void SVG_Nav2_EntitySemantics_ModuleAnchor( void ) {
}
