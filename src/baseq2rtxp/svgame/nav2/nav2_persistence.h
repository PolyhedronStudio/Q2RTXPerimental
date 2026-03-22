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
* @brief High-level bundle of nav2-owned state that can be serialized together.
* @note This stays pointer-free so the save/cache foundation can round-trip region layers,
*  hierarchy, mover-local models, and sparse dynamic overlay state in one versioned blob.
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
* @brief Structured summary returned when a persistence blob is built or decoded.
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
* @brief Build a versioned nav2 persistence blob containing the supplied bundle.
* @param bundle Nav2 bundle to serialize.
* @param outBlob [out] Blob receiving the serialized bytes.
* @return Structured build result including byte count.
**/
nav2_persistence_result_t SVG_Nav2_Persistence_BuildBundleBlob( const nav2_persistence_bundle_t &bundle, nav2_serialized_blob_t *outBlob );

/**
* @brief Decode a versioned nav2 persistence blob back into a nav2 bundle.
* @param blob Serialized blob to decode.
* @param policy Expected serialization policy.
* @param outBundle [out] Bundle receiving decoded state.
* @return Structured load result including validation state.
**/
nav2_persistence_result_t SVG_Nav2_Persistence_ReadBundleBlob( const nav2_serialized_blob_t &blob,
    const nav2_serialization_policy_t &policy, nav2_persistence_bundle_t *outBundle );

/**
* @brief Emit a bounded debug summary for the nav2 persistence bundle.
* @param bundle Bundle to report.
* @param limit Maximum number of entries to print.
**/
void SVG_Nav2_DebugPrintPersistenceBundle( const nav2_persistence_bundle_t &bundle, const int32_t limit = 16 );

/**
* @brief Keep the nav2 persistence module represented in the build.
**/
void SVG_Nav2_Persistence_ModuleAnchor( void );
