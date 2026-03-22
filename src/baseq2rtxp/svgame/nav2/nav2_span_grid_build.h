/********************************************************************
*
*
* ServerGame: Nav2 Span Grid Build
*
*
********************************************************************/
#pragma once

#include "svgame/svg_local.h"

#include "svgame/nav2/nav2_runtime.h"
#include "svgame/nav2/nav2_span_grid.h"


/**
*	@brief	Bounded diagnostics for one span-grid build pass.
*	@note	The builder updates this structure so Task 4.2 validation can report domain size,
*			sampling outcomes, and conservative rejection counts without introducing log spam.
**/
struct nav2_span_grid_build_stats_t {
    //! Total sampled XY columns inside the rasterized world bounds.
    int32_t sampled_columns = 0;
    //! Number of sparse columns emitted after conservative support/clearance validation.
    int32_t emitted_columns = 0;
    //! Number of spans emitted into sparse columns.
    int32_t emitted_spans = 0;
    //! Number of columns rejected because probe centers were in solid point contents.
    int32_t rejected_solid_point = 0;
    //! Number of columns rejected because support traces found start/all-solid collisions.
    int32_t rejected_floor_trace = 0;
    //! Number of columns rejected because support traces never reached a floor.
    int32_t rejected_floor_miss = 0;
    //! Number of columns rejected because floor normals failed conservative walkable slope checks.
    int32_t rejected_slope = 0;
    //! Number of columns rejected due to insufficient sampled vertical clearance.
    int32_t rejected_clearance = 0;
    //! Number of columns rejected because sampled standing boxes reported solid contents.
    int32_t rejected_solid_box = 0;
    //! Number of emitted spans tagged with water movement semantics.
    int32_t spans_with_water = 0;
    //! Number of emitted spans tagged with slime movement semantics.
    int32_t spans_with_slime = 0;
    //! Number of emitted spans tagged with lava movement semantics.
    int32_t spans_with_lava = 0;
};


/**
*	@brief	Build a sparse span-grid from one explicit nav2 mesh publication.
*	@param	mesh		Nav2-owned runtime mesh providing world bounds and sizing metadata.
*	@param	out_grid	[out] Grid receiving generated sparse columns and spans.
*	@param	out_stats	[out] Optional bounded diagnostics for the build pass.
*	@return	True when rasterization completed successfully.
**/
const bool SVG_Nav2_BuildSpanGridFromMesh( const nav2_mesh_t *mesh, nav2_span_grid_t *out_grid,
    nav2_span_grid_build_stats_t *out_stats = nullptr );

/**
*	@brief	Return bounded diagnostics from the most recent span-grid build pass.
*	@param	out_stats	[out] Receives the latest builder diagnostics snapshot.
*	@return	True when diagnostics were copied.
**/
const bool SVG_Nav2_GetLastSpanGridBuildStats( nav2_span_grid_build_stats_t *out_stats );


/**
*	@brief	Build a sparse span-grid placeholder from the currently published nav2 mesh.
*	@param	out_grid	[out] Grid receiving the generated span data.
*	@return	True when a grid could be produced.
*	@note	This is the Milestone 4 build seam; the initial implementation is deliberately conservative and
*			does not reintroduce any public fallback-style behavior.
**/
const bool SVG_Nav2_BuildSpanGrid( nav2_span_grid_t *out_grid );
