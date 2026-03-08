/********************************************************************
*
*
*	SVGame: VoxelMesh Traversing
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_debug.h"
#include "svgame/nav/svg_nav_generate.h"
#include "svgame/nav/svg_nav_path_process.h"
#include "svgame/nav/svg_nav_request.h"
#include "svgame/nav/svg_nav_traversal.h"
#include "svgame/nav/svg_nav_traversal_async.h"

#include "svgame/entities/svg_player_edict.h"

// <Q2RTXP>: TODO: Move to shared? Ehh.. 
// Common BSP access for navigation generation.
#include "common/bsp.h"
#include "common/files.h"

#include <cmath>

extern cvar_t *nav_max_step;
extern cvar_t *nav_max_drop_height_cap;
extern cvar_t *nav_debug_draw;
extern cvar_t *nav_debug_draw_path;
/**
*    Targeted pathfinding diagnostics:
*        These are intentionally gated behind `nav_debug_draw >= 2` to avoid
*        spamming logs during normal gameplay.
**/
inline bool Nav_PathDiagEnabled( void ) {
	return nav_debug_draw && nav_debug_draw->integer >= 2;
}

static void Nav_Debug_PrintNodeRef( const char *label, const nav_node_ref_t &n ) {
	if ( !label ) {
		label = "node";
	}

	gi.dprintf( "[DEBUG][NavPath][Diag] %s: pos(%.1f %.1f %.1f) key(leaf=%d tile=%d cell=%d layer=%d)\n",
		label,
		n.worldPosition.x, n.worldPosition.y, n.worldPosition.z,
		n.key.leaf_index, n.key.tile_index, n.key.cell_index, n.key.layer_index );
}

/**
*    @brief	Convert an edge rejection enum value into a stable diagnostic string.
*    @param	reason	Edge rejection reason to stringify.
*    @return	Constant string suitable for debug logging.
**/
static const char *Nav_EdgeRejectReasonToString( const nav_edge_reject_reason_t reason ) {
	switch ( reason ) {
	case nav_edge_reject_reason_t::None: return "None";
	case nav_edge_reject_reason_t::TileRouteFilter: return "TileRouteFilter";
	case nav_edge_reject_reason_t::NoNode: return "NoNode";
	case nav_edge_reject_reason_t::DropCap: return "DropCap";
	case nav_edge_reject_reason_t::StepTest: return "StepTest";
	case nav_edge_reject_reason_t::ObstructionJump: return "ObstructionJump";
	case nav_edge_reject_reason_t::Occupancy: return "Occupancy";
	case nav_edge_reject_reason_t::Other: return "Other";
	default: return "Unknown";
	}
}

/**
*    @brief	Emit per-reason edge rejection counters for an A* state.
*    @param	prefix	Diagnostic prefix/tag identifying the call site.
*    @param	state	A* state containing reason counters.
**/
static void Nav_Debug_LogEdgeRejectReasonCounters( const char *prefix, const nav_a_star_state_t &state ) {
	/**
	*    Iterate all known reject-reason slots so diagnostics remain complete
	*    even when only a subset of reasons is currently non-zero.
	**/
	for ( int32_t reasonIndex = ( int32_t )nav_edge_reject_reason_t::None;
		reasonIndex <= ( int32_t )nav_edge_reject_reason_t::Other;
		reasonIndex++ ) {
		const nav_edge_reject_reason_t reason = ( nav_edge_reject_reason_t )reasonIndex;
		gi.dprintf( "%s edge_reject_reason[%d]=%d (%s)\n",
			prefix ? prefix : "[NavPath][Diag]",
			reasonIndex,
			state.edge_reject_reason_counts[ reasonIndex ],
			Nav_EdgeRejectReasonToString( reason ) );
	}
}

/**
*    @brief	Find the nearest Z-layer delta for the given node within its cell.
*    @param	mesh		Navigation mesh containing the node.
*    @param	node		Navigation node to inspect.
*    @param	out_delta	[out] Closest absolute Z delta to another layer in the same cell.
*    @param	out_layers	[out] Number of layers found in the cell (optional).
*    @return	True if a valid delta was found, false otherwise.
*    @note	This is a diagnostic helper used to report missing vertical connectivity.
**/
/**
*    @brief    Find the nearest Z-layer delta for the given node within its cell.
*    @param    mesh        Navigation mesh containing the node.
*    @param    node        Navigation node to inspect.
*    @param    out_delta   [out] Closest absolute Z delta to another layer in the same cell.
*    @param    out_layers  [out] Number of layers found in the cell (optional).
*    @return   True if a valid delta was found, false otherwise.
*    @note     This is a diagnostic helper used to report missing vertical connectivity.
**/
static const bool Nav_Debug_FindNearestLayerDelta( const nav_mesh_t *mesh, const nav_node_ref_t &node, double *out_delta, int32_t *out_layers ) {
	/**
	*    Sanity: require mesh and output pointer.
	**/
	if ( !mesh || !out_delta ) {
		return false;
	}

	/**
	*    Validate tile/cell indices before inspecting layers.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	// Use safe tile cell accessor to avoid dereferencing a possibly-null
	// `tile.cells` pointer in sparse tiles.
	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, const_cast< nav_tile_t * >( &tile ) );
	const nav_xy_cell_t *cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || node.key.cell_index < 0 || node.key.cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t &cell = cellsPtr[ node.key.cell_index ];
	if ( out_layers ) {
		*out_layers = cell.num_layers;
	}
	if ( cell.num_layers <= 1 || !cell.layers ) {
		return false;
	}

	/**
	*    Scan all layers to find the closest Z delta.
	**/
	const double current_z = node.worldPosition.z;
	double best_delta = std::numeric_limits<double>::max();
	for ( int32_t li = 0; li < cell.num_layers; li++ ) {
		// Skip the current layer itself.
		if ( li == node.key.layer_index ) {
			continue;
		}
		const double layer_z = ( double )cell.layers[ li ].z_quantized * mesh->z_quant;
		const double dz = fabsf( ( float )( layer_z - current_z ) );
		if ( dz < best_delta ) {
			best_delta = dz;
		}
	}

	/**
	*    Output the closest delta when a candidate was found.
	**/
	if ( best_delta == std::numeric_limits<double>::max() ) {
		return false;
	}
	*out_delta = best_delta;
	return true;
}

/**
*  @brief	Resolve the canonical tile, cell, and layer for a nav node reference.
*  @param	mesh		Navigation mesh.
*  @param	node		Node reference to inspect.
*  @param	out_tile	[out] Resolved tile pointer.
*  @param	out_cell	[out] Resolved cell pointer.
*  @param	out_layer	[out] Resolved layer pointer.
*  @return	True if the node maps to valid canonical nav storage.
**/
static const bool Nav_TryGetNodeLayerView( const nav_mesh_t *mesh, const nav_node_ref_t &node, const nav_tile_t **out_tile, const nav_xy_cell_t **out_cell, const nav_layer_t **out_layer ) {
	/**
	*    Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_tile || !out_cell || !out_layer ) {
		return false;
	}

	/**
	*    Validate the canonical world-tile index before reading sparse storage.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, &tile );
	const nav_xy_cell_t *cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || node.key.cell_index < 0 || node.key.cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t &cell = cellsPtr[ node.key.cell_index ];
	auto layersView = SVG_Nav_Cell_GetLayers( &cell );
	const nav_layer_t *layersPtr = layersView.first;
	const int32_t layerCount = layersView.second;
	if ( !layersPtr || node.key.layer_index < 0 || node.key.layer_index >= layerCount ) {
		return false;
	}

	/**
	*    Return the resolved canonical views.
	**/
	*out_tile = &tile;
	*out_cell = &cell;
	*out_layer = &layersPtr[ node.key.layer_index ];
	return true;
}

/**
*  @brief	Convert a node reference into global cell-grid coordinates.
*  @param	mesh		Navigation mesh.
*  @param	node		Node reference to inspect.
*  @param	out_cell_x	[out] Global cell X coordinate.
*  @param	out_cell_y	[out] Global cell Y coordinate.
*  @return	True if the node could be mapped to a valid grid cell.
**/
static const bool Nav_TryGetGlobalCellCoords( const nav_mesh_t *mesh, const nav_node_ref_t &node, int32_t *out_cell_x, int32_t *out_cell_y ) {
	/**
	*    Sanity checks: require mesh and output storage.
	**/
	if ( !mesh || !out_cell_x || !out_cell_y ) {
		return false;
	}

	/**
	*    Resolve the canonical tile first so we can derive local cell coordinates.
	**/
	if ( node.key.tile_index < 0 || node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t &tile = mesh->world_tiles[ node.key.tile_index ];
	const int32_t local_cell_x = node.key.cell_index % mesh->tile_size;
	const int32_t local_cell_y = node.key.cell_index / mesh->tile_size;

	/**
	*    Expand tile-local coordinates into world-grid coordinates.
	**/
	*out_cell_x = ( tile.tile_x * mesh->tile_size ) + local_cell_x;
	*out_cell_y = ( tile.tile_y * mesh->tile_size ) + local_cell_y;
	return true;
}

/**
*  @brief	Convert a global cell coordinate into a cell-center world position.
*  @param	mesh		Navigation mesh.
*  @param	cell_x		Global cell X coordinate.
*  @param	cell_y		Global cell Y coordinate.
*  @param	z		Desired world-space Z for the query point.
*  @return	World-space cell center carrying the provided Z height.
**/
static inline Vector3 Nav_GlobalCellCenterToWorld( const nav_mesh_t *mesh, const int32_t cell_x, const int32_t cell_y, const double z ) {
	return Vector3{
		( float )( ( ( double )cell_x + 0.5 ) * mesh->cell_size_xy ),
		( float )( ( ( double )cell_y + 0.5 ) * mesh->cell_size_xy ),
		( float )z
	};
}

/**
*  @brief	Convert a quantized layer clearance value into world units.
*  @param	mesh		Navigation mesh.
*  @param	layer		Layer holding the quantized clearance.
*  @return	World-space clearance above this layer's walkable floor sample.
**/
static inline double Nav_GetLayerClearanceWorld( const nav_mesh_t *mesh, const nav_layer_t &layer ) {
	return mesh ? ( ( double )layer.clearance * mesh->z_quant ) : 0.0;
}

/**
*  @brief	Compute floor-division for signed global cell coordinates.
*  @param	value	Signed dividend.
*  @param	divisor	Positive divisor.
*  @return	Mathematical floor of `value / divisor`.
**/
static inline int32_t Nav_Traversal_FloorDiv( const int32_t value, const int32_t divisor ) {
	/**
	*    Sanity checks: require a positive divisor.
	**/
	if ( divisor <= 0 ) {
		return 0;
	}

	/**
	*    Use integer truncation for non-negative inputs.
	**/
	if ( value >= 0 ) {
		return value / divisor;
	}

	/**
	*    Negative inputs need explicit floor semantics.
	**/
	return -( ( -value + divisor - 1 ) / divisor );
}

/**
*  @brief	Compute a positive modulo for signed global cell coordinates.
*  @param	value	Signed input value.
*  @param	modulus	Positive modulus.
*  @return	Value wrapped into `[0, modulus)`.
**/
static inline int32_t Nav_Traversal_PosMod( const int32_t value, const int32_t modulus ) {
	/**
	*    Sanity checks: require a positive modulus.
	**/
	if ( modulus <= 0 ) {
		return 0;
	}

	/**
	*    Normalize the remainder into the tile-local range.
	**/
	const int32_t remainder = value % modulus;
	return ( remainder < 0 ) ? ( remainder + modulus ) : remainder;
}

/**
*  @brief	Resolve a canonical intermediate segment node directly from global cell coordinates.
*  @param	mesh		Navigation mesh.
*  @param	current_node	Current canonical segment start node.
*  @param	target_cell_x	Global target cell X coordinate.
*  @param	target_cell_y	Global target cell Y coordinate.
*  @param	desired_z	Desired world-space Z for layer selection.
*  @param	out_node	[out] Resolved canonical target node.
*  @return	True if a walkable layer in the target cell could be resolved.
*  @note	This keeps segmented XY bridge validation on exact walkable tile/cell storage instead of
*  			falling back to a world-position lookup that can miss sparse but valid intermediate cells.
**/
static const bool Nav_TryResolveIntermediateSegmentNodeExact( const nav_mesh_t *mesh, const nav_node_ref_t &current_node,
	const int32_t target_cell_x, const int32_t target_cell_y, const double desired_z, nav_node_ref_t *out_node ) {
	/**
	*    Sanity checks: require mesh storage, output storage, and a valid current tile reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}
	if ( current_node.key.tile_index < 0 || current_node.key.tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	/**
	*    Map the requested global cell back into canonical tile-local addressing.
	**/
	const int32_t target_tile_x = Nav_Traversal_FloorDiv( target_cell_x, mesh->tile_size );
	const int32_t target_tile_y = Nav_Traversal_FloorDiv( target_cell_y, mesh->tile_size );
	const int32_t target_local_x = Nav_Traversal_PosMod( target_cell_x, mesh->tile_size );
	const int32_t target_local_y = Nav_Traversal_PosMod( target_cell_y, mesh->tile_size );
	const int32_t target_cell_index = ( target_local_y * mesh->tile_size ) + target_local_x;

	/**
	*    Resolve the canonical tile and reject missing sparse cells before reading layer storage.
	**/
	const nav_world_tile_key_t target_tile_key = { .tile_x = target_tile_x, .tile_y = target_tile_y };
	auto tile_it = mesh->world_tile_id_of.find( target_tile_key );
	if ( tile_it == mesh->world_tile_id_of.end() ) {
		return false;
	}

	const int32_t target_tile_index = tile_it->second;
	if ( target_tile_index < 0 || target_tile_index >= ( int32_t )mesh->world_tiles.size() ) {
		return false;
	}

	const nav_tile_t *target_tile = &mesh->world_tiles[ target_tile_index ];
	if ( !target_tile || !target_tile->presence_bits ) {
		return false;
	}

	// Reject sparse cells that are not marked present in the tile bitset.
	const int32_t word_index = target_cell_index >> 5;
	const int32_t bit_index = target_cell_index & 31;
	if ( ( target_tile->presence_bits[ word_index ] & ( 1u << bit_index ) ) == 0 ) {
		return false;
	}

	/**
	*    Resolve the target cell storage and select the best walkable layer for the requested step height.
	**/
	auto cellsView = SVG_Nav_Tile_GetCells( mesh, target_tile );
	const nav_xy_cell_t *cellsPtr = cellsView.first;
	const int32_t cellsCount = cellsView.second;
	if ( !cellsPtr || target_cell_index < 0 || target_cell_index >= cellsCount ) {
		return false;
	}

	const nav_xy_cell_t *target_cell = &cellsPtr[ target_cell_index ];
	if ( !target_cell || target_cell->num_layers <= 0 || !target_cell->layers ) {
		return false;
	}

	int32_t target_layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, target_cell, desired_z, &target_layer_index ) ) {
		return false;
	}

	/**
	*    Populate the resolved canonical node using the selected walkable layer.
	**/
	const nav_layer_t *target_layer = &target_cell->layers[ target_layer_index ];
	out_node->key.leaf_index = current_node.key.leaf_index;
	out_node->key.tile_index = target_tile_index;
	out_node->key.cell_index = target_cell_index;
	out_node->key.layer_index = target_layer_index;
	out_node->worldPosition = Nav_NodeWorldPosition( mesh, target_tile, target_cell_index, target_layer );
	return true;
}

/**
*  @brief	Forward declaration for the single-step traversal helper used by segmented ramps.
*  @param	mesh		Navigation mesh.
*  @param	start_node	Resolved start node.
*  @param	end_node	Resolved end node.
*  @param	mins		Agent bounding box minimums.
*  @param	maxs		Agent bounding box maximums.
*  @param	clip_entity	Entity to use for clipping (nullptr for world).
*  @param	policy		Optional path policy for tuning step behavior.
*  @return	True if the traversal is possible, false otherwise.
**/
static const bool Nav_CanTraverseStep_ExplicitBBox_Single( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy, nav_edge_reject_reason_t *out_reason = nullptr, const cm_contents_t stepTraceMask = CM_CONTENTMASK_MONSTERSOLID );

/**
*  @brief	Performs a simple PMove-like step traversal test (3-trace) with explicit agent bbox.
* 			This is intentionally conservative and is used only to validate edges in A*:
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
*  @param	mesh			Navigation mesh.
*  @param	startPos		Start world position.
*  @param	endPos			End world position.
*  @param	mins			Agent bounding box minimums.
*  @param	maxs			Agent bounding box maximums.
*  @param	clip_entity		Entity to use for clipping (nullptr for world).
*  @param	policy			Optional path policy for tuning step behavior.
*  @return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep_ExplicitBBox( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy, nav_edge_reject_reason_t *out_reason, const cm_contents_t stepTraceMask ) {
	/**
	*    Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	*    Resolve canonical start/end nodes so edge validation can work from tile/cell/layer
	*    facts rather than generic world tracing.
	**/
	nav_node_ref_t start_node = {};
	nav_node_ref_t end_node = {};
	if ( !Nav_FindNodeForPosition( mesh, startPos, startPos[ 2 ], &start_node, true ) ||
		!Nav_FindNodeForPosition( mesh, endPos, endPos[ 2 ], &end_node, true ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	/**
	*    Early out when both points already resolve to the same canonical node.
	**/
	if ( start_node.key == end_node.key ) {
		return true;
	}

	/**
	*    Canonical-edge validation:
	*        Reuse the already resolved node references so the validator does not
	*        re-query the same walkable cell centers through `Nav_FindNodeForPosition()`.
	**/
	return Nav_CanTraverseStep_ExplicitBBox_NodeRefs( mesh, start_node, end_node, mins, maxs, clip_entity, policy, out_reason, stepTraceMask );
}

/**
*  @brief	Validate a traversal edge from canonical node references.
*  @param	mesh		Navigation mesh.
*  @param	start_node	Resolved canonical start node.
*  @param	end_node	Resolved canonical end node.
*  @param	mins		Agent bounding box minimums.
*  @param	maxs		Agent bounding box maximums.
*  @param	clip_entity	Entity to use for clipping (nullptr for world).
*  @param	policy		Optional path policy for tuning step behavior.
*  @param	out_reason	[out] Optional reject reason.
*  @param	stepTraceMask	Reserved mask parameter kept aligned with the position overload.
*  @return	True if the traversal is possible, false otherwise.
*  @note	This keeps XY validation on the walkable canonical cells already selected by the caller and
*  			limits additional Z probing to the segmented step path only when stepping requires it.
**/
const bool Nav_CanTraverseStep_ExplicitBBox_NodeRefs( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node,
	const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy,
	nav_edge_reject_reason_t *out_reason, const cm_contents_t stepTraceMask ) {
	/**
	*    Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	*    Early out when both endpoints already reference the same canonical node.
	**/
	if ( start_node.key == end_node.key ) {
		return true;
	}

	/**
	*    Compute the required climb to determine if segmentation is needed.
	**/
   const Vector3 &startPos = start_node.worldPosition;
	const Vector3 &endPos = end_node.worldPosition;
	const double desiredDz = ( double )endPos[ 2 ] - ( double )startPos[ 2 ];
	const double requiredUp = std::max( 0.0, desiredDz );
	const double stepSize = ( policy && policy->max_step_height > 0.0 ) ? ( double )policy->max_step_height : ( double )mesh->max_step;

	/**
	*    Drop cap enforcement (total edge):
	*        Ensure long downward transitions do not bypass the cap when we segment
	*        by horizontal distance for multi-cell ramps.
	**/
	const double dropCap = ( policy ? ( double )policy->max_drop_height_cap : ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 128.0f ) );
	const double requiredDown = std::max( 0.0, ( double )startPos[ 2 ] - ( double )endPos[ 2 ] );
	// Reject edges that drop farther than the configured cap.
	if ( requiredDown > 0.0 && dropCap >= 0.0 && requiredDown > dropCap ) {
		if ( out_reason ) *out_reason = nav_edge_reject_reason_t::DropCap;
		return false;
	}

	/**
	*    Compute horizontal distance to decide if we should segment long, shallow ramps.
	*        Multi-cell ramps can fail a single step-test even with a small Z delta, so
	*        we segment by XY distance to validate each cell-scale portion.
	**/
	const Vector3 fullDelta = QM_Vector3Subtract( endPos, startPos );
	int32_t start_cell_x = 0;
	int32_t start_cell_y = 0;
	int32_t end_cell_x = 0;
	int32_t end_cell_y = 0;
	if ( !Nav_TryGetGlobalCellCoords( mesh, start_node, &start_cell_x, &start_cell_y ) ||
		!Nav_TryGetGlobalCellCoords( mesh, end_node, &end_cell_x, &end_cell_y ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	const int32_t delta_cell_x = end_cell_x - start_cell_x;
	const int32_t delta_cell_y = end_cell_y - start_cell_y;
	// Determine how many segments we need for vertical climb and for multi-cell ramps.
	const int32_t stepCountVertical = ( requiredUp > stepSize && stepSize > 0.0 )
		? ( int32_t )std::ceil( requiredUp / stepSize )
		: 1;
	const int32_t stepCountHorizontal = std::max( std::abs( delta_cell_x ), std::abs( delta_cell_y ) );
	   // Use the most conservative segmentation to satisfy both climb and ramp constraints.
	const int32_t stepCount = std::max( 1, std::max( stepCountVertical, stepCountHorizontal ) );

	 /**
	 *    Segmented ramp/stair handling:
	 *        Split long climbs into <= max_step_height sub-steps so
	 *        step validation mirrors PMove-style step constraints.
	 **/
	if ( stepCount > 1 ) {
		  /**
		*    Segment by canonical cell hops instead of interpolated trace points.
		  *    This keeps each sub-step aligned with actual nav cells/layers.
		  **/
		nav_node_ref_t segmentStartNode = start_node;
		   // Validate each sub-step in sequence.
		for ( int32_t stepIndex = 1; stepIndex <= stepCount; stepIndex++ ) {
			const double t = ( double )stepIndex / ( double )stepCount;
			const int32_t segment_cell_x = start_cell_x + ( int32_t )std::lround( ( double )delta_cell_x * t );
			const int32_t segment_cell_y = start_cell_y + ( int32_t )std::lround( ( double )delta_cell_y * t );

			nav_node_ref_t segmentEndNode = {};
			if ( stepIndex == stepCount ) {
				segmentEndNode = end_node;
			} else {
             /**
				*    Resolve the next segment from the current segment surface instead of the full-edge
				*    interpolated Z. Intermediate cells on stairs/ramps often do not contain a node at the
				*    synthetic interpolated height even though a valid step-sized continuation exists.
				**/
				// Compute the remaining vertical delta from this segment start toward the final end node.
				const double remaining_segment_dz = ( double )end_node.worldPosition[ 2 ] - ( double )segmentStartNode.worldPosition[ 2 ];
				// Clamp the desired rise/drop for this sub-step to one PMove-sized step.
				const double segment_step_dz = QM_Clamp( remaining_segment_dz, -stepSize, stepSize );
				// Use the clamped per-step surface target as the primary lookup height for this segment cell.
				const double segment_desired_z = ( double )segmentStartNode.worldPosition[ 2 ] + segment_step_dz;
				/**
                 *    Resolve the intermediate segment from exact tile/cell storage so short XY bridge probes stay
					*    aligned with walkable surface cells instead of relying on a world-position lookup.
				**/
                    if ( !Nav_TryResolveIntermediateSegmentNodeExact( mesh, segmentStartNode, segment_cell_x, segment_cell_y, segment_desired_z, &segmentEndNode ) ) {
					/**
                     *    Conservative fallback: if the stepped desired Z does not map to a walkable layer in this
						*    exact cell, retry against the current surface height before rejecting the segment.
					**/
                      if ( !Nav_TryResolveIntermediateSegmentNodeExact( mesh, segmentStartNode, segment_cell_x, segment_cell_y, segmentStartNode.worldPosition[ 2 ], &segmentEndNode ) ) {
						if ( out_reason ) {
							*out_reason = nav_edge_reject_reason_t::NoNode;
						}
						return false;
					}
				}
			}

			/**
			*    Skip duplicate canonical nodes produced by rounding so segmentation stays monotonic.
			**/
			if ( segmentStartNode.key == segmentEndNode.key ) {
				continue;
			}

			/**
			*    Validate the current sub-step using the single-step routine.
			*    Abort immediately if any sub-step fails.
			**/
			if ( !Nav_CanTraverseStep_ExplicitBBox_Single( mesh, segmentStartNode, segmentEndNode, mins, maxs, clip_entity, policy, out_reason, stepTraceMask ) ) {
				return false;
			}

			// Advance to the next segment.
			segmentStartNode = segmentEndNode;
		}

		// All sub-steps succeeded.
		return true;
	}

	/**
	*    Fallback: use the single-step validation when no segmentation is needed.
	**/
	return Nav_CanTraverseStep_ExplicitBBox_Single( mesh, start_node, end_node, mins, maxs, clip_entity, policy, out_reason, stepTraceMask );
}

/**
*   @brief	Probe a small XY neighborhood to rescue sync endpoint node resolution on boundary origins.
*   @param	mesh		Navigation mesh.
*   @param	query_center	Endpoint center position that failed direct lookup.
*   @param	start_center	Original request start center used by blend lookup.
*   @param	goal_center	Original request goal center used by blend lookup.
*   @param	query_is_goal	True when rescuing the goal endpoint, false for the start endpoint.
*   @param	policy		Resolved path policy controlling blend behavior.
*   @param	out_node	[out] Best rescued canonical node.
*   @return	True when a nearby endpoint node was recovered.
*   @note	This mirrors the async worker's boundary-origin hardening so sync path generation can recover
*   		from sound or interaction points that land directly on tile or cell boundaries.
**/
static const bool Nav_Traversal_TryResolveBoundaryOriginNode( const nav_mesh_t *mesh, const Vector3 &query_center,
	const Vector3 &start_center, const Vector3 &goal_center, const bool query_is_goal,
	const svg_nav_path_policy_t &policy, nav_node_ref_t *out_node ) {
	/**
	*   	Sanity checks: require mesh storage and an output node reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/**
	*   	Build a compact probe ring around the failed endpoint.
	*   		Use half-cell and one-cell offsets so boundary-origin path requests can snap onto a nearby
	*   		valid walk surface without skipping across large gaps.
	**/
	const float mesh_cell_size = ( float )mesh->cell_size_xy;
	const float half_cell_raw = mesh_cell_size * 0.5f;
	const float half_cell = ( half_cell_raw > 1.0f ) ? half_cell_raw : 1.0f;
	const float full_cell = ( half_cell > mesh_cell_size ) ? half_cell : mesh_cell_size;
	const Vector3 probe_offsets[] = {
		{ half_cell, 0.0f, 0.0f },
		{ -half_cell, 0.0f, 0.0f },
		{ 0.0f, half_cell, 0.0f },
		{ 0.0f, -half_cell, 0.0f },
		{ half_cell, half_cell, 0.0f },
		{ half_cell, -half_cell, 0.0f },
		{ -half_cell, half_cell, 0.0f },
		{ -half_cell, -half_cell, 0.0f },
		{ full_cell, 0.0f, 0.0f },
		{ -full_cell, 0.0f, 0.0f },
		{ 0.0f, full_cell, 0.0f },
		{ 0.0f, -full_cell, 0.0f }
	};

	/**
	*   	Keep the closest rescued node to the original failed endpoint.
	**/
	bool found_node = false;
	double best_distance_sqr = std::numeric_limits<double>::infinity();
	nav_node_ref_t best_node = {};

	// Probe each nearby XY offset and keep the closest successfully resolved canonical node.
	for ( const Vector3 &probe_offset : probe_offsets ) {
		// Shift the failed endpoint by the current local probe offset.
		const Vector3 probe_center = QM_Vector3Add( query_center, probe_offset );
		nav_node_ref_t candidate_node = {};
		bool resolved = false;

		/**
		*   	Reuse the caller's configured lookup policy so boundary rescue stays aligned with the
		*   	same blend and fallback behavior as the primary endpoint resolution path.
		**/
		if ( policy.enable_goal_z_layer_blend ) {
			resolved = Nav_FindNodeForPosition_BlendZ( mesh, probe_center, start_center.z, goal_center.z,
				start_center, goal_center, policy.blend_start_dist, policy.blend_full_dist, &candidate_node, true );
		} else {
			const double desired_z = query_is_goal ? goal_center.z : start_center.z;
			resolved = Nav_FindNodeForPosition( mesh, probe_center, desired_z, &candidate_node, true );
		}

		// Continue probing until we find at least one nearby canonical node.
		if ( !resolved ) {
			continue;
		}

		// Score the candidate by how closely its recovered world position matches the original endpoint.
		const double candidate_distance_sqr = QM_Vector3DistanceSqr( candidate_node.worldPosition, query_center );
		if ( !found_node || candidate_distance_sqr < best_distance_sqr ) {
			found_node = true;
			best_distance_sqr = candidate_distance_sqr;
			best_node = candidate_node;
		}
	}

	/**
	*   	Commit the closest rescued endpoint node when probing succeeded.
	**/
	if ( !found_node ) {
		return false;
	}

	*out_node = best_node;
	return true;
}

/**
*   @brief	Resolve one traversal endpoint using direct lookup, boundary rescue, and same-cell rescue.
*   @param	mesh		Navigation mesh.
*   @param	query_center	Endpoint center position to resolve.
*   @param	start_center	Full request start center used by blended lookup.
*   @param	goal_center	Full request goal center used by blended lookup.
*   @param	query_is_goal	True when resolving the goal endpoint, false for the start endpoint.
*   @param	policy		Resolved path policy used for endpoint lookup.
*   @param	out_node	[out] Resolved canonical endpoint node.
*   @return	True when endpoint resolution succeeded.
*   @note	This keeps the sync traversal APIs aligned with the async worker's endpoint hardening.
**/
static const bool Nav_Traversal_TryResolveEndpointNode( const nav_mesh_t *mesh, const Vector3 &query_center,
	const Vector3 &start_center, const Vector3 &goal_center, const bool query_is_goal,
	const svg_nav_path_policy_t &policy, nav_node_ref_t *out_node ) {
	/**
	*   	Sanity checks: require mesh storage and an output node reference.
	**/
	if ( !mesh || !out_node ) {
		return false;
	}

	/**
	*   	Attempt direct endpoint lookup first using the requested blend policy.
	**/
	nav_node_ref_t resolved_node = {};
	bool resolved = false;
	if ( policy.enable_goal_z_layer_blend ) {
		resolved = Nav_FindNodeForPosition_BlendZ( mesh, query_center, start_center.z, goal_center.z,
			start_center, goal_center, policy.blend_start_dist, policy.blend_full_dist, &resolved_node, true );
	} else {
		const double desired_z = query_is_goal ? goal_center.z : start_center.z;
		resolved = Nav_FindNodeForPosition( mesh, query_center, desired_z, &resolved_node, true );
	}

	/**
	*   	Fallback to boundary-origin rescue when the raw endpoint center misses a walkable sample.
	**/
	if ( !resolved ) {
		resolved = Nav_Traversal_TryResolveBoundaryOriginNode( mesh, query_center, start_center, goal_center,
			query_is_goal, policy, &resolved_node );
	}

	/**
	*   	Early out when both direct lookup and boundary rescue failed.
	**/
	if ( !resolved ) {
		return false;
	}

	/**
	*   	Rescue isolated same-cell layers by preferring the best-connected layer variant in that cell.
	**/
	Nav_AStar_TrySelectConnectedSameCellLayer( mesh, resolved_node, &policy, &resolved_node );
	*out_node = resolved_node;
	return true;
}

/**
*   @brief	Resolve both traversal endpoints under one blend strategy.
*   @param	mesh		Navigation mesh.
*   @param	start_center	Request start in nav-center space.
*   @param	goal_center	Request goal in nav-center space.
*   @param	agent_mins	Agent bounding box minimums.
*   @param	agent_maxs	Agent bounding box maximums.
*   @param	policy		Optional path policy to copy and refine.
*   @param	enable_goal_z_layer_blend	Whether this strategy should use blended endpoint lookup.
*   @param	blend_start_dist	Distance at which goal-Z blending begins.
*   @param	blend_full_dist	Distance at which goal-Z blending fully favors the goal layer.
*   @param	out_start_node	[out] Resolved canonical start node.
*   @param	out_goal_node	[out] Resolved canonical goal node.
*   @return	True when both endpoints were resolved successfully.
**/
static const bool Nav_Traversal_TryResolveEndpointsForStrategy( const nav_mesh_t *mesh, const Vector3 &start_center,
	const Vector3 &goal_center, const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	nav_node_ref_t *out_start_node, nav_node_ref_t *out_goal_node ) {
	/**
	*   	Sanity checks: require mesh storage and output nodes.
	**/
	if ( !mesh || !out_start_node || !out_goal_node ) {
		return false;
	}

	/**
	*   	Build the resolved policy snapshot for this specific endpoint strategy.
	*   		This keeps sync endpoint lookup aligned with the agent hull and caller policy.
	**/
	svg_nav_path_policy_t resolvedPolicy = policy ? *policy : svg_nav_path_policy_t{};
	resolvedPolicy.agent_mins = agent_mins;
	resolvedPolicy.agent_maxs = agent_maxs;
	resolvedPolicy.enable_goal_z_layer_blend = enable_goal_z_layer_blend;
	if ( enable_goal_z_layer_blend ) {
		resolvedPolicy.blend_start_dist = ( blend_start_dist > 0.0 ) ? blend_start_dist : resolvedPolicy.blend_start_dist;
		resolvedPolicy.blend_full_dist = ( blend_full_dist > resolvedPolicy.blend_start_dist )
			? blend_full_dist
			: std::max( resolvedPolicy.blend_start_dist + mesh->z_quant, resolvedPolicy.blend_full_dist );
	}

	/**
	*   	Resolve the start and goal endpoints under the same policy snapshot.
	**/
	if ( !Nav_Traversal_TryResolveEndpointNode( mesh, start_center, start_center, goal_center, false, resolvedPolicy, out_start_node ) ) {
		return false;
	}
	if ( !Nav_Traversal_TryResolveEndpointNode( mesh, goal_center, start_center, goal_center, true, resolvedPolicy, out_goal_node ) ) {
		return false;
	}

	return true;
}

/**
*   @brief	Forward declaration for the shared sync A* search helper.
*   @param	mesh		Navigation mesh containing the voxel tiles/cells.
*   @param	start_node	Resolved canonical start node.
*   @param	goal_node	Resolved canonical goal node.
*   @param	agent_mins	Agent bounding box minimums in nav-center space.
*   @param	agent_maxs	Agent bounding box maximums in nav-center space.
*   @param	out_points	[out] Waypoints produced by the search.
*   @param	policy		Optional traversal policy.
*   @param	pathProcess	Optional per-entity path-process state.
*   @param	tileRouteFilter	Optional coarse tile-route restriction.
*   @return	True when a valid path was produced.
*   @note	`Nav_Traversal_TryGeneratePointsWithEndpointFallbacks()` calls this helper before its full
*   		definition appears later in the translation unit, so we declare it here explicitly.
**/
static bool Nav_AStarSearch( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points, const svg_nav_path_policy_t *policy = nullptr,
	const struct svg_nav_path_process_t *pathProcess = nullptr, const std::vector<nav_tile_cluster_key_t> *tileRouteFilter = nullptr );

/**
*   @brief	Generate traversal waypoints by trying primary and fallback endpoint-resolution strategies.
*   @param	mesh		Navigation mesh.
*   @param	start_center	Request start in nav-center space.
*   @param	goal_center	Request goal in nav-center space.
*   @param	agent_mins	Agent bounding box minimums.
*   @param	agent_maxs	Agent bounding box maximums.
*   @param	policy		Optional path policy for traversal and endpoint tuning.
*   @param	pathProcess	Optional path-process state for failure penalties.
*   @param	prefer_blended_lookup	True to try blended endpoint lookup first, false to try plain lookup first.
*   @param	blend_start_dist	Distance at which goal-Z blending begins.
*   @param	blend_full_dist	Distance at which goal-Z blending fully favors the goal layer.
*   @param	out_points	[out] Final traversal waypoints.
*   @return	True when any endpoint strategy produced a valid traversal path.
*   @note	This helper stops relying on a single layer-selection guess by trying both blended and unblended
*   		endpoint strategies before giving up.
**/
static const bool Nav_Traversal_TryGeneratePointsWithEndpointFallbacks( const nav_mesh_t *mesh, const Vector3 &start_center,
	const Vector3 &goal_center, const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy,
	const svg_nav_path_process_t *pathProcess, const bool prefer_blended_lookup, const double blend_start_dist,
	const double blend_full_dist, std::vector<Vector3> &out_points ) {
	/**
	*   	Sanity checks: require mesh storage.
	**/
	if ( !mesh ) {
		return false;
	}

	/**
	*   	Prepare the primary and secondary endpoint strategies.
	*   		The secondary strategy flips blend usage so sync traversal is not trapped by one layer guess.
	**/
	const bool strategyBlendModes[] = { prefer_blended_lookup, !prefer_blended_lookup };
	const int32_t strategyCount = ( strategyBlendModes[ 0 ] == strategyBlendModes[ 1 ] ) ? 1 : 2;

	/**
	*   	Evaluate each endpoint strategy until one produces a valid path.
	**/
	for ( int32_t strategyIndex = 0; strategyIndex < strategyCount; strategyIndex++ ) {
		const bool strategyUsesBlend = strategyBlendModes[ strategyIndex ];
		nav_node_ref_t start_node = {};
		nav_node_ref_t goal_node = {};
		if ( !Nav_Traversal_TryResolveEndpointsForStrategy( mesh, start_center, goal_center, agent_mins, agent_maxs,
			policy, strategyUsesBlend, blend_start_dist, blend_full_dist, &start_node, &goal_node ) ) {
			continue;
		}

		/**
		*   	Fast path: identical canonical endpoints already represent a trivial path.
		**/
		out_points.clear();
		if ( start_node.key == goal_node.key ) {
			out_points.push_back( start_node.worldPosition );
			out_points.push_back( goal_node.worldPosition );
			return true;
		}

		/**
		*   	Compute the coarse tile route from the resolved canonical endpoints so A* stays aligned
		*   	with the exact surfaces that endpoint resolution recovered.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( mesh, start_node.worldPosition, goal_node.worldPosition, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		/**
		*   	Attempt A* using the currently resolved endpoint pair.
		*   		If this fails, fall through and let the alternate endpoint strategy try again.
		**/
		if ( Nav_AStarSearch( mesh, start_node, goal_node, agent_mins, agent_maxs, out_points, policy, pathProcess, routeFilter ) ) {
			return true;
		}
	}

	return false;
}

/**
*
*
*
*   Navigation Pathfinding:
*
*
*
**/
/**
*	@brief	Perform A* search between two navigation nodes.
*	@param	mesh			Navigation mesh containing the voxel tiles/cells.
*	@param	start_node	Start navigation node reference.
*	@param	goal_node	Goal navigation node reference.
*	@param	agent_mins	Agent bounding box mins (in nav-center space).
*	@param	agent_maxs	Agent bounding box maxs (in nav-center space).
*	@param	out_points	[out] Waypoints (node positions) from start to goal.
*	@param	policy		Optional per-agent traversal tuning (step/drop/jump constraints).
*	@param	pathProcess	Optional per-entity path process used for failure backoff penalties.
*	@param	tileRouteFilter	Optional hierarchical filter restricting expansion to a coarse tile route.
*	@return	True if a path was found, false otherwise.
*	@note	This search validates each candidate edge using `Nav_CanTraverseStep_ExplicitBBox`.
*			It is therefore safe to consider additional neighbor offsets (e.g., 2-cell hops)
*			because collision/step feasibility is still enforced.
**/
/**
*	@brief	Perform a traversal search while reusing the async helper state machine.
*	@note	The shared helper documents neighbor offsets, drop limits, and chunked expansion so both sync and async callers stay aligned.
**/
static bool Nav_AStarSearch( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &goal_node,
  const Vector3 &agent_mins, const Vector3 &agent_maxs, std::vector<Vector3> &out_points, const svg_nav_path_policy_t *policy,
	const struct svg_nav_path_process_t *pathProcess, const std::vector<nav_tile_cluster_key_t> *tileRouteFilter ) {
	/**
	*\tPrepare the incremental A* state so we share the traversal rules with the async helpers.
	**/
	nav_a_star_state_t state = {};
	if ( !Nav_AStar_Init( &state, mesh, start_node, goal_node, agent_mins, agent_maxs, policy, tileRouteFilter, pathProcess ) ) {
		return false;
	}

 /**
	*    Synchronous rebuild budget:
	*        Disable per-step time limits so the helper can run to completion without per-step cap.
	**/
	state.search_budget_ms = 0;
	state.hit_time_budget = false;

	const int32_t expansionBudget = std::max( 1, SVG_Nav_GetAsyncRequestExpansions() );

	/**
	*    Step the helper repeatedly so it can honor its chunked expansion and time budgets.
	**/
	while ( state.status == nav_a_star_status_t::Running ) {
		Nav_AStar_Step( &state, expansionBudget );
	}

	   /**
	   *    Finalize the path when the search completes successfully.
	   **/
	if ( state.status == nav_a_star_status_t::Completed ) {
		const bool ok = Nav_AStar_Finalize( &state, &out_points );
		Nav_AStar_Reset( &state );
		return ok;
	}

	/**
	*    Cache diagnostic toggle so we can gate extra logging below.
	**/
	const bool navDiag = Nav_PathDiagEnabled();

	gi.dprintf( "[WARNING][NavPath][A*] Pathfinding failed: No path found from (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n",
		start_node.worldPosition.x, start_node.worldPosition.y, start_node.worldPosition.z,
		goal_node.worldPosition.x, goal_node.worldPosition.y, goal_node.worldPosition.z );
	gi.dprintf( "[WARNING][NavPath][A*] Search stats: explored=%d nodes, max_nodes=%d, open_list_empty=%s\n",
		( int32_t )state.nodes.size(),
		state.max_nodes,
		state.open_list.empty() ? "true" : "false" );

	/**
	*    Emit detailed diagnostics only when explicitly requested.
	**/
	if ( navDiag ) {
		// Rate-limit verbose diagnostics to avoid overflowing the net/console buffer
		// when many A* failures occur in quick succession. This keeps logs
		// useful while preventing spam that would drop messages.
		static uint64_t s_last_navpath_diag_ms = 0;
		const uint64_t nowMs = gi.GetRealSystemTime();
		const uint64_t diagCooldownMs = 200; // min interval between verbose prints
		if ( nowMs - s_last_navpath_diag_ms >= diagCooldownMs ) {
			s_last_navpath_diag_ms = nowMs;
			gi.dprintf( "[DEBUG][NavPath][Diag] A* summary: neighbor_tries=%d, no_node=%d, edge_reject=%d, tile_filter_reject=%d, stagnation=%d\n",
				state.neighbor_try_count, state.no_node_count, state.edge_reject_count, state.tile_filter_reject_count, state.stagnation_count );
			gi.dprintf( "[DEBUG][NavPath][Diag] A* inputs: max_step=%.1f max_drop=%.1f cell_size_xy=%.1f z_quant=%.1f\n",
				nav_max_step ? nav_max_step->value : -1.0f,
				nav_max_drop_height_cap ? nav_max_drop_height_cap->value : -1.0f,
				mesh ? ( float )mesh->cell_size_xy : -1.0f,
				mesh ? ( float )mesh->z_quant : -1.0f );
			Nav_Debug_PrintNodeRef( "start_node", start_node );
			Nav_Debug_PrintNodeRef( "goal_node", goal_node );

			if ( !state.saw_vertical_neighbor ) {
				double nearestDelta = 0.0;
				int32_t layerCount = 0;
				if ( Nav_Debug_FindNearestLayerDelta( mesh, start_node, &nearestDelta, &layerCount ) ) {
					gi.dprintf( "[DEBUG][NavPath][Diag] No vertical neighbors; nearest layer delta=%.1f (layers=%d)\n",
						nearestDelta, layerCount );
				} else {
					gi.dprintf( "[DEBUG][NavPath][Diag] No vertical neighbors; no alternate layers found in start cell.\n" );
				}
			}

			const std::vector<nav_tile_cluster_key_t> *routeFilter = state.tileRouteFilter;
			if ( routeFilter && !routeFilter->empty() ) {
				gi.dprintf( "[DEBUG][NavPath][Diag] cluster route enforced: %d tiles\n",
					( int32_t )routeFilter->size() );
			} else {
				gi.dprintf( "[DEBUG][NavPath][Diag] cluster route: (none)\n" );
			}
		} // rate limit
	}

 /**
	*    Print a concise failure reason based on the observed abort conditions.
	**/
	if ( state.hit_stagnation_limit ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Search failed due to excessive stagnation after %d iterations.\n", state.stagnation_count );
	} else if ( ( int32_t )state.nodes.size() >= state.max_nodes ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Search node limit reached (%d nodes). The navmesh may be too large or the goal is very far.\n", state.max_nodes );
	} else if ( state.open_list.empty() ) {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Open list exhausted. This typically means:\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   1. No navmesh connectivity exists between start and goal Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   2. All potential paths are blocked by obstacles\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   3. Edge validation (Nav_CanTraverseStep) rejected all connections\n" );
		gi.dprintf( "[WARNING][NavPath][A*] Suggestions:\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   - Check nav_max_step (current: %.1f) and nav_max_drop_height_cap (current: %.1f)\n",
			nav_max_step ? nav_max_step->value : -1.0f,
			nav_max_drop_height_cap ? nav_max_drop_height_cap->value : -1.0f );
		gi.dprintf( "[WARNING][NavPath][A*]   - Verify navmesh has stairs/ramps connecting the Z-levels\n" );
		gi.dprintf( "[WARNING][NavPath][A*]   - Use 'nav_debug_draw 1' to visualize the navmesh\n" );
	} else {
		gi.dprintf( "[WARNING][NavPath][A*] Failure reason: Unknown (status=%d).\n", ( int32_t )state.status );
	}

  // Emit extra diagnostic payload when A* fails so callers can inspect node mapping
	// and candidate rejection counts. This log is gated by the global async stats
	// cvar to avoid overwhelming release logs.
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[NavPath][Diag] neighbor_tries=%d no_node=%d edge_reject=%d tile_filter_reject=%d stagnation=%d\n",
			state.neighbor_try_count, state.no_node_count, state.edge_reject_count, state.tile_filter_reject_count, state.stagnation_count );
	   // Emit per-reason counters to show exactly which rejection paths dominated.
		Nav_Debug_LogEdgeRejectReasonCounters( "[NavPath][Diag]", state );
	}

	Nav_AStar_Reset( &state );
	return false;
}



//////////////////////////////////////////////////////////////////////////
/**
*
*
*
*   Navigation System "Traversal Operations":
*
*
*
**/

/**
*   @brief  Generate a traversal path between two world-space origins.
*           Uses the navigation voxelmesh and A* search to produce waypoints.
*   @param  start_origin    World-space starting origin.
*   @param  goal_origin     World-space destination origin.
*   @param  out_path        Output path result (caller must free).
*   @return True if a path was found, false otherwise.
**/
const bool SVG_Nav_GenerateTraversalPathForOrigin( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	// Mesh pointer used for lookup/conversions.
	const nav_mesh_t *mesh = g_nav_mesh.get();

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};

	// Public API accepts feet-origin (z at feet). Convert to nav-center
	// space by applying the mesh agent center offset so node lookups use the
	// same reference as the async path preparer.
	const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin );
	const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin );

	if ( !Nav_FindNodeForPosition( mesh, start_center, start_center[ 2 ], &start_node, true ) ) {
		return false;
	}

	if ( !Nav_FindNodeForPosition( mesh, goal_center, goal_center[ 2 ], &goal_node, true ) ) {
		return false;
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.worldPosition );
		points.push_back( goal_node.worldPosition );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
     /**
		*    Boundary-origin hardening:
		*        Use resolved canonical node positions for the coarse route query so tile-route selection
		*        stays aligned with the surfaces that node lookup actually recovered.
		**/
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_node.worldPosition, goal_node.worldPosition, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, g_nav_mesh->agent_mins, g_nav_mesh->agent_maxs, points, nullptr, nullptr, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 ) * out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );

	/**
	*	Debug draw: path segments.
	**/
	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			// Distance filter on segment midpoint.
			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}
	const nav_mesh_t *mesh = g_nav_mesh.get();

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	bool startResolved = false;
	bool goalResolved = false;

	if ( enable_goal_z_layer_blend ) {
		// Convert caller feet-origin to nav-center using mesh agent hull.
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin );

		if ( !Nav_FindNodeForPosition_BlendZ( mesh, start_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &start_node, true ) ) {
			return false;
		}
		// Use blend for goal selection as well to prefer the goal's Z layer when far away.
		if ( !Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &goal_node, true ) ) {
			return false;
		}
	} else {
		// Convert caller feet-origin to nav-center using provided agent bbox.
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin );

		if ( !Nav_FindNodeForPosition( mesh, start_center, start_center[ 2 ], &start_node, true ) ) {
			return false;
		}
		if ( !Nav_FindNodeForPosition( mesh, goal_center, goal_center[ 2 ], &goal_node, true ) ) {
			return false;
		}
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.worldPosition );
		points.push_back( goal_node.worldPosition );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
     /**
		*    Boundary-origin hardening:
		*        Use resolved canonical node positions for the coarse route query so tile-route selection
		*        stays aligned with the surfaces that node lookup actually recovered.
		**/
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_node.worldPosition, goal_node.worldPosition, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( mesh, start_node, goal_node, mesh->agent_mins, mesh->agent_maxs, points, nullptr, nullptr, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );

	// Cache for always-on debug draw.
	NavDebug_PurgeCachedPaths();

	nav_debug_cached_path_t cached = {};
	cached.path.num_points = out_path->num_points;
	cached.path.points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( cached.path.points, out_path->points, sizeof( Vector3 ) * out_path->num_points );
	cached.expireTime = level.time + NAV_DEBUG_PATH_RETENTION;
	s_nav_debug_cached_paths.push_back( cached );

 /**
	*   	Emit optional debug draw segments for the cached path.
	**/
	if ( NavDebug_Enabled() && nav_debug_draw_path && nav_debug_draw_path->integer != 0 ) {
		const int32_t segmentCount = std::max( 0, out_path->num_points - 1 );
		for ( int32_t i = 0; i < segmentCount; i++ ) {
			if ( !NavDebug_CanEmitSegments( 1 ) ) {
				break;
			}

			const Vector3 &a = out_path->points[ i ];
			const Vector3 &b = out_path->points[ i + 1 ];

			const Vector3 mid = { ( a[ 0 ] + b[ 0 ] ) * 0.5f, ( a[ 1 ] + b[ 1 ] ) * 0.5f, ( a[ 2 ] + b[ 2 ] ) * 0.5f };
			if ( !NavDebug_PassesDistanceFilter( mid ) ) {
				continue;
			}

			SVG_DebugDrawLine_TE( a, b, MULTICAST_PVS, false );
			NavDebug_ConsumeSegments( 1 );
		}
	}

	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOrigin_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy ) {
   /**
	*    Sanity checks: require output storage.
	**/
	if ( !out_path ) {
		return false;
	}

   /**
	*    Reset any previous path contents owned by the caller.
	**/
	SVG_Nav_FreeTraversalPath( out_path );

 /**
	*    Require a loaded navmesh before attempting sync traversal path generation.
	**/
	if ( !g_nav_mesh ) {
		return false;
	}

	/**
  *    Cache the navmesh pointer and convert public feet-origin inputs into nav-center space.
	**/
	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( !mesh ) {
		return false;
	}

	const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
	const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );

	/**
	*    Boundary diagnostics:
	*        This overload accepts feet-origin inputs and must convert exactly once
	*        before node resolution.
	**/
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[DEBUG][NavPath][Boundary] WithAgentBBox feet->center start=(%.1f %.1f %.1f)->(%.1f %.1f %.1f) goal=(%.1f %.1f %.1f)->(%.1f %.1f %.1f)\n",
			start_origin.x, start_origin.y, start_origin.z,
			start_center.x, start_center.y, start_center.z,
			goal_origin.x, goal_origin.y, goal_origin.z,
			goal_center.x, goal_center.y, goal_center.z );
	}

   /**
	*    Generate traversal waypoints while honoring any caller-supplied policy blend preference first.
	**/
	std::vector<Vector3> points;
    const bool prefer_blended_lookup = policy ? policy->enable_goal_z_layer_blend : false;
	const double preferred_blend_start = policy ? policy->blend_start_dist : NAV_DEFAULT_BLEND_DIST_START;
	const double preferred_blend_full = policy ? policy->blend_full_dist : NAV_DEFAULT_BLEND_DIST_FULL;
	if ( !Nav_Traversal_TryGeneratePointsWithEndpointFallbacks( mesh, start_center, goal_center,
		agent_mins, agent_maxs, policy, nullptr, prefer_blended_lookup,
		preferred_blend_start, preferred_blend_full, points ) ) {
		return false;
	}

 /**
	*    Reject empty waypoint output before allocating the public path buffer.
	**/
	if ( points.empty() ) {
		return false;
	}

    /**
	*    Copy the generated waypoints into the caller-owned traversal path.
	**/
	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
	return true;
}

const bool SVG_Nav_GenerateTraversalPathForOriginEx_WithAgentBBox( const Vector3 &start_origin, const Vector3 &goal_origin, nav_traversal_path_t *out_path,
	const Vector3 &agent_mins, const Vector3 &agent_maxs, const svg_nav_path_policy_t *policy, const bool enable_goal_z_layer_blend, const double blend_start_dist, const double blend_full_dist,
	const struct svg_nav_path_process_t *pathProcess ) {
	if ( !out_path ) {
		return false;
	}

	SVG_Nav_FreeTraversalPath( out_path );

	if ( !g_nav_mesh ) {
		return false;
	}

	nav_node_ref_t start_node = {};
	nav_node_ref_t goal_node = {};
	bool startResolved = false;
	bool goalResolved = false;

	if ( enable_goal_z_layer_blend ) {
		// Convert caller feet-origin to nav-center using provided agent bbox.
		const nav_mesh_t *mesh = g_nav_mesh.get();
		if ( !mesh ) {
			return false;
		}
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
		startResolved = Nav_FindNodeForPosition_BlendZ( mesh, start_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &start_node, true );
		goalResolved = Nav_FindNodeForPosition_BlendZ( mesh, goal_center, start_center[ 2 ], goal_center[ 2 ], start_center, goal_center,
			blend_start_dist, blend_full_dist, &goal_node, true );
	} else {
		const nav_mesh_t *mesh = g_nav_mesh.get();
		if ( !mesh ) {
			return false;
		}
		const Vector3 start_center = SVG_Nav_ConvertFeetToCenter( mesh, start_origin, &agent_mins, &agent_maxs );
		const Vector3 goal_center = SVG_Nav_ConvertFeetToCenter( mesh, goal_origin, &agent_mins, &agent_maxs );
		startResolved = Nav_FindNodeForPosition( mesh, start_center, start_center[ 2 ], &start_node, true );
		goalResolved = Nav_FindNodeForPosition( mesh, goal_center, goal_center[ 2 ], &goal_node, true );
	}

	/**
	*    Targeted diagnostics: node resolution.
	*        This is the quickest way to separate the "no nodes" case from true A* failures.
	**/
	if ( Nav_PathDiagEnabled() ) {
		gi.dprintf( "[DEBUG][NavPath][Diag] Resolve nodes: start_ok=%d goal_ok=%d blend=%d\n",
			startResolved ? 1 : 0, goalResolved ? 1 : 0, enable_goal_z_layer_blend ? 1 : 0 );
		if ( startResolved ) {
			Nav_Debug_PrintNodeRef( "start_node", start_node );
		}
		if ( goalResolved ) {
			Nav_Debug_PrintNodeRef( "goal_node", goal_node );
		}
	}

	// Early out: we cannot pathfind without both endpoints.
	if ( !startResolved || !goalResolved ) {
		if ( Nav_PathDiagEnabled() ) {
			gi.dprintf( "[WARNING][NavPath][Diag] Node resolution failed: start_ok=%d goal_ok=%d. Likely: missing nav tiles near endpoint(s) or bad Z-layer selection.\n",
				startResolved ? 1 : 0, goalResolved ? 1 : 0 );
		}
		return false;
	}

	std::vector<Vector3> points;
	if ( start_node.key == goal_node.key ) {
		points.push_back( start_node.worldPosition );
		points.push_back( goal_node.worldPosition );
	} else {
		/**
		*	Hierarchical pre-pass: compute a coarse tile route and restrict A* expansions
		*	to tiles on that route.
		*
		*	We still pass `pathProcess` through so the fine A* can apply per-entity
		*	failure penalties.
		**/
		std::vector<nav_tile_cluster_key_t> tileRoute;
     /**
		*    Boundary-origin hardening:
		*        Use resolved canonical node positions for the coarse route query so tile-route selection
		*        stays aligned with the surfaces that node lookup actually recovered.
		**/
		const bool hasTileRoute = SVG_Nav_ClusterGraph_FindRoute( g_nav_mesh.get(), start_node.worldPosition, goal_node.worldPosition, tileRoute );
		const std::vector<nav_tile_cluster_key_t> *routeFilter = hasTileRoute ? &tileRoute : nullptr;

		if ( !Nav_AStarSearch( g_nav_mesh.get(), start_node, goal_node, agent_mins, agent_maxs, points, policy, pathProcess, routeFilter ) ) {
			return false;
		}
	}

	if ( points.empty() ) {
		return false;
	}

	out_path->num_points = ( int32_t )points.size();
	out_path->points = ( Vector3 * )gi.TagMallocz( sizeof( Vector3 ) * out_path->num_points, TAG_SVGAME_NAVMESH );
	memcpy( out_path->points, points.data(), sizeof( Vector3 ) * out_path->num_points );
	return true;
}

/**
*   @brief  Free a traversal path allocated by SVG_Nav_GenerateTraversalPathForOrigin.
*   @param  path    Path structure to free.
**/
void SVG_Nav_FreeTraversalPath( nav_traversal_path_t *path ) {
	if ( !path ) {
		return;
	}

	if ( path->points ) {
		gi.TagFree( path->points );
	}

	path->points = nullptr;
	path->num_points = 0;
}

/**
*   @brief  Query movement direction along a traversal path.
*           Advances the waypoint index as the caller reaches waypoints.
*   @param  path            Path to follow.
*   @param  current_origin  Current world-space origin.
*   @param  waypoint_radius Radius for waypoint completion.
*   @param  policy          Optional traversal policy for per-agent constraints.
*   @param  inout_index     Current waypoint index (updated on success).
*   @param  out_direction   Output normalized movement direction.
*   @return True if a valid direction was produced, false if path is complete/invalid.
**/
const bool SVG_Nav_QueryMovementDirection( const nav_traversal_path_t *path, const Vector3 &current_origin,
	double waypoint_radius, const svg_nav_path_policy_t *policy, int32_t *inout_index, Vector3 *out_direction ) {
/**
*	Sanity checks: validate inputs and ensure we have a usable path.
**/
	if ( !path || !inout_index || !out_direction ) {
		return false;
	}
	if ( path->num_points <= 0 || !path->points ) {
		return false;
	}

	/**
	*	Clamp the waypoint radius to a minimum value.
	*	This avoids degenerate behavior where micro radii prevent waypoint completion
	*	when the mover cannot stand exactly on the path point due to collision.
	**/
	waypoint_radius = std::max( waypoint_radius, 8.0 );

	int32_t index = *inout_index;
	if ( index < 0 ) {
		index = 0;
	}

	const double waypoint_radius_sqr = waypoint_radius * waypoint_radius;

	// Advance waypoints using 3D distance so stairs count toward completion.
	while ( index < path->num_points ) {
		const Vector3 delta = QM_Vector3Subtract( path->points[ index ], current_origin );
		const double dist_sqr = ( delta[ 0 ] * delta[ 0 ] ) + ( delta[ 1 ] * delta[ 1 ] ) + ( delta[ 2 ] * delta[ 2 ] );
		if ( dist_sqr > waypoint_radius_sqr ) {
			break;
		}
		index++;
	}

	/**
	*	Stuck heuristic (3D query): if we are not making 2D progress towards the current waypoint,
	*	advance it after a few frames. This is primarily for cornering; we intentionally keep
	*	it based on XY so stairs still work with the 2D based Advance2D variant.
	**/
	if ( index < path->num_points ) {
		thread_local int32_t s_last_index = -1;
		thread_local double s_last_dist2d_sqr = 0.0f;
		thread_local int32_t s_no_progress_frames = 0;
		thread_local int32_t s_last_frame = -1;

		if ( s_last_frame != ( int32_t )level.frameNumber || s_last_index != index ) {
			s_last_frame = ( int32_t )level.frameNumber;
			s_last_index = index;
			const Vector3 d0 = QM_Vector3Subtract( path->points[ index ], current_origin );
			s_last_dist2d_sqr = ( d0[ 0 ] * d0[ 0 ] ) + ( d0[ 1 ] * d0[ 1 ] );
			s_no_progress_frames = 0;
		} else {
			const Vector3 d1 = QM_Vector3Subtract( path->points[ index ], current_origin );
			const double dist2d_sqr = ( d1[ 0 ] * d1[ 0 ] ) + ( d1[ 1 ] * d1[ 1 ] );
			const double near_sqr = waypoint_radius_sqr * 9.0f;
			if ( dist2d_sqr <= near_sqr ) {
				const double improve_eps = 1.0f;
				if ( dist2d_sqr >= ( s_last_dist2d_sqr - improve_eps ) ) {
					s_no_progress_frames++;
				} else {
					s_no_progress_frames = 0;
				}
				if ( s_no_progress_frames >= 6 && ( index + 1 ) < path->num_points ) {
					index++;
					s_no_progress_frames = 0;
				}
			}
			s_last_dist2d_sqr = dist2d_sqr;
			s_last_frame = ( int32_t )level.frameNumber;
		}
	}

	if ( index >= path->num_points ) {
		*inout_index = path->num_points;
		return false;
	}

	// Produce a 3D direction. Clamp vertical component to avoid large up/down jumps.
	Vector3 direction = QM_Vector3Subtract( path->points[ index ], current_origin );

	// clamp Z to a reasonable step height if the nav mesh is available
	if ( g_nav_mesh ) {
		// Determine the effective step limit (policy overrides mesh defaults when present).
		double stepLimit = g_nav_mesh->max_step;
		if ( policy && policy->max_step_height > 0.0 ) {
			stepLimit = policy->max_step_height;
		}
		const double maxDz = stepLimit + g_nav_mesh->z_quant;
		if ( direction[ 2 ] > maxDz ) {
			direction[ 2 ] = maxDz;
		} else if ( direction[ 2 ] < -maxDz ) {
			direction[ 2 ] = -maxDz;
		}
	}

	const double length = ( double )QM_Vector3LengthDP( direction );
	if ( length <= std::numeric_limits<double>::epsilon() ) {
		return false;
	}

	*out_direction = QM_Vector3NormalizeDP( direction );
	*inout_index = index;

	return true;
}

/**
*
*
*
*	Navigation Movement Tests:
*
*
*
**/
/**
*	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
const inline svg_trace_t Nav_Trace( const Vector3 &start, const Vector3 &mins, const Vector3 &maxs, const Vector3 &end,
	const edict_ptr_t *clip_entity, const cm_contents_t mask ) {
	/**
	*    `Inline-model` traversal:
	*        Treat only `non-world` entities as explicit `inline-model` clips.
	*        World clipping is handled by the dedicated `world-only` branch below.
	**/
	if ( clip_entity && clip_entity->s.number != ENTITYNUM_WORLD ) {
		// Note that `gi.clip` treats a `nullptr` entity as a world trace, so we can directly
		// pass the `clip_entity` through without special handling.

		// <Q2RTXP>: WID: TODO: Should perform a lifted retry here as well to filter startsolid/allsolid artifacts on inline models, 
		// but that is not currently a known issue so defer until we have repro cases.
		return gi.clip( clip_entity, &start, &mins, &maxs, &end, mask );
	}

 /**
	*    World traversal policy for nav edge validation:
	*        Keep nav-owned collision semantics here and clip only against the
	*        world or explicit inline-model entity. A* step testing should not
	*        depend on the generic scene trace path because that would pull all
	*        world entities into a validator that is supposed to reason about nav
	*        tiles, cells, and layers.
	*
	*        @note	When the initial world clip starts in solid, perform a small
	*                lifted retry to filter floor-boundary precision cases that can
	*                otherwise appear as false startsolid/allsolid hits.
	**/
	const svg_trace_t worldTrace = gi.clip( nullptr, &start, &mins, &maxs, &end, mask );

	/**
	*    Fast path: keep the primary world trace when it is already valid.
	**/
	if ( !worldTrace.startsolid && !worldTrace.allsolid ) {
		return worldTrace;
	}

	/**
	*    Boundary hardening:
	*        Retry once with a tiny vertical lift to avoid contact-on-plane
	*        precision artifacts that can mark traces as startsolid/allsolid.
	**/
	Vector3 retryStart = start;
	Vector3 retryEnd = end;
	constexpr float kWorldRetryLift = 0.125f;
	retryStart[ 2 ] += kWorldRetryLift;
	retryEnd[ 2 ] += kWorldRetryLift;

	svg_trace_t retryTrace = gi.clip( nullptr, &retryStart, &mins, &maxs, &retryEnd, mask );

	/**
	*    Prefer the retry only when it clearly resolves solid-start artifacts.
	**/
	if ( !retryTrace.startsolid && !retryTrace.allsolid &&
		( retryTrace.fraction >= worldTrace.fraction || worldTrace.fraction <= 0.0f ) ) {
		// Reproject the lifted endpos back to the original Z frame for callers.
		retryTrace.endpos[ 2 ] -= kWorldRetryLift;
		return retryTrace;
	}

	// Fall back to the original world trace when retry did not improve confidence.
	return worldTrace;
}

/**
*	@brief	Low-level trace wrapper that supports world or inline-model clip.
**/
/**
*	@brief	Performs a simple PMove-like step traversal test (3-trace).
*
*			This is intentionally conservative and is used only to validate edges in A*:
*			1) Try direct horizontal move.
*			2) If blocked, try stepping up <= max step, then horizontal move.
*			3) Trace down to find ground.
*	@param	mesh			Navigation mesh.
* 	@param	startPos		Start world position.
* 	@param	endPos			End world position.
* 	@param	clip_entity		Entity to use for clipping (nullptr for world).
*
*	@return	True if the traversal is possible, false otherwise.
**/
const bool Nav_CanTraverseStep( const nav_mesh_t *mesh, const Vector3 &startPos, const Vector3 &endPos, const edict_ptr_t *clip_entity, svg_nav_path_policy_t &policy ) {
	if ( !mesh ) {
		return false;
	}

	// Ignore Z from caller; we compute step behavior ourselves.
	Vector3 start = startPos;
	Vector3 goal = endPos;
	goal[ 2 ] = start[ 2 ];

	const Vector3 mins = mesh->agent_mins;
	const Vector3 maxs = mesh->agent_maxs;

	return Nav_CanTraverseStep_ExplicitBBox( mesh, startPos, endPos, mins, maxs, clip_entity, &policy );
}

/**
*  @brief	Performs a simple PMove-like step traversal test (3-trace) with explicit agent bbox.
* 			This is intentionally conservative and is used only to validate edges in A*:
* 			1) Try direct horizontal move.
* 			2) If blocked, try stepping up <= max step, then horizontal move.
* 			3) Trace down to find ground.
* 	@param	mesh			Navigation mesh.
* 	@param	startPos		Start world position.
* 	@param	endPos			End world position.
* 	@param	mins			Agent bounding box minimums.
* 	@param	maxs			Agent bounding box maximums.
* 	@param	clip_entity		Entity to use for clipping (nullptr for world).
* 	@param	policy			Optional path policy for tuning step behavior.
* 	@return	True if the traversal is possible, false otherwise.
**/
/**
*  @brief	Internal single-step traversal validation used by the segmented ramp helper.
*  @param	mesh			Navigation mesh.
*  @param	startPos	Start world position.
*  @param	endPos		End world position.
*  @param	mins		Agent bounding box minimums.
*  @param	maxs		Agent bounding box maximums.
*  @param	clip_entity	Entity to use for clipping (nullptr for world).
*  @param	policy		Optional path policy for tuning step behavior.
*  @return	True if the traversal is possible, false otherwise.
*  @note	This function assumes any segmented-ramp handling has already been applied.
**/
static const bool Nav_CanTraverseStep_ExplicitBBox_Single( const nav_mesh_t *mesh, const nav_node_ref_t &start_node, const nav_node_ref_t &end_node, const Vector3 &mins, const Vector3 &maxs, const edict_ptr_t *clip_entity, const svg_nav_path_policy_t *policy, nav_edge_reject_reason_t *out_reason, const cm_contents_t stepTraceMask ) {
  /**
	*    Sanity checks: require a valid mesh.
	**/
	if ( !mesh ) {
		return false;
	}
	( void )clip_entity;
	( void )stepTraceMask;

	const Vector3 &startPos = start_node.worldPosition;
	const Vector3 &endPos = end_node.worldPosition;

	//! Tracks the last frame we reported a step-test failure.
	static int32_t s_nav_step_test_failure_frame = -1;
	//! Indicates whether we already printed a failure reason this frame.
	static bool s_nav_step_test_failure_logged = false;
	auto ReportStepTestFailureOnce = [startPos, endPos]( const char *reason ) {
		const int32_t currentFrame = ( int32_t )level.frameNumber;
		if ( s_nav_step_test_failure_frame != currentFrame ) {
			s_nav_step_test_failure_frame = currentFrame;
			s_nav_step_test_failure_logged = false;
		}
		if ( s_nav_step_test_failure_logged ) {
			return;
		}
		s_nav_step_test_failure_logged = true;
		if ( !nav_debug_draw_rejects || nav_debug_draw_rejects->integer == 0 ) {
			return;
		}
		gi.dprintf( "[DEBUG][NavPath][StepTest] Edge from (%.1f %.1f %.1f) to (%.1f %.1f %.1f) failed: %s\n",
			startPos[ 0 ], startPos[ 1 ], startPos[ 2 ],
			endPos[ 0 ], endPos[ 1 ], endPos[ 2 ],
			reason ? reason : "unspecified" );
	};

	/**
	*    Resolve canonical tile/cell/layer views for both endpoints.
	**/
	const nav_tile_t *start_tile = nullptr;
	const nav_xy_cell_t *start_cell = nullptr;
	const nav_layer_t *start_layer = nullptr;
	const nav_tile_t *end_tile = nullptr;
	const nav_xy_cell_t *end_cell = nullptr;
	const nav_layer_t *end_layer = nullptr;
	if ( !Nav_TryGetNodeLayerView( mesh, start_node, &start_tile, &start_cell, &start_layer ) ||
		!Nav_TryGetNodeLayerView( mesh, end_node, &end_tile, &end_cell, &end_layer ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	/**
	*    Resolve global cell-grid coordinates so the validator can reason about local hops.
	**/
	int32_t start_cell_x = 0;
	int32_t start_cell_y = 0;
	int32_t end_cell_x = 0;
	int32_t end_cell_y = 0;
	if ( !Nav_TryGetGlobalCellCoords( mesh, start_node, &start_cell_x, &start_cell_y ) ||
		!Nav_TryGetGlobalCellCoords( mesh, end_node, &end_cell_x, &end_cell_y ) ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::NoNode;
		}
		return false;
	}

	/**
	*    Compute local traversal limits and vertical deltas.
	**/
	const double desiredDz = ( double )endPos[ 2 ] - ( double )startPos[ 2 ];
	const double requiredUp = std::max( 0.0, desiredDz );
	const double requiredDown = std::max( 0.0, -desiredDz );
	const double stepSize = ( policy && policy->max_step_height > 0.0 ) ? ( double )policy->max_step_height : ( double )mesh->max_step;
	const double dropCap = ( policy && policy->max_drop_height_cap > 0.0 )
		? ( double )policy->max_drop_height_cap
		: ( nav_max_drop_height_cap ? nav_max_drop_height_cap->value : 128.0f );
	const double sameLevelTolerance = std::max( ( double )PHYS_STEP_GROUND_DIST, ( double )mesh->z_quant * 0.5 );
	const double requiredClearance = std::max( 0.0, ( double )maxs[ 2 ] - ( double )mins[ 2 ] );

	/**
	*    Reject non-local single-step edges.
	*        Segmented callers should already have split longer moves into adjacent hops.
	**/
	const int32_t delta_cell_x = end_cell_x - start_cell_x;
	const int32_t delta_cell_y = end_cell_y - start_cell_y;
	if ( std::abs( delta_cell_x ) > 1 || std::abs( delta_cell_y ) > 1 ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "non-local single-step edge" );
		return false;
	}

	/**
	*    Ensure both endpoint layers are walkable and have enough clearance for the agent hull.
	**/
	if ( ( start_layer->flags & NAV_FLAG_WALKABLE ) == 0 || ( end_layer->flags & NAV_FLAG_WALKABLE ) == 0 ) {
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "layer is not marked walkable" );
		return false;
	}

	const double startClearance = Nav_GetLayerClearanceWorld( mesh, *start_layer );
	const double endClearance = Nav_GetLayerClearanceWorld( mesh, *end_layer );
	if ( startClearance + PHYS_STEP_GROUND_DIST < requiredClearance ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_CLEARANCE );
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "start layer clearance below agent hull height" );
		return false;
	}
	if ( endClearance + PHYS_STEP_GROUND_DIST < requiredClearance ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_CLEARANCE );
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::StepTest;
		}
		ReportStepTestFailureOnce( "end layer clearance below agent hull height" );
		return false;
	}

	/**
	*    Same-level transitions are valid once local adjacency and clearance are satisfied.
	**/
	if ( std::fabs( desiredDz ) <= sameLevelTolerance ) {
		return true;
	}

	/**
	*    Uphill transitions must fit within the configured step height.
	**/
	if ( requiredUp > 0.0 ) {
		if ( requiredUp > stepSize ) {
			if ( out_reason ) {
				*out_reason = nav_edge_reject_reason_t::StepTest;
			}
			ReportStepTestFailureOnce( "required climb exceeds configured step" );
			return false;
		}
		return true;
	}

	/**
	*    Downward transitions must stay within the configured drop cap.
	**/
	if ( requiredDown > dropCap ) {
		NavDebug_RecordReject( startPos, endPos, NAV_DEBUG_REJECT_REASON_DROP_CAP );
		if ( out_reason ) {
			*out_reason = nav_edge_reject_reason_t::DropCap;
		}
		ReportStepTestFailureOnce( "drop cap exceeded" );
		return false;
	}

	/**
	*    Conservative success case:
	*        This canonical local hop is adjacent, walkable, and satisfies clearance plus
	*        step/drop limits, so no generic tracing is needed for A* edge validation.
	**/
	return true;
}