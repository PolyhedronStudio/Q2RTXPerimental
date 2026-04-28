/********************************************************************
*
*
*	ServerGame: Nav2 Connector Extraction
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_entity_semantics.h"
#include "svgame/nav2/nav2_mover_local_nav.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <unordered_map>
#include <vector>


/**
*
*
*	Nav2 Connector Enumerations:
*
*
**/
/**
* @brief Explicit connector kinds used by the coarse hierarchy and corridor layer.
* @note The kinds intentionally separate mover boarding from ride and exit semantics so
*  connector extraction can stay precise and persistence-friendly.
**/
enum nav2_connector_kind_t : uint32_t {
    //! No connector kind assigned yet.
    NAV2_CONNECTOR_KIND_NONE = 0,
    //! Generic portal or opening transition.
    NAV2_CONNECTOR_KIND_PORTAL = ( 1u << 0 ),
    //! Stair band transition between adjacent vertical bands.
    NAV2_CONNECTOR_KIND_STAIR_BAND = ( 1u << 1 ),
    //! Ladder startpoint or endpoint transition.
    NAV2_CONNECTOR_KIND_LADDER_ENDPOINT = ( 1u << 2 ),
    //! Door or timed passage transition.
    NAV2_CONNECTOR_KIND_DOOR_TRANSITION = ( 1u << 3 ),
    //! Mover boarding transition.
    NAV2_CONNECTOR_KIND_MOVER_BOARDING = ( 1u << 4 ),
    //! Mover ride-state transition.
    NAV2_CONNECTOR_KIND_MOVER_RIDE = ( 1u << 5 ),
    //! Mover exit transition.
    NAV2_CONNECTOR_KIND_MOVER_EXIT = ( 1u << 6 ),
    //! Explicit opening transition used for brushes that behave like openings.
    NAV2_CONNECTOR_KIND_OPENING = ( 1u << 7 )
};

/**
 * @brief Explicit endpoint semantics attached to connector anchors.
 * @note These flags let topology, hierarchy, and corridor code distinguish ladder bottoms from tops,
 *  mover boarding anchors from exit anchors, and generic portal barriers from free-form openings.
 **/
enum nav2_connector_endpoint_semantic_bits_t : uint32_t {
    NAV2_CONNECTOR_ENDPOINT_NONE = 0,
    NAV2_CONNECTOR_ENDPOINT_LADDER_BOTTOM = ( 1u << 0 ),
    NAV2_CONNECTOR_ENDPOINT_LADDER_TOP = ( 1u << 1 ),
    NAV2_CONNECTOR_ENDPOINT_STAIR_ENTRY = ( 1u << 2 ),
    NAV2_CONNECTOR_ENDPOINT_STAIR_EXIT = ( 1u << 3 ),
    NAV2_CONNECTOR_ENDPOINT_PORTAL_BARRIER = ( 1u << 4 ),
    NAV2_CONNECTOR_ENDPOINT_MOVER_BOARDING = ( 1u << 5 ),
    NAV2_CONNECTOR_ENDPOINT_MOVER_RIDE = ( 1u << 6 ),
    NAV2_CONNECTOR_ENDPOINT_MOVER_EXIT = ( 1u << 7 )
};

/**
 * @brief Explicit transition semantics attached to one connector record.
 **/
enum nav2_connector_transition_semantic_bits_t : uint32_t {
    NAV2_CONNECTOR_TRANSITION_NONE = 0,
    NAV2_CONNECTOR_TRANSITION_VERTICAL = ( 1u << 0 ),
    NAV2_CONNECTOR_TRANSITION_LADDER = ( 1u << 1 ),
    NAV2_CONNECTOR_TRANSITION_STAIR = ( 1u << 2 ),
    NAV2_CONNECTOR_TRANSITION_PORTAL = ( 1u << 3 ),
    NAV2_CONNECTOR_TRANSITION_OPENING = ( 1u << 4 ),
    NAV2_CONNECTOR_TRANSITION_MOVER = ( 1u << 5 )
};


/**
*
*
*	Nav2 Connector Data Structures:
*
*
**/
/**
* @brief Pointer-free attachment record for one connector endpoint.
* @note The anchor stores both world-space and topology-local metadata so connectors remain
*  usable for hierarchy publication, corridor extraction, and save/load references.
**/
struct nav2_connector_anchor_t {
    //! Stable span reference when the anchor is tied to a precision-layer span.
    nav2_span_ref_t span_ref = {};
    //! Owning BSP leaf when known.
    int32_t leaf_id = -1;
    //! Owning BSP cluster when known.
    int32_t cluster_id = -1;
    //! Owning BSP area when known.
    int32_t area_id = -1;
    //! World-space anchor position.
    Vector3 world_origin = {};
    //! Mover-local anchor position when the connector is tied to a traversal-capable brush entity.
    Vector3 local_origin = {};
    //! Explicit endpoint semantics for this anchor.
    uint32_t endpoint_semantics = NAV2_CONNECTOR_ENDPOINT_NONE;
    //! True when the anchor has a usable point and topology metadata.
    bool valid = false;
};

/**
* @brief Stable, pointer-free connector record.
* @note Connector ids are kept explicit so downstream hierarchy and serialization layers can
*  reference them without depending on container addresses.
**/
struct nav2_connector_t {
    //! Stable connector id.
    int32_t connector_id = -1;
    //! Connector kind bitmask.
    uint32_t connector_kind = NAV2_CONNECTOR_KIND_NONE;
    //! Owning entity number when the connector is derived from an inline BSP entity.
    int32_t owner_entnum = -1;
    //! Inline-model index when the connector is derived from a mover-local brush entity.
    int32_t inline_model_index = -1;
    //! Primary connector endpoint.
    nav2_connector_anchor_t start = {};
    //! Secondary connector endpoint.
    nav2_connector_anchor_t end = {};
    //! Allowed minimum Z for traversal through this connector.
    double allowed_min_z = 0.0;
    //! Allowed maximum Z for traversal through this connector.
    double allowed_max_z = 0.0;
    //! Base traversal cost assigned by extraction heuristics.
    double base_cost = 0.0;
    //! Additional policy penalty that can be layered on by the hierarchy.
    double policy_penalty = 0.0;
    //! Movement restriction bits copied from entity or span semantics.
    uint32_t movement_restrictions = 0;
    //! Source role flags copied from inline BSP entity classification.
    uint32_t source_role_flags = NAV2_INLINE_BSP_ROLE_NONE;
    //! Explicit transition semantics derived during connector extraction.
    uint32_t transition_semantics = NAV2_CONNECTOR_TRANSITION_NONE;
    //! True when the connector can be reused as a stable topology element.
    bool reusable = false;
    //! True when this connector is currently available for routing.
    bool dynamically_available = true;
    //! Version stamp for dynamic availability overlays.
    uint32_t availability_version = 0;
};

/**
* @brief Pointer-free connector reverse index.
* @note Used to keep connector ids stable while allowing vector storage.
**/
struct nav2_connector_ref_t {
    //! Stable connector id.
    int32_t connector_id = -1;
    //! Owning list index.
    int32_t connector_index = -1;

    /**
    * @brief Return whether the connector reference points to a concrete connector slot.
    * @return True when both id and index are valid.
    **/
    const bool IsValid() const {
        return connector_id >= 0 && connector_index >= 0;
    }
};

/**
* @brief Stable connector collection used by hierarchy and corridor construction.
**/
struct nav2_connector_list_t {
    //! All extracted connectors in deterministic order.
    std::vector<nav2_connector_t> connectors = {};
    //! Stable id lookup for direct resolution.
    std::unordered_map<int32_t, nav2_connector_ref_t> by_id = {};
};

/**
* @brief Compact extraction summary used by Task 6.3 runtime validation and bounded diagnostics.
* @note The summary keeps connector-type coverage explicit without requiring per-connector log spam.
**/
struct nav2_connector_summary_t {
    //! Total connectors in the extracted list.
    int32_t total_count = 0;
    //! Portal/opening transition connectors.
    int32_t portal_count = 0;
    //! Stair-band transition connectors.
    int32_t stair_count = 0;
    //! Ladder endpoint connectors.
    int32_t ladder_count = 0;
    //! Door/timed transition connectors.
    int32_t door_count = 0;
    //! Mover boarding connectors.
    int32_t mover_boarding_count = 0;
    //! Mover ride connectors.
    int32_t mover_ride_count = 0;
    //! Mover exit connectors.
    int32_t mover_exit_count = 0;
    //! Inline-entity-derived connector count.
    int32_t entity_connector_count = 0;
    //! Span-derived connector count.
    int32_t span_connector_count = 0;
    //! Connectors currently unavailable due to area-portal or entity state checks.
    int32_t unavailable_count = 0;
    //! Connectors tagged as reusable topology anchors.
    int32_t reusable_count = 0;
};


/**
*
*
*	Nav2 Connector Public API:
*
*
**/
/**
* @brief Clear a connector list to an empty state.
* @param list Connector collection to reset.
**/
void SVG_Nav2_ConnectorList_Clear( nav2_connector_list_t *list );

/**
* @brief Append a connector to a connector list and register its stable id.
* @param list Connector collection to mutate.
* @param connector Connector payload to append.
* @return True when the connector was appended.
**/
const bool SVG_Nav2_ConnectorList_Append( nav2_connector_list_t *list, const nav2_connector_t &connector );

/**
* @brief Resolve a connector by stable id.
* @param list Connector collection to inspect.
* @param connector_id Stable connector id to resolve.
* @param out_ref [out] Resolved connector reference.
* @return True when the connector exists.
**/
const bool SVG_Nav2_ConnectorList_TryResolve( const nav2_connector_list_t &list, const int32_t connector_id, nav2_connector_ref_t *out_ref );

/**
* @brief Build a single connector candidate from one inline BSP entity.
* @param ent Entity being inspected.
* @param entityInfo Navigation classification for the entity.
* @param in_transform Optional mover-local transform when available.
* @param out_connector [out] Connector payload receiving the extracted state.
* @return True when the entity produced a connector candidate.
**/
const bool SVG_Nav2_BuildConnectorFromEntity( const svg_base_edict_t *ent, const nav2_inline_bsp_entity_info_t &entityInfo, const nav2_mover_transform_t *in_transform, nav2_connector_t *out_connector );

/**
* @brief Extract connector candidates from sparse span-grid topology.
* @param grid Sparse precision grid to inspect.
* @param out_list [out] Connector list receiving extracted connectors.
* @return True when at least one span-derived connector was produced.
**/
const bool SVG_Nav2_ExtractSpanConnectors( const nav2_span_grid_t &grid, nav2_connector_list_t *out_list );

/**
* @brief Extract connector candidates from inline BSP entities.
* @param entities Entity set to inspect.
* @param out_list [out] Connector list receiving extracted connectors.
* @return True when at least one entity-derived connector was produced.
**/
const bool SVG_Nav2_ExtractEntityConnectors( const std::vector<svg_base_edict_t *> &entities, nav2_connector_list_t *out_list );

/**
* @brief Extract connector candidates from both sparse spans and inline BSP entities.
* @param grid Sparse precision grid to inspect.
* @param entities Entity set to inspect.
* @param out_list [out] Connector list receiving extracted connectors.
* @return True when at least one connector was produced.
**/
const bool SVG_Nav2_ExtractConnectors( const nav2_span_grid_t &grid, const std::vector<svg_base_edict_t *> &entities, nav2_connector_list_t *out_list );

/**
* @brief Build a bounded connector-type summary for diagnostics and runtime validation.
* @param list Connector list to summarize.
* @param out_summary [out] Compact summary counters.
* @return True when the summary was produced.
**/
const bool SVG_Nav2_BuildConnectorSummary( const nav2_connector_list_t &list, nav2_connector_summary_t *out_summary );

/**
* @brief Emit a bounded debug summary for connector extraction results.
* @param list Connector list to report.
* @param limit Maximum number of connectors to print.
**/
void SVG_Nav2_DebugPrintConnectors( const nav2_connector_list_t &list, const int32_t limit = 16 );

/**
* @brief Keep the nav2 connector module represented in the build.
**/
void SVG_Nav2_Connectors_ModuleAnchor( void );
