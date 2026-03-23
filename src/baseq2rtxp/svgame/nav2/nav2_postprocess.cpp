/********************************************************************
*
*
*	ServerGame: Nav2 Path Postprocess and LOS Shortcuts - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_postprocess.h"

#include <algorithm>
#include <cmath>
#include <limits>


/**
*
*
*	Nav2 Postprocess Local Helpers:
*
*
**/
/**
* @brief	Resolve one fine-search node by stable node id.
* @param	fine_state	Fine-search state to inspect.
* @param	node_id	Stable node id to resolve.
* @return	Pointer to the resolved node, or nullptr when missing.
**/
static const nav2_fine_astar_node_t *SVG_Nav2_Postprocess_FindFineNodeById( const nav2_fine_astar_state_t &fine_state, const int32_t node_id ) {
    /**
    *    Prefer stable id-to-index map lookup first for deterministic access.
    **/
    const auto nodeIt = fine_state.node_id_to_index.find( node_id );
    if ( nodeIt != fine_state.node_id_to_index.end() ) {
        const int32_t nodeIndex = nodeIt->second;
        if ( nodeIndex >= 0 && nodeIndex < ( int32_t )fine_state.nodes.size() ) {
            return &fine_state.nodes[ ( size_t )nodeIndex ];
        }
    }

    /**
    *    Fall back to linear scan only when map lookup fails.
    **/
    for ( const nav2_fine_astar_node_t &node : fine_state.nodes ) {
        if ( node.node_id == node_id ) {
            return &node;
        }
    }
    return nullptr;
}

/**
* @brief	Resolve one world-space waypoint for a fine-search node.
* @param	fine_state	Fine-search state containing corridor anchors.
* @param	node	Fine-search node whose waypoint should be resolved.
* @param	out_world	[out] Resolved world-space waypoint.
* @return	True when waypoint resolution succeeded.
**/
static const bool SVG_Nav2_Postprocess_ResolveNodeWaypoint( const nav2_fine_astar_state_t &fine_state,
    const nav2_fine_astar_node_t &node, Vector3 *out_world ) {
    /**
    *    Require output storage.
    **/
    if ( !out_world ) {
        return false;
    }
    *out_world = {};

    /**
    *    Reject when corridor segments are unavailable.
    **/
    if ( fine_state.corridor.segments.empty() ) {
        return false;
    }

    /**
    *    Select best matching corridor segment from node topology hints.
    **/
    const nav2_corridor_segment_t *bestSegment = nullptr;
    int32_t bestScore = std::numeric_limits<int32_t>::max();
    for ( const nav2_corridor_segment_t &segment : fine_state.corridor.segments ) {
        int32_t score = 0;

        // Prefer explicit region matches when both sides have region ids.
        if ( node.topology.region_id != NAV_REGION_ID_NONE && segment.topology.region_id != NAV_REGION_ID_NONE
            && node.topology.region_id != segment.topology.region_id ) {
            score += 6;
        }

        // Prefer area and cluster continuity where available.
        if ( node.topology.area_id >= 0 && segment.topology.area_id >= 0 && node.topology.area_id != segment.topology.area_id ) {
            score += 3;
        }
        if ( node.topology.cluster_id >= 0 && segment.topology.cluster_id >= 0 && node.topology.cluster_id != segment.topology.cluster_id ) {
            score += 2;
        }

        // Prefer connector match when node carries one.
        if ( node.connector_id >= 0 && segment.topology.portal_id >= 0 && node.connector_id != segment.topology.portal_id ) {
            score += 2;
        }

        if ( !bestSegment || score < bestScore ) {
            bestSegment = &segment;
            bestScore = score;
        }
    }

    /**
    *    Resolve waypoint to segment midpoint for deterministic path output.
    **/
    if ( !bestSegment ) {
        return false;
    }
    out_world->x = ( bestSegment->start_anchor.x + bestSegment->end_anchor.x ) * 0.5f;
    out_world->y = ( bestSegment->start_anchor.y + bestSegment->end_anchor.y ) * 0.5f;
    out_world->z = ( bestSegment->start_anchor.z + bestSegment->end_anchor.z ) * 0.5f;
    return true;
}

/**
* @brief	Return whether one point is in hazardous contents.
* @param	point	World-space point to test.
* @return	True when the point is in lava or slime contents.
**/
static const bool SVG_Nav2_Postprocess_PointIsHazardous( const Vector3 &point ) {
    /**
    *    Fall back to non-hazard when pointcontents import is unavailable.
    **/
    if ( !gi.pointcontents ) {
        return false;
    }

    /**
    *    Query point contents and reject lava/slime transitions.
    **/
    const cm_contents_t contents = gi.pointcontents( &point );
 const bool hasLava = ( contents & CONTENTS_LAVA ) != 0;
    const bool hasSlime = ( contents & CONTENTS_SLIME ) != 0;
    return hasLava || hasSlime;
}

/**
* @brief	Run LOS legality checks between two waypoints.
* @param	start	Start world-space point.
* @param	end	End world-space point.
* @param	policy	Postprocess policy controls.
* @param	out_trace_reject	[out] True when rejection came from trace blockage.
* @param	out_contents_reject	[out] True when rejection came from hazardous contents.
* @return	True when LOS shortcut is legal.
**/
static const bool SVG_Nav2_Postprocess_CheckLOS( const Vector3 &start, const Vector3 &end,
    const nav2_postprocess_policy_t &policy, bool *out_trace_reject, bool *out_contents_reject ) {
    /**
    *    Reset optional rejection outputs.
    **/
    if ( out_trace_reject ) {
        *out_trace_reject = false;
    }
    if ( out_contents_reject ) {
        *out_contents_reject = false;
    }

    /**
    *    Build conservative hull extents used by trace and box-content checks.
    **/
    const float clearance = ( float )std::max( 0.0, policy.los_clearance_radius );
    const Vector3 mins = { -clearance, -clearance, -4.0f };
    const Vector3 maxs = { clearance, clearance, 4.0f };

    /**
    *    Reject LOS when world trace reports collision.
    **/
    if ( gi.trace ) {
      const cm_trace_t tr = gi.trace( &start, &mins, &maxs, &end, nullptr, CM_CONTENTMASK_PLAYERSOLID );
        if ( tr.allsolid || tr.startsolid || tr.fraction < 0.999f ) {
            if ( out_trace_reject ) {
                *out_trace_reject = true;
            }
            return false;
        }
    }

    /**
    *    Reject LOS when box-contents checks detect explicit hazard flags.
    **/
    if ( policy.reject_hazardous_contents && gi.CM_BoxContents ) {
        cm_contents_t sampledContents = CONTENTS_NONE;
        mleaf_t *leafList[ 8 ] = { nullptr };
        mnode_t *topnode = nullptr;
     const Vector3 boxMins = start + mins;
        const Vector3 boxMaxs = start + maxs;
       const int32_t count = gi.CM_BoxContents( ( const vec_t * )&boxMins.x, ( const vec_t * )&boxMaxs.x, &sampledContents, leafList, 8, &topnode );
        if ( count > 0 ) {
          const bool hasLava = ( sampledContents & CONTENTS_LAVA ) != 0;
            const bool hasSlime = ( sampledContents & CONTENTS_SLIME ) != 0;
            if ( hasLava || hasSlime ) {
                if ( out_contents_reject ) {
                    *out_contents_reject = true;
                }
                return false;
            }
        }
    }

    /**
    *    Reject LOS when endpoint pointcontents checks detect hazards.
    **/
    if ( policy.reject_hazardous_contents && ( SVG_Nav2_Postprocess_PointIsHazardous( start ) || SVG_Nav2_Postprocess_PointIsHazardous( end ) ) ) {
        if ( out_contents_reject ) {
            *out_contents_reject = true;
        }
        return false;
    }

    return true;
}


/**
*
*
*	Nav2 Postprocess Public API:
*
*
**/
/**
* @brief	Build world-space waypoint candidates from a fine-search path state.
* @param	fine_state	Fine-search state supplying stable node ids and metadata.
* @param	max_points	Maximum points to emit.
* @param	out_points	[out] World-space point list built from fine-search nodes.
* @return	True when waypoint extraction succeeded.
**/
const bool SVG_Nav2_Postprocess_ExtractFineWaypoints( const nav2_fine_astar_state_t &fine_state, const int32_t max_points, std::vector<Vector3> *out_points ) {
    /**
    *    Require output storage and at least one path node.
    **/
    if ( !out_points || fine_state.path.node_ids.empty() ) {
        return false;
    }
    out_points->clear();

    /**
    *    Clamp output count so extraction remains bounded.
    **/
    const int32_t maxPoints = std::max( 0, std::min( max_points, 1024 ) );

    /**
    *    Resolve node waypoints in path order.
    **/
    for ( const int32_t nodeId : fine_state.path.node_ids ) {
        // Stop when extraction reached caller cap.
        if ( maxPoints > 0 && ( int32_t )out_points->size() >= maxPoints ) {
            break;
        }

        // Resolve fine node metadata for waypoint conversion.
        const nav2_fine_astar_node_t *node = SVG_Nav2_Postprocess_FindFineNodeById( fine_state, nodeId );
        if ( !node ) {
            continue;
        }

        // Resolve world-space waypoint and append when available.
        Vector3 worldPoint = {};
        if ( !SVG_Nav2_Postprocess_ResolveNodeWaypoint( fine_state, *node, &worldPoint ) ) {
            continue;
        }
        out_points->push_back( worldPoint );
    }

    return !out_points->empty();
}

/**
* @brief	Apply bounded LOS and smoothing postprocess to a waypoint path.
* @param	mesh	Optional nav2 query mesh context for future topology-aware checks.
* @param	policy	Postprocess policy controls.
* @param	input_waypoints	Input world-space waypoints.
* @param	out_result	[out] Smoothed waypoints and diagnostics.
* @return	True when postprocess completed.
**/
const bool SVG_Nav2_Postprocess_Path( const nav2_query_mesh_t *mesh, const nav2_postprocess_policy_t &policy,
    const std::vector<Vector3> &input_waypoints, nav2_postprocess_result_t *out_result ) {
    /**
    *    Currently unused mesh parameter reserved for future topology-aware postprocess checks.
    **/
    (void)mesh;

    /**
    *    Require output storage and at least one input waypoint.
    **/
    if ( !out_result || input_waypoints.empty() ) {
        return false;
    }
    *out_result = {};
    out_result->diagnostics.input_waypoints = ( uint32_t )input_waypoints.size();

    /**
    *    Clamp waypoint window for bounded processing.
    **/
    const int32_t cappedMax = std::max( 1, std::min( policy.max_waypoints, ( int32_t )input_waypoints.size() ) );
    std::vector<Vector3> clippedInput = {};
    clippedInput.reserve( ( size_t )cappedMax );
    for ( int32_t i = 0; i < cappedMax; i++ ) {
        clippedInput.push_back( input_waypoints[ ( size_t )i ] );
    }

    /**
    *    Skip LOS reduction when disabled or waypoint count is trivial.
    **/
    if ( !policy.enable_los_shortcuts || clippedInput.size() <= 2 ) {
        out_result->waypoints = clippedInput;
        out_result->diagnostics.output_waypoints = ( uint32_t )out_result->waypoints.size();
        out_result->success = true;
        return true;
    }

    /**
    *    Run bounded LOS-based waypoint reduction.
    **/
    out_result->waypoints.clear();
    out_result->waypoints.push_back( clippedInput.front() );
    int32_t anchorIndex = 0;
    while ( anchorIndex < ( int32_t )clippedInput.size() - 1 ) {
        // Track farthest legal shortcut candidate from current anchor.
        int32_t bestReachableIndex = anchorIndex + 1;
        for ( int32_t candidateIndex = anchorIndex + 2; candidateIndex < ( int32_t )clippedInput.size(); candidateIndex++ ) {
            // Respect endpoint-preservation semantics when requested.
            if ( policy.preserve_endpoints && candidateIndex != ( ( int32_t )clippedInput.size() - 1 ) ) {
                // Keep considering candidates; endpoint preservation handled by final append behavior.
            }

           /**
            *    Reject semantically incompatible shortcuts that skip large vertical changes when corridor Z-band guards are enabled.
            **/
            if ( policy.enforce_corridor_z_bands ) {
                const double verticalDelta = std::fabs( ( double )clippedInput[ ( size_t )candidateIndex ].z - ( double )clippedInput[ ( size_t )anchorIndex ].z );
                if ( verticalDelta > 192.0 ) {
                    out_result->diagnostics.semantics_rejects++;
                    continue;
                }
            }

            // Attempt LOS shortcut between anchor and candidate.
            out_result->diagnostics.los_attempts++;
            bool traceReject = false;
            bool contentsReject = false;
            if ( !SVG_Nav2_Postprocess_CheckLOS( clippedInput[ ( size_t )anchorIndex ], clippedInput[ ( size_t )candidateIndex ],
                policy, &traceReject, &contentsReject ) ) {
                if ( traceReject ) {
                    out_result->diagnostics.trace_rejects++;
                }
                if ( contentsReject ) {
                    out_result->diagnostics.contents_rejects++;
                }
                continue;
            }

            // Accept candidate as current best reachable waypoint.
            bestReachableIndex = candidateIndex;
            out_result->diagnostics.los_accepts++;
        }

        // Append the farthest legal candidate and continue.
        out_result->waypoints.push_back( clippedInput[ ( size_t )bestReachableIndex ] );
        anchorIndex = bestReachableIndex;
    }

    /**
    *    Publish final diagnostics and success state.
    **/
    out_result->diagnostics.output_waypoints = ( uint32_t )out_result->waypoints.size();
    out_result->success = !out_result->waypoints.empty();
    return out_result->success;
}

/**
* @brief	Run postprocess directly on a fine-search state.
* @param	mesh	Optional nav2 query mesh context for future topology-aware checks.
* @param	fine_state	Fine-search state supplying path and node metadata.
* @param	policy	Postprocess policy controls.
* @param	out_result	[out] Smoothed waypoints and diagnostics.
* @return	True when postprocess completed.
**/
const bool SVG_Nav2_Postprocess_FinePath( const nav2_query_mesh_t *mesh, const nav2_fine_astar_state_t &fine_state,
    const nav2_postprocess_policy_t &policy, nav2_postprocess_result_t *out_result ) {
    /**
    *    Require output storage.
    **/
    if ( !out_result ) {
        return false;
    }
    *out_result = {};

    /**
    *    Extract intermediate world waypoints from fine-search node path.
    **/
    std::vector<Vector3> extractedWaypoints = {};
    if ( !SVG_Nav2_Postprocess_ExtractFineWaypoints( fine_state, policy.max_waypoints, &extractedWaypoints ) ) {
        return false;
    }

    /**
    *    Run bounded LOS and smoothing on extracted waypoints.
    **/
    return SVG_Nav2_Postprocess_Path( mesh, policy, extractedWaypoints, out_result );
}

/**
* @brief	Emit bounded debug diagnostics for postprocess output.
* @param	result	Postprocess result to inspect.
* @param	max_print	Maximum waypoint lines to print.
**/
void SVG_Nav2_DebugPrintPostprocess( const nav2_postprocess_result_t &result, const int32_t max_print ) {
    /**
    *    Clamp print count to bounded output.
    **/
    const int32_t printCount = std::max( 0, std::min( max_print, ( int32_t )result.waypoints.size() ) );

    /**
    *    Emit one compact summary line.
    **/
    gi.dprintf( "[NAV2][Postprocess] success=%d input=%u output=%u los=(attempt=%u accept=%u) reject=(trace=%u contents=%u semantics=%u)\n",
        result.success ? 1 : 0,
        result.diagnostics.input_waypoints,
        result.diagnostics.output_waypoints,
        result.diagnostics.los_attempts,
        result.diagnostics.los_accepts,
        result.diagnostics.trace_rejects,
        result.diagnostics.contents_rejects,
        result.diagnostics.semantics_rejects );

    /**
    *    Emit bounded waypoint details.
    **/
    for ( int32_t i = 0; i < printCount; i++ ) {
        const Vector3 &waypoint = result.waypoints[ ( size_t )i ];
        gi.dprintf( "[NAV2][Postprocess] #%d waypoint=(%.1f %.1f %.1f)\n", i, waypoint.x, waypoint.y, waypoint.z );
    }
}

/**
* @brief	Keep the nav2 postprocess module represented in the build.
**/
void SVG_Nav2_Postprocess_ModuleAnchor( void ) {
}
