/********************************************************************
*
*
* ServerGame: Nav2 Serialization Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_serialize.h"

#include <chrono>
#include <cstring>

#include "common/files.h"


/**
*
*
*	Nav2 Serialization Local Payload Types:
*
*
**/
//! Current section version used for serialized span-grid payloads.
static constexpr uint32_t NAV2_SERIALIZED_SECTION_VERSION_SPAN_GRID = 1;
//! Current section version used for serialized adjacency payloads.
static constexpr uint32_t NAV2_SERIALIZED_SECTION_VERSION_ADJACENCY = 1;

/**
*	@brief	Compact serialized metadata describing one persisted span-grid payload.
*	@note	This header keeps the section pointer-free while documenting the counts needed to decode columns and spans safely.
**/
struct nav2_serialized_span_grid_meta_t {
	//! Tile size mirrored from the runtime span-grid.
	int32_t tile_size = 0;
	//! World-space XY cell size mirrored from the runtime span-grid.
	double cell_size_xy = 0.0;
	//! Vertical quantization mirrored from the runtime span-grid.
	double z_quant = 0.0;
	//! Number of serialized sparse columns.
	uint32_t column_count = 0;
	//! Number of serialized spans across all columns.
	uint32_t span_count = 0;
};

/**
*	@brief	Compact serialized descriptor for one sparse span-grid column.
*	@note	Span ranges remain explicit so the reader can rebuild column ownership without pointer fixups.
**/
struct nav2_serialized_span_column_t {
	//! Sparse column X coordinate.
	int32_t tile_x = 0;
	//! Sparse column Y coordinate.
	int32_t tile_y = 0;
	//! Canonical world-tile id when known.
	int32_t tile_id = -1;
	//! Offset into the serialized span array.
	uint32_t first_span_index = 0;
	//! Number of spans owned by this sparse column.
	uint32_t span_count = 0;
};

/**
*	@brief	Compact serialized payload for one traversable span.
*	@note	This mirrors only stable scalar metadata so the format remains durable and endian-safe.
**/
struct nav2_serialized_span_t {
	//! Stable span id.
	int32_t span_id = -1;
	//! Minimum traversable Z.
	double floor_z = 0.0;
	//! Maximum traversable Z.
	double ceiling_z = 0.0;
	//! Preferred standing Z.
	double preferred_z = 0.0;
	//! BSP leaf id when known.
	int32_t leaf_id = -1;
	//! BSP cluster id when known.
	int32_t cluster_id = -1;
	//! BSP area id when known.
	int32_t area_id = -1;
	//! Region-layer id when known.
	int32_t region_layer_id = NAV_REGION_ID_NONE;
	//! Compact movement flags.
	uint32_t movement_flags = NAV_FLAG_WALKABLE;
	//! Compact surface flags.
	uint32_t surface_flags = NAV_TILE_SUMMARY_NONE;
	//! Compact topology flags.
	uint32_t topology_flags = 0;
	//! Connector hint mask.
	uint32_t connector_hint_mask = 0;
	//! Occupancy hard-block state.
	uint8_t occupancy_hard_block = 0;
	//! Reserved padding for future use and stable alignment.
	uint8_t reserved0 = 0;
	//! Reserved padding for future use and stable alignment.
	uint16_t reserved1 = 0;
   //! Serialized occupancy soft-cost contribution.
	double serialized_soft_cost = 0.0;
	//! Serialized occupancy stamp/frame id.
	int64_t serialized_stamp_frame = -1;
};

/**
*	@brief	Compact serialized metadata describing one persisted adjacency payload.
*	@note	The adjacency section currently stores only a flat edge array, so one count is sufficient.
**/
struct nav2_serialized_adjacency_meta_t {
	//! Number of serialized adjacency edges.
	uint32_t edge_count = 0;
};

/**
*	@brief	Compact serialized payload for one local span adjacency edge.
*	@note	This mirrors the pointer-free edge contract exactly enough for a strict round-trip.
**/
struct nav2_serialized_span_edge_t {
	//! Stable edge id.
	int32_t edge_id = -1;
	//! Source span id.
	int32_t from_span_id = -1;
	//! Destination span id.
	int32_t to_span_id = -1;
	//! Source sparse-column index.
	int32_t from_column_index = -1;
	//! Destination sparse-column index.
	int32_t to_column_index = -1;
	//! Source span index within the source sparse column.
	int32_t from_span_index = -1;
	//! Destination span index within the destination sparse column.
	int32_t to_span_index = -1;
	//! Signed X offset between sparse columns.
	int32_t delta_x = 0;
	//! Signed Y offset between sparse columns.
	int32_t delta_y = 0;
	//! Signed preferred-height delta.
	double vertical_delta = 0.0;
	//! Published traversal cost.
	double cost = 0.0;
	//! Published soft penalty contribution.
	double soft_penalty_cost = 0.0;
	//! Pointer-free movement classification.
	nav2_span_edge_movement_t movement = nav2_span_edge_movement_t::None;
	//! Explicit legality classification.
	nav2_span_edge_legality_t legality = nav2_span_edge_legality_t::None;
	//! Fine-search feature mask.
	uint32_t feature_bits = NAV_EDGE_FEATURE_NONE;
	//! Hard/soft legality flags.
	uint32_t flags = 0;
};


/**
*
*
*	Nav2 Serialization Local Helpers:
*
*
**/
/**
*	@brief	Increment one round-trip mismatch bucket and the total mismatch count.
*	@param	result	[in,out] Structured round-trip result receiving the mismatch count.
*	@param	mismatch	Stable mismatch category to increment.
**/
static void Nav2_Serialize_RecordRoundTripMismatch( nav2_serialized_roundtrip_result_t *result,
	const nav2_serialized_roundtrip_mismatch_t mismatch ) {
	/**
	*    Require output storage and a valid mismatch category before mutating the aggregate mismatch counters.
	**/
	if ( !result || mismatch == nav2_serialized_roundtrip_mismatch_t::None || mismatch == nav2_serialized_roundtrip_mismatch_t::Count ) {
		return;
	}

	/**
	*    Increment both the aggregate mismatch count and the stable bucket-specific counter.
	**/
	result->mismatch_count++;
	result->mismatch_counts[ ( size_t )mismatch ]++;
}

/**
*	@brief	Compare two serialized headers field-by-field for round-trip identity.
*	@param	expectedHeader	Original header produced during serialization.
*	@param	actualHeader	Decoded header read back from the in-memory blob.
*	@param	outResult	[in,out] Round-trip result receiving mismatch counts.
*	@return	True when the two headers match exactly.
**/
static const bool Nav2_Serialize_CompareRoundTripHeader( const nav2_serialized_header_t &expectedHeader,
	const nav2_serialized_header_t &actualHeader, nav2_serialized_roundtrip_result_t *outResult ) {
	/**
	*    Collapse the header comparison into one deterministic boolean so the helper records only a single mismatch category when any header field drifts.
	**/
	const bool matches = expectedHeader.magic == actualHeader.magic
		&& expectedHeader.format_version == actualHeader.format_version
		&& expectedHeader.build_version == actualHeader.build_version
		&& expectedHeader.category == actualHeader.category
		&& expectedHeader.transport == actualHeader.transport
		&& expectedHeader.map_identity == actualHeader.map_identity
		&& expectedHeader.section_count == actualHeader.section_count
		&& expectedHeader.compatibility_flags == actualHeader.compatibility_flags;
	if ( !matches ) {
		Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::Header );
	}
	return matches;
}

/**
*	@brief	Compare two sparse span-grid payloads field-by-field for round-trip identity.
*	@param	expectedSpanGrid	Original span-grid payload.
*	@param	actualSpanGrid	Decoded span-grid payload.
*	@param	outResult	[in,out] Round-trip result receiving mismatch counts.
*	@return	True when the two span-grid payloads match exactly.
**/
static const bool Nav2_Serialize_CompareRoundTripSpanGrid( const nav2_span_grid_t &expectedSpanGrid,
	const nav2_span_grid_t &actualSpanGrid, nav2_serialized_roundtrip_result_t *outResult ) {
	bool matches = true;

	/**
	*    Compare top-level span-grid sizing and container counts first so structural drift is detected before descending into columns or spans.
	**/
	if ( expectedSpanGrid.tile_size != actualSpanGrid.tile_size
		|| expectedSpanGrid.cell_size_xy != actualSpanGrid.cell_size_xy
		|| expectedSpanGrid.z_quant != actualSpanGrid.z_quant
		|| expectedSpanGrid.columns.size() != actualSpanGrid.columns.size() ) {
		Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::SpanGridMeta );
		matches = false;
	}

	/**
	*    Compare columns in deterministic order while stopping at the minimum shared count so mismatch accounting remains bounded even after structural drift.
	**/
	const size_t sharedColumnCount = std::min( expectedSpanGrid.columns.size(), actualSpanGrid.columns.size() );
	for ( size_t columnIndex = 0; columnIndex < sharedColumnCount; columnIndex++ ) {
		const nav2_span_column_t &expectedColumn = expectedSpanGrid.columns[ columnIndex ];
		const nav2_span_column_t &actualColumn = actualSpanGrid.columns[ columnIndex ];
		if ( expectedColumn.tile_x != actualColumn.tile_x
			|| expectedColumn.tile_y != actualColumn.tile_y
			|| expectedColumn.tile_id != actualColumn.tile_id
			|| expectedColumn.spans.size() != actualColumn.spans.size() ) {
			Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::SpanGridColumn );
			matches = false;
		}

		/**
		*    Compare spans in deterministic order inside the shared column prefix so payload-field drift is counted even when a column's span count changed.
		**/
		const size_t sharedSpanCount = std::min( expectedColumn.spans.size(), actualColumn.spans.size() );
		for ( size_t spanIndex = 0; spanIndex < sharedSpanCount; spanIndex++ ) {
			const nav2_span_t &expectedSpan = expectedColumn.spans[ spanIndex ];
			const nav2_span_t &actualSpan = actualColumn.spans[ spanIndex ];
			const bool spanMatches = expectedSpan.span_id == actualSpan.span_id
				&& expectedSpan.floor_z == actualSpan.floor_z
				&& expectedSpan.ceiling_z == actualSpan.ceiling_z
				&& expectedSpan.preferred_z == actualSpan.preferred_z
				&& expectedSpan.leaf_id == actualSpan.leaf_id
				&& expectedSpan.cluster_id == actualSpan.cluster_id
				&& expectedSpan.area_id == actualSpan.area_id
				&& expectedSpan.region_layer_id == actualSpan.region_layer_id
				&& expectedSpan.movement_flags == actualSpan.movement_flags
				&& expectedSpan.surface_flags == actualSpan.surface_flags
				&& expectedSpan.topology_flags == actualSpan.topology_flags
				&& expectedSpan.connector_hint_mask == actualSpan.connector_hint_mask
				&& expectedSpan.occupancy.hard_block == actualSpan.occupancy.hard_block
				&& expectedSpan.occupancy.soft_cost == actualSpan.occupancy.soft_cost
				&& expectedSpan.occupancy.stamp_frame == actualSpan.occupancy.stamp_frame;
			if ( !spanMatches ) {
				Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::SpanGridSpan );
				matches = false;
			}
		}
	}
	return matches;
}

/**
*	@brief	Compare two adjacency payloads field-by-field for round-trip identity.
*	@param	expectedAdjacency	Original adjacency payload.
*	@param	actualAdjacency	Decoded adjacency payload.
*	@param	outResult	[in,out] Round-trip result receiving mismatch counts.
*	@return	True when the two adjacency payloads match exactly.
**/
static const bool Nav2_Serialize_CompareRoundTripAdjacency( const nav2_span_adjacency_t &expectedAdjacency,
	const nav2_span_adjacency_t &actualAdjacency, nav2_serialized_roundtrip_result_t *outResult ) {
	bool matches = true;

	/**
	*    Compare edge counts first so structural drift is reported before descending into individual edge payload fields.
	**/
	if ( expectedAdjacency.edges.size() != actualAdjacency.edges.size() ) {
		Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::AdjacencyEdge );
		matches = false;
	}

	/**
	*    Compare edges in deterministic order across the shared prefix so payload-field drift is still reported even if the edge count changed.
	**/
	const size_t sharedEdgeCount = std::min( expectedAdjacency.edges.size(), actualAdjacency.edges.size() );
	for ( size_t edgeIndex = 0; edgeIndex < sharedEdgeCount; edgeIndex++ ) {
		const nav2_span_edge_t &expectedEdge = expectedAdjacency.edges[ edgeIndex ];
		const nav2_span_edge_t &actualEdge = actualAdjacency.edges[ edgeIndex ];
		const bool edgeMatches = expectedEdge.edge_id == actualEdge.edge_id
			&& expectedEdge.from_span_id == actualEdge.from_span_id
			&& expectedEdge.to_span_id == actualEdge.to_span_id
			&& expectedEdge.from_column_index == actualEdge.from_column_index
			&& expectedEdge.to_column_index == actualEdge.to_column_index
			&& expectedEdge.from_span_index == actualEdge.from_span_index
			&& expectedEdge.to_span_index == actualEdge.to_span_index
			&& expectedEdge.delta_x == actualEdge.delta_x
			&& expectedEdge.delta_y == actualEdge.delta_y
			&& expectedEdge.vertical_delta == actualEdge.vertical_delta
			&& expectedEdge.cost == actualEdge.cost
			&& expectedEdge.soft_penalty_cost == actualEdge.soft_penalty_cost
			&& expectedEdge.movement == actualEdge.movement
			&& expectedEdge.legality == actualEdge.legality
			&& expectedEdge.feature_bits == actualEdge.feature_bits
			&& expectedEdge.flags == actualEdge.flags;
		if ( !edgeMatches ) {
			Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::AdjacencyEdge );
			matches = false;
		}
	}
	return matches;
}

/**
*	@brief	Append raw bytes into a serialized blob.
*	@param	blob	Serialized blob being built.
*	@param	data	Source bytes to append.
*	@param	size	Number of bytes to append.
**/
static void Nav2_Serialize_AppendBytes( nav2_serialized_blob_t *blob, const void *data, const size_t size ) {
	// Validate the destination blob and source pointer before mutating the byte buffer.
	if ( !blob || !data || size == 0 ) {
		return;
	}

	// Append the requested number of bytes into the serialized blob buffer.
	const uint8_t *src = static_cast<const uint8_t *>( data );
	blob->bytes.insert( blob->bytes.end(), src, src + size );
}

/**
*	@brief	Overwrite bytes already present in a serialized blob.
*	@param	blob	Serialized blob being updated.
*	@param	offset	Byte offset to overwrite.
*	@param	data	Source bytes to copy into the blob.
*	@param	size	Number of bytes to overwrite.
*	@return	True when the overwrite fit inside the blob.
**/
static const bool Nav2_Serialize_OverwriteBytes( nav2_serialized_blob_t *blob, const size_t offset, const void *data, const size_t size ) {
	// Reject invalid overwrite requests before mutating any existing bytes.
	if ( !blob || !data || size == 0 || ( offset + size ) > blob->bytes.size() ) {
		return false;
	}

	// Copy the replacement bytes into the already-reserved blob range.
	std::memcpy( blob->bytes.data() + offset, data, size );
	return true;
}

/**
*	@brief	Initialize a bounded read cursor for one in-memory serialized blob.
*	@param	blob	Serialized blob to read from.
*	@param	out_reader	[out] Reader receiving the initialized bounds and cursor.
*	@return	True when the reader was initialized successfully.
**/
static const bool Nav2_Serialize_InitReader( const nav2_serialized_blob_t &blob, nav2_serialized_blob_reader_t *out_reader ) {
	// Require output storage and a non-empty blob before publishing a bounded reader state.
	if ( !out_reader || blob.bytes.empty() ) {
		return false;
	}

	// Initialize the reader so future reads stay inside the known byte range.
	*out_reader = {};
	out_reader->data = blob.bytes.data();
	out_reader->size = ( uint32_t )blob.bytes.size();
	out_reader->offset = 0;
	out_reader->failed = false;
	return true;
}

/**
*	@brief	Read one POD value from the bounded blob reader.
*	@param	reader	[in,out] Reader tracking the current byte cursor.
*	@param	out_value	[out] Value receiving the decoded bytes.
*	@return	True when the read succeeded without exceeding the blob bounds.
**/
template<typename T>
static const bool Nav2_Serialize_ReadValue( nav2_serialized_blob_reader_t *reader, T *out_value ) {
	// Reject invalid reader or output storage before attempting a bounded copy.
	if ( !reader || !out_value || reader->failed ) {
		return false;
	}

	// Reject truncated reads and permanently fail the reader once bounds are exceeded.
	if ( ( reader->offset + sizeof( T ) ) > reader->size ) {
		reader->failed = true;
		return false;
	}

	// Copy the POD value from the current cursor and advance the reader afterward.
	std::memcpy( out_value, reader->data + reader->offset, sizeof( T ) );
	reader->offset += ( uint32_t )sizeof( T );
	return true;
}

/**
*	@brief	Seek the bounded blob reader to a specific section offset.
*	@param	reader	[in,out] Reader tracking the current byte cursor.
*	@param	offset	Absolute byte offset to seek to.
*	@return	True when the requested offset lies within the blob bounds.
**/
static const bool Nav2_Serialize_SeekReader( nav2_serialized_blob_reader_t *reader, const uint32_t offset ) {
	// Reject invalid readers or out-of-range seek targets before mutating the cursor.
	if ( !reader || reader->failed || offset > reader->size ) {
		if ( reader ) {
			reader->failed = true;
		}
		return false;
	}

	// Commit the new cursor position for the next bounded read sequence.
	reader->offset = offset;
	return true;
}

/**
*	@brief	Build the serialized metadata payload for one span-grid.
*	@param	spanGrid	Runtime span-grid to summarize.
*	@return	Compact serialized metadata used by the span-grid section.
**/
static nav2_serialized_span_grid_meta_t Nav2_Serialize_BuildSpanGridMeta( const nav2_span_grid_t &spanGrid ) {
	// Start from deterministic defaults and then mirror the runtime counts and sizing metadata.
	nav2_serialized_span_grid_meta_t meta = {};
	meta.tile_size = spanGrid.tile_size;
	meta.cell_size_xy = spanGrid.cell_size_xy;
	meta.z_quant = spanGrid.z_quant;
	meta.column_count = ( uint32_t )spanGrid.columns.size();
	for ( const nav2_span_column_t &column : spanGrid.columns ) {
		// Accumulate the total span count so the reader can validate column span ranges strictly.
		meta.span_count += ( uint32_t )column.spans.size();
	}
	return meta;
}

/**
*	@brief	Build one compact serialized span record from the runtime span metadata.
*	@param	span	Runtime span to mirror.
*	@return	Compact serialized span payload.
**/
static nav2_serialized_span_t Nav2_Serialize_BuildSerializedSpan( const nav2_span_t &span ) {
	// Mirror the runtime span into a plain old data payload suitable for stable binary serialization.
	nav2_serialized_span_t serializedSpan = {};
	serializedSpan.span_id = span.span_id;
	serializedSpan.floor_z = span.floor_z;
	serializedSpan.ceiling_z = span.ceiling_z;
	serializedSpan.preferred_z = span.preferred_z;
	serializedSpan.leaf_id = span.leaf_id;
	serializedSpan.cluster_id = span.cluster_id;
	serializedSpan.area_id = span.area_id;
	serializedSpan.region_layer_id = span.region_layer_id;
	serializedSpan.movement_flags = span.movement_flags;
	serializedSpan.surface_flags = span.surface_flags;
	serializedSpan.topology_flags = span.topology_flags;
	serializedSpan.connector_hint_mask = span.connector_hint_mask;
	serializedSpan.occupancy_hard_block = span.occupancy.hard_block ? 1u : 0u;
  serializedSpan.serialized_soft_cost = span.occupancy.soft_cost;
	serializedSpan.serialized_stamp_frame = span.occupancy.stamp_frame;
	return serializedSpan;
}

/**
*	@brief	Rebuild one runtime span record from its compact serialized payload.
*	@param	serializedSpan	Serialized span payload to decode.
*	@return	Runtime span rebuilt from the serialized scalar fields.
**/
static nav2_span_t Nav2_Serialize_BuildRuntimeSpan( const nav2_serialized_span_t &serializedSpan ) {
	// Start from deterministic defaults and then mirror every persisted scalar field back into the runtime span shape.
	nav2_span_t runtimeSpan = {};
	runtimeSpan.span_id = serializedSpan.span_id;
	runtimeSpan.floor_z = serializedSpan.floor_z;
	runtimeSpan.ceiling_z = serializedSpan.ceiling_z;
	runtimeSpan.preferred_z = serializedSpan.preferred_z;
	runtimeSpan.leaf_id = serializedSpan.leaf_id;
	runtimeSpan.cluster_id = serializedSpan.cluster_id;
	runtimeSpan.area_id = serializedSpan.area_id;
	runtimeSpan.region_layer_id = serializedSpan.region_layer_id;
	runtimeSpan.movement_flags = serializedSpan.movement_flags;
	runtimeSpan.surface_flags = serializedSpan.surface_flags;
	runtimeSpan.topology_flags = serializedSpan.topology_flags;
	runtimeSpan.connector_hint_mask = serializedSpan.connector_hint_mask;

	/**
	*    Rebuild the sparse occupancy entry explicitly so the runtime span regains the same hard-block, soft-cost, and stamp metadata after deserialization.
	**/
	runtimeSpan.occupancy.hard_block = ( serializedSpan.occupancy_hard_block != 0 );
	runtimeSpan.occupancy.soft_cost = serializedSpan.serialized_soft_cost;
	runtimeSpan.occupancy.stamp_frame = serializedSpan.serialized_stamp_frame;
	return runtimeSpan;
}

/**
*	@brief	Build one compact serialized adjacency edge record from the runtime edge metadata.
*	@param	edge	Runtime edge to mirror.
*	@return	Compact serialized edge payload.
**/
static nav2_serialized_span_edge_t Nav2_Serialize_BuildSerializedEdge( const nav2_span_edge_t &edge ) {
	// Mirror the runtime edge into a plain old data payload suitable for stable binary serialization.
	nav2_serialized_span_edge_t serializedEdge = {};
	serializedEdge.edge_id = edge.edge_id;
	serializedEdge.from_span_id = edge.from_span_id;
	serializedEdge.to_span_id = edge.to_span_id;
	serializedEdge.from_column_index = edge.from_column_index;
	serializedEdge.to_column_index = edge.to_column_index;
	serializedEdge.from_span_index = edge.from_span_index;
	serializedEdge.to_span_index = edge.to_span_index;
	serializedEdge.delta_x = edge.delta_x;
	serializedEdge.delta_y = edge.delta_y;
	serializedEdge.vertical_delta = edge.vertical_delta;
	serializedEdge.cost = edge.cost;
	serializedEdge.soft_penalty_cost = edge.soft_penalty_cost;
	serializedEdge.movement = edge.movement;
	serializedEdge.legality = edge.legality;
	serializedEdge.feature_bits = edge.feature_bits;
	serializedEdge.flags = edge.flags;
	return serializedEdge;
}

/**
*	@brief	Append the span-grid section payload into the serialized blob.
*	@param	spanGrid	Runtime span-grid to serialize.
*	@param	blob	[in,out] Serialized blob receiving the section payload.
*	@param	out_desc	[out] Section descriptor receiving the final offset, size, and version.
*	@return	True when the section payload was appended successfully.
**/
static const bool Nav2_Serialize_AppendSpanGridSection( const nav2_span_grid_t &spanGrid, nav2_serialized_blob_t *blob,
	nav2_serialized_section_desc_t *out_desc ) {
	// Require output storage for both the blob and the resulting section descriptor before appending any bytes.
	if ( !blob || !out_desc ) {
		return false;
	}

	// Start the section descriptor at the current end of the serialized blob so the payload stays offset-addressable.
	*out_desc = {};
	out_desc->type = nav2_serialized_section_type_t::SpanGrid;
	out_desc->offset = ( uint32_t )blob->bytes.size();
	out_desc->version = NAV2_SERIALIZED_SECTION_VERSION_SPAN_GRID;

	// Write the compact section metadata first so the reader knows how many columns and spans to expect.
	const nav2_serialized_span_grid_meta_t meta = Nav2_Serialize_BuildSpanGridMeta( spanGrid );
	Nav2_Serialize_AppendBytes( blob, &meta, sizeof( meta ) );

	// Serialize each sparse column together with its explicit span range inside the flattened span array.
	uint32_t runningSpanIndex = 0;
	for ( const nav2_span_column_t &column : spanGrid.columns ) {
		nav2_serialized_span_column_t serializedColumn = {};
		serializedColumn.tile_x = column.tile_x;
		serializedColumn.tile_y = column.tile_y;
		serializedColumn.tile_id = column.tile_id;
		serializedColumn.first_span_index = runningSpanIndex;
		serializedColumn.span_count = ( uint32_t )column.spans.size();
		Nav2_Serialize_AppendBytes( blob, &serializedColumn, sizeof( serializedColumn ) );
		runningSpanIndex += serializedColumn.span_count;
	}

	// Serialize every span in canonical column order so round-trip stability matches the runtime layout.
	for ( const nav2_span_column_t &column : spanGrid.columns ) {
		for ( const nav2_span_t &span : column.spans ) {
			const nav2_serialized_span_t serializedSpan = Nav2_Serialize_BuildSerializedSpan( span );
			Nav2_Serialize_AppendBytes( blob, &serializedSpan, sizeof( serializedSpan ) );
		}
	}

	// Publish the final section size after all payload bytes were appended.
	out_desc->size = ( uint32_t )blob->bytes.size() - out_desc->offset;
	return true;
}

/**
*	@brief	Append the adjacency section payload into the serialized blob.
*	@param	adjacency	Runtime adjacency payload to serialize.
*	@param	blob	[in,out] Serialized blob receiving the section payload.
*	@param	out_desc	[out] Section descriptor receiving the final offset, size, and version.
*	@return	True when the section payload was appended successfully.
**/
static const bool Nav2_Serialize_AppendAdjacencySection( const nav2_span_adjacency_t &adjacency, nav2_serialized_blob_t *blob,
	nav2_serialized_section_desc_t *out_desc ) {
	// Require output storage for both the blob and the resulting section descriptor before appending any bytes.
	if ( !blob || !out_desc ) {
		return false;
	}

	// Start the section descriptor at the current end of the serialized blob so the payload stays offset-addressable.
	*out_desc = {};
	out_desc->type = nav2_serialized_section_type_t::Adjacency;
	out_desc->offset = ( uint32_t )blob->bytes.size();
	out_desc->version = NAV2_SERIALIZED_SECTION_VERSION_ADJACENCY;

	// Write the compact adjacency metadata first so the reader knows how many edges to decode.
	const nav2_serialized_adjacency_meta_t meta = { ( uint32_t )adjacency.edges.size() };
	Nav2_Serialize_AppendBytes( blob, &meta, sizeof( meta ) );

	// Serialize every pointer-free adjacency edge in published deterministic order.
	for ( const nav2_span_edge_t &edge : adjacency.edges ) {
		const nav2_serialized_span_edge_t serializedEdge = Nav2_Serialize_BuildSerializedEdge( edge );
		Nav2_Serialize_AppendBytes( blob, &serializedEdge, sizeof( serializedEdge ) );
	}

	// Publish the final section size after all payload bytes were appended.
	out_desc->size = ( uint32_t )blob->bytes.size() - out_desc->offset;
	return true;
}

/**
*	@brief	Find the first matching section descriptor inside a decoded section table.
*	@param	sections	Decoded section descriptor array.
*	@param	type	Stable section type to locate.
*	@return	Pointer to the first matching section descriptor, or `nullptr` when absent.
**/
static const nav2_serialized_section_desc_t *Nav2_Serialize_FindSection( const std::vector<nav2_serialized_section_desc_t> &sections,
	const nav2_serialized_section_type_t type ) {
	// Scan the decoded section list in order and return the first matching stable section identifier.
	for ( const nav2_serialized_section_desc_t &section : sections ) {
		if ( section.type == type ) {
			return &section;
		}
	}
	return nullptr;
}

/**
*	@brief	Validate that one section descriptor lies entirely within the serialized blob.
*	@param	blobSize	Total byte size of the serialized blob.
*	@param	section	Section descriptor to validate.
*	@return	True when the section offset and size lie inside the blob bounds.
**/
static const bool Nav2_Serialize_IsSectionRangeValid( const uint32_t blobSize, const nav2_serialized_section_desc_t &section ) {
	// Reject sections whose range would exceed the serialized blob bounds or underflow on addition.
	return section.offset <= blobSize && section.size <= ( blobSize - section.offset );
}

/**
*	@brief	Decode all section descriptors that follow a validated nav2 serialized header.
*	@param	reader	[in,out] Bounded reader positioned immediately after the file header.
*	@param	header	Decoded nav2 serialized header.
*	@param	out_sections	[out] Vector receiving the decoded section descriptors.
*	@return	True when all section descriptors were decoded successfully.
**/
static const bool Nav2_Serialize_ReadSectionTable( nav2_serialized_blob_reader_t *reader, const nav2_serialized_header_t &header,
	std::vector<nav2_serialized_section_desc_t> *out_sections ) {
	// Require output storage before attempting to decode any section metadata.
	if ( !reader || !out_sections ) {
		return false;
	}

	// Reset the destination vector so callers never observe stale section descriptors from an earlier decode attempt.
	out_sections->clear();
	out_sections->reserve( header.section_count );
	for ( uint32_t sectionIndex = 0; sectionIndex < header.section_count; sectionIndex++ ) {
		// Decode one section descriptor at a time and reject truncated tables immediately.
		nav2_serialized_section_desc_t section = {};
		if ( !Nav2_Serialize_ReadValue( reader, &section ) ) {
			return false;
		}
		out_sections->push_back( section );
	}
	return true;
}

/**
*	@brief	Decode the serialized span-grid section into runtime data structures.
*	@param	blob	Serialized blob containing the section payload.
*	@param	section	Section descriptor describing the span-grid payload.
*	@param	outSpanGrid	[out] Runtime span-grid receiving the decoded payload.
*	@return	True when the span-grid payload decoded successfully.
**/
static const bool Nav2_Serialize_ReadSpanGridSection( const nav2_serialized_blob_t &blob, const nav2_serialized_section_desc_t &section,
	nav2_span_grid_t *outSpanGrid ) {
	// Require output storage and a valid section range before decoding the span-grid payload.
	if ( !outSpanGrid || !Nav2_Serialize_IsSectionRangeValid( ( uint32_t )blob.bytes.size(), section ) ) {
		return false;
	}

	// Reject unsupported section versions up front so later payload decoding stays simple and safe.
	if ( section.version != NAV2_SERIALIZED_SECTION_VERSION_SPAN_GRID ) {
		return false;
	}

	// Initialize a bounded reader for the section payload range.
	nav2_serialized_blob_reader_t reader = {};
	if ( !Nav2_Serialize_InitReader( blob, &reader ) || !Nav2_Serialize_SeekReader( &reader, section.offset ) ) {
		return false;
	}
	const uint32_t sectionEnd = section.offset + section.size;

	// Decode the section metadata first so the reader knows exactly how many columns and spans must follow.
	nav2_serialized_span_grid_meta_t meta = {};
	if ( !Nav2_Serialize_ReadValue( &reader, &meta ) ) {
		return false;
	}

	// Reject obviously malformed sizing or count metadata before resizing any runtime containers.
	if ( meta.tile_size < 0 || meta.cell_size_xy < 0.0 || meta.z_quant < 0.0 ) {
		return false;
	}

	// Decode the serialized sparse columns into a temporary list so span ranges can be validated before committing the runtime result.
	std::vector<nav2_serialized_span_column_t> serializedColumns = {};
	serializedColumns.reserve( meta.column_count );
	for ( uint32_t columnIndex = 0; columnIndex < meta.column_count; columnIndex++ ) {
		nav2_serialized_span_column_t serializedColumn = {};
		if ( !Nav2_Serialize_ReadValue( &reader, &serializedColumn ) ) {
			return false;
		}
		if ( serializedColumn.span_count > meta.span_count || serializedColumn.first_span_index > meta.span_count ) {
			return false;
		}
		if ( serializedColumn.first_span_index + serializedColumn.span_count > meta.span_count ) {
			return false;
		}
		serializedColumns.push_back( serializedColumn );
	}

	// Decode the flattened span payload array once so columns can rebuild their ownership through the validated ranges.
	std::vector<nav2_serialized_span_t> serializedSpans = {};
	serializedSpans.reserve( meta.span_count );
	for ( uint32_t spanIndex = 0; spanIndex < meta.span_count; spanIndex++ ) {
		nav2_serialized_span_t serializedSpan = {};
		if ( !Nav2_Serialize_ReadValue( &reader, &serializedSpan ) ) {
			return false;
		}
		serializedSpans.push_back( serializedSpan );
	}

	// Reject trailing garbage or truncated section payloads by requiring the read cursor to end exactly at the declared section boundary.
	if ( reader.failed || reader.offset != sectionEnd ) {
		return false;
	}

	// Rebuild the runtime span-grid deterministically from the validated serialized payload.
	nav2_span_grid_t decodedSpanGrid = {};
	decodedSpanGrid.tile_size = meta.tile_size;
	decodedSpanGrid.cell_size_xy = meta.cell_size_xy;
	decodedSpanGrid.z_quant = meta.z_quant;
	decodedSpanGrid.columns.reserve( serializedColumns.size() );
 for ( const nav2_serialized_span_column_t &serializedColumn : serializedColumns ) {
		nav2_span_column_t decodedColumn = {};
		decodedColumn.tile_x = serializedColumn.tile_x;
		decodedColumn.tile_y = serializedColumn.tile_y;
		decodedColumn.tile_id = serializedColumn.tile_id;
		decodedColumn.spans.reserve( serializedColumn.span_count );
		for ( uint32_t localSpanIndex = 0; localSpanIndex < serializedColumn.span_count; localSpanIndex++ ) {
			const nav2_serialized_span_t &serializedSpan = serializedSpans[ serializedColumn.first_span_index + localSpanIndex ];

			/**
			*    Rebuild the runtime span through the dedicated helper so the decode loop stays compact and the persisted occupancy metadata is restored consistently.
			**/
			decodedColumn.spans.push_back( Nav2_Serialize_BuildRuntimeSpan( serializedSpan ) );
		}
        decodedSpanGrid.columns.push_back( decodedColumn );
	}

	// Publish the decoded runtime span-grid only after the entire section has been validated successfully.
	*outSpanGrid = decodedSpanGrid;
	return true;
}

/**
*	@brief	Decode the serialized adjacency section into runtime data structures.
*	@param	blob	Serialized blob containing the section payload.
*	@param	section	Section descriptor describing the adjacency payload.
*	@param	outAdjacency	[out] Runtime adjacency receiving the decoded payload.
*	@return	True when the adjacency payload decoded successfully.
**/
static const bool Nav2_Serialize_ReadAdjacencySection( const nav2_serialized_blob_t &blob, const nav2_serialized_section_desc_t &section,
	nav2_span_adjacency_t *outAdjacency ) {
	// Require output storage and a valid section range before decoding the adjacency payload.
	if ( !outAdjacency || !Nav2_Serialize_IsSectionRangeValid( ( uint32_t )blob.bytes.size(), section ) ) {
		return false;
	}

	// Reject unsupported section versions up front so later payload decoding stays simple and safe.
	if ( section.version != NAV2_SERIALIZED_SECTION_VERSION_ADJACENCY ) {
		return false;
	}

	// Initialize a bounded reader for the section payload range.
	nav2_serialized_blob_reader_t reader = {};
	if ( !Nav2_Serialize_InitReader( blob, &reader ) || !Nav2_Serialize_SeekReader( &reader, section.offset ) ) {
		return false;
	}
	const uint32_t sectionEnd = section.offset + section.size;

	// Decode the section metadata first so the reader knows exactly how many edges must follow.
	nav2_serialized_adjacency_meta_t meta = {};
	if ( !Nav2_Serialize_ReadValue( &reader, &meta ) ) {
		return false;
	}

	// Decode the flat adjacency edge payload array deterministically.
	nav2_span_adjacency_t decodedAdjacency = {};
	decodedAdjacency.edges.reserve( meta.edge_count );
	for ( uint32_t edgeIndex = 0; edgeIndex < meta.edge_count; edgeIndex++ ) {
		nav2_serialized_span_edge_t serializedEdge = {};
		if ( !Nav2_Serialize_ReadValue( &reader, &serializedEdge ) ) {
			return false;
		}
		nav2_span_edge_t edge = {};
		edge.edge_id = serializedEdge.edge_id;
		edge.from_span_id = serializedEdge.from_span_id;
		edge.to_span_id = serializedEdge.to_span_id;
		edge.from_column_index = serializedEdge.from_column_index;
		edge.to_column_index = serializedEdge.to_column_index;
		edge.from_span_index = serializedEdge.from_span_index;
		edge.to_span_index = serializedEdge.to_span_index;
		edge.delta_x = serializedEdge.delta_x;
		edge.delta_y = serializedEdge.delta_y;
		edge.vertical_delta = serializedEdge.vertical_delta;
		edge.cost = serializedEdge.cost;
		edge.soft_penalty_cost = serializedEdge.soft_penalty_cost;
		edge.movement = serializedEdge.movement;
		edge.legality = serializedEdge.legality;
		edge.feature_bits = serializedEdge.feature_bits;
		edge.flags = serializedEdge.flags;
		decodedAdjacency.edges.push_back( edge );
	}

	// Reject trailing garbage or truncated section payloads by requiring the read cursor to end exactly at the declared section boundary.
	if ( reader.failed || reader.offset != sectionEnd ) {
		return false;
	}

	// Publish the decoded runtime adjacency only after the entire section has been validated successfully.
	*outAdjacency = decodedAdjacency;
	return true;
}


/**
*
*
*	Nav2 Serialization Public API:
*
*
**/
/**
*	@brief	Initialize a nav2 serialized header from a policy descriptor.
*	@param	header	[out] Header to initialize.
*	@param	policy	Serialization policy driving the header contents.
*	@param	magic	Magic value to embed in the header.
**/
void SVG_Nav2_Serialize_InitHeader( nav2_serialized_header_t *header, const nav2_serialization_policy_t &policy, const uint32_t magic ) {
	// Validate the destination header before attempting to fill any fields.
	if ( !header ) {
		return;
	}

	// Start from deterministic defaults so omitted fields remain stable across builds.
	*header = nav2_serialized_header_t{};
	header->magic = magic;
	header->format_version = NAV_SERIALIZED_FORMAT_VERSION;
	header->build_version = NAV_SERIALIZED_BUILD_VERSION;
	header->category = policy.category;
	header->transport = policy.transport;
	header->map_identity = policy.map_identity;
	header->section_count = 0;
	header->compatibility_flags = 0;
}

/**
*	@brief	Validate a nav2 serialized header against an expected policy and transport magic.
*	@param	header	Header to validate.
*	@param	policy	Expected serialization policy.
*	@param	expectedMagic	Expected transport magic.
*	@return	Structured validation result used by later cache and save/load code.
**/
nav2_serialized_validation_t SVG_Nav2_Serialize_ValidateHeader( const nav2_serialized_header_t &header,
	const nav2_serialization_policy_t &policy, const uint32_t expectedMagic ) {
	// Start from a conservative reject stance and only relax once all required header checks pass.
	nav2_serialized_validation_t validation = {};
	validation.compatibility = nav2_serialized_compatibility_t::Reject;

	// Validate transport identity first so callers can reject obviously wrong files immediately.
	validation.magic_ok = ( header.magic == expectedMagic );
	validation.format_ok = ( header.format_version == NAV_SERIALIZED_FORMAT_VERSION );
	validation.build_ok = ( header.build_version == NAV_SERIALIZED_BUILD_VERSION );
	validation.category_ok = ( header.category == policy.category );
	validation.transport_ok = ( header.transport == policy.transport );

	// Static format or transport mismatches are hard rejects because later section parsing would be unsafe.
	if ( !validation.magic_ok || !validation.format_ok || !validation.category_ok || !validation.transport_ok ) {
		validation.compatibility = nav2_serialized_compatibility_t::Reject;
		return validation;
	}

	// Build-version mismatches are softer: the payload shape is understood but the content should be rebuilt.
	if ( !validation.build_ok ) {
		validation.compatibility = nav2_serialized_compatibility_t::RebuildRequired;
		return validation;
	}

	// All relevant compatibility checks passed, so the payload header is acceptable.
	validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return validation;
}

/**
*	@brief	Build a placeholder standalone nav2 cache blob for the requested policy.
*	@param	policy	Serialization policy describing the payload.
*	@param	outBlob	[out] Blob receiving the serialized bytes.
*	@return	Structured serialization result including byte count.
*	@note	The current foundation writes only the versioned header and section container.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_BuildCacheBlob( const nav2_serialization_policy_t &policy,
	nav2_serialized_blob_t *outBlob ) {
	// Reject requests that do not provide output storage for the serialized blob.
	nav2_serialization_result_t result = {};
	if ( !outBlob ) {
		return result;
	}

	// Reset the output blob so callers never observe stale bytes from a prior build attempt.
	outBlob->bytes.clear();

	// Build a header-only placeholder payload that establishes versioning and transport policy.
	nav2_serialized_header_t header = {};
	SVG_Nav2_Serialize_InitHeader( &header, policy, NAV_SERIALIZED_FILE_MAGIC );
	Nav2_Serialize_AppendBytes( outBlob, &header, sizeof( header ) );

	// Publish the serialization result summary.
	result.success = true;
	result.byte_count = ( uint32_t )outBlob->bytes.size();
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return result;
}

/**
*	@brief	Build a standalone nav2 cache blob containing the current span-grid and adjacency payloads.
*	@param	policy	Serialization policy describing the payload.
*	@param	spanGrid	Sparse span-grid payload to serialize.
*	@param	adjacency	Local adjacency payload to serialize.
*	@param	outBlob	[out] Blob receiving the serialized bytes.
*	@return	Structured serialization result including byte count and validation state.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_BuildStaticNavBlob( const nav2_serialization_policy_t &policy,
	const nav2_span_grid_t &spanGrid, const nav2_span_adjacency_t &adjacency, nav2_serialized_blob_t *outBlob ) {
	// Reject requests that do not provide output storage for the serialized blob.
	nav2_serialization_result_t result = {};
	if ( !outBlob ) {
		return result;
	}

	// Reset the output blob so callers never observe stale bytes from a prior build attempt.
	outBlob->bytes.clear();

	// Reserve space for the header and both section descriptors up front so they can be patched after payload sizes are known.
	nav2_serialized_header_t header = {};
	SVG_Nav2_Serialize_InitHeader( &header, policy, NAV_SERIALIZED_FILE_MAGIC );
	header.section_count = 2;
	const size_t headerOffset = outBlob->bytes.size();
	Nav2_Serialize_AppendBytes( outBlob, &header, sizeof( header ) );
	const size_t sectionTableOffset = outBlob->bytes.size();
	nav2_serialized_section_desc_t placeholderSections[ 2 ] = {};
	Nav2_Serialize_AppendBytes( outBlob, placeholderSections, sizeof( placeholderSections ) );

	// Append the span-grid and adjacency payloads and capture their final section descriptors.
	nav2_serialized_section_desc_t sections[ 2 ] = {};
	if ( !Nav2_Serialize_AppendSpanGridSection( spanGrid, outBlob, &sections[ 0 ] ) ) {
		outBlob->bytes.clear();
		return result;
	}
	if ( !Nav2_Serialize_AppendAdjacencySection( adjacency, outBlob, &sections[ 1 ] ) ) {
		outBlob->bytes.clear();
		return result;
	}

	// Patch the finalized header and section descriptors into the reserved space at the front of the blob.
	if ( !Nav2_Serialize_OverwriteBytes( outBlob, headerOffset, &header, sizeof( header ) )
		|| !Nav2_Serialize_OverwriteBytes( outBlob, sectionTableOffset, sections, sizeof( sections ) ) ) {
		outBlob->bytes.clear();
		return result;
	}

	// Publish the successful serialization summary.
	result.success = true;
	result.byte_count = ( uint32_t )outBlob->bytes.size();
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return result;
}

/**
*	@brief	Write a nav2 standalone cache blob through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative path to write.
*	@param	blob	Serialized blob to write.
*	@return	Structured serialization result including byte count.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_WriteCacheFile( const char *cachePath, const nav2_serialized_blob_t &blob ) {
	// Validate the caller inputs before attempting filesystem IO.
	nav2_serialization_result_t result = {};
	if ( !cachePath || cachePath[ 0 ] == '\0' || blob.bytes.empty() ) {
		return result;
	}

	// Build a writable path buffer and let the filesystem helper resolve the final location.
	char pathBuffer[ MAX_OSPATH ] = {};
	const bool writeOk = ( gi.FS_EasyWriteFile )( pathBuffer, sizeof( pathBuffer ), FS_MODE_WRITE, nullptr, cachePath, nullptr,
		blob.bytes.data(), blob.bytes.size() );
	if ( !writeOk ) {
		return result;
	}

	// Publish the byte count on successful write.
	result.success = true;
	result.byte_count = ( uint32_t )blob.bytes.size();
	result.validation.compatibility = nav2_serialized_compatibility_t::Accept;
	return result;
}

/**
*	@brief	Load and validate a nav2 standalone cache blob through engine filesystem helpers.
*	@param	cachePath	Absolute or game-relative path to load.
*	@param	policy	Expected serialization policy.
*	@param	outHeader	[out] Decoded header on success.
*	@return	Structured load result including validation state.
*	@note	Loaded FS buffers remain owned by the engine and are released internally via `gi.FS_FreeFile`.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadCacheHeader( const char *cachePath,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader ) {
	// Validate input storage before attempting filesystem IO.
	nav2_serialization_result_t result = {};
	if ( !cachePath || cachePath[ 0 ] == '\0' || !outHeader ) {
		return result;
	}

	// Load the serialized file through the engine filesystem so ownership stays compatible with the
	// existing svgame memory rules.
	void *fileBuffer = nullptr;
	const int32_t fileLength = ( gi.FS_LoadFile )( cachePath, &fileBuffer );
	if ( fileLength < ( int32_t )sizeof( nav2_serialized_header_t ) || !fileBuffer ) {
		if ( fileBuffer ) {
			( gi.FS_FreeFile )( fileBuffer );
		}
		return result;
	}

	// Copy the header out of the FS-owned buffer before validating or freeing the engine allocation.
	nav2_serialized_header_t header = {};
	std::memcpy( &header, fileBuffer, sizeof( header ) );
	( gi.FS_FreeFile )( fileBuffer );

	// Validate the header against the expected standalone-cache policy.
	result.validation = SVG_Nav2_Serialize_ValidateHeader( header, policy, NAV_SERIALIZED_FILE_MAGIC );
	if ( result.validation.compatibility == nav2_serialized_compatibility_t::Reject ) {
		return result;
	}

	// Publish the decoded header and load summary once validation succeeded or requested rebuild.
	*outHeader = header;
	result.success = true;
	result.byte_count = ( uint32_t )fileLength;
	return result;
}

/**
*	@brief	Decode span-grid and adjacency payloads from a standalone nav2 cache blob already loaded in memory.
*	@param	blob	Serialized nav2 blob to decode.
*	@param	policy	Expected serialization policy.
*	@param	outHeader	[out] Decoded serialized header on success.
*	@param	outSpanGrid	[out] Decoded span-grid payload on success.
*	@param	outAdjacency	[out] Decoded adjacency payload on success.
*	@return	Structured deserialization result including byte count and validation state.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadStaticNavBlob( const nav2_serialized_blob_t &blob,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader,
	nav2_span_grid_t *outSpanGrid, nav2_span_adjacency_t *outAdjacency ) {
	// Require output storage for every decoded payload before attempting any bounded reads.
	nav2_serialization_result_t result = {};
	if ( !outHeader || !outSpanGrid || !outAdjacency ) {
		return result;
	}

	// Initialize a bounded reader for the full serialized blob and reject obviously too-small payloads immediately.
	nav2_serialized_blob_reader_t reader = {};
	if ( !Nav2_Serialize_InitReader( blob, &reader ) || blob.bytes.size() < sizeof( nav2_serialized_header_t ) ) {
		return result;
	}

	// Decode and validate the top-level header before touching any section payloads.
	nav2_serialized_header_t header = {};
	if ( !Nav2_Serialize_ReadValue( &reader, &header ) ) {
		return result;
	}
	result.validation = SVG_Nav2_Serialize_ValidateHeader( header, policy, NAV_SERIALIZED_FILE_MAGIC );
	if ( result.validation.compatibility == nav2_serialized_compatibility_t::Reject ) {
		return result;
	}

	// Decode the section table so the reader can locate span-grid and adjacency payloads by stable section id.
	std::vector<nav2_serialized_section_desc_t> sections = {};
	if ( !Nav2_Serialize_ReadSectionTable( &reader, header, &sections ) ) {
		return result;
	}

	// Resolve the mandatory span-grid and adjacency sections and reject payloads that omit either one.
	const nav2_serialized_section_desc_t *spanGridSection = Nav2_Serialize_FindSection( sections, nav2_serialized_section_type_t::SpanGrid );
	const nav2_serialized_section_desc_t *adjacencySection = Nav2_Serialize_FindSection( sections, nav2_serialized_section_type_t::Adjacency );
	if ( !spanGridSection || !adjacencySection ) {
		return result;
	}

	// Decode the two mandatory static-nav sections with strict bounded validation.
	nav2_span_grid_t decodedSpanGrid = {};
	nav2_span_adjacency_t decodedAdjacency = {};
	if ( !Nav2_Serialize_ReadSpanGridSection( blob, *spanGridSection, &decodedSpanGrid )
		|| !Nav2_Serialize_ReadAdjacencySection( blob, *adjacencySection, &decodedAdjacency ) ) {
		return result;
	}

	// Publish the decoded payloads only after every required section has been validated successfully.
	*outHeader = header;
	*outSpanGrid = decodedSpanGrid;
	*outAdjacency = decodedAdjacency;
	result.success = true;
	result.byte_count = ( uint32_t )blob.bytes.size();
	return result;
}

/**
*	@brief	Load and decode a standalone nav2 cache file containing span-grid and adjacency payloads.
*	@param	cachePath	Absolute or game-relative path to load.
*	@param	policy	Expected serialization policy.
*	@param	outHeader	[out] Decoded serialized header on success.
*	@param	outSpanGrid	[out] Decoded span-grid payload on success.
*	@param	outAdjacency	[out] Decoded adjacency payload on success.
*	@return	Structured load result including byte count and validation state.
*	@note	Loaded FS buffers remain owned by the engine and are released internally via `gi.FS_FreeFile`.
**/
nav2_serialization_result_t SVG_Nav2_Serialize_ReadStaticNavCacheFile( const char *cachePath,
	const nav2_serialization_policy_t &policy, nav2_serialized_header_t *outHeader,
	nav2_span_grid_t *outSpanGrid, nav2_span_adjacency_t *outAdjacency ) {
	// Require every output slot and a usable cache path before attempting filesystem IO.
	nav2_serialization_result_t result = {};
	if ( !cachePath || cachePath[ 0 ] == '\0' || !outHeader || !outSpanGrid || !outAdjacency ) {
		return result;
	}

	// Load the serialized blob through the engine filesystem so ownership stays compatible with existing svgame rules.
	void *fileBuffer = nullptr;
	const int32_t fileLength = ( gi.FS_LoadFile )( cachePath, &fileBuffer );
	if ( fileLength <= 0 || !fileBuffer ) {
		if ( fileBuffer ) {
			( gi.FS_FreeFile )( fileBuffer );
		}
		return result;
	}

	// Copy the filesystem-owned bytes into a serializer-owned blob before decoding so the FS buffer can be released immediately.
	nav2_serialized_blob_t blob = {};
	blob.bytes.resize( ( size_t )fileLength );
	std::memcpy( blob.bytes.data(), fileBuffer, ( size_t )fileLength );
	( gi.FS_FreeFile )( fileBuffer );

	// Decode the copied blob through the bounded in-memory static-nav reader.
	result = SVG_Nav2_Serialize_ReadStaticNavBlob( blob, policy, outHeader, outSpanGrid, outAdjacency );
	if ( result.success ) {
		result.byte_count = ( uint32_t )fileLength;
	}
	return result;
}

/**
*	@brief	Validate that static-nav payloads survive an in-memory serialize/read round-trip unchanged.
*	@param	policy	Serialization policy describing the payload.
*	@param	spanGrid	Source span-grid payload to round-trip.
*	@param	adjacency	Source adjacency payload to round-trip.
*	@param	outResult	[out] Structured round-trip validation result.
*	@return	True when the round-trip completed and the decoded payload matched exactly.
**/
const bool SVG_Nav2_Serialize_ValidateStaticNavRoundTrip( const nav2_serialization_policy_t &policy,
	const nav2_span_grid_t &spanGrid, const nav2_span_adjacency_t &adjacency, nav2_serialized_roundtrip_result_t *outResult ) {
	/**
	*    Require output storage before performing any serializer work so the caller always receives a deterministic result object.
	**/
	if ( !outResult ) {
		return false;
	}

	/**
	*    Reset the structured result up front so all counters and nested serialization summaries start from deterministic defaults.
	**/
	*outResult = {};

	/**
	*    Build the in-memory static-nav blob first because every later comparison depends on the encoded payload.
	**/
	nav2_serialized_blob_t blob = {};
  const auto buildStartTime = std::chrono::steady_clock::now();
	outResult->build_result = SVG_Nav2_Serialize_BuildStaticNavBlob( policy, spanGrid, adjacency, &blob );
   const auto buildEndTime = std::chrono::steady_clock::now();
	outResult->build_elapsed_ms = std::chrono::duration<double, std::milli>( buildEndTime - buildStartTime ).count();
	if ( !outResult->build_result.success ) {
		Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::DeserializeFailure );
		return false;
	}

	/**
	*    Decode the just-built blob through the same bounded reader path used by cache-file loads so the round-trip validation exercises the real deserializer logic.
	**/
	nav2_serialized_header_t decodedHeader = {};
	nav2_span_grid_t decodedSpanGrid = {};
	nav2_span_adjacency_t decodedAdjacency = {};
 const auto readStartTime = std::chrono::steady_clock::now();
	outResult->read_result = SVG_Nav2_Serialize_ReadStaticNavBlob( blob, policy, &decodedHeader, &decodedSpanGrid, &decodedAdjacency );
    const auto readEndTime = std::chrono::steady_clock::now();
	outResult->read_elapsed_ms = std::chrono::duration<double, std::milli>( readEndTime - readStartTime ).count();
	if ( !outResult->read_result.success ) {
		Nav2_Serialize_RecordRoundTripMismatch( outResult, nav2_serialized_roundtrip_mismatch_t::DeserializeFailure );
		return false;
	}

	/**
	*    Rebuild the expected header deterministically so the validation helper can confirm the encoded top-level metadata survived the round-trip unchanged.
	**/
	nav2_serialized_header_t expectedHeader = {};
	SVG_Nav2_Serialize_InitHeader( &expectedHeader, policy, NAV_SERIALIZED_FILE_MAGIC );
	expectedHeader.section_count = 2;

	/**
	*    Compare the decoded payloads field-by-field and accumulate stable mismatch counts for any serializer drift.
	**/
	bool matches = true;
	if ( !Nav2_Serialize_CompareRoundTripHeader( expectedHeader, decodedHeader, outResult ) ) {
		matches = false;
	}
	if ( !Nav2_Serialize_CompareRoundTripSpanGrid( spanGrid, decodedSpanGrid, outResult ) ) {
		matches = false;
	}
	if ( !Nav2_Serialize_CompareRoundTripAdjacency( adjacency, decodedAdjacency, outResult ) ) {
		matches = false;
	}

	/**
	*    Publish the final round-trip status only after all comparison helpers have had a chance to record their mismatch counts.
	**/
	outResult->success = matches && ( outResult->mismatch_count == 0 );
	return outResult->success;
}

/**
*	@brief	Return a stable display name for a static-nav round-trip mismatch category.
*	@param	mismatch	Mismatch category to convert.
*	@return	Constant string used by higher-level diagnostics or benchmarks.
**/
const char *SVG_Nav2_Serialize_RoundTripMismatchName( const nav2_serialized_roundtrip_mismatch_t mismatch ) {
	/**
	*    Convert the stable mismatch enum into a concise diagnostics string without allocating or formatting dynamically.
	**/
	switch ( mismatch ) {
	case nav2_serialized_roundtrip_mismatch_t::None:
		return "None";
	case nav2_serialized_roundtrip_mismatch_t::Header:
		return "Header";
	case nav2_serialized_roundtrip_mismatch_t::SpanGridMeta:
		return "SpanGridMeta";
	case nav2_serialized_roundtrip_mismatch_t::SpanGridColumn:
		return "SpanGridColumn";
	case nav2_serialized_roundtrip_mismatch_t::SpanGridSpan:
		return "SpanGridSpan";
	case nav2_serialized_roundtrip_mismatch_t::AdjacencyEdge:
		return "AdjacencyEdge";
	case nav2_serialized_roundtrip_mismatch_t::DeserializeFailure:
		return "DeserializeFailure";
	case nav2_serialized_roundtrip_mismatch_t::Count:
		break;
	default:
		break;
	}
	return "Unknown";
}
