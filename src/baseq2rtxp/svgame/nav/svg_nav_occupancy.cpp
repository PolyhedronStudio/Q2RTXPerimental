/** * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
*
*	SVGame: Navigation Occupancy Helpers - Implementation
*
*
*	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "svgame/svg_local.h"
#include "svgame/nav/svg_nav_occupancy.h"
#include "svgame/nav/svg_nav.h"

#include <algorithm>

/**
*    @brief	Builds a compact occupancy key from canonical tile/cell/layer indices.
*    @note	Stateless helper used by all occupancy helpers to avoid duplicating bit manipulation code.
**/
static inline uint64_t Nav_OccupancyKey( const int32_t tileId, const int32_t cellIndex, const int32_t layerIndex ) {
	return ( ( ( uint64_t )tileId & 0xFFFF ) << 32 ) | ( ( ( uint64_t )cellIndex & 0xFFFF ) << 16 ) | ( ( uint64_t )layerIndex & 0xFFFF );
}

/**
*    @brief	Keep the sparse occupancy overlay aligned with the latest frame.
*    @note	Clears the overlay when `level.frameNumber` advances, preventing stale entries from accumulating.
**/
static inline void Nav_Occupancy_BeginFrame( nav_mesh_t * mesh ) {
	if ( !mesh ) {
		return;
	}

	const int32_t frame = ( int32_t )level.frameNumber;
	if ( mesh->occupancy_frame != frame ) {
		mesh->occupancy.clear();
		mesh->occupancy_frame = frame;
	}
}

/**
*    @brief	Clear the dynamic occupancy overlay stored on the mesh.
*    @param	mesh	Navigation mesh whose overlay should be emptied.
**/
void SVG_Nav_Occupancy_Clear( nav_mesh_t * mesh ) {
	/**
	*    Sanity: require a valid mesh before touching the overlay.
	**/
	if ( !mesh ) {
		return;
	}

	/**
	*    Remove every occupancy record so future queries start from an empty overlay.
	**/
	mesh->occupancy.clear();
	mesh->occupancy_frame = ( int32_t )level.frameNumber;
}

/**
*    @brief	Add or update occupancy metadata for a canonical tile/cell/layer slot.
*    @param	cost	Soft cost to accumulate (clamped at minimum 1).
*    @param	blocked	True when this slot should be treated as hard-blocked.
**/
void SVG_Nav_Occupancy_Add( nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, int32_t cost, bool blocked ) {
	/**
	*    Sanity: guard against invalid pointers or negative indices.
	**/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return;
	}

	/**
	*    Ensure the overlay corresponds to the current frame before mutating it.
	**/
	Nav_Occupancy_BeginFrame( mesh );

	/**
	*    Accumulate soft cost while respecting minimum increments and hard blocks.
	**/
	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	nav_occupancy_entry_t &entry = mesh->occupancy[ key ];
	entry.soft_cost += std::max( 1, cost );
	if ( blocked ) {
		entry.blocked = true;
	}
}

/**
*    @brief	Return the soft cost recorded for a canonical node slot.
*    @return	Soft cost or 0 when no entry exists or inputs are invalid.
**/
int32_t SVG_Nav_Occupancy_SoftCost( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex ) {
	/**
	*    Sanity: require valid mesh and non-negative indices.
	**/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return 0;
	}

	/**
	*    Delegate to the canonical overlay lookup to read the snapshot.
	**/
	nav_occupancy_entry_t occupancy = {};
	if ( !SVG_Nav_Occupancy_TryGet( mesh, tileId, cellIndex, layerIndex, &occupancy ) ) {
		return 0;
	}

	return occupancy.soft_cost;
}

/**
*    @brief	Return whether a canonical slot is marked as hard-blocked.
*    @return	True when the slot exists and its hard-block flag is set.
**/
bool SVG_Nav_Occupancy_Blocked( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex ) {
	/**
	*    Sanity: guard pointer and index validity before querying the overlay.
	**/
	if ( !mesh || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return false;
	}

	/**
	*    Inspect the stored entry without mutating the map.
	**/
	nav_occupancy_entry_t occupancy = {};
	if ( !SVG_Nav_Occupancy_TryGet( mesh, tileId, cellIndex, layerIndex, &occupancy ) ) {
		return false;
	}

	return occupancy.blocked;
}

/**
*    @brief	Fetch the cached occupancy entry for a canonical slot.
*    @param	mesh	Navigation mesh whose overlay will be inspected.
*    @param	out_entry	[out] Snapshot storage populated when an entry exists.
*    @return	True when the overlay contains the requested slot.
*    @note	This helper performs a readonly lookup and does not mutate the overlay.
**/
bool SVG_Nav_Occupancy_TryGet( const nav_mesh_t * mesh, int32_t tileId, int32_t cellIndex, int32_t layerIndex, nav_occupancy_entry_t * out_entry ) {
	/**
	*    Guard pointers and indices before reading from the map.
	**/
	if ( !mesh || !out_entry || tileId < 0 || cellIndex < 0 || layerIndex < 0 ) {
		return false;
	}

	/**
	*    Perform the lookup without inserting new entries.
	**/
	const uint64_t key = Nav_OccupancyKey( tileId, cellIndex, layerIndex );
	auto it = mesh->occupancy.find( key );
	if ( it == mesh->occupancy.end() ) {
		return false;
	}

	/**
	*    Copy the stored snapshot so the caller sees a stable view.
	**/
	*out_entry = it->second;
	return true;
}
