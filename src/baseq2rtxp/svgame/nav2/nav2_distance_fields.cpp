/********************************************************************
*
*
*	ServerGame: Nav2 Distance-Derived Utility Fields - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_distance_fields.h"

#include <algorithm>
#include <cmath>
#include <limits>


/**
*
*
*	Nav2 Distance Field Local Helpers:
*
*
**/
/**
*	@brief	Return the squared Euclidean distance between two world-space points.
*	@param	a	First point.
*	@param	b	Second point.
*	@return	Squared distance between both points.
**/
static inline double SVG_Nav2_DistanceSq( const Vector3 &a, const Vector3 &b ) {
    // Compute per-axis deltas first so the expression remains explicit for debug inspection.
    const double dx = ( double )a.x - ( double )b.x;
    const double dy = ( double )a.y - ( double )b.y;
    const double dz = ( double )a.z - ( double )b.z;
    return ( dx * dx ) + ( dy * dy ) + ( dz * dz );
}

/**
*	@brief	Return the Euclidean distance between two world-space points.
*	@param	a	First point.
*	@param	b	Second point.
*	@return	Distance between both points.
**/
static inline double SVG_Nav2_Distance( const Vector3 &a, const Vector3 &b ) {
    return std::sqrt( SVG_Nav2_DistanceSq( a, b ) );
}

/**
*	@brief	Build a deterministic world-space representative point for one span record.
*	@param	grid	Sparse precision grid providing cell sizing metadata.
*	@param	column	Owning sparse column for the span.
*	@param	span	Span payload.
*	@return	World-space span representative point.
**/
static Vector3 SVG_Nav2_DistanceField_BuildSpanWorldOrigin( const nav2_span_grid_t &grid, const nav2_span_column_t &column, const nav2_span_t &span ) {
    // Resolve a conservative cell size first so world-origin conversion remains stable on partially initialized grids.
    const double cellSizeXY = ( grid.cell_size_xy > 0.0 ) ? grid.cell_size_xy : 32.0;

    // Convert sparse XY cell coordinates into centered world-space points and mirror span preferred height for Z.
    Vector3 origin = {};
    origin.x = ( float )( ( ( double )column.tile_x + 0.5 ) * cellSizeXY );
    origin.y = ( float )( ( ( double )column.tile_y + 0.5 ) * cellSizeXY );
    origin.z = ( float )span.preferred_z;
    return origin;
}

/**
*	@brief	Return whether one connector kind bitmask contains a specific connector kind.
*	@param	connector	Connector to inspect.
*	@param	kind	Connector kind bit to test.
*	@return	True when the connector contains the requested kind bit.
**/
static inline const bool SVG_Nav2_ConnectorHasKind( const nav2_connector_t &connector, const uint32_t kind ) {
    return ( connector.connector_kind & kind ) != 0;
}

/**
*	@brief	Return the best available world-space connector anchor for distance-field evaluation.
*	@param	connector	Connector to inspect.
*	@return	Pointer to the preferred valid anchor, or `nullptr` when no valid anchor is available.
**/
static const nav2_connector_anchor_t *SVG_Nav2_DistanceField_SelectConnectorAnchor( const nav2_connector_t &connector ) {
    // Prefer the start anchor when valid because connector extraction stores deterministic primary endpoints there.
    if ( connector.start.valid ) {
        return &connector.start;
    }

    // Fallback to the secondary anchor when only the end anchor is valid.
    if ( connector.end.valid ) {
        return &connector.end;
    }

    // Return null when both anchors are invalid.
    return nullptr;
}

/**
*	@brief	Compute distance to the closest connector matching one kind bit.
*	@param	span_origin	World-space span representative point.
*	@param	connectors	Connector list providing candidate anchors.
*	@param	kind_mask	Connector kind bitmask to match.
*	@param	out_distance	[out] Closest connector distance.
*	@param	out_connector_id	[out] Stable connector id for the closest match.
*	@return	True when a matching connector anchor was found.
**/
static const bool SVG_Nav2_DistanceField_FindNearestConnectorDistance( const Vector3 &span_origin, const nav2_connector_list_t &connectors,
    const uint32_t kind_mask, double *out_distance, int32_t *out_connector_id ) {
    /**
    *    Require output storage before scanning connectors.
    **/
    if ( !out_distance || !out_connector_id ) {
        return false;
    }
    *out_distance = 0.0;
    *out_connector_id = -1;

    /**
    *    Scan all connectors and retain the nearest valid anchor that matches the requested semantic kind.
    **/
    bool hasMatch = false;
    double bestDistanceSq = std::numeric_limits<double>::max();
    for ( const nav2_connector_t &connector : connectors.connectors ) {
        if ( !SVG_Nav2_ConnectorHasKind( connector, kind_mask ) ) {
            continue;
        }

        const nav2_connector_anchor_t *anchor = SVG_Nav2_DistanceField_SelectConnectorAnchor( connector );
        if ( !anchor ) {
            continue;
        }

        const double candidateDistanceSq = SVG_Nav2_DistanceSq( span_origin, anchor->world_origin );
        if ( candidateDistanceSq < bestDistanceSq ) {
            bestDistanceSq = candidateDistanceSq;
            *out_connector_id = connector.connector_id;
            hasMatch = true;
        }
    }

    /**
    *    Publish the closest distance only when a matching connector was found.
    **/
    if ( !hasMatch ) {
        return false;
    }
    *out_distance = std::sqrt( bestDistanceSq );
    return true;
}

/**
*	@brief	Compute distance to the nearest corridor segment anchor pair.
*	@param	span_origin	World-space span representative point.
*	@param	corridor	Optional corridor commitments.
*	@param	out_distance	[out] Closest corridor distance.
*	@return	True when at least one corridor segment is available.
**/
static const bool SVG_Nav2_DistanceField_FindCorridorDistance( const Vector3 &span_origin, const nav2_corridor_t *corridor, double *out_distance ) {
    /**
    *    Require output storage and at least one corridor segment before evaluating corridor distance.
    **/
    if ( !out_distance || !corridor || corridor->segments.empty() ) {
        return false;
    }
    *out_distance = 0.0;

    /**
    *    Evaluate both anchors from each segment and keep the nearest one as a cheap corridor proximity signal.
    **/
    double bestDistanceSq = std::numeric_limits<double>::max();
    for ( const nav2_corridor_segment_t &segment : corridor->segments ) {
        const double startDistanceSq = SVG_Nav2_DistanceSq( span_origin, segment.start_anchor );
        const double endDistanceSq = SVG_Nav2_DistanceSq( span_origin, segment.end_anchor );
        bestDistanceSq = std::min( bestDistanceSq, std::min( startDistanceSq, endDistanceSq ) );
    }

    /**
    *    Publish the nearest corridor distance.
    **/
    if ( bestDistanceSq >= std::numeric_limits<double>::max() ) {
        return false;
    }
    *out_distance = std::sqrt( bestDistanceSq );
    return true;
}

/**
*	@brief	Compute a conservative region-boundary distance using sparse-neighbor topology mismatch checks.
*	@param	grid	Sparse precision grid.
*	@param	column	Current sparse column.
*	@param	span	Current span.
*	@param	params	Build settings controlling probe radius.
*	@return	Distance estimate to the nearest region boundary mismatch.
**/
static double SVG_Nav2_DistanceField_ComputeRegionBoundaryDistance( const nav2_span_grid_t &grid, const nav2_span_column_t &column,
    const nav2_span_t &span, const nav2_distance_field_build_params_t &params ) {
    // Resolve conservative cell sizing so boundary distance stays deterministic even when grid metadata is partially initialized.
    const double cellSizeXY = ( grid.cell_size_xy > 0.0 ) ? grid.cell_size_xy : 32.0;

    // Start from the maximum probe range and tighten only when a region mismatch is discovered.
    const int32_t maxRadiusCells = std::max( 1, params.boundary_search_radius_cells );
    double bestDistance = ( double )maxRadiusCells * cellSizeXY;

    // Search four cardinal directions because they provide a cheap region-boundary proxy without expensive radial flood scans.
    static constexpr int32_t offsets[ 4 ][ 2 ] = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 }
    };
    for ( int32_t directionIndex = 0; directionIndex < 4; directionIndex++ ) {
        const int32_t offsetX = offsets[ directionIndex ][ 0 ];
        const int32_t offsetY = offsets[ directionIndex ][ 1 ];

        // Probe outward by radius and stop as soon as a mismatch or sparse-column gap is found.
        for ( int32_t radius = 1; radius <= maxRadiusCells; radius++ ) {
            const int32_t probeTileX = column.tile_x + ( offsetX * radius );
            const int32_t probeTileY = column.tile_y + ( offsetY * radius );
            const nav2_span_column_t *neighborColumn = SVG_Nav2_SpanGrid_TryGetColumn( grid, probeTileX, probeTileY );
            if ( !neighborColumn || neighborColumn->spans.empty() ) {
                bestDistance = std::min( bestDistance, ( double )radius * cellSizeXY );
                break;
            }

            bool mismatchFound = false;
            for ( const nav2_span_t &neighborSpan : neighborColumn->spans ) {
                if ( neighborSpan.region_layer_id != span.region_layer_id || neighborSpan.cluster_id != span.cluster_id || neighborSpan.area_id != span.area_id ) {
                    mismatchFound = true;
                    break;
                }
            }
            if ( mismatchFound ) {
                bestDistance = std::min( bestDistance, ( double )radius * cellSizeXY );
                break;
            }
        }
    }

    return bestDistance;
}

/**
*	@brief	Compute a conservative obstacle-boundary distance using point-contents probing.
*	@param	span_origin	World-space span representative point.
*	@param	params	Build settings controlling radial probe behavior.
*	@return	Distance estimate to the nearest obstacle boundary.
**/
static double SVG_Nav2_DistanceField_ComputeObstacleBoundaryDistance( const Vector3 &span_origin, const nav2_distance_field_build_params_t &params ) {
    /**
    *    Require point-contents import before attempting obstacle probing.
    **/
    if ( !gi.pointcontents ) {
        return std::max( params.obstacle_probe_step, 1.0 );
    }

    /**
    *    Clamp probe settings so radial stepping remains bounded and deterministic.
    **/
    const double probeRadius = std::max( params.obstacle_probe_radius, 1.0 );
    const double probeStep = std::max( params.obstacle_probe_step, 1.0 );

    /**
    *    Scan radial samples in eight directions and keep the nearest solid-contact distance.
    **/
    static constexpr double kRootHalf = 0.7071067811865476;
    static constexpr double directions[ 8 ][ 2 ] = {
        { 1.0, 0.0 },
        { -1.0, 0.0 },
        { 0.0, 1.0 },
        { 0.0, -1.0 },
        { kRootHalf, kRootHalf },
        { -kRootHalf, kRootHalf },
        { kRootHalf, -kRootHalf },
        { -kRootHalf, -kRootHalf }
    };

    double bestDistance = probeRadius;
    for ( int32_t directionIndex = 0; directionIndex < 8; directionIndex++ ) {
        const double dirX = directions[ directionIndex ][ 0 ];
        const double dirY = directions[ directionIndex ][ 1 ];

        // March outward and stop at the first solid contents classification in this direction.
        for ( double probeDistance = probeStep; probeDistance <= probeRadius; probeDistance += probeStep ) {
            const Vector3 samplePoint = {
                ( float )( ( double )span_origin.x + ( dirX * probeDistance ) ),
                ( float )( ( double )span_origin.y + ( dirY * probeDistance ) ),
                span_origin.z
            };
            const cm_contents_t sampleContents = gi.pointcontents( &samplePoint );
            if ( ( sampleContents & CONTENTS_SOLID ) != 0 ) {
                bestDistance = std::min( bestDistance, probeDistance );
                break;
            }
        }
    }

    return bestDistance;
}

/**
*	@brief	Compute a centerline-deviation proxy from region-boundary distance.
*	@param	boundary_distance	Distance to nearest region boundary.
*	@param	params	Build settings providing target half-width.
*	@return	Absolute deviation from the open-space centerline proxy.
**/
static inline double SVG_Nav2_DistanceField_ComputeCenterlineDistance( const double boundary_distance, const nav2_distance_field_build_params_t &params ) {
    const double targetHalfWidth = std::max( params.open_space_half_width, 1.0 );
    return std::fabs( boundary_distance - targetHalfWidth );
}

/**
*	@brief	Compute clearance-bottleneck distance from one span height range.
*	@param	span	Span payload to evaluate.
*	@param	params	Build settings carrying bottleneck thresholds.
*	@return	Distance to the configured bottleneck threshold.
**/
static inline double SVG_Nav2_DistanceField_ComputeClearanceBottleneckDistance( const nav2_span_t &span, const nav2_distance_field_build_params_t &params ) {
    const double clearance = std::max( 0.0, span.ceiling_z - span.floor_z );
    const double threshold = std::max( params.clearance_bottleneck_threshold, 1.0 );
    if ( clearance <= threshold ) {
        return 0.0;
    }
    return clearance - threshold;
}

/**
*	@brief	Evaluate optional fat-PVS relevance metadata for one span origin.
*	@param	span_origin	World-space span representative point.
*	@param	params	Build settings containing optional fat-PVS inputs.
*	@param	out_relevance	[out] Receives `true` when the span origin is fat-PVS relevant.
*	@return	True when the relevance probe was evaluated.
**/
static const bool SVG_Nav2_DistanceField_EvaluateFatPvsRelevance( const Vector3 &span_origin, const nav2_distance_field_build_params_t &params,
    bool *out_relevance ) {
    /**
    *    Require output storage and all optional fat-PVS inputs before evaluating relevance.
    **/
    if ( !out_relevance || !params.use_fat_pvs_relevance || !gi.inFatPVS ) {
        return false;
    }
    *out_relevance = false;

    /**
    *    Evaluate point-vs-AABB fat visibility relevance as a guidance-only hint.
    **/
    const vec3_t p1 = { span_origin.x, span_origin.y, span_origin.z };
    const vec3_t mins = { params.fat_pvs_mins.x, params.fat_pvs_mins.y, params.fat_pvs_mins.z };
    const vec3_t maxs = { params.fat_pvs_maxs.x, params.fat_pvs_maxs.y, params.fat_pvs_maxs.z };
    *out_relevance = gi.inFatPVS( &p1, &mins, &maxs, params.fat_pvs_vis );
    return true;
}

/**
*	@brief	Accumulate one distance-field record into the bounded summary output.
*	@param	record	Distance-field record to accumulate.
*	@param	out_summary	[in,out] Summary receiving aggregate counters and ranges.
*	@param	obstacle_sum	[in,out] Running obstacle-distance sum for average computation.
*	@param	obstacle_count	[in,out] Running obstacle-distance sample count.
*	@param	clearance_sum	[in,out] Running clearance-distance sum for average computation.
*	@param	clearance_count	[in,out] Running clearance-distance sample count.
**/
static void SVG_Nav2_DistanceField_AccumulateSummary( const nav2_distance_field_record_t &record, nav2_distance_field_summary_t *out_summary,
    double *obstacle_sum, int32_t *obstacle_count, double *clearance_sum, int32_t *clearance_count ) {
    /**
    *    Require summary and average accumulators before mutating aggregate counters.
    **/
    if ( !out_summary || !obstacle_sum || !obstacle_count || !clearance_sum || !clearance_count ) {
        return;
    }

    /**
    *    Count record-level metric availability flags.
    **/
    out_summary->record_count++;
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_GOAL_DISTANCE ) != 0 ) {
        out_summary->with_goal_distance_count++;
        if ( out_summary->with_goal_distance_count == 1 ) {
            out_summary->min_goal_distance = record.distance_to_goal;
            out_summary->max_goal_distance = record.distance_to_goal;
        } else {
            out_summary->min_goal_distance = std::min( out_summary->min_goal_distance, record.distance_to_goal );
            out_summary->max_goal_distance = std::max( out_summary->max_goal_distance, record.distance_to_goal );
        }
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_CORRIDOR_DISTANCE ) != 0 ) {
        out_summary->with_corridor_distance_count++;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_PORTAL_DISTANCE ) != 0 ) {
        out_summary->with_portal_distance_count++;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_STAIR_DISTANCE ) != 0 ) {
        out_summary->with_stair_distance_count++;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_LADDER_DISTANCE ) != 0 ) {
        out_summary->with_ladder_distance_count++;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_FAT_PVS_RELEVANCE ) != 0 ) {
        out_summary->with_fat_pvs_relevance_count++;
    }

    /**
    *    Track average-friendly sums for obstacle and clearance metrics when present.
    **/
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_OBSTACLE_DISTANCE ) != 0 ) {
        *obstacle_sum += record.distance_to_obstacle_boundary;
        ( *obstacle_count )++;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_CLEARANCE_BOTTLENECK_DISTANCE ) != 0 ) {
        *clearance_sum += record.distance_to_clearance_bottleneck;
        ( *clearance_count )++;
    }
}


/**
*
*
*	Nav2 Distance Field Public API:
*
*
**/
/**
*	@brief	Keep the nav2 distance-field module represented in the build.
**/
void SVG_Nav2_DistanceFields_ModuleAnchor( void ) {
}

/**
*	@brief	Clear one distance-field set back to deterministic defaults.
*	@param	fields	[in,out] Distance-field set to clear.
**/
void SVG_Nav2_DistanceField_Clear( nav2_distance_field_set_t *fields ) {
    /**
    *    Require mutable storage before clearing the distance-field payload.
    **/
    if ( !fields ) {
        return;
    }

    /**
    *    Reset parameters, records, and lookup maps so subsequent builds start from deterministic defaults.
    **/
    fields->params = {};
    fields->records.clear();
    fields->by_span_id.clear();
}

/**
*	@brief	Build per-span distance-derived utility fields from sparse topology and connector data.
*	@param	grid	Sparse precision-layer span grid.
*	@param	connectors	Connector list providing portal/stair/ladder anchors.
*	@param	corridor	Optional corridor used for corridor-segment distance metrics.
*	@param	params	Build policy and tuning knobs.
*	@param	out_fields	[out] Distance-field set receiving generated metrics.
*	@param	out_summary	[out] Optional bounded summary counters.
*	@return	True when at least one span record was generated.
*	@note	This helper computes guidance metrics only; it must not bypass corridor correctness or connector commitments.
**/
const bool SVG_Nav2_BuildDistanceFields( const nav2_span_grid_t &grid, const nav2_connector_list_t &connectors, const nav2_corridor_t *corridor,
    const nav2_distance_field_build_params_t &params, nav2_distance_field_set_t *out_fields, nav2_distance_field_summary_t *out_summary ) {
    /**
    *    Require output storage and at least one sparse span column before starting the build.
    **/
    if ( !out_fields || SVG_Nav2_SpanGridIsEmpty( grid ) ) {
        return false;
    }

    /**
    *    Reset output storage first so partial failures cannot leak stale metrics.
    **/
    SVG_Nav2_DistanceField_Clear( out_fields );
    out_fields->params = params;
    if ( out_summary ) {
        *out_summary = {};
    }

    /**
    *    Track average accumulators for bounded summary reporting.
    **/
    double obstacleDistanceSum = 0.0;
    int32_t obstacleDistanceCount = 0;
    double clearanceDistanceSum = 0.0;
    int32_t clearanceDistanceCount = 0;

    /**
    *    Iterate sparse columns and spans in deterministic order and emit one utility record per span.
    **/
    for ( int32_t columnIndex = 0; columnIndex < ( int32_t )grid.columns.size(); columnIndex++ ) {
        const nav2_span_column_t &column = grid.columns[ ( size_t )columnIndex ];
        for ( int32_t spanIndex = 0; spanIndex < ( int32_t )column.spans.size(); spanIndex++ ) {
            const nav2_span_t &span = column.spans[ ( size_t )spanIndex ];
            if ( span.span_id < 0 || !SVG_Nav2_SpanIsValid( span ) ) {
                continue;
            }

            /**
            *    Initialize stable record identity and representative world-space origin.
            **/
            nav2_distance_field_record_t record = {};
            record.span_id = span.span_id;
            record.column_index = columnIndex;
            record.span_index = spanIndex;
            record.region_layer_id = span.region_layer_id;
            record.span_world_origin = SVG_Nav2_DistanceField_BuildSpanWorldOrigin( grid, column, span );

            /**
            *    Populate optional goal-distance metric when a query goal was supplied.
            **/
            if ( params.has_goal_origin ) {
                record.distance_to_goal = SVG_Nav2_Distance( record.span_world_origin, params.goal_origin );
                record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_GOAL_DISTANCE;
            }

            /**
            *    Populate optional corridor-distance metric from explicit corridor segment anchors.
            **/
            double corridorDistance = 0.0;
            if ( SVG_Nav2_DistanceField_FindCorridorDistance( record.span_world_origin, corridor, &corridorDistance ) ) {
                record.distance_to_corridor_segment = corridorDistance;
                record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_CORRIDOR_DISTANCE;
            }

            /**
            *    Populate connector-distance metrics for portal, stair, and ladder semantics.
            **/
            double connectorDistance = 0.0;
            int32_t connectorId = -1;
            if ( SVG_Nav2_DistanceField_FindNearestConnectorDistance( record.span_world_origin, connectors, NAV2_CONNECTOR_KIND_PORTAL, &connectorDistance, &connectorId ) ) {
                record.distance_to_portal_endpoint = connectorDistance;
                record.nearest_portal_connector_id = connectorId;
                record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_PORTAL_DISTANCE;
            }
            if ( SVG_Nav2_DistanceField_FindNearestConnectorDistance( record.span_world_origin, connectors, NAV2_CONNECTOR_KIND_STAIR_BAND, &connectorDistance, &connectorId ) ) {
                record.distance_to_stair_connector = connectorDistance;
                record.nearest_stair_connector_id = connectorId;
                record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_STAIR_DISTANCE;
            }
            if ( SVG_Nav2_DistanceField_FindNearestConnectorDistance( record.span_world_origin, connectors, NAV2_CONNECTOR_KIND_LADDER_ENDPOINT, &connectorDistance, &connectorId ) ) {
                record.distance_to_ladder_endpoint = connectorDistance;
                record.nearest_ladder_connector_id = connectorId;
                record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_LADDER_DISTANCE;
            }

            /**
            *    Populate topology-derived open-space and boundary metrics from sparse neighborhood structure.
            **/
            record.distance_to_region_boundary = SVG_Nav2_DistanceField_ComputeRegionBoundaryDistance( grid, column, span, params );
            record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_REGION_BOUNDARY_DISTANCE;
            record.distance_to_open_space_centerline = SVG_Nav2_DistanceField_ComputeCenterlineDistance( record.distance_to_region_boundary, params );
            record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_CENTERLINE_DISTANCE;

            /**
            *    Populate ledge/drop metric from existing span surface and topology hazard metadata.
            **/
            if ( ( span.surface_flags & NAV_TILE_SUMMARY_WALK_OFF ) != 0 || ( span.topology_flags & NAV_TRAVERSAL_FEATURE_LEDGE_ADJACENCY ) != 0 ) {
                record.distance_to_ledge_or_drop = 0.0;
            } else {
                record.distance_to_ledge_or_drop = std::max( grid.cell_size_xy, 1.0 );
            }
            record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_LEDGE_DISTANCE;

            /**
            *    Populate obstacle and clearance-bottleneck metrics from collision probes and span geometry.
            **/
            record.distance_to_obstacle_boundary = SVG_Nav2_DistanceField_ComputeObstacleBoundaryDistance( record.span_world_origin, params );
            record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_OBSTACLE_DISTANCE;
            record.distance_to_clearance_bottleneck = SVG_Nav2_DistanceField_ComputeClearanceBottleneckDistance( span, params );
            record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_CLEARANCE_BOTTLENECK_DISTANCE;

            /**
            *    Evaluate optional fat-PVS relevance as a tie-breaker hint only.
            **/
            bool fatPvsRelevant = false;
            if ( SVG_Nav2_DistanceField_EvaluateFatPvsRelevance( record.span_world_origin, params, &fatPvsRelevant ) && fatPvsRelevant ) {
                record.flags |= NAV2_DISTANCE_FIELD_FLAG_HAS_FAT_PVS_RELEVANCE;
            }

            /**
            *    Build deterministic tie-breaker guidance from available utility metrics.
            **/
            double tieBreaker = 0.0;
            (void)SVG_Nav2_DistanceField_BuildTieBreakerScore( record, &tieBreaker );
            record.heuristic_tie_breaker = tieBreaker;

            /**
            *    Append the new record and register stable span-id lookup.
            **/
            out_fields->by_span_id[ record.span_id ] = ( int32_t )out_fields->records.size();
            out_fields->records.push_back( record );

            /**
            *    Update bounded summary metrics when requested.
            **/
            if ( out_summary ) {
                SVG_Nav2_DistanceField_AccumulateSummary( record, out_summary, &obstacleDistanceSum, &obstacleDistanceCount, &clearanceDistanceSum, &clearanceDistanceCount );
            }
        }
    }

    /**
    *    Finalize average summary values once all records have been processed.
    **/
    if ( out_summary ) {
        out_summary->average_obstacle_distance = ( obstacleDistanceCount > 0 ) ? ( obstacleDistanceSum / ( double )obstacleDistanceCount ) : 0.0;
        out_summary->average_clearance_bottleneck_distance = ( clearanceDistanceCount > 0 ) ? ( clearanceDistanceSum / ( double )clearanceDistanceCount ) : 0.0;
    }

    /**
    *    Report success only when at least one record was generated.
    **/
    return !out_fields->records.empty();
}

/**
*	@brief	Resolve one distance-field record by stable span id.
*	@param	fields	Distance-field set to inspect.
*	@param	span_id	Stable span id to resolve.
*	@return	Pointer to the matching record, or `nullptr` when unavailable.
**/
const nav2_distance_field_record_t *SVG_Nav2_DistanceField_TryResolve( const nav2_distance_field_set_t &fields, const int32_t span_id ) {
    /**
    *    Resolve the stable lookup entry first so vector indexing remains bounds-safe.
    **/
    const auto recordIt = fields.by_span_id.find( span_id );
    if ( recordIt == fields.by_span_id.end() ) {
        return nullptr;
    }

    /**
    *    Validate the resolved index before returning record storage.
    **/
    const int32_t recordIndex = recordIt->second;
    if ( recordIndex < 0 || recordIndex >= ( int32_t )fields.records.size() ) {
        return nullptr;
    }
    return &fields.records[ ( size_t )recordIndex ];
}

/**
*	@brief	Build a compact heuristic tie-breaker score from one distance-field record.
*	@param	record	Distance-field record to evaluate.
*	@param	out_score	[out] Composite score where lower is preferred.
*	@return	True when a score was produced.
*	@note	The score is guidance-only and must not replace legality checks in coarse/fine traversal.
**/
const bool SVG_Nav2_DistanceField_BuildTieBreakerScore( const nav2_distance_field_record_t &record, double *out_score ) {
    /**
    *    Require output storage before composing weighted tie-breaker guidance.
    **/
    if ( !out_score ) {
        return false;
    }

    /**
    *    Start at zero and add weighted metric contributions only when each metric is available.
    **/
    double score = 0.0;
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_GOAL_DISTANCE ) != 0 ) {
        score += record.distance_to_goal * 0.60;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_CORRIDOR_DISTANCE ) != 0 ) {
        score += record.distance_to_corridor_segment * 0.20;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_PORTAL_DISTANCE ) != 0 ) {
        score += record.distance_to_portal_endpoint * 0.08;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_STAIR_DISTANCE ) != 0 ) {
        score += record.distance_to_stair_connector * 0.04;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_LADDER_DISTANCE ) != 0 ) {
        score += record.distance_to_ladder_endpoint * 0.04;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_OBSTACLE_DISTANCE ) != 0 ) {
        score += record.distance_to_obstacle_boundary * 0.02;
    }
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_CLEARANCE_BOTTLENECK_DISTANCE ) != 0 ) {
        score += record.distance_to_clearance_bottleneck * 0.02;
    }

    /**
    *    Apply optional fat-PVS relevance as a small negative bias so relevant records are preferred on ties.
    **/
    if ( ( record.flags & NAV2_DISTANCE_FIELD_FLAG_HAS_FAT_PVS_RELEVANCE ) != 0 ) {
        score -= 8.0;
    }

    /**
    *    Publish the final tie-breaker score.
    **/
    *out_score = score;
    return true;
}

/**
*	@brief	Emit a bounded debug summary for one built distance-field set.
*	@param	fields	Distance-field set to summarize.
*	@param	summary	Optional precomputed summary to print.
*	@param	limit	Maximum number of per-record lines to emit.
**/
void SVG_Nav2_DebugPrintDistanceFields( const nav2_distance_field_set_t &fields, const nav2_distance_field_summary_t *summary, const int32_t limit ) {
    /**
    *    Emit one compact summary line first so callers can inspect high-level build coverage quickly.
    **/
    if ( summary ) {
        gi.dprintf( "[nav2][distance_fields] records=%d goal=%d corridor=%d portal=%d stair=%d ladder=%d fatpvs=%d goal_min=%.2f goal_max=%.2f obstacle_avg=%.2f bottleneck_avg=%.2f\n",
            summary->record_count,
            summary->with_goal_distance_count,
            summary->with_corridor_distance_count,
            summary->with_portal_distance_count,
            summary->with_stair_distance_count,
            summary->with_ladder_distance_count,
            summary->with_fat_pvs_relevance_count,
            summary->min_goal_distance,
            summary->max_goal_distance,
            summary->average_obstacle_distance,
            summary->average_clearance_bottleneck_distance );
    } else {
        gi.dprintf( "[nav2][distance_fields] records=%d\n", ( int32_t )fields.records.size() );
    }

    /**
    *    Keep per-record diagnostics bounded to avoid log spam during repeated validation calls.
    **/
    const int32_t boundedLimit = std::max( 0, limit );
    for ( int32_t recordIndex = 0; recordIndex < ( int32_t )fields.records.size() && recordIndex < boundedLimit; recordIndex++ ) {
        const nav2_distance_field_record_t &record = fields.records[ ( size_t )recordIndex ];
        gi.dprintf( "  [nav2][distance_fields][%d] span=%d region=%d tie=%.2f goal=%.2f corridor=%.2f portal=%.2f stair=%.2f ladder=%.2f boundary=%.2f ledge=%.2f centerline=%.2f obstacle=%.2f bottleneck=%.2f flags=0x%08x\n",
            recordIndex,
            record.span_id,
            record.region_layer_id,
            record.heuristic_tie_breaker,
            record.distance_to_goal,
            record.distance_to_corridor_segment,
            record.distance_to_portal_endpoint,
            record.distance_to_stair_connector,
            record.distance_to_ladder_endpoint,
            record.distance_to_region_boundary,
            record.distance_to_ledge_or_drop,
            record.distance_to_open_space_centerline,
            record.distance_to_obstacle_boundary,
            record.distance_to_clearance_bottleneck,
            record.flags );
    }
}
