/********************************************************************
*
*
*	ServerGame: Nav2 Sparse Span Grid - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_span_grid.h"

#include <algorithm>


/**
* @brief Keep the sparse span-grid module represented in the build.
* @note The concrete builder and adjacency logic arrive in the dedicated build and adjacency modules.
**/
void SVG_Nav2_SpanGrid_ModuleAnchor( void );

/**
* @brief Return the sparse column index for one tile coordinate pair.
* @param grid Grid to inspect.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @return Zero-based column index when found, or `-1` otherwise.
**/
int32_t SVG_Nav2_SpanGrid_FindColumnIndex( const nav2_span_grid_t &grid, const int32_t tile_x, const int32_t tile_y ) {
	// Scan the sparse columns in canonical order so the first exact coordinate match can be resolved deterministically.
	for ( int32_t columnIndex = 0; columnIndex < ( int32_t )grid.columns.size(); columnIndex++ ) {
		const nav2_span_column_t &column = grid.columns[ ( size_t )columnIndex ];
		if ( column.tile_x == tile_x && column.tile_y == tile_y ) {
			return columnIndex;
		}
	}

	// Return a sentinel when the sparse column does not exist.
	return -1;
}

/**
* @brief Resolve a sparse column for one tile coordinate pair.
* @param grid Grid to inspect.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @return Pointer to the sparse column, or `nullptr` when absent.
**/
const nav2_span_column_t *SVG_Nav2_SpanGrid_TryGetColumn( const nav2_span_grid_t &grid, const int32_t tile_x, const int32_t tile_y ) {
	// Resolve the index first so the mutable and const helpers share one search policy.
	const int32_t columnIndex = SVG_Nav2_SpanGrid_FindColumnIndex( grid, tile_x, tile_y );
	if ( columnIndex < 0 ) {
		return nullptr;
	}

	// Return the matching sparse column slot.
	return &grid.columns[ ( size_t )columnIndex ];
}

/**
* @brief Resolve a mutable sparse column for one tile coordinate pair.
* @param grid Grid to inspect.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @return Pointer to the sparse column, or `nullptr` when absent.
**/
nav2_span_column_t *SVG_Nav2_SpanGrid_TryGetColumn( nav2_span_grid_t *grid, const int32_t tile_x, const int32_t tile_y ) {
	// Require mutable grid storage before returning a writable column pointer.
	if ( !grid ) {
		return nullptr;
	}

	// Reuse the const lookup logic and then cast the resolved slot back to mutable storage.
	const int32_t columnIndex = SVG_Nav2_SpanGrid_FindColumnIndex( *grid, tile_x, tile_y );
	if ( columnIndex < 0 ) {
		return nullptr;
	}

	// Return the writable sparse column slot.
	return &grid->columns[ ( size_t )columnIndex ];
}

/**
* @brief Find or create a sparse column for one tile coordinate pair.
* @param grid Grid to mutate.
* @param tile_x Sparse column X coordinate.
* @param tile_y Sparse column Y coordinate.
* @param tile_id Canonical tile id to assign when creating a new column.
* @return Pointer to the existing or newly created sparse column, or `nullptr` on invalid input.
**/
nav2_span_column_t *SVG_Nav2_SpanGrid_FindOrAddColumn( nav2_span_grid_t *grid, const int32_t tile_x, const int32_t tile_y, const int32_t tile_id ) {
	// Require mutable output storage before attempting to create or resolve a sparse column.
	if ( !grid ) {
		return nullptr;
	}

	// Reuse an existing column when the sparse XY key already exists.
	if ( nav2_span_column_t *column = SVG_Nav2_SpanGrid_TryGetColumn( grid, tile_x, tile_y ) ) {
		if ( tile_id >= 0 && column->tile_id < 0 ) {
			column->tile_id = tile_id;
		}
		return column;
	}

	// Append a new sparse column in insertion order; canonical sorting can normalize the final layout later.
	nav2_span_column_t column = {};
	column.tile_x = tile_x;
	column.tile_y = tile_y;
	column.tile_id = tile_id;
	grid->columns.push_back( column );
	return &grid->columns.back();
}

/**
* @brief Append one span to a sparse column.
* @param column Sparse column receiving the span.
* @param span Span payload to append.
* @return Pointer to the appended span slot, or `nullptr` when no column was supplied.
**/
nav2_span_t *SVG_Nav2_SpanGrid_AppendSpan( nav2_span_column_t *column, const nav2_span_t &span ) {
	// Require a destination column before appending span payload.
	if ( !column ) {
		return nullptr;
	}

	// Append the span and return the newly created storage slot.
	column->spans.push_back( span );
	return &column->spans.back();
}

/**
* @brief Resolve one span by pointer-free span reference.
* @param grid Grid to inspect.
* @param ref Pointer-free span reference to resolve.
* @return Pointer to the span slot, or `nullptr` when the reference is invalid.
**/
const nav2_span_t *SVG_Nav2_SpanGrid_TryResolveSpan( const nav2_span_grid_t &grid, const nav2_span_ref_t &ref ) {
	// Require a valid reference before dereferencing the sparse span grid.
	if ( !ref.IsValid() || ref.column_index < 0 || ref.column_index >= ( int32_t )grid.columns.size() ) {
		return nullptr;
	}

	// Resolve the owning sparse column first so the span index can be validated against the correct container.
	const nav2_span_column_t &column = grid.columns[ ( size_t )ref.column_index ];
	if ( ref.span_index < 0 || ref.span_index >= ( int32_t )column.spans.size() ) {
		return nullptr;
	}

	// Return the concrete span slot referenced by the pointer-free handle.
	const nav2_span_t &span = column.spans[ ( size_t )ref.span_index ];
	return ( span.span_id == ref.span_id ) ? &span : nullptr;
}

/**
* @brief Resolve one mutable span by pointer-free span reference.
* @param grid Grid to inspect.
* @param ref Pointer-free span reference to resolve.
* @return Pointer to the span slot, or `nullptr` when the reference is invalid.
**/
nav2_span_t *SVG_Nav2_SpanGrid_TryResolveSpan( nav2_span_grid_t *grid, const nav2_span_ref_t &ref ) {
	// Require mutable grid storage before publishing a writable span pointer.
	if ( !grid ) {
		return nullptr;
	}

	// Reuse the const resolver and then convert the returned slot into mutable storage.
	const nav2_span_t *span = SVG_Nav2_SpanGrid_TryResolveSpan( *grid, ref );
	return const_cast<nav2_span_t *>( span );
}

/**
* @brief Sort sparse columns and spans into a canonical deterministic order.
* @param grid Grid to mutate.
* @return True when sorting completed.
* @note This keeps worker-published snapshots and serialization output stable.
**/
const bool SVG_Nav2_SpanGrid_SortCanonical( nav2_span_grid_t *grid ) {
	// Require mutable grid storage before sorting.
	if ( !grid ) {
		return false;
	}

	// Sort sparse columns first so the grid layout is deterministic across rebuilds and serialization passes.
	std::sort( grid->columns.begin(), grid->columns.end(), []( const nav2_span_column_t &lhs, const nav2_span_column_t &rhs ) {
		if ( lhs.tile_x != rhs.tile_x ) {
			return lhs.tile_x < rhs.tile_x;
		}
		if ( lhs.tile_y != rhs.tile_y ) {
			return lhs.tile_y < rhs.tile_y;
		}
		return lhs.tile_id < rhs.tile_id;
	} );

	// Sort spans within each column so span indices remain stable after canonicalization.
	for ( nav2_span_column_t &column : grid->columns ) {
		std::sort( column.spans.begin(), column.spans.end(), []( const nav2_span_t &lhs, const nav2_span_t &rhs ) {
			if ( lhs.floor_z != rhs.floor_z ) {
				return lhs.floor_z < rhs.floor_z;
			}
			if ( lhs.ceiling_z != rhs.ceiling_z ) {
				return lhs.ceiling_z < rhs.ceiling_z;
			}
			if ( lhs.preferred_z != rhs.preferred_z ) {
				return lhs.preferred_z < rhs.preferred_z;
			}
			return lhs.span_id < rhs.span_id;
		} );
	}

	return true;
}

/**
* @brief Resolve one pointer-free span reference by stable span id.
* @param grid Grid to inspect.
* @param span_id Stable span id to resolve.
* @param out_ref [out] Resolved pointer-free span reference.
* @return True when the span id exists in the reverse index.
**/
const bool SVG_Nav2_SpanGrid_TryResolveSpanRef( const nav2_span_grid_t &grid, const int32_t span_id, nav2_span_ref_t *out_ref ) {
	/**
	*    Require a valid output slot and non-negative span id before lookup.
	**/
	if ( !out_ref || span_id < 0 ) {
		return false;
	}

	/**
	*    Resolve the pointer-free span reference from the stable span-id map.
	**/
	const auto it = grid.reverse_indices.by_span_id.find( span_id );
	if ( it == grid.reverse_indices.by_span_id.end() ) {
		return false;
	}

	/**
	*    Publish the resolved reference only when it still points to a valid slot.
	**/
	if ( !it->second.IsValid() ) {
		return false;
	}

	*out_ref = it->second;
	return true;
}

/**
* @brief Reset one reverse-index container so rebuild starts from a clean state.
* @param indices [in,out] Reverse-index payload to clear.
**/
static void SVG_Nav2_SpanGrid_ClearReverseIndices( nav2_span_grid_reverse_indices_t *indices ) {
	// Require output storage before clearing reverse-index payloads.
	if ( !indices ) {
		return;
	}

	// Clear every reverse-index container so a new rebuild can repopulate deterministic contents.
	indices->by_span_id.clear();
	indices->by_leaf.clear();
	indices->by_cluster.clear();
	indices->by_area.clear();
	indices->by_connector.clear();
}

/**
* @brief Ensure one reverse-index vector has capacity for the requested key.
* @param values [in,out] Reverse-index vector to resize.
* @param key Stable key that should become addressable.
**/
static void SVG_Nav2_SpanGrid_EnsureReverseKey( std::vector<std::vector<nav2_span_ref_t>> *values, const int32_t key ) {
	// Ignore invalid keys and missing output storage because only non-negative topology ids participate in reverse indexing.
	if ( !values || key < 0 ) {
		return;
	}

	// Resize just enough to cover the requested key while preserving existing deterministic order.
	if ( key >= ( int32_t )values->size() ) {
		values->resize( ( size_t )key + 1u );
	}
}

/**
* @brief Append one span reference to a reverse-index key exactly once.
* @param values [in,out] Reverse-index vector receiving the span reference.
* @param key Stable reverse-index key.
* @param ref Pointer-free span reference to append.
**/
static void SVG_Nav2_SpanGrid_AppendUniqueReverseRef( std::vector<std::vector<nav2_span_ref_t>> *values, const int32_t key,
	const nav2_span_ref_t &ref ) {
	// Skip invalid refs and invalid keys because reverse-index storage should only contain concrete span slots.
	if ( !values || key < 0 || !ref.IsValid() ) {
		return;
	}

	// Ensure the destination key is present before appending this pointer-free span reference.
	SVG_Nav2_SpanGrid_EnsureReverseKey( values, key );
	std::vector<nav2_span_ref_t> &bucket = ( *values )[ ( size_t )key ];

	/**
	*    Rebuild callers generate each concrete span reference exactly once per reverse-index key while
	*    traversing sparse columns in deterministic order, so a full linear duplicate scan is unnecessary
	*    and becomes the dominant self-time hotspot at larger bucket sizes.
	**/
	// Keep a tiny immediate-duplicate guard to preserve resilience if a caller accidentally submits the same ref twice in a row.
	if ( !bucket.empty() ) {
		const nav2_span_ref_t &last = bucket.back();
		if ( last.span_id == ref.span_id && last.column_index == ref.column_index && last.span_index == ref.span_index ) {
			return;
		}
	}

	// Append the reference directly to avoid O(n^2) growth from repeated bucket-wide duplicate scans.
	bucket.push_back( ref );
}

/**
* @brief Keep the sparse span-grid module represented in the build.
* @note The concrete builder and adjacency logic arrive in the dedicated build and adjacency modules.
**/
void SVG_Nav2_SpanGrid_ModuleAnchor( void ) {
}

/**
* @brief Reset one span-grid instance to a clean empty state.
* @param grid [in,out] Grid to clear.
**/
void SVG_Nav2_SpanGrid_Clear( nav2_span_grid_t *grid ) {
	// Require output storage before clearing.
	if ( !grid ) {
		return;
	}

	// Reset sizing metadata and sparse column payload.
	grid->tile_size = 8.;
	grid->cell_size_xy = 4.0;
	grid->z_quant = 4.0;
	grid->columns.clear();

	// Reset reverse indices so no stale references survive the clear operation.
	SVG_Nav2_SpanGrid_ClearReverseIndices( &grid->reverse_indices );
}

/**
* @brief Rebuild pointer-free reverse indices from the current sparse-column payload.
* @param grid [in,out] Grid whose reverse indices should be regenerated.
* @return True when rebuild completed.
**/
const bool SVG_Nav2_SpanGrid_RebuildReverseIndices( nav2_span_grid_t *grid ) {
	// Require grid storage before rebuilding.
	if ( !grid ) {
		return false;
	}

	// Start from a clean reverse-index payload each rebuild.
	SVG_Nav2_SpanGrid_ClearReverseIndices( &grid->reverse_indices );

	// Walk every sparse column/span slot in deterministic order and mirror pointer-free references into the index maps.
	for ( int32_t columnIndex = 0; columnIndex < ( int32_t )grid->columns.size(); columnIndex++ ) {
		const nav2_span_column_t &column = grid->columns[ ( size_t )columnIndex ];
		for ( int32_t spanIndex = 0; spanIndex < ( int32_t )column.spans.size(); spanIndex++ ) {
			const nav2_span_t &span = column.spans[ ( size_t )spanIndex ];

			// Ignore invalid span ids because reverse-index lookup and serialization references require stable non-negative identifiers.
			if ( span.span_id < 0 ) {
				continue;
			}

			// Build one pointer-free span reference for all reverse-index buckets.
			nav2_span_ref_t spanRef = {};
			spanRef.span_id = span.span_id;
			spanRef.column_index = columnIndex;
			spanRef.span_index = spanIndex;

			// Publish span-id lookup and topology-indexed memberships.
			grid->reverse_indices.by_span_id[ spanRef.span_id ] = spanRef;
			SVG_Nav2_SpanGrid_AppendUniqueReverseRef( &grid->reverse_indices.by_leaf, span.leaf_id, spanRef );
			SVG_Nav2_SpanGrid_AppendUniqueReverseRef( &grid->reverse_indices.by_cluster, span.cluster_id, spanRef );
			SVG_Nav2_SpanGrid_AppendUniqueReverseRef( &grid->reverse_indices.by_area, span.area_id, spanRef );
			SVG_Nav2_SpanGrid_AppendUniqueReverseRef( &grid->reverse_indices.by_connector, ( int32_t )span.connector_hint_mask, spanRef );
		}
	}

	return true;
}

