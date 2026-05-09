/********************************************************************
*
*
*    ServerGame: Nav3 Stage-5 Static BSP Span Generation - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_generate.h"
#include "svgame/nav3/nav3_debug_draw.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <limits>
#include <unordered_map>

#include "common/bsp.h"
#include "shared/cm/cm_model.h"


/**
*
*
*    Nav3 Stage-5 Generation Constants:
*
*
**/
//! Maximum probe iterations performed for one sampled XY column.
static constexpr int32_t NAV3_GENERATE_MAX_PROBES_PER_COLUMN = 64;
//! Maximum spans emitted for one sampled XY column.
static constexpr int32_t NAV3_GENERATE_MAX_SPANS_PER_COLUMN = 16;
//! World-space top padding used when starting per-column downward probes.
static constexpr float NAV3_GENERATE_PROBE_TOP_PADDING = 24.0f;
//! Extra world-space depth below map mins used by support-floor probes.
static constexpr float NAV3_GENERATE_PROBE_BOTTOM_PADDING = 64.0f;
//! Minimum downward progress applied between probes to avoid infinite loops.
static constexpr float NAV3_GENERATE_MIN_Z_STEP = 4.0f;
//! Epsilon used to offset traces away from exact plane boundaries.
static constexpr float NAV3_GENERATE_TRACE_EPSILON = 1.0f;
//! Minimum floor delta treated as a stair transition candidate.
static constexpr float NAV3_GENERATE_STAIR_MIN_DELTA = 2.0f;
//! Maximum debug-draw columns emitted from one stage-5 draw call.
static constexpr int32_t NAV3_GENERATE_DEBUG_DRAW_MAX_COLUMNS = 512;
//! Maximum filter-reject samples drawn in one bounded debug-draw queue call.
static constexpr int32_t NAV3_GENERATE_DEBUG_DRAW_MAX_FILTER_SAMPLES = 128;


/**
*
*
*    Nav3 Stage-5 Local Types:
*
*
**/
/**
*    @brief  Probe outcome used by per-column multi-floor sampling.
**/
struct nav3_generate_probe_result_t {
    //! True when one valid span was generated for the probe.
    bool span_found = false;
    //! True when no deeper probe should be attempted for this column.
    bool stop_column = false;
    //! Next Z position to probe from.
    float next_probe_start_z = 0.0f;
};

/**
*    @brief  One reusable sparse-column lookup map for neighbor-based classification/filter passes.
**/
using nav3_generate_column_lookup_t = std::unordered_map<uint64_t, int32_t>;


/**
*
*
*    Nav3 Stage-5 Local Helpers:
*
*
**/
/**
*    @brief  Resolve active BSP world bounds from the collision-model cache.
*    @param  outWorldBounds  [out] Active world bounds.
*    @return True when valid world bounds were resolved.
**/
static const bool Nav3_Generate_ResolveWorldBounds( BBox3 *outWorldBounds ) {
    /**
    *    Require output storage and collision-model imports before resolving map bounds.
    **/
    if ( !outWorldBounds || !gi.GetCollisionModel ) {
        return false;
    }

    /**
    *    Require an active collision model with at least one BSP model.
    **/
    const cm_t *collisionModel = gi.GetCollisionModel();
    if ( !collisionModel || !collisionModel->cache || !collisionModel->cache->models || collisionModel->cache->nummodels <= 0 ) {
        return false;
    }

    /**
    *    Use BSP world-model bounds as authoritative generation bounds.
    **/
    const mmodel_t &worldModel = collisionModel->cache->models[ 0 ];
    if ( worldModel.maxs[ 0 ] <= worldModel.mins[ 0 ]
        || worldModel.maxs[ 1 ] <= worldModel.mins[ 1 ]
        || worldModel.maxs[ 2 ] <= worldModel.mins[ 2 ] ) {
        return false;
    }

    outWorldBounds->mins = { worldModel.mins[ 0 ], worldModel.mins[ 1 ], worldModel.mins[ 2 ] };
    outWorldBounds->maxs = { worldModel.maxs[ 0 ], worldModel.maxs[ 1 ], worldModel.maxs[ 2 ] };
    return true;
}

/**
*    @brief  Resolve first centered cell index for one axis.
*    @param  worldMin  Axis minimum in world units.
*    @param  cellSize  Axis cell size in world units.
*    @return First sampled cell index for centered-cell sampling.
**/
static inline int32_t Nav3_Generate_GetMinCenteredCellIndex( const double worldMin, const double cellSize ) {
    if ( cellSize <= 0.0 ) {
        return 0;
    }

    return ( int32_t )std::ceil( ( worldMin / cellSize ) - 0.5 );
}

/**
*    @brief  Resolve one-past-end centered cell index for one axis.
*    @param  worldMax  Axis maximum in world units.
*    @param  cellSize  Axis cell size in world units.
*    @return Exclusive max sampled cell index for centered-cell sampling.
**/
static inline int32_t Nav3_Generate_GetMaxCenteredCellIndexExclusive( const double worldMax, const double cellSize ) {
    if ( cellSize <= 0.0 ) {
        return 0;
    }

    return ( int32_t )std::floor( ( worldMax / cellSize ) - 0.5 ) + 1;
}

/**
*    @brief  Resolve feet-to-origin lift for center-origin hulls.
*    @param  agentMins  Agent mins used for span validation traces.
*    @return Positive Z offset when the hull extends below the origin.
**/
static inline float Nav3_Generate_GetFeetToOriginOffsetZ( const Vector3 &agentMins ) {
    return agentMins.z < 0.0f ? -agentMins.z : 0.0f;
}

/**
*    @brief  Build a stable 64-bit key for one sparse column coordinate.
*    @param  cellX  Sparse column X coordinate.
*    @param  cellY  Sparse column Y coordinate.
*    @return Stable packed key for unordered lookup maps.
**/
static inline uint64_t Nav3_Generate_MakeCellKey( const int32_t cellX, const int32_t cellY ) {
    return ( static_cast<uint64_t>( static_cast<uint32_t>( cellX ) ) << 32 )
        | static_cast<uint64_t>( static_cast<uint32_t>( cellY ) );
}

/**
*    @brief  Build sparse-column lookup map for XY-neighbor queries.
*    @param  mesh  Mesh containing sparse generated columns.
*    @return Built lookup map keyed by packed XY coordinate.
**/
static nav3_generate_column_lookup_t Nav3_Generate_BuildColumnLookup( const nav3_generated_mesh_t &mesh ) {
    nav3_generate_column_lookup_t lookup = {};
    lookup.reserve( mesh.columns.size() );
    for ( int32_t columnIndex = 0; columnIndex < ( int32_t )mesh.columns.size(); columnIndex++ ) {
        const nav3_generated_column_t &column = mesh.columns[ columnIndex ];
        lookup[ Nav3_Generate_MakeCellKey( column.cell_x, column.cell_y ) ] = columnIndex;
    }
    return lookup;
}

/**
*    @brief  Reset per-pass stage-6 filter counters and seed pass IDs.
*    @param  inOutStats  [in,out] Stats storage to initialize.
**/
static void Nav3_Generate_ResetFilterStats( nav3_generation_stats_t *inOutStats ) {
    if ( !inOutStats ) {
        return;
    }

    inOutStats->filter_input_spans = 0;
    inOutStats->filter_output_spans = 0;
    inOutStats->filter_erosion_radius_cells = 0;
    inOutStats->filter_erosion_blocked_missing_columns = 0;
    inOutStats->filter_erosion_blocked_neighbor_columns = 0;
    inOutStats->filter_reject_sample_count = 0;
    inOutStats->filter_reject_sample_dropped = 0;
    inOutStats->filter_elapsed_ms = 0.0;
    for ( int32_t i = 0; i < static_cast<int32_t>( nav3_generation_filter_pass_t::Count ); i++ ) {
        inOutStats->filter_pass_stats[ i ] = {};
        inOutStats->filter_pass_stats[ i ].pass = static_cast<nav3_generation_filter_pass_t>( i );
    }
}

/**
*    @brief  Record one rejected span sample for bounded filter debug-draw diagnostics.
*    @param  pass        Rejecting filter pass.
*    @param  span        Rejected span payload.
*    @param  inOutStats  [in,out] Stats receiving bounded reject sample data.
**/
static void Nav3_Generate_RecordFilterRejectSample( const nav3_generation_filter_pass_t pass,
    const nav3_generated_span_t &span,
    nav3_generation_stats_t *inOutStats ) {
    if ( !inOutStats ) {
        return;
    }

    if ( inOutStats->filter_reject_sample_count >= NAV3_GENERATION_MAX_FILTER_REJECT_SAMPLES ) {
        inOutStats->filter_reject_sample_dropped++;
        return;
    }

    nav3_generation_filter_reject_sample_t &sample =
        inOutStats->filter_reject_samples[ inOutStats->filter_reject_sample_count ];
    sample.pass = pass;
    sample.sample_origin = span.source_standing_coord;
    sample.floor_z = span.floor_z;
    inOutStats->filter_reject_sample_count++;
}

/**
*    @brief  Mark one span as rejected by the current filter pass and update diagnostics.
*    @param  pass        Rejecting filter pass.
*    @param  span        [in,out] Span payload receiving reject metadata.
*    @param  inOutStats  [in,out] Stats receiving pass/reject counters.
**/
static void Nav3_Generate_RejectSpanByPass( const nav3_generation_filter_pass_t pass,
    nav3_generated_span_t *span,
    nav3_generation_stats_t *inOutStats ) {
    if ( !span || !inOutStats ) {
        return;
    }

    const int32_t passIndex = static_cast<int32_t>( pass );
    if ( passIndex <= static_cast<int32_t>( nav3_generation_filter_pass_t::None )
        || passIndex >= static_cast<int32_t>( nav3_generation_filter_pass_t::Count ) ) {
        return;
    }

    if ( span->filtered_out ) {
        return;
    }

    span->filtered_out = true;
    span->filtered_pass = pass;
    inOutStats->filter_pass_stats[ passIndex ].rejected_count++;
    Nav3_Generate_RecordFilterRejectSample( pass, *span, inOutStats );
}

/**
*    @brief  Record one accepted span for the current filter pass.
*    @param  pass        Filter pass that accepted this span.
*    @param  inOutStats  [in,out] Stats receiving accepted-pass counters.
**/
static void Nav3_Generate_AcceptSpanForPass( const nav3_generation_filter_pass_t pass, nav3_generation_stats_t *inOutStats ) {
    if ( !inOutStats ) {
        return;
    }

    const int32_t passIndex = static_cast<int32_t>( pass );
    if ( passIndex <= static_cast<int32_t>( nav3_generation_filter_pass_t::None )
        || passIndex >= static_cast<int32_t>( nav3_generation_filter_pass_t::Count ) ) {
        return;
    }
    inOutStats->filter_pass_stats[ passIndex ].accepted_count++;
}

/**
*    @brief  Resolve nearest vertical delta to any span in one neighbor column.
*    @param  floorZ       Baseline span floor height.
*    @param  neighborCol  Neighbor sparse column to compare.
*    @param  outDelta     [out] Nearest absolute floor-height delta.
*    @return True when a valid delta was resolved.
**/
static const bool Nav3_Generate_FindNearestNeighborDelta( const float floorZ,
    const nav3_generated_column_t &neighborCol,
    float *outDelta ) {
    if ( !outDelta || neighborCol.spans.empty() ) {
        return false;
    }

    float nearestDelta = std::numeric_limits<float>::max();
    for ( const nav3_generated_span_t &neighborSpan : neighborCol.spans ) {
        if ( neighborSpan.filtered_out ) {
            continue;
        }
        const float dz = std::fabs( neighborSpan.floor_z - floorZ );
        if ( dz < nearestDelta ) {
            nearestDelta = dz;
        }
    }

    if ( nearestDelta == std::numeric_limits<float>::max() ) {
        return false;
    }

    *outDelta = nearestDelta;
    return true;
}

/**
*    @brief  Run ordered stage-6 filter pipeline and compact surviving spans.
*    @param  buildConfig  Runtime build configuration.
*    @param  inOutMesh    [in,out] Generated mesh to filter/compact.
*    @param  inOutStats   [in,out] Generation stats receiving per-pass diagnostics.
**/
static void Nav3_Generate_RunFilterPipeline( const nav3_build_config_t &buildConfig,
    nav3_generated_mesh_t *inOutMesh,
    nav3_generation_stats_t *inOutStats ) {
    /**
    *    Require mesh and stats storage before running stage-6 filters.
    **/
    if ( !inOutMesh || !inOutStats ) {
        return;
    }

    Nav3_Generate_ResetFilterStats( inOutStats );

    /**
    *    Build XY neighbor lookup map shared by drop and erosion passes.
    **/
    const nav3_generate_column_lookup_t columnLookup = Nav3_Generate_BuildColumnLookup( *inOutMesh );
    static constexpr int32_t kNeighborOffsets[ 4 ][ 2 ] = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 }
    };

    const float requiredStandingClearance = std::max( buildConfig.agent_height_units, 16.0f );
    const float lowHangingClearance = requiredStandingClearance + std::max( buildConfig.cell_height, 1.0f );
    const float ledgeDropThreshold = std::max( buildConfig.agent_step_height_units * 2.0f,
        buildConfig.agent_height_units * 0.75f );
    const int32_t erosionRadiusCells = std::max( 0, buildConfig.walkable_radius_cells );
    inOutStats->filter_erosion_radius_cells = erosionRadiusCells;

    /**
    *    Count raw spans entering stage-6 filter pipeline.
    **/
    for ( const nav3_generated_column_t &column : inOutMesh->columns ) {
        inOutStats->filter_input_spans += ( int32_t )column.spans.size();
    }

    /**
    *    Pass 1 - Low ceiling rejection.
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            if ( span.clearance < requiredStandingClearance ) {
                Nav3_Generate_RejectSpanByPass( nav3_generation_filter_pass_t::LowCeiling, &span, inOutStats );
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::LowCeiling, inOutStats );
        }
    }

    /**
    *    Pass 2 - Low-hanging obstruction rejection.
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            if ( span.clearance < lowHangingClearance || ( span.classification_bits & NAV3_SPAN_CLASS_TIGHT_CLEARANCE ) != 0 ) {
                Nav3_Generate_RejectSpanByPass( nav3_generation_filter_pass_t::LowHangingObstruction, &span, inOutStats );
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::LowHangingObstruction, inOutStats );
        }
    }

    /**
    *    Pass 3 - Ledge/drop rejection based on neighboring floor deltas.
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }

            bool rejectByDrop = false;
            if ( ( span.classification_bits & NAV3_SPAN_CLASS_LEDGE ) != 0 ) {
                float nearestDelta = std::numeric_limits<float>::max();
                bool foundNeighbor = false;

                for ( int32_t neighborIndex = 0; neighborIndex < 4; neighborIndex++ ) {
                    const int32_t neighborCellX = column.cell_x + kNeighborOffsets[ neighborIndex ][ 0 ];
                    const int32_t neighborCellY = column.cell_y + kNeighborOffsets[ neighborIndex ][ 1 ];
                    const auto neighborIt = columnLookup.find( Nav3_Generate_MakeCellKey( neighborCellX, neighborCellY ) );
                    if ( neighborIt == columnLookup.end() ) {
                        continue;
                    }

                    float neighborDelta = 0.0f;
                    if ( !Nav3_Generate_FindNearestNeighborDelta( span.floor_z,
                        inOutMesh->columns[ neighborIt->second ],
                        &neighborDelta ) ) {
                        continue;
                    }

                    foundNeighbor = true;
                    nearestDelta = std::min( nearestDelta, neighborDelta );
                }

                if ( !foundNeighbor || nearestDelta > ledgeDropThreshold ) {
                    rejectByDrop = true;
                }
            }

            if ( rejectByDrop ) {
                Nav3_Generate_RejectSpanByPass( nav3_generation_filter_pass_t::LedgeDrop, &span, inOutStats );
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::LedgeDrop, inOutStats );
        }
    }

    /**
    *    Pass 4 - Slope rejection.
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            if ( span.floor_normal_z < PHYS_MAX_SLOPE_NORMAL ) {
                Nav3_Generate_RejectSpanByPass( nav3_generation_filter_pass_t::Slope, &span, inOutStats );
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::Slope, inOutStats );
        }
    }

    /**
    *    Pass 5 - Radius erosion around missing or incompatible neighboring support.
    **/
    std::vector<bool> columnHasPreErosionSupport = {};
    columnHasPreErosionSupport.resize( inOutMesh->columns.size(), false );
    for ( int32_t columnIndex = 0; columnIndex < ( int32_t )inOutMesh->columns.size(); columnIndex++ ) {
        const nav3_generated_column_t &column = inOutMesh->columns[ columnIndex ];
        for ( const nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            columnHasPreErosionSupport[ columnIndex ] = true;
            break;
        }
    }

    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }

            bool rejectByErosion = false;
            if ( erosionRadiusCells > 0 ) {
                for ( int32_t offsetY = -erosionRadiusCells; offsetY <= erosionRadiusCells && !rejectByErosion; offsetY++ ) {
                    for ( int32_t offsetX = -erosionRadiusCells; offsetX <= erosionRadiusCells; offsetX++ ) {
                        const int32_t manhattanDistance = std::abs( offsetX ) + std::abs( offsetY );
                        if ( manhattanDistance == 0 || manhattanDistance > erosionRadiusCells ) {
                            continue;
                        }

                        const int32_t neighborCellX = column.cell_x + offsetX;
                        const int32_t neighborCellY = column.cell_y + offsetY;
                        const auto neighborIt = columnLookup.find( Nav3_Generate_MakeCellKey( neighborCellX, neighborCellY ) );
                        if ( neighborIt == columnLookup.end() ) {
                            rejectByErosion = true;
                            inOutStats->filter_erosion_blocked_missing_columns++;
                            break;
                        }

                        if ( !columnHasPreErosionSupport[ neighborIt->second ] ) {
                            rejectByErosion = true;
                            inOutStats->filter_erosion_blocked_neighbor_columns++;
                            break;
                        }
                    }
                }
            }

            if ( rejectByErosion ) {
                Nav3_Generate_RejectSpanByPass( nav3_generation_filter_pass_t::RadiusErosion, &span, inOutStats );
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::RadiusErosion, inOutStats );
        }
    }

    /**
    *    Pass 6 - Liquid/hazard rejection for baseline standing profile.
    **/
    const uint32_t hazardMask = ( uint32_t )( CONTENTS_LAVA | CONTENTS_SLIME );
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            if ( ( span.contents_flags & hazardMask ) != 0 ) {
                Nav3_Generate_RejectSpanByPass( nav3_generation_filter_pass_t::LiquidHazard, &span, inOutStats );
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::LiquidHazard, inOutStats );
        }
    }

    /**
    *    Pass 7 - Ladder/special surface gate (currently metadata-preserving for baseline profile).
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::LadderSpecialSurface, inOutStats );
        }
    }

    /**
    *    Pass 8 - Capability requirement gate for baseline standing profile.
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            // Baseline profile keeps capability pass metadata-only in stage 6.
            Nav3_Generate_AcceptSpanForPass( nav3_generation_filter_pass_t::CapabilityRequirement, inOutStats );
        }
    }

    /**
    *    Compact surviving spans and columns so runtime publication contains canonical filtered data.
    **/
    std::vector<nav3_generated_column_t> filteredColumns = {};
    filteredColumns.reserve( inOutMesh->columns.size() );

    int32_t filteredSpanCount = 0;
    int32_t maxSpansPerColumn = 0;
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        nav3_generated_column_t filteredColumn = {};
        filteredColumn.cell_x = column.cell_x;
        filteredColumn.cell_y = column.cell_y;
        filteredColumn.spans.reserve( column.spans.size() );

        for ( const nav3_generated_span_t &span : column.spans ) {
            if ( span.filtered_out ) {
                continue;
            }
            filteredColumn.spans.push_back( span );
        }

        if ( filteredColumn.spans.empty() ) {
            continue;
        }

        maxSpansPerColumn = std::max( maxSpansPerColumn, ( int32_t )filteredColumn.spans.size() );
        filteredSpanCount += ( int32_t )filteredColumn.spans.size();
        filteredColumns.push_back( std::move( filteredColumn ) );
    }

    inOutMesh->columns = std::move( filteredColumns );
    inOutStats->filter_output_spans = filteredSpanCount;
    inOutStats->emitted_columns = ( int32_t )inOutMesh->columns.size();
    inOutStats->emitted_spans = filteredSpanCount;
    inOutStats->max_spans_in_column = maxSpansPerColumn;
}

/**
*    @brief  Resolve BSP leaf/cluster/area metadata for one sampled standing point.
*    @param  samplePoint  World-space standing point.
*    @param  outSpan      [in,out] Span metadata receiving topology fields.
**/
static void Nav3_Generate_ResolvePointTopology( const Vector3 &samplePoint, nav3_generated_span_t *outSpan ) {
    /**
    *    Require output storage and BSP imports before topology classification.
    **/
    if ( !outSpan || !gi.GetCollisionModel || !gi.BSP_PointLeaf ) {
        return;
    }

    /**
    *    Require world BSP cache pointers before dereferencing leaf arrays.
    **/
    const cm_t *collisionModel = gi.GetCollisionModel();
    if ( !collisionModel || !collisionModel->cache || !collisionModel->cache->nodes || !collisionModel->cache->leafs ) {
        return;
    }

    /**
    *    Resolve world leaf and mirror area/cluster/leaf ids into span metadata.
    **/
    const vec3_t pointVec = { samplePoint.x, samplePoint.y, samplePoint.z };
    mleaf_t *leaf = gi.BSP_PointLeaf( collisionModel->cache->nodes, pointVec );
    if ( !leaf ) {
        return;
    }

    outSpan->cluster_id = leaf->cluster;
    outSpan->area_id = leaf->area;

    if ( leaf >= collisionModel->cache->leafs && leaf < ( collisionModel->cache->leafs + collisionModel->cache->numleafs ) ) {
        outSpan->leaf_id = ( int32_t )( leaf - collisionModel->cache->leafs );
    }
}

/**
*    @brief  Evaluate one downward probe and optionally emit one valid generated span.
*    @param  buildConfig   Runtime build configuration.
*    @param  worldBounds   Active world bounds.
*    @param  columnCenter  XY column center in world space.
*    @param  probeStartZ   Current probe start Z in world space.
*    @param  spanId        Stable span id to assign when one span is emitted.
*    @param  outSpan       [out] Span payload when `span_found` is true.
*    @param  inOutStats    [in,out] Rejection and classification counters.
*    @return Probe outcome describing success, stop state, and next probe Z.
**/
static nav3_generate_probe_result_t Nav3_Generate_TrySampleSpanAtProbe( const nav3_build_config_t &buildConfig,
    const BBox3 &worldBounds,
    const Vector3 &columnCenter,
    const float probeStartZ,
    const int32_t spanId,
    nav3_generated_span_t *outSpan,
    nav3_generation_stats_t *inOutStats ) {
    nav3_generate_probe_result_t result = {};
    result.next_probe_start_z = probeStartZ - NAV3_GENERATE_MIN_Z_STEP;

    /**
    *    Require output storage and collision imports before running the probe.
    **/
    if ( !outSpan || !gi.trace || !gi.pointcontents || !gi.CM_BoxContents ) {
        result.stop_column = true;
        return result;
    }
    *outSpan = {};

    /**
    *    Resolve baseline standing hull and derived standing-clearance threshold.
    **/
    const Vector3 agentMins = PHYS_DEFAULT_BBOX_STANDUP_MINS;
    const Vector3 agentMaxs = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
    const float feetToOriginOffsetZ = Nav3_Generate_GetFeetToOriginOffsetZ( agentMins );
    const float requiredStandingClearance = std::max( buildConfig.agent_height_units, 16.0f );

    /**
    *    Reject probes that start in solid before attempting floor traces.
    **/
    const Vector3 samplePoint = { columnCenter.x, columnCenter.y, probeStartZ };
    const cm_contents_t probePointContents = gi.pointcontents( &samplePoint );
    if ( ( probePointContents & CONTENTS_SOLID ) != 0 ) {
        if ( inOutStats ) {
            inOutStats->rejected_solid_point++;
        }
        return result;
    }

    /**
    *    Trace downward from the probe point to find the nearest support floor.
    **/
    Vector3 floorProbeEnd = samplePoint;
    floorProbeEnd.z = worldBounds.mins.z - NAV3_GENERATE_PROBE_BOTTOM_PADDING;
    const Vector3 traceMins = { agentMins.x, agentMins.y, 0.0f };
    const Vector3 traceMaxs = { agentMaxs.x, agentMaxs.y, 0.0f };
    const cm_trace_t floorTrace = gi.trace( &samplePoint, &traceMins, &traceMaxs, &floorProbeEnd, nullptr, CM_CONTENTMASK_PLAYERSOLID );

    /**
    *    Reject probes that remained in solid during support-floor traces.
    **/
    if ( floorTrace.startsolid || floorTrace.allsolid ) {
        if ( inOutStats ) {
            inOutStats->rejected_floor_trace++;
        }
        return result;
    }

    /**
    *    Stop scanning this column when no deeper support floor exists.
    **/
    if ( floorTrace.fraction >= 1.0f ) {
        if ( inOutStats ) {
            inOutStats->rejected_floor_miss++;
        }
        result.stop_column = true;
        result.next_probe_start_z = worldBounds.mins.z - NAV3_GENERATE_PROBE_BOTTOM_PADDING;
        return result;
    }

    /**
    *    Reject steep support planes so non-walkable slopes are not emitted as spans.
    **/
    const float floorZ = floorTrace.endpos.z;
    const float minimumSlopeNormal = PHYS_MAX_SLOPE_NORMAL;
    if ( floorTrace.plane.type != PLANE_Z && floorTrace.plane.normal[ 2 ] < minimumSlopeNormal ) {
        if ( inOutStats ) {
            inOutStats->rejected_slope++;
        }
        result.next_probe_start_z = floorZ - NAV3_GENERATE_MIN_Z_STEP;
        return result;
    }

    /**
    *    Probe upward from the detected floor to estimate standing clearance.
    **/
    Vector3 clearanceProbeStart = floorTrace.endpos;
    clearanceProbeStart.z += NAV3_GENERATE_TRACE_EPSILON;
    Vector3 clearanceProbeEnd = clearanceProbeStart;
    clearanceProbeEnd.z += std::max( buildConfig.agent_height_units * 2.0f, 128.0f );
    const cm_trace_t clearanceTrace = gi.trace( &clearanceProbeStart, &traceMins, &traceMaxs, &clearanceProbeEnd, nullptr, CM_CONTENTMASK_PLAYERSOLID );

    const float ceilingZ = clearanceTrace.endpos.z;
    const float clearance = ceilingZ - floorZ;
    if ( clearance < requiredStandingClearance ) {
        if ( inOutStats ) {
            inOutStats->rejected_clearance++;
        }
        result.next_probe_start_z = floorZ - NAV3_GENERATE_MIN_Z_STEP;
        return result;
    }

    /**
    *    Validate final standing occupancy using an in-place hull trace at standing origin.
    **/
    Vector3 standingOrigin = floorTrace.endpos;
    Vector3 standingOriginForBox = standingOrigin;
    standingOriginForBox.z += feetToOriginOffsetZ;

    Vector3 validationMins = agentMins;
    Vector3 validationMaxs = agentMaxs;
    const float hullHeight = validationMaxs.z - validationMins.z;
    if ( hullHeight > ( NAV3_GENERATE_TRACE_EPSILON * 2.0f ) ) {
        validationMins.z += NAV3_GENERATE_TRACE_EPSILON;
        validationMaxs.z -= NAV3_GENERATE_TRACE_EPSILON;
    }

    const cm_trace_t occupancyTrace = gi.trace( &standingOriginForBox, &validationMins, &validationMaxs,
        &standingOriginForBox, nullptr, CM_CONTENTMASK_PLAYERSOLID );
    if ( occupancyTrace.startsolid || occupancyTrace.allsolid ) {
        if ( inOutStats ) {
            inOutStats->rejected_solid_box++;
        }
        result.next_probe_start_z = floorZ - NAV3_GENERATE_MIN_Z_STEP;
        return result;
    }

    /**
    *    Sample broad contents categories from both point and box queries.
    **/
    const cm_contents_t standingContents = gi.pointcontents( &standingOriginForBox );
    cm_contents_t boxContents = CONTENTS_NONE;
    std::array<mleaf_t *, 16> leafList = {};
    mnode_t *topNode = nullptr;

    const Vector3 sampleMins = QM_Vector3Add( standingOriginForBox, validationMins );
    const Vector3 sampleMaxs = QM_Vector3Add( standingOriginForBox, validationMaxs );
    const vec3_t sampleMinsVec = { sampleMins.x, sampleMins.y, sampleMins.z };
    const vec3_t sampleMaxsVec = { sampleMaxs.x, sampleMaxs.y, sampleMaxs.z };
    (void)gi.CM_BoxContents( sampleMinsVec, sampleMaxsVec, &boxContents, leafList.data(), ( int32_t )leafList.size(), &topNode );

    const cm_contents_t combinedContents = ( cm_contents_t )( standingContents | boxContents );

    /**
    *    Publish accepted span metadata for this probe.
    **/
    outSpan->span_id = spanId;
    outSpan->floor_z = floorZ;
    outSpan->ceiling_z = ceilingZ;
    outSpan->floor_normal_z = floorTrace.plane.normal[ 2 ];
    outSpan->clearance = clearance;
    outSpan->contents_flags = ( uint32_t )combinedContents;
    outSpan->classification_bits = NAV3_SPAN_CLASS_WALKABLE;
    outSpan->source_sample_coord = samplePoint;
    outSpan->source_standing_coord = standingOriginForBox;

    /**
    *    Mirror runtime content categories into stable span classification bits.
    **/
    if ( ( combinedContents & CONTENTS_WATER ) != 0 ) {
        outSpan->classification_bits |= NAV3_SPAN_CLASS_WATER;
        if ( inOutStats ) {
            inOutStats->spans_with_water++;
        }
    }
    if ( ( combinedContents & CONTENTS_SLIME ) != 0 ) {
        outSpan->classification_bits |= NAV3_SPAN_CLASS_SLIME;
        if ( inOutStats ) {
            inOutStats->spans_with_slime++;
        }
    }
    if ( ( combinedContents & CONTENTS_LAVA ) != 0 ) {
        outSpan->classification_bits |= NAV3_SPAN_CLASS_LAVA;
        if ( inOutStats ) {
            inOutStats->spans_with_lava++;
        }
    }
    if ( ( combinedContents & CONTENTS_LADDER ) != 0 ) {
        outSpan->classification_bits |= NAV3_SPAN_CLASS_LADDER;
        if ( inOutStats ) {
            inOutStats->spans_with_ladder++;
        }
    }

    /**
    *    Tag accepted but sloped floors so stage-5 diagnostics can report slope-classified spans.
    **/
    if ( outSpan->floor_normal_z < 0.999f ) {
        outSpan->classification_bits |= NAV3_SPAN_CLASS_SLOPED;
    }

    /**
    *    Tag spans whose clearance is valid but close to the minimum threshold.
    **/
    if ( clearance <= ( requiredStandingClearance + std::max( buildConfig.cell_height, 1.0f ) ) ) {
        outSpan->classification_bits |= NAV3_SPAN_CLASS_TIGHT_CLEARANCE;
    }

    /**
    *    Resolve BSP leaf/cluster/area metadata from the standing-origin point.
    **/
    Nav3_Generate_ResolvePointTopology( standingOriginForBox, outSpan );

    /**
    *    Return one emitted span and advance the next probe beneath this floor.
    **/
    result.span_found = true;
    result.next_probe_start_z = floorZ - std::max( NAV3_GENERATE_MIN_Z_STEP, buildConfig.cell_height * 0.5f );
    return result;
}

/**
*    @brief  Post-classify emitted spans as stair-adjacent or ledge-adjacent.
*    @param  buildConfig  Runtime build configuration.
*    @param  inOutMesh    [in,out] Generated mesh to classify.
*    @param  inOutStats   [in,out] Stats receiving classification counts.
**/
static void Nav3_Generate_ClassifyStairsAndLedges( const nav3_build_config_t &buildConfig,
    nav3_generated_mesh_t *inOutMesh,
    nav3_generation_stats_t *inOutStats ) {
    /**
    *    Require generated mesh storage and stats before running neighbor classification.
    **/
    if ( !inOutMesh || !inOutStats ) {
        return;
    }

    /**
    *    Reset stage-5 stairs/ledge counters before recomputing classifications.
    **/
    inOutStats->spans_with_stairs = 0;
    inOutStats->spans_with_ledge = 0;

    /**
    *    Build sparse-column lookup map for fast neighbor checks.
    **/
    const nav3_generate_column_lookup_t columnIndexByKey = Nav3_Generate_BuildColumnLookup( *inOutMesh );

    /**
    *    Resolve conservative classification thresholds from runtime build settings.
    **/
    const float stairMinDelta = std::max( NAV3_GENERATE_STAIR_MIN_DELTA, buildConfig.cell_height * 0.5f );
    const float stairMaxDelta = std::max( buildConfig.agent_step_height_units + buildConfig.cell_height, buildConfig.cell_height * 2.0f );
    const float ledgeDropDelta = std::max( stairMaxDelta * 1.5f, buildConfig.agent_height_units * 0.75f );
    static constexpr int32_t kNeighborOffsets[ 4 ][ 2 ] = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 }
    };

    /**
    *    Walk every emitted span and compare vertical deltas against four-neighbor columns.
    **/
    for ( nav3_generated_column_t &column : inOutMesh->columns ) {
        for ( nav3_generated_span_t &span : column.spans ) {
            bool isStairAdjacent = false;
            bool isLedgeAdjacent = false;

            /**
            *    Evaluate immediate XY neighbors so stacked-floor spans stay distinct by Z comparisons.
            **/
            for ( int32_t neighborIndex = 0; neighborIndex < 4; neighborIndex++ ) {
                const int32_t neighborCellX = column.cell_x + kNeighborOffsets[ neighborIndex ][ 0 ];
                const int32_t neighborCellY = column.cell_y + kNeighborOffsets[ neighborIndex ][ 1 ];
                const uint64_t neighborKey = Nav3_Generate_MakeCellKey( neighborCellX, neighborCellY );

                const auto neighborIt = columnIndexByKey.find( neighborKey );
                if ( neighborIt == columnIndexByKey.end() ) {
                    // Missing neighboring columns indicate a potential open drop edge.
                    isLedgeAdjacent = true;
                    continue;
                }

                const nav3_generated_column_t &neighborColumn = inOutMesh->columns[ neighborIt->second ];
                float nearestDelta = FLT_MAX;
                for ( const nav3_generated_span_t &neighborSpan : neighborColumn.spans ) {
                    const float dz = std::fabs( neighborSpan.floor_z - span.floor_z );
                    if ( dz < nearestDelta ) {
                        nearestDelta = dz;
                    }
                }

                /**
                *    Promote stair classification when one nearby floor differs by a controlled step amount.
                **/
                if ( nearestDelta >= stairMinDelta && nearestDelta <= stairMaxDelta ) {
                    isStairAdjacent = true;
                }

                /**
                *    Promote ledge classification when neighboring floors are significantly lower/higher.
                **/
                if ( nearestDelta > ledgeDropDelta ) {
                    isLedgeAdjacent = true;
                }
            }

            /**
            *    Commit stair/ledge bits and update bounded classification counters.
            **/
            if ( isStairAdjacent && ( span.classification_bits & NAV3_SPAN_CLASS_STAIRS ) == 0 ) {
                span.classification_bits |= NAV3_SPAN_CLASS_STAIRS;
                inOutStats->spans_with_stairs++;
            }
            if ( isLedgeAdjacent && ( span.classification_bits & NAV3_SPAN_CLASS_LEDGE ) == 0 ) {
                span.classification_bits |= NAV3_SPAN_CLASS_LEDGE;
                inOutStats->spans_with_ledge++;
            }
        }
    }
}


/**
*
*
*    Nav3 Stage-5 Generation Public API:
*
*
**/
/**
*    @brief  Generate stage-5 raw sparse spans from active BSP collision data.
*    @param  buildConfig  Runtime build configuration used for sampling thresholds.
*    @param  outMesh      [out] Generated sparse span dataset.
*    @return True when generation completed and emitted one or more spans.
**/
const bool SVG_Nav3_GenerateStaticSpans( const nav3_build_config_t &buildConfig, nav3_generated_mesh_t *outMesh ) {
    /**
    *    Require output storage before generation.
    **/
    if ( !outMesh ) {
        return false;
    }
    *outMesh = {};

    /**
    *    Require collision/BSP imports on the main thread before sampling.
    **/
    if ( !gi.GetCollisionModel || !gi.BSP_PointLeaf || !gi.CM_BoxContents || !gi.trace || !gi.pointcontents ) {
        return false;
    }

    /**
    *    Start full-pass timing and resolve active BSP world bounds.
    **/
    nav3_generation_stats_t stats = {};
    const auto totalStartTime = std::chrono::steady_clock::now();

    const auto resolveBoundsStartTime = std::chrono::steady_clock::now();
    BBox3 worldBounds = { Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } };
    if ( !Nav3_Generate_ResolveWorldBounds( &worldBounds ) ) {
        const auto totalEndTime = std::chrono::steady_clock::now();
        stats.total_elapsed_ms = std::chrono::duration<double, std::milli>( totalEndTime - totalStartTime ).count();
        outMesh->stats = stats;
        return false;
    }
    const auto resolveBoundsEndTime = std::chrono::steady_clock::now();
    stats.resolve_bounds_elapsed_ms = std::chrono::duration<double, std::milli>( resolveBoundsEndTime - resolveBoundsStartTime ).count();

    /**
    *    Publish generation metadata and derived config values into the output mesh.
    **/
    outMesh->world_bounds = worldBounds;
    outMesh->cell_size = buildConfig.cell_size;
    outMesh->cell_height = buildConfig.cell_height;
    outMesh->walkable_height_cells = buildConfig.walkable_height_cells;
    outMesh->walkable_climb_cells = buildConfig.walkable_climb_cells;
    outMesh->walkable_radius_cells = buildConfig.walkable_radius_cells;

    /**
    *    Validate cell sizing before deriving raster-domain bounds.
    **/
    const double cellSize = ( double )buildConfig.cell_size;
    if ( cellSize <= 0.0 ) {
        const auto totalEndTime = std::chrono::steady_clock::now();
        stats.total_elapsed_ms = std::chrono::duration<double, std::milli>( totalEndTime - totalStartTime ).count();
        outMesh->stats = stats;
        return false;
    }

    const int32_t minCellX = Nav3_Generate_GetMinCenteredCellIndex( worldBounds.mins.x, cellSize );
    const int32_t maxCellX = Nav3_Generate_GetMaxCenteredCellIndexExclusive( worldBounds.maxs.x, cellSize );
    const int32_t minCellY = Nav3_Generate_GetMinCenteredCellIndex( worldBounds.mins.y, cellSize );
    const int32_t maxCellY = Nav3_Generate_GetMaxCenteredCellIndexExclusive( worldBounds.maxs.y, cellSize );
    if ( maxCellX <= minCellX || maxCellY <= minCellY ) {
        const auto totalEndTime = std::chrono::steady_clock::now();
        stats.total_elapsed_ms = std::chrono::duration<double, std::milli>( totalEndTime - totalStartTime ).count();
        outMesh->stats = stats;
        return false;
    }

    /**
    *    Sample sparse columns in deterministic Y/X order and gather multi-floor spans per column.
    **/
    const auto samplingStartTime = std::chrono::steady_clock::now();
    int32_t nextSpanId = 0;
    for ( int32_t cellY = minCellY; cellY < maxCellY; cellY++ ) {
        // Traverse columns in deterministic row-major order for stable diagnostics.
        for ( int32_t cellX = minCellX; cellX < maxCellX; cellX++ ) {
            stats.sampled_columns++;

            nav3_generated_column_t column = {};
            column.cell_x = cellX;
            column.cell_y = cellY;

            const Vector3 columnCenter = {
                ( float )( ( ( double )cellX + 0.5 ) * cellSize ),
                ( float )( ( ( double )cellY + 0.5 ) * cellSize ),
                0.0f
            };

            float probeStartZ = worldBounds.maxs.z + NAV3_GENERATE_PROBE_TOP_PADDING;
            int32_t probeIterations = 0;
            while ( probeIterations < NAV3_GENERATE_MAX_PROBES_PER_COLUMN
                && ( int32_t )column.spans.size() < NAV3_GENERATE_MAX_SPANS_PER_COLUMN
                && probeStartZ > ( worldBounds.mins.z - NAV3_GENERATE_PROBE_BOTTOM_PADDING ) ) {
                nav3_generated_span_t span = {};
                const nav3_generate_probe_result_t probeResult = Nav3_Generate_TrySampleSpanAtProbe( buildConfig,
                    worldBounds,
                    columnCenter,
                    probeStartZ,
                    nextSpanId,
                    &span,
                    &stats );

                // Ensure strict downward progress even if probe helper returns a non-decreasing Z.
                if ( probeResult.next_probe_start_z < probeStartZ ) {
                    probeStartZ = probeResult.next_probe_start_z;
                } else {
                    probeStartZ -= NAV3_GENERATE_MIN_Z_STEP;
                }

                if ( probeResult.span_found ) {
                    column.spans.push_back( span );
                    stats.emitted_spans++;
                    nextSpanId++;
                }

                if ( probeResult.stop_column ) {
                    break;
                }

                probeIterations++;
            }

            /**
            *    Publish sparse columns only when one or more candidate spans were accepted.
            **/
            if ( !column.spans.empty() ) {
                stats.emitted_columns++;
                stats.max_spans_in_column = std::max( stats.max_spans_in_column, ( int32_t )column.spans.size() );
                outMesh->columns.push_back( std::move( column ) );
            }
        }
    }
    const auto samplingEndTime = std::chrono::steady_clock::now();
    stats.sampling_elapsed_ms = std::chrono::duration<double, std::milli>( samplingEndTime - samplingStartTime ).count();

    /**
    *    Classify stairs and ledges from local neighbor floor deltas.
    **/
    const auto classifyStartTime = std::chrono::steady_clock::now();
    Nav3_Generate_ClassifyStairsAndLedges( buildConfig, outMesh, &stats );
    const auto classifyEndTime = std::chrono::steady_clock::now();
    stats.classify_elapsed_ms = std::chrono::duration<double, std::milli>( classifyEndTime - classifyStartTime ).count();

    /**
    *    Run stage-6 ordered filter pipeline and compact filtered runtime span data.
    **/
    const auto filterStartTime = std::chrono::steady_clock::now();
    Nav3_Generate_RunFilterPipeline( buildConfig, outMesh, &stats );
    const auto filterEndTime = std::chrono::steady_clock::now();
    stats.filter_elapsed_ms = std::chrono::duration<double, std::milli>( filterEndTime - filterStartTime ).count();

    /**
    *    Finalize stats and return true only when at least one span was emitted.
    **/
    stats.generation_succeeded = stats.emitted_spans > 0;
    const auto totalEndTime = std::chrono::steady_clock::now();
    stats.total_elapsed_ms = std::chrono::duration<double, std::milli>( totalEndTime - totalStartTime ).count();
    outMesh->stats = stats;
    return stats.generation_succeeded;
}

/**
*    @brief  Return a readable label for one stage-6 filter pass.
*    @param  pass  Filter pass enum to convert.
*    @return Stable pass label used by bounded diagnostics.
**/
const char *SVG_Nav3_Generate_FilterPassName( const nav3_generation_filter_pass_t pass ) {
    switch ( pass ) {
    case nav3_generation_filter_pass_t::None:
        return "none";
    case nav3_generation_filter_pass_t::LowCeiling:
        return "low_ceiling";
    case nav3_generation_filter_pass_t::LowHangingObstruction:
        return "low_hanging_obstruction";
    case nav3_generation_filter_pass_t::LedgeDrop:
        return "ledge_drop";
    case nav3_generation_filter_pass_t::Slope:
        return "slope";
    case nav3_generation_filter_pass_t::RadiusErosion:
        return "radius_erosion";
    case nav3_generation_filter_pass_t::LiquidHazard:
        return "liquid_hazard";
    case nav3_generation_filter_pass_t::LadderSpecialSurface:
        return "ladder_special_surface";
    case nav3_generation_filter_pass_t::CapabilityRequirement:
        return "capability_requirement";
    case nav3_generation_filter_pass_t::Count:
    default:
        break;
    }
    return "unknown";
}

/**
*    @brief  Queue bounded span debug-draw primitives for one generated mesh.
*    @param  mesh        Generated mesh to visualize.
*    @param  maxColumns  Maximum sparse columns to draw.
*    @note   Submission is a no-op when nav3 debug or the spans subsystem cvar is disabled.
**/
void SVG_Nav3_Generate_DebugDrawSpans( const nav3_generated_mesh_t &mesh, const int32_t maxColumns ) {
    /**
    *    Require top-level and spans-subsystem debug toggles before queueing primitives.
    **/
    if ( !SVG_Nav3_DebugDraw_IsSubsystemEnabled( nav3_debug_draw_subsystem_t::Spans, 1 ) ) {
        return;
    }

    /**
    *    Require generated columns and valid sizing before drawing.
    **/
    if ( mesh.columns.empty() || mesh.cell_size <= 0.0f ) {
        return;
    }

    /**
    *    Resolve bounded draw limits from command input and subsystem detail level.
    **/
    const int32_t detailLevel = SVG_Nav3_DebugDraw_GetSubsystemLevel( nav3_debug_draw_subsystem_t::Spans );
    const int32_t drawColumnLimit = std::max( 1, std::min( maxColumns, NAV3_GENERATE_DEBUG_DRAW_MAX_COLUMNS ) );
    const int32_t drawSpansPerColumnLimit = ( detailLevel >= 2 ) ? NAV3_GENERATE_MAX_SPANS_PER_COLUMN : 1;

    /**
    *    Anchor span drawing around first active client when available.
    **/
    Vector3 anchor = {
        ( mesh.world_bounds.mins.x + mesh.world_bounds.maxs.x ) * 0.5f,
        ( mesh.world_bounds.mins.y + mesh.world_bounds.maxs.y ) * 0.5f,
        mesh.world_bounds.mins.z
    };
    for ( int32_t i = 1; i <= game.maxclients; i++ ) {
        svg_base_edict_t *playerEntity = g_edict_pool.EdictForNumber( i );
        if ( !SVG_Entity_IsActive( playerEntity ) ) {
            continue;
        }
        if ( !playerEntity->client ) {
            continue;
        }

        anchor = playerEntity->currentOrigin;
        break;
    }

    /**
    *    Draw a bounded local window around the anchor so debug traffic remains predictable.
    **/
    const float drawRadius = std::max( mesh.cell_size * ( detailLevel >= 2 ? 64.0f : 48.0f ), 256.0f );
    const float drawRadiusSq = drawRadius * drawRadius;
    const float halfExtent = std::max( mesh.cell_size * 0.45f, 4.0f );

    int32_t drawnColumns = 0;
    for ( const nav3_generated_column_t &column : mesh.columns ) {
        if ( drawnColumns >= drawColumnLimit ) {
            break;
        }

        const Vector3 columnCenter = {
            ( ( float )column.cell_x + 0.5f ) * mesh.cell_size,
            ( ( float )column.cell_y + 0.5f ) * mesh.cell_size,
            0.0f
        };
        const float dx = columnCenter.x - anchor.x;
        const float dy = columnCenter.y - anchor.y;
        if ( ( dx * dx ) + ( dy * dy ) > drawRadiusSq ) {
            continue;
        }

        drawnColumns++;
        int32_t drawnSpansInColumn = 0;
        for ( const nav3_generated_span_t &span : column.spans ) {
            if ( drawnSpansInColumn >= drawSpansPerColumnLimit ) {
                break;
            }
            drawnSpansInColumn++;

            /**
            *    Resolve one classification-driven color for this span box.
            **/
            uint32_t drawColor = U32_GREEN;
            if ( ( span.classification_bits & NAV3_SPAN_CLASS_LAVA ) != 0 ) {
                drawColor = U32_RED;
            } else if ( ( span.classification_bits & NAV3_SPAN_CLASS_SLIME ) != 0 ) {
                drawColor = U32_MAGENTA;
            } else if ( ( span.classification_bits & NAV3_SPAN_CLASS_WATER ) != 0 ) {
                drawColor = U32_BLUE;
            } else if ( ( span.classification_bits & NAV3_SPAN_CLASS_LADDER ) != 0 ) {
                drawColor = U32_CYAN;
            } else if ( ( span.classification_bits & NAV3_SPAN_CLASS_STAIRS ) != 0 ) {
                drawColor = U32_YELLOW;
            } else if ( ( span.classification_bits & NAV3_SPAN_CLASS_LEDGE ) != 0 ) {
                drawColor = U32_WHITE;
            }

            /**
            *    Draw one bounded span AABB from floor to a conservative capped ceiling height.
            **/
            const float cappedCeiling = std::min( span.ceiling_z, span.floor_z + 96.0f );
            const float topZ = std::max( span.floor_z + 1.0f, cappedCeiling );
            const Vector3 mins = {
                columnCenter.x - halfExtent,
                columnCenter.y - halfExtent,
                span.floor_z
            };
            const Vector3 maxs = {
                columnCenter.x + halfExtent,
                columnCenter.y + halfExtent,
                topZ
            };
            SVG_Nav3_DebugDraw_AddAabb( mins, maxs, drawColor );

            /**
            *    In full-detail mode, draw additional vertical and ledge cues.
            **/
            if ( detailLevel >= 2 ) {
                const Vector3 lineStart = { columnCenter.x, columnCenter.y, span.floor_z };
                const Vector3 lineEnd = { columnCenter.x, columnCenter.y, topZ };
                SVG_Nav3_DebugDraw_AddLine( lineStart, lineEnd, U32_WHITE,
                    SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
                    1.0f,
                    0.0f );

                if ( ( span.classification_bits & NAV3_SPAN_CLASS_LEDGE ) != 0 ) {
                    SVG_Nav3_DebugDraw_AddArrow( lineStart,
                        Vector3{ lineStart.x, lineStart.y, lineStart.z - 24.0f },
                        8.0f,
                        U32_RED );
                }
            }
        }
    }
}

/**
*    @brief  Queue bounded reject-sample debug primitives for stage-6 filter diagnostics.
*    @param  mesh         Generated mesh containing filter reject samples.
*    @param  maxSamples   Maximum reject samples to draw.
*    @note   Submission is a no-op when nav3 debug or the filters subsystem cvar is disabled.
**/
void SVG_Nav3_Generate_DebugDrawFilterRejects( const nav3_generated_mesh_t &mesh, const int32_t maxSamples ) {
    /**
    *    Require top-level and filters-subsystem debug toggles before queueing primitives.
    **/
    if ( !SVG_Nav3_DebugDraw_IsSubsystemEnabled( nav3_debug_draw_subsystem_t::Filters, 1 ) ) {
        return;
    }

    /**
    *    Require at least one bounded reject sample before drawing.
    **/
    if ( mesh.stats.filter_reject_sample_count <= 0 ) {
        return;
    }

    const int32_t detailLevel = SVG_Nav3_DebugDraw_GetSubsystemLevel( nav3_debug_draw_subsystem_t::Filters );
    const int32_t drawSampleLimit = std::max( 1,
        std::min( maxSamples, NAV3_GENERATE_DEBUG_DRAW_MAX_FILTER_SAMPLES ) );
    const int32_t totalSamples = std::min( mesh.stats.filter_reject_sample_count, drawSampleLimit );

    /**
    *    Emit one bounded marker per reject sample with pass-specific color and optional labels.
    **/
    for ( int32_t sampleIndex = 0; sampleIndex < totalSamples; sampleIndex++ ) {
        const nav3_generation_filter_reject_sample_t &sample = mesh.stats.filter_reject_samples[ sampleIndex ];

        uint32_t drawColor = U32_RED;
        switch ( sample.pass ) {
        case nav3_generation_filter_pass_t::LowCeiling:
            drawColor = U32_RED;
            break;
        case nav3_generation_filter_pass_t::LowHangingObstruction:
            drawColor = U32_MAGENTA;
            break;
        case nav3_generation_filter_pass_t::LedgeDrop:
            drawColor = U32_YELLOW;
            break;
        case nav3_generation_filter_pass_t::Slope:
            drawColor = U32_CYAN;
            break;
        case nav3_generation_filter_pass_t::RadiusErosion:
            drawColor = U32_BLUE;
            break;
        case nav3_generation_filter_pass_t::LiquidHazard:
            drawColor = U32_RED;
            break;
        case nav3_generation_filter_pass_t::LadderSpecialSurface:
            drawColor = U32_WHITE;
            break;
        case nav3_generation_filter_pass_t::CapabilityRequirement:
            drawColor = U32_GREEN;
            break;
        case nav3_generation_filter_pass_t::None:
        case nav3_generation_filter_pass_t::Count:
        default:
            break;
        }

        const Vector3 markerOrigin = {
            sample.sample_origin.x,
            sample.sample_origin.y,
            sample.floor_z + 4.0f
        };

        SVG_Nav3_DebugDraw_AddSphere(
            markerOrigin,
            5.0f,
            drawColor,
            SG_SVC_DEBUG_DRAW_STYLE_FLAG_DEPTH_TEST,
            1.0f,
            0.0f );

        if ( detailLevel >= 2 ) {
            const Vector3 textOrigin = { markerOrigin.x, markerOrigin.y, markerOrigin.z + 10.0f };
            SVG_Nav3_DebugDraw_AddText( textOrigin, SVG_Nav3_Generate_FilterPassName( sample.pass ), drawColor );
        }
    }
}
