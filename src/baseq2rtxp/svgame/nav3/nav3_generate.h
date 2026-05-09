/********************************************************************
*
*
*    ServerGame: Nav3 Stage-5 Static BSP Span Generation
*
*
********************************************************************/
#pragma once

#include "svgame/nav3/nav3_core.h"

#include <array>
#include <vector>


/**
*
*
*    Nav3 Stage-5 Span Classification Flags:
*
*
**/
//! No extra classification flags are set.
static constexpr uint32_t NAV3_SPAN_CLASS_NONE = 0;
//! Span is considered walkable for the baseline standing profile.
static constexpr uint32_t NAV3_SPAN_CLASS_WALKABLE = BIT( 0 );
//! Span intersects water contents.
static constexpr uint32_t NAV3_SPAN_CLASS_WATER = BIT( 1 );
//! Span intersects slime contents.
static constexpr uint32_t NAV3_SPAN_CLASS_SLIME = BIT( 2 );
//! Span intersects lava contents.
static constexpr uint32_t NAV3_SPAN_CLASS_LAVA = BIT( 3 );
//! Span intersects ladder contents.
static constexpr uint32_t NAV3_SPAN_CLASS_LADDER = BIT( 4 );
//! Span was classified as stair-adjacent by local step-height comparisons.
static constexpr uint32_t NAV3_SPAN_CLASS_STAIRS = BIT( 5 );
//! Span was classified as ledge-adjacent by local drop comparisons.
static constexpr uint32_t NAV3_SPAN_CLASS_LEDGE = BIT( 6 );
//! Span floor normal indicates a sloped floor.
static constexpr uint32_t NAV3_SPAN_CLASS_SLOPED = BIT( 7 );
//! Span clearance is valid but near the minimum standing threshold.
static constexpr uint32_t NAV3_SPAN_CLASS_TIGHT_CLEARANCE = BIT( 8 );


/**
*
*
*    Nav3 Stage-6 Filter Pipeline Types:
*
*
**/
/**
*    @brief  Ordered stage-6 filter passes applied to raw generated spans.
*    @note   Ordering is significant for deterministic pass/reject diagnostics.
**/
enum class nav3_generation_filter_pass_t : uint8_t {
    None = 0,
    LowCeiling,
    LowHangingObstruction,
    LedgeDrop,
    Slope,
    RadiusErosion,
    LiquidHazard,
    LadderSpecialSurface,
    CapabilityRequirement,
    Count
};

//! Bounded maximum number of reject samples retained for filter debug draw diagnostics.
static constexpr int32_t NAV3_GENERATION_MAX_FILTER_REJECT_SAMPLES = 128;

/**
*    @brief  One filter-pass accepted/rejected counter tuple.
**/
struct nav3_generation_filter_pass_stats_t {
    //! Filter pass identifier.
    nav3_generation_filter_pass_t pass = nav3_generation_filter_pass_t::None;
    //! Number of spans that passed this filter stage.
    int32_t accepted_count = 0;
    //! Number of spans rejected at this filter stage.
    int32_t rejected_count = 0;
};

/**
*    @brief  One bounded reject sample used for `nav3_debug_filters` diagnostics.
**/
struct nav3_generation_filter_reject_sample_t {
    //! Filter pass that rejected the sampled span.
    nav3_generation_filter_pass_t pass = nav3_generation_filter_pass_t::None;
    //! World-space sample origin retained for debug draw.
    Vector3 sample_origin = {};
    //! Rejected span floor height retained for small vertical marker offsets.
    float floor_z = 0.0f;
};


/**
*
*
*    Nav3 Stage-5 Runtime Raw Generation Data:
*
*
**/
/**
*    @brief  One generated walkable floor span owned by one sparse XY column.
*    @note   Stage 5 stores raw BSP-derived classification and topology metadata only.
**/
struct nav3_generated_span_t {
    //! Stable span identifier inside one generated mesh pass.
    int32_t span_id = -1;
    //! Walkable floor contact height in world units.
    float floor_z = 0.0f;
    //! Conservative ceiling sample height in world units.
    float ceiling_z = 0.0f;
    //! Floor plane normal Z used for slope classification.
    float floor_normal_z = 0.0f;
    //! Conservative standing clearance in world units.
    float clearance = 0.0f;
    //! BSP leaf backing the sampled standing origin when known.
    int32_t leaf_id = -1;
    //! BSP cluster backing the sampled standing origin when known.
    int32_t cluster_id = -1;
    //! BSP area backing the sampled standing origin when known.
    int32_t area_id = -1;
    //! Raw combined contents flags sampled for this span.
    uint32_t contents_flags = 0;
    //! Stage-5 span classification bits.
    uint32_t classification_bits = NAV3_SPAN_CLASS_NONE;
    //! True when this span was rejected by one stage-6 filter pass.
    bool filtered_out = false;
    //! First stage-6 filter pass that rejected this span.
    nav3_generation_filter_pass_t filtered_pass = nav3_generation_filter_pass_t::None;
    //! World-space XY probe coordinate used to produce this span.
    Vector3 source_sample_coord = {};
    //! World-space standing-origin coordinate used for topology resolution.
    Vector3 source_standing_coord = {};
};

/**
*    @brief  One sparse XY column containing one or more generated floor spans.
**/
struct nav3_generated_column_t {
    //! World-cell X coordinate for this sparse column.
    int32_t cell_x = 0;
    //! World-cell Y coordinate for this sparse column.
    int32_t cell_y = 0;
    //! Ordered floor spans produced for this XY coordinate.
    std::vector<nav3_generated_span_t> spans = {};
};

/**
*    @brief  Bounded diagnostics for one stage-5 generation pass.
**/
struct nav3_generation_stats_t {
    //! True when the generation pass completed and published at least one span.
    bool generation_succeeded = false;
    //! Number of sampled XY columns inside resolved world bounds.
    int32_t sampled_columns = 0;
    //! Number of emitted sparse columns.
    int32_t emitted_columns = 0;
    //! Number of emitted spans across all columns.
    int32_t emitted_spans = 0;
    //! Highest span count emitted in one sparse column.
    int32_t max_spans_in_column = 0;
    //! Number of spans entering the stage-6 filter pipeline.
    int32_t filter_input_spans = 0;
    //! Number of spans remaining after the stage-6 filter pipeline.
    int32_t filter_output_spans = 0;
    //! Radius (in cells) used by stage-6 erosion pass.
    int32_t filter_erosion_radius_cells = 0;
    //! Erosion rejections caused by missing neighboring columns.
    int32_t filter_erosion_blocked_missing_columns = 0;
    //! Erosion rejections caused by incompatible neighboring floor connectivity.
    int32_t filter_erosion_blocked_neighbor_columns = 0;

    //! Columns/probes rejected because the probe start point was inside solid.
    int32_t rejected_solid_point = 0;
    //! Probes rejected because floor traces began or remained in solid.
    int32_t rejected_floor_trace = 0;
    //! Probes rejected because no support floor was found.
    int32_t rejected_floor_miss = 0;
    //! Candidate floors rejected by slope threshold.
    int32_t rejected_slope = 0;
    //! Candidate floors rejected by insufficient standing clearance.
    int32_t rejected_clearance = 0;
    //! Candidate floors rejected by in-place hull occupancy validation.
    int32_t rejected_solid_box = 0;

    //! Emitted spans tagged with water contents.
    int32_t spans_with_water = 0;
    //! Emitted spans tagged with slime contents.
    int32_t spans_with_slime = 0;
    //! Emitted spans tagged with lava contents.
    int32_t spans_with_lava = 0;
    //! Emitted spans tagged with ladder contents.
    int32_t spans_with_ladder = 0;
    //! Emitted spans tagged as stairs.
    int32_t spans_with_stairs = 0;
    //! Emitted spans tagged as ledge-adjacent.
    int32_t spans_with_ledge = 0;

    //! Per-pass accepted/rejected counters for stage-6 filter diagnostics.
    std::array<nav3_generation_filter_pass_stats_t, static_cast<size_t>( nav3_generation_filter_pass_t::Count )> filter_pass_stats = {};
    //! Bounded reject samples for `nav3_debug_filters` visualization.
    std::array<nav3_generation_filter_reject_sample_t, NAV3_GENERATION_MAX_FILTER_REJECT_SAMPLES> filter_reject_samples = {};
    //! Number of valid entries in `filter_reject_samples`.
    int32_t filter_reject_sample_count = 0;
    //! Number of rejected samples dropped due to bounded sample storage.
    int32_t filter_reject_sample_dropped = 0;

    //! Elapsed milliseconds resolving world bounds and raster domain.
    double resolve_bounds_elapsed_ms = 0.0;
    //! Elapsed milliseconds spent sampling and validating floor spans.
    double sampling_elapsed_ms = 0.0;
    //! Elapsed milliseconds spent classifying stairs and ledges.
    double classify_elapsed_ms = 0.0;
    //! Elapsed milliseconds spent running stage-6 filter pipeline and erosion.
    double filter_elapsed_ms = 0.0;
    //! Total elapsed milliseconds for the full generation pass.
    double total_elapsed_ms = 0.0;
};

/**
*    @brief  Runtime-owned stage-5 generated sparse span dataset.
**/
struct nav3_generated_mesh_t {
    //! Active BSP world bounds used by this generation pass.
    BBox3 world_bounds = { Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } };
    //! Resolved stage-5 cell size used for XY sampling.
    float cell_size = 0.0f;
    //! Resolved stage-5 cell height used for quantized vertical rules.
    float cell_height = 0.0f;
    //! Derived standing clearance threshold in cells.
    int32_t walkable_height_cells = 0;
    //! Derived step/climb threshold in cells.
    int32_t walkable_climb_cells = 0;
    //! Derived erosion radius in cells.
    int32_t walkable_radius_cells = 0;
    //! Sparse generated columns in deterministic XY traversal order.
    std::vector<nav3_generated_column_t> columns = {};
    //! Bounded generation diagnostics and timing for this mesh.
    nav3_generation_stats_t stats = {};
};


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
const bool SVG_Nav3_GenerateStaticSpans( const nav3_build_config_t &buildConfig, nav3_generated_mesh_t *outMesh );

/**
*    @brief  Return a readable label for one stage-6 filter pass.
*    @param  pass  Filter pass enum to convert.
*    @return Stable pass label used by bounded diagnostics.
**/
const char *SVG_Nav3_Generate_FilterPassName( const nav3_generation_filter_pass_t pass );

/**
*    @brief  Queue bounded span debug-draw primitives for one generated mesh.
*    @param  mesh        Generated mesh to visualize.
*    @param  maxColumns  Maximum sparse columns to draw.
*    @note   Submission is a no-op when nav3 debug or the spans subsystem cvar is disabled.
**/
void SVG_Nav3_Generate_DebugDrawSpans( const nav3_generated_mesh_t &mesh, const int32_t maxColumns );

/**
*    @brief  Queue bounded reject-sample debug primitives for stage-6 filter diagnostics.
*    @param  mesh         Generated mesh containing filter reject samples.
*    @param  maxSamples   Maximum reject samples to draw.
*    @note   Submission is a no-op when nav3 debug or the filters subsystem cvar is disabled.
**/
void SVG_Nav3_Generate_DebugDrawFilterRejects( const nav3_generated_mesh_t &mesh, const int32_t maxSamples );
