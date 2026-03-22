/********************************************************************
*
*
*	ServerGame: Nav2 Persistence Foundation
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_corridor.h"
#include "svgame/nav2/nav2_hierarchy_graph.h"
#include "svgame/nav2/nav2_mover_local_nav.h"
#include "svgame/nav2/nav2_occupancy.h"
#include "svgame/nav2/nav2_region_layers.h"
#include "svgame/nav2/nav2_serialize.h"

#include <vector>


/**
*
*
*	Nav2 Persistence Data Structures:
*
*
**/
/**
*\t@brief\tHigh-level bundle of nav2-owned state that can be serialized together.
*\t@note\tThis stays pointer-free so the save/cache foundation can round-trip region layers,
*\t\thierarchy, mover-local models, and sparse dynamic overlay state in one versioned blob.
**/
struct nav2_persistence_bundle_t {
    //! Policy used to validate the encoded blob.
    nav2_serialization_policy_t policy = {};
    //! Region-layer graph snapshot.
    nav2_region_layer_graph_t region_layers = {};
    //! Coarse hierarchy graph snapshot.
    nav2_hierarchy_graph_t hierarchy_graph = {};
    //! Mover-local nav model snapshots.
    std::vector<nav2_mover_local_model_t> mover_local_models = {};
    //! Sparse occupancy snapshot.
    nav2_occupancy_grid_t occupancy = {};
    //! Dynamic overlay snapshot.
    nav2_dynamic_overlay_t dynamic_overlay = {};
};

/**
*\t@brief\tStructured summary returned when a persistence blob is built or decoded.
**/
struct nav2_persistence_result_t {
    //! True when the operation completed successfully.
    bool success = false;
    //! Structured compatibility result for read-side operations.
    nav2_serialized_validation_t validation = {};
    //! Number of bytes written to or loaded from the blob.
    uint32_t byte_count = 0;
};


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
nav2_persistence_result_t SVG_Nav2_Persistence_BuildBundleBlob( const nav2_persistence_bundle_t &bundle, nav2_serialized_blob_t *outBlob );

/**
*\t@brief\tDecode a versioned nav2 persistence blob back into a nav2 bundle.
*\t@param\tblob\tSerialized blob to decode.
*\t@param\tpolicy\tExpected serialization policy.
*\t@param\toutBundle\t[out] Bundle receiving decoded state.
*\t@return\tStructured load result including validation state.
**/
nav2_persistence_result_t SVG_Nav2_Persistence_ReadBundleBlob( const nav2_serialized_blob_t &blob,
    const nav2_serialization_policy_t &policy, nav2_persistence_bundle_t *outBundle );

/**
*\t@brief\tEmit a bounded debug summary for the nav2 persistence bundle.
*\t@param\tbundle\tBundle to report.
*\t@param\tlimit\tMaximum number of entries to print.
**/
void SVG_Nav2_DebugPrintPersistenceBundle( const nav2_persistence_bundle_t &bundle, const int32_t limit = 16 );

/**
*\t@brief\tKeep the nav2 persistence module represented in the build.
**/
void SVG_Nav2_Persistence_ModuleAnchor( void );
