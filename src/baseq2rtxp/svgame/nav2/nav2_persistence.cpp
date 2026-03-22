/********************************************************************
*
*
*	ServerGame: Nav2 Persistence Foundation - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_persistence.h"

#include <algorithm>
#include <cstring>


/**
*
*
*	Nav2 Persistence Local Payload Types:
*
*
**/
/**
*\t@brief\tCompact bundle metadata written at the start of the persistence payload section table.
**/
struct nav2_persistence_bundle_meta_t {
    //! Number of serialized region-layer records.
    uint32_t region_layer_count = 0;
    //! Number of serialized hierarchy node records.
    uint32_t hierarchy_node_count = 0;
    //! Number of serialized hierarchy edge records.
    uint32_t hierarchy_edge_count = 0;
    //! Number of serialized mover-local model records.
    uint32_t mover_model_count = 0;
    //! Number of serialized occupancy records.
    uint32_t occupancy_count = 0;
    //! Number of serialized dynamic overlay entries.
    uint32_t overlay_count = 0;
};

/**
*\t@brief\tFlat serialized representation of one region-layer node.
**/
struct nav2_persistence_region_layer_t {
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    uint32_t kind = 0;
    int32_t leaf_id = -1;
    int32_t cluster_id = -1;
    int32_t area_id = -1;
    int32_t connector_id = -1;
    int32_t mover_entnum = -1;
    int32_t inline_model_index = -1;
    nav2_corridor_z_band_t allowed_z_band = {};
    nav2_corridor_topology_ref_t topology = {};
    nav2_corridor_tile_ref_t tile_ref = {};
    uint32_t flags = 0;
};

/**
*\t@brief\tFlat serialized representation of one hierarchy node.
**/
struct nav2_persistence_hierarchy_node_t {
    int32_t node_id = -1;
    uint32_t kind = 0;
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    int32_t region_layer_index = -1;
    int32_t connector_id = -1;
    int32_t mover_entnum = -1;
    int32_t inline_model_index = -1;
    nav2_corridor_topology_ref_t topology = {};
    nav2_corridor_z_band_t allowed_z_band = {};
    uint32_t flags = 0;
};

/**
*\t@brief\tFlat serialized representation of one hierarchy edge.
**/
struct nav2_persistence_hierarchy_edge_t {
    int32_t edge_id = -1;
    int32_t from_node_id = -1;
    int32_t to_node_id = -1;
    uint32_t kind = 0;
    uint32_t flags = 0;
    double base_cost = 0.0;
    double topology_penalty = 0.0;
    nav2_corridor_z_band_t allowed_z_band = {};
    int32_t connector_id = -1;
    int32_t region_layer_id = NAV_REGION_ID_NONE;
    nav2_corridor_mover_ref_t mover_ref = {};
};

/**
*\t@brief\tFlat serialized representation of one mover-local model record.
**/
struct nav2_persistence_mover_model_t {
    int32_t owner_entnum = -1;
    int32_t inline_model_index = -1;
    uint32_t role_flags = 0;
    uint8_t headnode_valid = 0;
    uint8_t reusable_local_space = 0;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
};

/**
*\t@brief\tFlat serialized representation of one sparse occupancy record.
**/
struct nav2_persistence_occupancy_record_t {
    int32_t occupancy_id = -1;
    uint32_t kind = 0;
    int32_t span_id = -1;
    int32_t edge_id = -1;
    int32_t connector_id = -1;
    int32_t mover_entnum = -1;
    int32_t inline_model_index = -1;
    int32_t leaf_id = -1;
    int32_t cluster_id = -1;
    int32_t area_id = -1;
    uint32_t decision = 0;
    double soft_penalty = 0.0;
    nav2_corridor_z_band_t allowed_z_band = {};
    uint32_t flags = 0;
    int64_t stamp_frame = -1;
};

/**
*\t@brief\tFlat serialized representation of one dynamic overlay entry.
**/
struct nav2_persistence_overlay_entry_t {
    int32_t overlay_id = -1;
    nav2_corridor_topology_ref_t topology = {};
    nav2_corridor_mover_ref_t mover_ref = {};
    uint32_t decision = 0;
    double soft_penalty = 0.0;
    uint32_t flags = 0;
    int64_t stamp_frame = -1;
};

/**
*\t@brief\tDecoded bundle section offsets used while writing or reading a persistence blob.
**/
struct nav2_persistence_section_offsets_t {
    //! Region-layer section descriptor.
    nav2_serialized_section_desc_t region_layers = {};
    //! Hierarchy section descriptor.
    nav2_serialized_section_desc_t hierarchy = {};
    //! Mover-local models section descriptor.
    nav2_serialized_section_desc_t mover_models = {};
    //! Dynamic overlay section descriptor.
    nav2_serialized_section_desc_t overlay = {};
};

//! Section table size used by the persistence blob format.
static constexpr uint32_t NAV2_PERSISTENCE_SECTION_COUNT = 4;


/**
*
*
*	Nav2 Persistence Local Helpers:
*
*
**/
/**
*\t@brief\tAppend raw bytes into a serialized blob.
*\t@param\tblob\tSerialized blob being built.
*\t@param\tdata\tSource bytes to append.
*\t@param\tsize\tNumber of bytes to append.
**/
static void SVG_Nav2_Persistence_AppendBytes( nav2_serialized_blob_t *blob, const void *data, const size_t size ) {
    // Validate the destination blob and source pointer before mutating the byte buffer.
    if ( !blob || !data || size == 0 ) {
        return;
    }

    // Append the requested bytes into the serialized blob buffer.
    const uint8_t *src = static_cast<const uint8_t *>( data );
    blob->bytes.insert( blob->bytes.end(), src, src + size );
}

/**
*\t@brief\tOverwrite bytes already present in a serialized blob.
*\t@param\tblob\tSerialized blob being updated.
*\t@param\toffset\tByte offset to overwrite.
*\t@param\tdata\tSource bytes to copy into the blob.
*\t@param\tsize\tNumber of bytes to overwrite.
*\t@return\tTrue when the overwrite fit inside the blob.
**/
static const bool SVG_Nav2_Persistence_OverwriteBytes( nav2_serialized_blob_t *blob, const size_t offset, const void *data, const size_t size ) {
    // Reject invalid overwrite requests before mutating any existing bytes.
    if ( !blob || !data || size == 0 || ( offset + size ) > blob->bytes.size() ) {
        return false;
    }

    // Copy the replacement bytes into the already-reserved blob range.
    std::memcpy( blob->bytes.data() + offset, data, size );
    return true;
}

/**
*\t@brief\tRead one POD value from a byte buffer.
*\t@param\tdata\tSource buffer to read from.
*\t@param\tsize\tBuffer size in bytes.
*\t@param\toffset\t[in,out] Cursor offset into the buffer.
*\t@param\toutValue\t[out] Value receiving the decoded bytes.
*\t@return\tTrue when the read succeeded without exceeding the bounds.
**/
template<typename T>
static const bool SVG_Nav2_Persistence_ReadValue( const uint8_t *data, const size_t size, size_t *offset, T *outValue ) {
    // Reject invalid inputs before attempting a bounded copy.
    if ( !data || !offset || !outValue || ( *offset + sizeof( T ) ) > size ) {
        return false;
    }

    // Copy the POD value from the current cursor and advance the reader afterward.
    std::memcpy( outValue, data + *offset, sizeof( T ) );
    *offset += sizeof( T );
    return true;
}

/**
*\t@brief\tInitialize a versioned nav2 persistence header.
*\t@param\tpolicy\tSerialization policy driving the header contents.
*\t@param\tsectionCount\tNumber of payload sections that will be written.
*\t@param\tmagic\tMagic value to embed in the header.
*\t@return\tInitialized nav2 header.
**/
static nav2_serialized_header_t SVG_Nav2_Persistence_MakeHeader( const nav2_serialization_policy_t &policy, const uint32_t sectionCount, const uint32_t magic ) {
    // Build a stable header that mirrors the chosen policy and declared section count.
    nav2_serialized_header_t header = {};
    header.magic = magic;
    header.format_version = NAV_SERIALIZED_FORMAT_VERSION;
    header.build_version = NAV_SERIALIZED_BUILD_VERSION;
    header.category = policy.category != nav2_serialized_category_t::None ? policy.category : nav2_serialized_category_t::SavegameRuntime;
    header.transport = policy.transport != nav2_serialized_transport_t::None ? policy.transport : nav2_serialized_transport_t::EngineFilesystem;
    header.map_identity = policy.map_identity;
    header.section_count = sectionCount;
    header.compatibility_flags = 0;
    return header;
}

/**
*\t@brief\tCreate a section descriptor at the current end of a blob.
*\t@param\tblob\tBlob being built.
*\t@param\ttype\tSection type being appended.
*\t@param\tversion\tSection version to publish.
*\t@return\tInitialized section descriptor.
**/
static nav2_serialized_section_desc_t SVG_Nav2_Persistence_BeginSection( nav2_serialized_blob_t *blob, const nav2_serialized_section_type_t type, const uint32_t version ) {
    // Capture the current byte offset so the final descriptor stays offset-addressable.
    nav2_serialized_section_desc_t section = {};
    section.type = type;
    section.offset = blob ? ( uint32_t )blob->bytes.size() : 0;
    section.version = version;
    return section;
}

/**
*\t@brief\tFinalize a section descriptor after the payload bytes were appended.
*\t@param\tblob\tBlob that owns the payload bytes.
*\t@param\tsection\tSection descriptor to finalize.
**/
static void SVG_Nav2_Persistence_EndSection( nav2_serialized_blob_t *blob, nav2_serialized_section_desc_t *section ) {
    // Publish the final section size only after all payload bytes have been appended.
    if ( !blob || !section ) {
        return;
    }
    section->size = ( uint32_t )blob->bytes.size() - section->offset;
}

/**
*\t@brief\tAppend one serialized region layer record into a blob.
*\t@param\tlayer\tRegion-layer source record.
*\t@param\tblob\tBlob receiving the bytes.
**/
static void SVG_Nav2_Persistence_AppendRegionLayerRecord( const nav2_region_layer_t &layer, nav2_serialized_blob_t *blob ) {
    // Mirror the region-layer into a flat POD payload suitable for round-trip persistence.
    nav2_persistence_region_layer_t record = {};
    record.region_layer_id = layer.region_layer_id;
    record.kind = ( uint32_t )layer.kind;
    record.leaf_id = layer.leaf_id;
    record.cluster_id = layer.cluster_id;
    record.area_id = layer.area_id;
    record.connector_id = layer.connector_id;
    record.mover_entnum = layer.mover_entnum;
    record.inline_model_index = layer.inline_model_index;
    record.allowed_z_band = layer.allowed_z_band;
    record.topology = layer.topology;
    record.tile_ref = layer.tile_ref;
    record.flags = layer.flags;
    SVG_Nav2_Persistence_AppendBytes( blob, &record, sizeof( record ) );
}

/**
*\t@brief\tAppend one serialized hierarchy node record into a blob.
*\t@param\tnode\tHierarchy node source record.
*\t@param\tblob\tBlob receiving the bytes.
**/
static void SVG_Nav2_Persistence_AppendHierarchyNodeRecord( const nav2_hierarchy_node_t &node, nav2_serialized_blob_t *blob ) {
    // Mirror the hierarchy node into a flat POD payload suitable for round-trip persistence.
    nav2_persistence_hierarchy_node_t record = {};
    record.node_id = node.node_id;
    record.kind = ( uint32_t )node.kind;
    record.region_layer_id = node.region_layer_id;
    record.region_layer_index = node.region_layer_index;
    record.connector_id = node.connector_id;
    record.mover_entnum = node.mover_entnum;
    record.inline_model_index = node.inline_model_index;
    record.topology = node.topology;
    record.allowed_z_band = node.allowed_z_band;
    record.flags = node.flags;
    SVG_Nav2_Persistence_AppendBytes( blob, &record, sizeof( record ) );
}

/**
*\t@brief\tAppend one serialized hierarchy edge record into a blob.
*\t@param\tedge\tHierarchy edge source record.
*\t@param\tblob\tBlob receiving the bytes.
**/
static void SVG_Nav2_Persistence_AppendHierarchyEdgeRecord( const nav2_hierarchy_edge_t &edge, nav2_serialized_blob_t *blob ) {
    // Mirror the hierarchy edge into a flat POD payload suitable for round-trip persistence.
    nav2_persistence_hierarchy_edge_t record = {};
    record.edge_id = edge.edge_id;
    record.from_node_id = edge.from_node_id;
    record.to_node_id = edge.to_node_id;
    record.kind = ( uint32_t )edge.kind;
    record.flags = edge.flags;
    record.base_cost = edge.base_cost;
    record.topology_penalty = edge.topology_penalty;
    record.allowed_z_band = edge.allowed_z_band;
    record.connector_id = edge.connector_id;
    record.region_layer_id = edge.region_layer_id;
    record.mover_ref = edge.mover_ref;
    SVG_Nav2_Persistence_AppendBytes( blob, &record, sizeof( record ) );
}

/**
*\t@brief\tAppend one serialized mover-local model record into a blob.
*\t@param\tmodel\tMover-local model source record.
*\t@param\tblob\tBlob receiving the bytes.
**/
static void SVG_Nav2_Persistence_AppendMoverModelRecord( const nav2_mover_local_model_t &model, nav2_serialized_blob_t *blob ) {
    // Mirror the mover-local model into a flat POD payload suitable for round-trip persistence.
    nav2_persistence_mover_model_t record = {};
    record.owner_entnum = model.owner_entnum;
    record.inline_model_index = model.inline_model_index;
    record.role_flags = model.role_flags;
    record.headnode_valid = model.headnode_valid ? 1u : 0u;
    record.reusable_local_space = model.reusable_local_space ? 1u : 0u;
    SVG_Nav2_Persistence_AppendBytes( blob, &record, sizeof( record ) );
}

/**
*\t@brief\tAppend one serialized occupancy record into a blob.
*\t@param\trecord\tOccupancy source record.
*\t@param\tblob\tBlob receiving the bytes.
**/
static void SVG_Nav2_Persistence_AppendOccupancyRecord( const nav2_occupancy_record_t &record, nav2_serialized_blob_t *blob ) {
    // Mirror the occupancy record into a flat POD payload suitable for round-trip persistence.
    nav2_persistence_occupancy_record_t encoded = {};
    encoded.occupancy_id = record.occupancy_id;
    encoded.kind = ( uint32_t )record.kind;
    encoded.span_id = record.span_id;
    encoded.edge_id = record.edge_id;
    encoded.connector_id = record.connector_id;
    encoded.mover_entnum = record.mover_entnum;
    encoded.inline_model_index = record.inline_model_index;
    encoded.leaf_id = record.leaf_id;
    encoded.cluster_id = record.cluster_id;
    encoded.area_id = record.area_id;
    encoded.decision = ( uint32_t )record.decision;
    encoded.soft_penalty = record.soft_penalty;
    encoded.allowed_z_band = record.allowed_z_band;
    encoded.flags = record.flags;
    encoded.stamp_frame = record.stamp_frame;
    SVG_Nav2_Persistence_AppendBytes( blob, &encoded, sizeof( encoded ) );
}

/**
*\t@brief\tAppend one serialized overlay entry into a blob.
*\t@param\tentry\tOverlay source record.
*\t@param\tblob\tBlob receiving the bytes.
**/
static void SVG_Nav2_Persistence_AppendOverlayEntry( const nav2_occupancy_overlay_entry_t &entry, nav2_serialized_blob_t *blob ) {
    // Mirror the overlay entry into a flat POD payload suitable for round-trip persistence.
    nav2_persistence_overlay_entry_t encoded = {};
    encoded.overlay_id = entry.overlay_id;
    encoded.topology = entry.topology;
    encoded.mover_ref = entry.mover_ref;
    encoded.decision = ( uint32_t )entry.decision;
    encoded.soft_penalty = entry.soft_penalty;
    encoded.flags = entry.flags;
    encoded.stamp_frame = entry.stamp_frame;
    SVG_Nav2_Persistence_AppendBytes( blob, &encoded, sizeof( encoded ) );
}

/**
*\t@brief\tRebuild region-layer graph data from a decoded payload.
*\t@param\tbundle\tBundle receiving the decoded region-layer graph.
*\t@param\tdata\tSource payload bytes.
*\t@param\tsize\tPayload byte size.
*\t@param\toffset\t[in,out] Cursor into the payload.
*\t@return\tTrue when the payload decoded successfully.
**/
static const bool SVG_Nav2_Persistence_ReadRegionLayers( nav2_persistence_bundle_t *bundle, const uint8_t *data, const size_t size, size_t *offset ) {
    // Decode the count header before resizing the runtime graph.
    uint32_t count = 0;
    if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &count ) ) {
        return false;
    }

   // Read each flat layer record in deterministic order and stage it in a temporary vector before rebuilding the runtime graph.
    std::vector<nav2_region_layer_t> decodedLayers = {};
    decodedLayers.reserve( count );
    for ( uint32_t i = 0; i < count; i++ ) {
        nav2_persistence_region_layer_t record = {};
        if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &record ) ) {
            return false;
        }
        nav2_region_layer_t layer = {};
        layer.region_layer_id = record.region_layer_id;
        layer.kind = ( nav2_region_layer_kind_t )record.kind;
        layer.leaf_id = record.leaf_id;
        layer.cluster_id = record.cluster_id;
        layer.area_id = record.area_id;
        layer.connector_id = record.connector_id;
        layer.mover_entnum = record.mover_entnum;
        layer.inline_model_index = record.inline_model_index;
        layer.allowed_z_band = record.allowed_z_band;
        layer.topology = record.topology;
        layer.tile_ref = record.tile_ref;
        layer.flags = record.flags;
        decodedLayers.push_back( layer );
    }

    // Rebuild the reverse lookup so the runtime graph remains queryable after load.
    SVG_Nav2_RegionLayerGraph_Clear( &bundle->region_layers );
   bundle->region_layers.layers = decodedLayers;
    for ( const nav2_region_layer_t &layer : bundle->region_layers.layers ) {
        SVG_Nav2_RegionLayerGraph_AppendLayer( &bundle->region_layers, layer );
    }
    return true;
}

/**
*\t@brief\tRebuild hierarchy graph data from a decoded payload.
*\t@param\tbundle\tBundle receiving the decoded hierarchy graph.
*\t@param\tdata\tSource payload bytes.
*\t@param\tsize\tPayload byte size.
*\t@param\toffset\t[in,out] Cursor into the payload.
*\t@return\tTrue when the payload decoded successfully.
**/
static const bool SVG_Nav2_Persistence_ReadHierarchy( nav2_persistence_bundle_t *bundle, const uint8_t *data, const size_t size, size_t *offset ) {
    // Decode the node count before rebuilding the runtime graph.
    uint32_t nodeCount = 0;
    if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &nodeCount ) ) {
        return false;
    }

    // Rebuild nodes in deterministic order through a temporary vector so the final graph can be rebuilt cleanly.
    std::vector<nav2_hierarchy_node_t> decodedNodes = {};
    decodedNodes.reserve( nodeCount );
    for ( uint32_t i = 0; i < nodeCount; i++ ) {
        nav2_persistence_hierarchy_node_t record = {};
        if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &record ) ) {
            return false;
        }
        nav2_hierarchy_node_t node = {};
        node.node_id = record.node_id;
        node.kind = ( nav2_hierarchy_node_kind_t )record.kind;
        node.region_layer_id = record.region_layer_id;
        node.region_layer_index = record.region_layer_index;
        node.connector_id = record.connector_id;
        node.mover_entnum = record.mover_entnum;
        node.inline_model_index = record.inline_model_index;
        node.topology = record.topology;
        node.allowed_z_band = record.allowed_z_band;
        node.flags = record.flags;
        decodedNodes.push_back( node );
    }

    // Decode and append the hierarchy edges after the nodes so adjacency wiring can be rebuilt.
    uint32_t edgeCount = 0;
    if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &edgeCount ) ) {
        return false;
    }
  std::vector<nav2_hierarchy_edge_t> decodedEdges = {};
    decodedEdges.reserve( edgeCount );
    for ( uint32_t i = 0; i < edgeCount; i++ ) {
        nav2_persistence_hierarchy_edge_t record = {};
        if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &record ) ) {
            return false;
        }
        nav2_hierarchy_edge_t edge = {};
        edge.edge_id = record.edge_id;
        edge.from_node_id = record.from_node_id;
        edge.to_node_id = record.to_node_id;
        edge.kind = ( nav2_hierarchy_edge_kind_t )record.kind;
        edge.flags = record.flags;
        edge.base_cost = record.base_cost;
        edge.topology_penalty = record.topology_penalty;
        edge.allowed_z_band = record.allowed_z_band;
        edge.connector_id = record.connector_id;
        edge.region_layer_id = record.region_layer_id;
        edge.mover_ref = record.mover_ref;
        decodedEdges.push_back( edge );
    }

    // Rebuild adjacency lists and stable id lookup after all payload bytes were validated.
    SVG_Nav2_HierarchyGraph_Clear( &bundle->hierarchy_graph );
 bundle->hierarchy_graph.nodes = decodedNodes;
    bundle->hierarchy_graph.edges = decodedEdges;
    for ( const nav2_hierarchy_node_t &node : bundle->hierarchy_graph.nodes ) {
        SVG_Nav2_HierarchyGraph_AppendNode( &bundle->hierarchy_graph, node );
    }
    for ( const nav2_hierarchy_edge_t &edge : bundle->hierarchy_graph.edges ) {
        SVG_Nav2_HierarchyGraph_AppendEdge( &bundle->hierarchy_graph, edge );
    }
    return true;
}

/**
*\t@brief\tRebuild mover-local models from a decoded payload.
*\t@param\tbundle\tBundle receiving the decoded mover-local models.
*\t@param\tdata\tSource payload bytes.
*\t@param\tsize\tPayload byte size.
*\t@param\toffset\t[in,out] Cursor into the payload.
*\t@return\tTrue when the payload decoded successfully.
**/
static const bool SVG_Nav2_Persistence_ReadMoverModels( nav2_persistence_bundle_t *bundle, const uint8_t *data, const size_t size, size_t *offset ) {
    // Decode the mover-local model count before rebuilding the runtime vector.
    uint32_t count = 0;
    if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &count ) ) {
        return false;
    }

    // Read each mover-local model record in deterministic order.
    bundle->mover_local_models.clear();
    bundle->mover_local_models.reserve( count );
    for ( uint32_t i = 0; i < count; i++ ) {
        nav2_persistence_mover_model_t record = {};
        if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &record ) ) {
            return false;
        }
        nav2_mover_local_model_t model = {};
        model.owner_entnum = record.owner_entnum;
        model.inline_model_index = record.inline_model_index;
        model.role_flags = record.role_flags;
        model.headnode_valid = ( record.headnode_valid != 0 );
        model.reusable_local_space = ( record.reusable_local_space != 0 );
        bundle->mover_local_models.push_back( model );
    }
    return true;
}

/**
*\t@brief\tRebuild occupancy and dynamic overlay data from a decoded payload.
*\t@param\tbundle\tBundle receiving the decoded occupancy and overlay state.
*\t@param\tdata\tSource payload bytes.
*\t@param\tsize\tPayload byte size.
*\t@param\toffset\t[in,out] Cursor into the payload.
*\t@return\tTrue when the payload decoded successfully.
**/
static const bool SVG_Nav2_Persistence_ReadOccupancyOverlay( nav2_persistence_bundle_t *bundle, const uint8_t *data, const size_t size, size_t *offset ) {
    // Decode both counts before rebuilding the two runtime containers.
    uint32_t occupancyCount = 0;
    uint32_t overlayCount = 0;
    if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &occupancyCount ) || !SVG_Nav2_Persistence_ReadValue( data, size, offset, &overlayCount ) ) {
        return false;
    }

    // Rebuild the sparse occupancy grid deterministically.
    bundle->occupancy.records.clear();
    bundle->occupancy.by_span_id.clear();
    bundle->occupancy.by_edge_id.clear();
    bundle->occupancy.by_connector_id.clear();
    bundle->occupancy.by_mover_entnum.clear();
    for ( uint32_t i = 0; i < occupancyCount; i++ ) {
        nav2_persistence_occupancy_record_t record = {};
        if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &record ) ) {
            return false;
        }
        nav2_occupancy_record_t decoded = {};
        decoded.occupancy_id = record.occupancy_id;
        decoded.kind = ( nav2_occupancy_kind_t )record.kind;
        decoded.span_id = record.span_id;
        decoded.edge_id = record.edge_id;
        decoded.connector_id = record.connector_id;
        decoded.mover_entnum = record.mover_entnum;
        decoded.inline_model_index = record.inline_model_index;
        decoded.leaf_id = record.leaf_id;
        decoded.cluster_id = record.cluster_id;
        decoded.area_id = record.area_id;
        decoded.decision = ( nav2_occupancy_decision_t )record.decision;
        decoded.soft_penalty = record.soft_penalty;
        decoded.allowed_z_band = record.allowed_z_band;
        decoded.flags = record.flags;
        decoded.stamp_frame = record.stamp_frame;
        SVG_Nav2_OccupancyGrid_Upsert( &bundle->occupancy, decoded );
    }

    // Rebuild the dynamic overlay entries deterministically.
    bundle->dynamic_overlay.entries.clear();
    bundle->dynamic_overlay.by_overlay_id.clear();
    for ( uint32_t i = 0; i < overlayCount; i++ ) {
        nav2_persistence_overlay_entry_t record = {};
        if ( !SVG_Nav2_Persistence_ReadValue( data, size, offset, &record ) ) {
            return false;
        }
        nav2_occupancy_overlay_entry_t decoded = {};
        decoded.overlay_id = record.overlay_id;
        decoded.topology = record.topology;
        decoded.mover_ref = record.mover_ref;
        decoded.decision = ( nav2_occupancy_decision_t )record.decision;
        decoded.soft_penalty = record.soft_penalty;
        decoded.flags = record.flags;
        decoded.stamp_frame = record.stamp_frame;
        SVG_Nav2_DynamicOverlay_Upsert( &bundle->dynamic_overlay, decoded );
    }
    return true;
}

/**
*\t@brief\tFind the first section descriptor matching a requested type.
*\t@param\tsections\tDecoded section descriptor table.
*\t@param\ttype\tRequested section type.
*\t@return\tPointer to the matching descriptor, or nullptr when absent.
**/
static const nav2_serialized_section_desc_t *SVG_Nav2_Persistence_FindSection( const std::vector<nav2_serialized_section_desc_t> &sections, const nav2_serialized_section_type_t type ) {
    // Scan the small section list directly because the persistence foundation uses a fixed number of sections.
    for ( const nav2_serialized_section_desc_t &section : sections ) {
        if ( section.type == type ) {
            return &section;
        }
    }
    return nullptr;
}


/**
*
*
*	Nav2 Persistence Public API:
*
*
**/
/**
*\t@brief\tBuild a versioned nav2 persistence blob containing the supplied bundle.
*\t@param\tbundle\tNav2 bundle to serialize.
*\t@param\toutBlob\t[out] Blob receiving the serialized bytes.
*\t@return\tStructured build result including byte count.
**/
nav2_persistence_result_t SVG_Nav2_Persistence_BuildBundleBlob( const nav2_persistence_bundle_t &bundle, nav2_serialized_blob_t *outBlob ) {
    // Reject requests that do not provide output storage for the serialized blob.
    nav2_persistence_result_t result = {};
    if ( !outBlob ) {
        return result;
    }

    // Reset the output blob so callers never observe stale bytes from a prior build attempt.
    outBlob->bytes.clear();

    // Reserve the versioned header and fixed section table up front so the finalized descriptors can be patched in-place.
    constexpr uint32_t sectionCount = 4;
    nav2_serialized_header_t header = SVG_Nav2_Persistence_MakeHeader( bundle.policy, sectionCount, NAV_SERIALIZED_FILE_MAGIC );
    const size_t headerOffset = outBlob->bytes.size();
    SVG_Nav2_Persistence_AppendBytes( outBlob, &header, sizeof( header ) );
    const size_t sectionTableOffset = outBlob->bytes.size();
    nav2_serialized_section_desc_t placeholderSections[ sectionCount ] = {};
    SVG_Nav2_Persistence_AppendBytes( outBlob, placeholderSections, sizeof( placeholderSections ) );

   // Build the section descriptors and payloads in a fixed order so loads remain deterministic.
    nav2_serialized_section_desc_t sections[ NAV2_PERSISTENCE_SECTION_COUNT ] = {};
    sections[ 0 ] = SVG_Nav2_Persistence_BeginSection( outBlob, nav2_serialized_section_type_t::RegionLayers, 1 );
    nav2_persistence_bundle_meta_t bundleMeta = {};
    bundleMeta.region_layer_count = ( uint32_t )bundle.region_layers.layers.size();
    bundleMeta.hierarchy_node_count = ( uint32_t )bundle.hierarchy_graph.nodes.size();
    bundleMeta.hierarchy_edge_count = ( uint32_t )bundle.hierarchy_graph.edges.size();
    bundleMeta.mover_model_count = ( uint32_t )bundle.mover_local_models.size();
    bundleMeta.occupancy_count = ( uint32_t )bundle.occupancy.records.size();
    bundleMeta.overlay_count = ( uint32_t )bundle.dynamic_overlay.entries.size();
    SVG_Nav2_Persistence_AppendBytes( outBlob, &bundleMeta, sizeof( bundleMeta ) );
    for ( const nav2_region_layer_t &layer : bundle.region_layers.layers ) {
        SVG_Nav2_Persistence_AppendRegionLayerRecord( layer, outBlob );
    }
    SVG_Nav2_Persistence_EndSection( outBlob, &sections[ 0 ] );

   sections[ 1 ] = SVG_Nav2_Persistence_BeginSection( outBlob, nav2_serialized_section_type_t::HierarchyGraph, 1 );
    SVG_Nav2_Persistence_AppendBytes( outBlob, &bundleMeta.hierarchy_node_count, sizeof( bundleMeta.hierarchy_node_count ) );
    for ( const nav2_hierarchy_node_t &node : bundle.hierarchy_graph.nodes ) {
        SVG_Nav2_Persistence_AppendHierarchyNodeRecord( node, outBlob );
    }
    SVG_Nav2_Persistence_AppendBytes( outBlob, &bundleMeta.hierarchy_edge_count, sizeof( bundleMeta.hierarchy_edge_count ) );
    for ( const nav2_hierarchy_edge_t &edge : bundle.hierarchy_graph.edges ) {
        SVG_Nav2_Persistence_AppendHierarchyEdgeRecord( edge, outBlob );
    }
    SVG_Nav2_Persistence_EndSection( outBlob, &sections[ 1 ] );

  sections[ 2 ] = SVG_Nav2_Persistence_BeginSection( outBlob, nav2_serialized_section_type_t::MoverLocalModels, 1 );
    SVG_Nav2_Persistence_AppendBytes( outBlob, &bundleMeta.mover_model_count, sizeof( bundleMeta.mover_model_count ) );
    for ( const nav2_mover_local_model_t &model : bundle.mover_local_models ) {
        SVG_Nav2_Persistence_AppendMoverModelRecord( model, outBlob );
    }
 SVG_Nav2_Persistence_EndSection( outBlob, &sections[ 2 ] );

 sections[ 3 ] = SVG_Nav2_Persistence_BeginSection( outBlob, nav2_serialized_section_type_t::DynamicOverlay, 1 );
    SVG_Nav2_Persistence_AppendBytes( outBlob, &bundleMeta.occupancy_count, sizeof( bundleMeta.occupancy_count ) );
    SVG_Nav2_Persistence_AppendBytes( outBlob, &bundleMeta.overlay_count, sizeof( bundleMeta.overlay_count ) );
    for ( const nav2_occupancy_record_t &record : bundle.occupancy.records ) {
        SVG_Nav2_Persistence_AppendOccupancyRecord( record, outBlob );
    }
    for ( const nav2_occupancy_overlay_entry_t &entry : bundle.dynamic_overlay.entries ) {
        SVG_Nav2_Persistence_AppendOverlayEntry( entry, outBlob );
    }
  SVG_Nav2_Persistence_EndSection( outBlob, &sections[ 3 ] );

    // Patch the finalized header and section descriptors into the reserved space at the front of the blob.
    if ( !SVG_Nav2_Persistence_OverwriteBytes( outBlob, headerOffset, &header, sizeof( header ) )
     || !SVG_Nav2_Persistence_OverwriteBytes( outBlob, sectionTableOffset, sections, sizeof( sections ) ) ) {
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
*\t@brief\tDecode a versioned nav2 persistence blob back into a nav2 bundle.
*\t@param\tblob\tSerialized blob to decode.
*\t@param\tpolicy\tExpected serialization policy.
*\t@param\toutBundle\t[out] Bundle receiving decoded state.
*\t@return\tStructured load result including validation state.
**/
nav2_persistence_result_t SVG_Nav2_Persistence_ReadBundleBlob( const nav2_serialized_blob_t &blob,
    const nav2_serialization_policy_t &policy, nav2_persistence_bundle_t *outBundle ) {
    // Reject requests that do not provide output storage for the decoded bundle.
    nav2_persistence_result_t result = {};
    if ( !outBundle ) {
        return result;
    }

    // Validate the payload header and section table before touching any runtime bundle state.
    const size_t blobSize = blob.bytes.size();
    size_t offset = 0;
    nav2_serialized_header_t header = {};
    if ( !SVG_Nav2_Persistence_ReadValue( blob.bytes.data(), blobSize, &offset, &header ) ) {
        return result;
    }
    result.validation = SVG_Nav2_Serialize_ValidateHeader( header, policy, NAV_SERIALIZED_FILE_MAGIC );
    if ( result.validation.compatibility == nav2_serialized_compatibility_t::Reject ) {
        return result;
    }

    // Read the fixed section table before attempting to decode any section payloads.
    std::vector<nav2_serialized_section_desc_t> sections = {};
    sections.reserve( header.section_count );
    for ( uint32_t i = 0; i < header.section_count; i++ ) {
        nav2_serialized_section_desc_t section = {};
        if ( !SVG_Nav2_Persistence_ReadValue( blob.bytes.data(), blobSize, &offset, &section ) ) {
            return result;
        }
        sections.push_back( section );
    }

    // Resolve each mandatory section by its stable identifier and decode it in deterministic order.
    const nav2_serialized_section_desc_t *regionLayerSection = SVG_Nav2_Persistence_FindSection( sections, nav2_serialized_section_type_t::RegionLayers );
    const nav2_serialized_section_desc_t *hierarchySection = SVG_Nav2_Persistence_FindSection( sections, nav2_serialized_section_type_t::HierarchyGraph );
    const nav2_serialized_section_desc_t *moverModelSection = SVG_Nav2_Persistence_FindSection( sections, nav2_serialized_section_type_t::MoverLocalModels );
    const nav2_serialized_section_desc_t *overlaySection = SVG_Nav2_Persistence_FindSection( sections, nav2_serialized_section_type_t::DynamicOverlay );
    if ( !regionLayerSection || !hierarchySection || !moverModelSection || !overlaySection ) {
        return result;
    }

    // Decode the region-layer section from the section payload table.
    const uint8_t *sectionData = blob.bytes.data();
    size_t regionOffset = regionLayerSection->offset;
    if ( !SVG_Nav2_Persistence_ReadRegionLayers( outBundle, sectionData, blobSize, &regionOffset ) ) {
        return result;
    }

    // Decode the hierarchy section from the section payload table.
    size_t hierarchyOffset = hierarchySection->offset;
    if ( !SVG_Nav2_Persistence_ReadHierarchy( outBundle, sectionData, blobSize, &hierarchyOffset ) ) {
        return result;
    }

    // Decode the mover-local model section from the section payload table.
    size_t moverOffset = moverModelSection->offset;
    if ( !SVG_Nav2_Persistence_ReadMoverModels( outBundle, sectionData, blobSize, &moverOffset ) ) {
        return result;
    }

    // Decode the occupancy and overlay section from the section payload table.
    size_t overlayOffset = overlaySection->offset;
    if ( !SVG_Nav2_Persistence_ReadOccupancyOverlay( outBundle, sectionData, blobSize, &overlayOffset ) ) {
        return result;
    }

    // Publish the decoded bundle only after every required section has been validated successfully.
    result.success = true;
    result.byte_count = ( uint32_t )blob.bytes.size();
    return result;
}

/**
*\t@brief\tEmit a bounded debug summary for the nav2 persistence bundle.
*\t@param\tbundle\tBundle to report.
*\t@param\tlimit\tMaximum number of entries to print.
**/
void SVG_Nav2_DebugPrintPersistenceBundle( const nav2_persistence_bundle_t &bundle, const int32_t limit ) {
    // Avoid noisy diagnostics when no useful output was requested.
    if ( limit <= 0 ) {
        return;
    }

    // Print a concise aggregate summary first.
    gi.dprintf( "[NAV2][Persistence] regionLayers=%d hierarchyNodes=%d hierarchyEdges=%d moverModels=%d occupancy=%d overlays=%d limit=%d\n",
        ( int32_t )bundle.region_layers.layers.size(),
        ( int32_t )bundle.hierarchy_graph.nodes.size(),
        ( int32_t )bundle.hierarchy_graph.edges.size(),
        ( int32_t )bundle.mover_local_models.size(),
        ( int32_t )bundle.occupancy.records.size(),
        ( int32_t )bundle.dynamic_overlay.entries.size(),
        limit );

    const int32_t reportRegionCount = std::min( ( int32_t )bundle.region_layers.layers.size(), limit );
    for ( int32_t i = 0; i < reportRegionCount; i++ ) {
        const nav2_region_layer_t &layer = bundle.region_layers.layers[ ( size_t )i ];
        gi.dprintf( "[NAV2][Persistence][Region] id=%d kind=%d leaf=%d cluster=%d area=%d portal=%d mover=%d flags=0x%08x z=(%.1f..%.1f) pref=%.1f\n",
            layer.region_layer_id,
            ( int32_t )layer.kind,
            layer.leaf_id,
            layer.cluster_id,
            layer.area_id,
            layer.topology.portal_id,
            layer.mover_entnum,
            layer.flags,
            layer.allowed_z_band.min_z,
            layer.allowed_z_band.max_z,
            layer.allowed_z_band.preferred_z );
    }

    const int32_t reportMoverCount = std::min( ( int32_t )bundle.mover_local_models.size(), limit );
    for ( int32_t i = 0; i < reportMoverCount; i++ ) {
        const nav2_mover_local_model_t &model = bundle.mover_local_models[ ( size_t )i ];
        gi.dprintf( "[NAV2][Persistence][Mover] ent=%d inline=%d roles=0x%08x headnode=%d reusable=%d\n",
            model.owner_entnum,
            model.inline_model_index,
            model.role_flags,
            model.headnode_valid ? 1 : 0,
            model.reusable_local_space ? 1 : 0 );
    }
}

/**
*\t@brief\tKeep the nav2 persistence module represented in the build.
**/
void SVG_Nav2_Persistence_ModuleAnchor( void ) {
}
