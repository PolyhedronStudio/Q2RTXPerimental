/********************************************************************
*
*
*	ServerGame: Nav2 Debug Draw Visualization - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_debug_draw.h"
#include "svgame/svg_utils.h"

#include <algorithm>

static void SVG_Nav2_DebugDrawCrossMarker( const Vector3 &origin, const multicast_t multicast_type, const float half_extent = 6.0f );
static void SVG_Nav2_DebugDrawSnapshotFlag( const Vector3 &base, const Vector3 &offset, const float height, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawSerializationCompatibilityOverlay( const nav2_query_job_t &job,
    const nav2_snapshot_staleness_t &staleness, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawSegmentTypeMarker( const nav2_corridor_segment_t &segment, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawSegmentEndpointMarker( const nav2_corridor_segment_t &segment, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawSegmentPolicyMarker( const nav2_corridor_segment_t &segment, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawPortalTypeOverlay( const Vector3 &mid, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawLadderTypeOverlay( const Vector3 &mid, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawStairTypeOverlay( const Vector3 &mid, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawMoverTypeOverlay( const Vector3 &mid, const multicast_t multicast_type );
static const char *SVG_Nav2_QueryStageOverlayName( const nav2_query_stage_t stage );
static void SVG_Nav2_DebugDrawStageOverlay( const nav2_query_job_t &job, const multicast_t multicast_type );
static void SVG_Nav2_DebugDrawSpanMarker( const nav2_span_t &span, const Vector3 &column_center, const multicast_t multicast_type );


/**
*	@brief    Return a stable text label for one goal-candidate semantic type.
*	@param    type    Goal-candidate type to stringify.
*	@return    Stable human-readable label for debug output.
**/
static const char *SVG_Nav2_GoalCandidateTypeName( const nav2_goal_candidate_type_t type ) {
	/**
	*	Translate the stable enum into a compact debug label.
	**/
	switch ( type ) {
	case nav2_goal_candidate_type_t::RawGoal:
		return "RawGoal";
	case nav2_goal_candidate_type_t::ProjectedSameCell:
		return "ProjectedSameCell";
	case nav2_goal_candidate_type_t::NearbySupport:
		return "NearbySupport";
	case nav2_goal_candidate_type_t::SameLeafHint:
		return "SameLeafHint";
	case nav2_goal_candidate_type_t::SameClusterHint:
		return "SameClusterHint";
	case nav2_goal_candidate_type_t::PortalEndpoint:
		return "PortalEndpoint";
	case nav2_goal_candidate_type_t::LadderEndpoint:
		return "LadderEndpoint";
	case nav2_goal_candidate_type_t::StairBand:
		return "StairBand";
	case nav2_goal_candidate_type_t::MoverBoarding:
		return "MoverBoarding";
	case nav2_goal_candidate_type_t::MoverExit:
		return "MoverExit";
	case nav2_goal_candidate_type_t::LegacyProjectedFallback:
		return "LegacyProjectedFallback";
	default:
		return "None";
	}
}	
/**
*	@brief    Return a stable label describing serialization compatibility for the consumed query snapshot.
*	@param    staleness    Snapshot comparison result to classify.
*	@return    Human-readable compatibility label used by live query diagnostics.
**/
static const char *SVG_Nav2_SerializationCompatibilityName( const nav2_snapshot_staleness_t &staleness ) {
    /**
    * Report hard incompatibility first so logs explain when the query must be resubmitted from scratch.
    **/
    if ( staleness.requires_resubmit ) {
        return "Incompatible";
    }

    /**
    * Report restart-level incompatibility next when dynamic publications changed enough to invalidate current stage work.
    **/
    if ( staleness.requires_restart ) {
        return "RestartRequired";
    }

    /**
    * Report soft compatibility drift when the result can still be revalidated on the main thread.
    **/
    if ( staleness.requires_revalidation ) {
        return "Revalidate";
    }

    /**
    * When no mismatch exists, the consumed snapshot remains serialization-compatible with the latest publication.
    **/
    if ( staleness.is_current ) {
        return "Compatible";
    }

    /**
    * Fallback label for conservative stale states that did not map onto explicit restart/resubmit flags.
    **/
    return "Stale";
}

/**
*	@brief    Return a stable text label for one snapshot staleness condition.
*	@param    staleness    Snapshot comparison result to stringify.
*	@return    Human-readable summary for debug output.
**/
static const char *SVG_Nav2_SnapshotStalenessName( const nav2_snapshot_staleness_t &staleness ) {
    /**
    *	Prefer the strongest invalidation reason first so the label explains why the job needs further work.
    **/
    if ( staleness.requires_resubmit ) {
        return "Resubmit";
    }
    if ( staleness.requires_restart ) {
        return "Restart";
    }
    if ( staleness.requires_revalidation ) {
        return "Revalidate";
    }
    if ( staleness.is_current ) {
        return "Current";
    }
    return "Stale";
}

/**
*	@brief    Count the number of changed snapshot domains for concise diagnostics.
*	@param    staleness    Snapshot comparison result to summarize.
*	@return    Number of changed domains that contributed to the decision.
**/
static int32_t SVG_Nav2_CountSnapshotChanges( const nav2_snapshot_staleness_t &staleness ) {
    int32_t changedCount = 0;
    if ( staleness.static_nav_changed ) {
        changedCount++;
    }
    if ( staleness.occupancy_changed ) {
        changedCount++;
    }
    if ( staleness.mover_changed ) {
        changedCount++;
    }
    if ( staleness.connector_changed ) {
        changedCount++;
    }
    if ( staleness.model_changed ) {
        changedCount++;
    }
    return changedCount;
}

/**
*	@brief    Return a stable text label for one query stage.
*	@param    stage    Stage to stringify.
*	@return    Human-readable stage label for debug output.
**/
static const char *SVG_Nav2_QueryStageName( const nav2_query_stage_t stage ) {
    return SVG_Nav2_Budget_StageName( stage );
}

/**
*	@brief    Return a stable text label for one query result state.
*	@param    status    Result state to stringify.
*	@return    Human-readable result label for debug output.
**/
static const char *SVG_Nav2_QueryResultStatusName( const nav2_query_result_status_t status ) {
    switch ( status ) {
    case nav2_query_result_status_t::Pending:
        return "Pending";
    case nav2_query_result_status_t::Partial:
        return "Partial";
    case nav2_query_result_status_t::Success:
        return "Success";
    case nav2_query_result_status_t::Failed:
        return "Failed";
    case nav2_query_result_status_t::Stale:
        return "Stale";
    case nav2_query_result_status_t::Cancelled:
        return "Cancelled";
    default:
        return "None";
    }
}

/**
*	@brief		Return a stable text label for one goal-candidate rejection reason.
*	@param		reason    Rejection category to stringify.
*	@return	Stable human-readable label for debug output.
**/
static const char *SVG_Nav2_GoalCandidateRejectionName( const nav2_goal_candidate_rejection_t reason ) {
    /**
    *	Translate the stable enum into a compact debug label.
    **/
    switch ( reason ) {
    case nav2_goal_candidate_rejection_t::MissingMesh:
        return "MissingMesh";
    case nav2_goal_candidate_rejection_t::InvalidAgentBounds:
        return "InvalidAgentBounds";
    case nav2_goal_candidate_rejection_t::MissingCollisionModel:
        return "MissingCollisionModel";
    case nav2_goal_candidate_rejection_t::MissingWorldTile:
        return "MissingWorldTile";
    case nav2_goal_candidate_rejection_t::MissingCellLayers:
        return "MissingCellLayers";
    case nav2_goal_candidate_rejection_t::SolidPointContents:
        return "SolidPointContents";
    case nav2_goal_candidate_rejection_t::SolidBoxContents:
        return "SolidBoxContents";
    case nav2_goal_candidate_rejection_t::DuplicateCandidate:
        return "DuplicateCandidate";
    case nav2_goal_candidate_rejection_t::CandidateLimitReached:
        return "CandidateLimitReached";
    default:
        return "None";
    }
}

/**
*	@brief	Emit a bounded debug summary for one nav2 query job and its snapshot state.
*	@param	jobjob    Query job to report.
*	@param	latest_snapshot    Latest published snapshot state to compare against.
*	@note	This helper remains allocation-free and intentionally reports only compact stage/snapshot state.
**/
void SVG_Nav2_DebugDrawQueryJob( const nav2_query_job_t &job, const nav2_snapshot_runtime_t &latest_snapshot ) {
    /**
    *	Compare the query's consumed snapshot against the latest published state so we can explain whether
    *	the job needs revalidation, restart, or a full resubmit.
    **/
    const nav2_snapshot_staleness_t staleness = SVG_Nav2_Snapshot_Compare( &latest_snapshot, job.state.snapshot );

    /**
    *	Emit a single compact header that summarizes the job, its stage, and its freshness state.
    **/
    gi.dprintf( "[NAV2][DebugDraw][Job] job=%llu request=%llu agent=%d goal=%d priority=%s stage=%s result=%s slice=(%.2fms/%u) queue=%.2fms exec=%.2fms revalidate=%.2fms restarts=%u snapshot=%s changed=%d bench=%s stale=%d/%d/%d/%d/%d\n",
        ( unsigned long long )job.job_id,
        ( unsigned long long )job.request.request_id,
        job.request.agent_entnum,
        job.request.goal_entnum,
        SVG_Nav2_Budget_PriorityName( job.request.priority ),
        SVG_Nav2_QueryStageName( job.state.stage ),
        SVG_Nav2_QueryResultStatusName( job.state.result_status ),
        job.state.active_slice.granted_ms,
        job.state.active_slice.granted_expansions,
        job.queue_wait_ms,
        job.execution_time_ms,
        job.revalidation_time_ms,
        job.restart_count,
        SVG_Nav2_SnapshotStalenessName( staleness ),
        SVG_Nav2_CountSnapshotChanges( staleness ),
        SVG_Nav2_Bench_ScenarioName( job.request.bench_scenario ),
        staleness.static_nav_changed ? 1 : 0,
        staleness.occupancy_changed ? 1 : 0,
        staleness.mover_changed ? 1 : 0,
        staleness.connector_changed ? 1 : 0,
        staleness.model_changed ? 1 : 0 );

    /**
    * Emit one compatibility trace line when snapshot domains diverged so serialization/version drift is visible in live query traces.
    **/
    if ( SVG_Nav2_CountSnapshotChanges( staleness ) > 0 || staleness.requires_revalidation || staleness.requires_restart || staleness.requires_resubmit ) {
        gi.dprintf( "[NAV2][DebugDraw][Job] snapshot_versions consumed=(%u/%u/%u/%u/%u) current=(%u/%u/%u/%u/%u) compat=%s\n",
            job.state.snapshot.static_nav_version,
            job.state.snapshot.occupancy_version,
            job.state.snapshot.mover_version,
            job.state.snapshot.connector_version,
            job.state.snapshot.model_version,
            latest_snapshot.current.static_nav_version,
            latest_snapshot.current.occupancy_version,
            latest_snapshot.current.mover_version,
            latest_snapshot.current.connector_version,
            latest_snapshot.current.model_version,
            SVG_Nav2_SerializationCompatibilityName( staleness ) );
    }

    /**
    *	Emit a second line only when the query is carrying a provisional or terminal state that may matter
    *	for live debugging.
    **/
    if ( job.state.has_provisional_result || job.state.requires_revalidation || job.state.cancel_requested || job.state.restart_requested ) {
        gi.dprintf( "[NAV2][DebugDraw][Job] provisional=%d revalidate=%d cancel=%d restart=%d worker_in_flight=%d priority_bias=%s\n",
            job.state.has_provisional_result ? 1 : 0,
            job.state.requires_revalidation ? 1 : 0,
            job.state.cancel_requested ? 1 : 0,
            job.state.restart_requested ? 1 : 0,
            job.worker_in_flight ? 1 : 0,
            staleness.requires_resubmit ? "resubmit" : ( staleness.requires_restart ? "restart" : ( staleness.requires_revalidation ? "revalidate" : "current" ) ) );
    }

    /**
    *    Overlay: highlight the requested corridor and attach any snapshot-state indicators so the job-only view stays actionable.
    **/
    SVG_Nav2_DebugDrawJobSchedulerOverlay( job );
    SVG_Nav2_DebugDrawQuerySnapshotOverlay( job, staleness );
}

/**
*	@brief    Return a stable text label for one corridor-segment policy state.
*	@param    segment    Corridor segment to classify for debug output.
*	@return    Human-readable policy state label.
**/
static const char *SVG_Nav2_CorridorSegmentPolicyStatusName( const nav2_corridor_segment_t &segment ) {
    /**
    *	Prefer the most restrictive state first so the debug label explains why the segment may be skipped.
    **/
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ) {
        return "WouldReject";
    }
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) != 0 ) {
        return "SoftPenalized";
    }
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE ) != 0 ) {
        return "Compatible";
    }
    return "Pending";
}

/**
*	@brief    Count corridor segment policy states for the high-level debug summary.
*	@param    corridor    Corridor to analyze.
*	@param    out_compatible    [out] Number of compatible segments.
*	@param    out_soft_penalized    [out] Number of soft-penalized segments.
*	@param    out_would_reject    [out] Number of would-reject segments.
**/
static void SVG_Nav2_CountCorridorSegmentPolicyStates( const nav2_corridor_t &corridor, int32_t *out_compatible,
    int32_t *out_soft_penalized, int32_t *out_would_reject ) {
    /**
    *	Start from zero so the caller always gets a clean tally.
    **/
    if ( out_compatible ) {
        *out_compatible = 0;
    }
    if ( out_soft_penalized ) {
        *out_soft_penalized = 0;
    }
    if ( out_would_reject ) {
        *out_would_reject = 0;
    }

    /**
    *	Walk the segment list once because the debug summary needs only simple state counts.
    **/
    for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
        if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ) {
            if ( out_would_reject ) {
                ++( *out_would_reject );
            }
            continue;
        }
        if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) != 0 ) {
            if ( out_soft_penalized ) {
                ++( *out_soft_penalized );
            }
            continue;
        }
        if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE ) != 0 ) {
            if ( out_compatible ) {
                ++( *out_compatible );
            }
        }
    }
}


/**
*	@brief	Render or print a concise debug summary for the currently selected nav2 goal candidates.
*	@param	candidates	Candidate set to report.
*	@param	selected_candidate	Optional selected candidate to highlight.
*	@note	This helper stays allocation-free and only emits compact diagnostics when called explicitly.
**/
void SVG_Nav2_DebugDrawGoalCandidates( const nav2_goal_candidate_list_t &candidates, const nav2_goal_candidate_t *selected_candidate ) {
   /**
    *	Delegate to the existing candidate printer so the initial Task 3.3 visualization seam stays low-risk and contract-safe.
    **/
    SVG_Nav2_DebugPrintGoalCandidates( candidates, selected_candidate );
}

/**
*	@brief	Return the explicit segment output limit for the requested debug verbosity level.
*	@details	When the caller requests segment-level diagnostics, this helper enforces the explicit segment output limit so the Task 3.3 visualization seam remains useful during live debugging without log spam. When the caller requests summary-level diagnostics, this helper suppresses all explicit segment output so the Task 3.3 visualization seam remains useful during live debugging without log spam.
*	@note	This helper is intentionally separate from the main debug draw function so the verbosity policy stays explainable and maintainable without being tangled up in the main corridor reporting logic.
**/
static int32_t SVG_Nav2_DebugDrawGetCorridorSegmentLimit( const nav2_corridor_t &corridor, const nav2_debug_corridor_verbosity_t verbosity, const int32_t max_segments ) {
	/**
	*	Enforce the explicit segment output limit when the caller requested segment-level diagnostics so the Task 3.3 visualization seam remains useful during live debugging.
	**/
	if ( verbosity == nav2_debug_corridor_verbosity_t::IncludeSegments ) {
		return std::min( max_segments, ( int32_t )corridor.segments.size() );
	}
	/**
	*	Otherwise, suppress all explicit segment output when the caller only requested summary-level diagnostics so the Task 3.3 visualization seam remains useful during live debugging without log spam.
	**/
	return 0;
}

/**
*	@brief	Render or print a concise debug summary for the currently published nav2 corridor commitments.
*	@param	corridor	Corridor to report.
*	@param	verbosity	Requested debug verbosity for corridor output.
*	@param	max_segments	Maximum number of explicit segments to emit when segment output is enabled.
*	@note	This helper stays allocation-free and only emits compact diagnostics when called explicitly.
**/
void SVG_Nav2_DebugDrawCorridor( const nav2_corridor_t &corridor,
    const nav2_debug_corridor_verbosity_t verbosity, const int32_t max_segments ) {
    /**
    *	Emit a compact summary first so callers always see the corridor-wide commitment state before any optional per-segment detail.
    **/
    int32_t compatibleSegments = 0;
    int32_t softPenalizedSegments = 0;
    int32_t rejectedSegments = 0;
    SVG_Nav2_CountCorridorSegmentPolicyStates( corridor, &compatibleSegments, &softPenalizedSegments, &rejectedSegments );

    gi.dprintf( "[NAV2][DebugDraw][Corridor] source=%s flags=0x%08x segments=%d compat=%d penalized=%d rejected=%d regions=%d portals=%d tiles=%d mismatches=%d penalty=%.1f z=[%.1f %.1f] pref=%.1f\n",
        SVG_Nav2_CorridorSourceName( corridor.source ),
        corridor.flags,
        ( int32_t )corridor.segments.size(),
        compatibleSegments,
        softPenalizedSegments,
        rejectedSegments,
        ( int32_t )corridor.region_path.size(),
        ( int32_t )corridor.portal_path.size(),
        ( int32_t )corridor.exact_tile_route.size(),
        corridor.policy_mismatch_count,
        corridor.policy_penalty_total,
        corridor.global_z_band.min_z,
        corridor.global_z_band.max_z,
        corridor.global_z_band.preferred_z );

    /**
    * Emit one refinement-policy summary line so policy membership diagnostics can be read without dumping every segment.
    **/
    gi.dprintf( "[NAV2][DebugDraw][Corridor] refine_policy flags=0x%08x topology_penalty=%.1f mover_penalty=%.1f allow=(leaf=%d cluster=%d area=%d mover=%d)\n",
        corridor.refine_policy.flags,
        corridor.refine_policy.topology_mismatch_penalty,
        corridor.refine_policy.mover_state_penalty,
        ( int32_t )corridor.refine_policy.allowed_leaf_indices.size(),
        ( int32_t )corridor.refine_policy.allowed_cluster_ids.size(),
        ( int32_t )corridor.refine_policy.allowed_area_ids.size(),
        ( int32_t )corridor.refine_policy.mover_refs.size() );

    /**
    *	Emit only a bounded prefix of explicit segments so the first Task 3.3 visualization layer remains useful during live debugging without flooding the log.
    **/
    const int32_t segmentLimit = SVG_Nav2_DebugDrawGetCorridorSegmentLimit( corridor, verbosity, max_segments );
    for ( int32_t segmentIndex = 0; segmentIndex < segmentLimit; segmentIndex++ ) {
        const nav2_corridor_segment_t &segment = corridor.segments[ segmentIndex ];
        const int32_t startEndpoint = ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_START_ENDPOINT ) != 0 ) ? 1 : 0;
        const int32_t goalEndpoint = ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_GOAL_ENDPOINT ) != 0 ) ? 1 : 0;
        gi.dprintf( "[NAV2][DebugDraw][Corridor] segment=%d/%d type=%s policy=%s flags=0x%08x penalty=%.1f start=(%.1f %.1f %.1f) end=(%.1f %.1f %.1f) topo=(leaf=%d cluster=%d area=%d region=%d portal=%d tile=%d) mover=(ent=%d model=%d) endpoints=(start=%d goal=%d)\n",
            segment.segment_id,
            ( int32_t )corridor.segments.size(),
            SVG_Nav2_CorridorSegmentTypeName( segment.type ),
            SVG_Nav2_CorridorSegmentPolicyStatusName( segment ),
            segment.flags,
            segment.policy_penalty,
            segment.start_anchor.x,
            segment.start_anchor.y,
            segment.start_anchor.z,
            segment.end_anchor.x,
            segment.end_anchor.y,
            segment.end_anchor.z,
            segment.topology.leaf_index,
            segment.topology.cluster_id,
            segment.topology.area_id,
            segment.topology.region_id,
            segment.topology.portal_id,
            segment.topology.tile_id,
            segment.mover_ref.owner_entnum,
            segment.mover_ref.model_index,
            startEndpoint,
            goalEndpoint );
    }

    /**
    *	Tell the caller when additional segment detail was intentionally suppressed so the bounded output stays explainable.
    **/
    if ( segmentLimit < ( int32_t )corridor.segments.size() ) {
        gi.dprintf( "[NAV2][DebugDraw][Corridor] suppressed_segments=%d\n", ( int32_t )corridor.segments.size() - segmentLimit );
    }

    /**
    *    Overlay: draw explicit start/end anchors immediately after the textual summary so operators can see exact commitment points.
    **/
    SVG_Nav2_DebugDrawCorridorAnchors( corridor );

    /**
    *    Overlay: trace each explicit segment that passed the verbosity guard so the live overlay matches the printed segment list.
    **/
    SVG_Nav2_DebugDrawCorridorSegmentOverlay( corridor, segmentLimit );
}

/**
*    @brief    Render explicit corridor anchor markers for visual debugging.
*    @param    corridor        Corridor whose anchor commitments to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     This overlay uses the explicit segment anchors when they exist, so it stays safe for diagnostics.
**/
void SVG_Nav2_DebugDrawCorridorAnchors( const nav2_corridor_t &corridor, const multicast_t multicast_type ) {
    /**
    *    Draw a small cross at every start and end anchor so debug overlays match the explicit segment markers.
    **/
    for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
        SVG_Nav2_DebugDrawCrossMarker( segment.start_anchor, multicast_type );
        SVG_Nav2_DebugDrawCrossMarker( segment.end_anchor, multicast_type );
    }
}

/**
*    @brief    Render corridor segment overlays using debug trails between anchors.
*    @param    corridor        Corridor whose segments to trace.
*    @param    limit           Maximum number of segments to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     The caller controls `limit` through the verbosity policy so the overlay remains bounded.
**/
void SVG_Nav2_DebugDrawCorridorSegmentOverlay( const nav2_corridor_t &corridor, const int32_t limit,
    const multicast_t multicast_type ) {
    /**
    *    Guard against disabled overlays so the visual debugger stays responsive.
    **/
    if ( limit <= 0 ) {
        return;
    }

    /**
    *    Trace each explicit segment with a debug trail so actors can see the corridor path at a glance.
    **/
    const int32_t concreteLimit = std::min( limit, ( int32_t )corridor.segments.size() );
    for ( int32_t segmentIndex = 0; segmentIndex < concreteLimit; ++segmentIndex ) {
        const nav2_corridor_segment_t &segment = corridor.segments[ segmentIndex ];

        /**
        *    Draw the anchor-to-anchor route spine first so all segment overlays remain visually connected.
        **/
        SVG_DebugDrawLine_TE( segment.start_anchor, segment.end_anchor, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( segment.start_anchor, multicast_type );
        SVG_Nav2_DebugDrawCrossMarker( segment.end_anchor, multicast_type );

        /**
        *    Draw endpoint markers so start/goal endpoint commitments are visible without reading textual flags.
        **/
        SVG_Nav2_DebugDrawSegmentEndpointMarker( segment, multicast_type );

        /**
        *    Draw a semantic marker per segment type so ladder, stair, portal, and mover stages are distinguishable at runtime.
        **/
        SVG_Nav2_DebugDrawSegmentTypeMarker( segment, multicast_type );

        /**
        *    Draw policy-state marker overlays so compatibility mismatches are visible directly on the segment.
        **/
        SVG_Nav2_DebugDrawSegmentPolicyMarker( segment, multicast_type );
    }
}

/**
*    @brief    Draw the scheduler request overlay for a nav2 job.
*    @param    job             Job whose request span to render.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     This uses the request start/goal to highlight the desired travel corridor.
**/
void SVG_Nav2_DebugDrawJobSchedulerOverlay( const nav2_query_job_t &job, const multicast_t multicast_type ) {
    /**
    *    Outline the requested corridor with a single debug trail and mark its endpoints.
    **/
    SVG_DebugDrawLine_TE( job.request.start_origin, job.request.goal_origin, multicast_type, false );
    SVG_Nav2_DebugDrawCrossMarker( job.request.start_origin, multicast_type );
    SVG_Nav2_DebugDrawCrossMarker( job.request.goal_origin, multicast_type );

    /**
    *    Draw a compact stage-state indicator near the request start so worker-stage progression is visible in-world.
    **/
    SVG_Nav2_DebugDrawStageOverlay( job, multicast_type );
}

/**
*    @brief    Visualize snapshot staleness or serialization requirements for a query job.
*    @param    job             Job whose snapshot diagnostics to highlight.
*    @param    staleness       Snapshot comparison result used to determine overlay state.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     Each staleness flag draws a short indicator so developers can see why the job may restart or resubmit.
**/
void SVG_Nav2_DebugDrawQuerySnapshotOverlay( const nav2_query_job_t &job, const nav2_snapshot_staleness_t &staleness,
    const multicast_t multicast_type ) {
    /**
    *    Early-out when the snapshot matches current state so overlays stay meaningful.
    **/
    if ( staleness.is_current && !staleness.requires_resubmit && !staleness.requires_restart && !staleness.requires_revalidation ) {
        return;
    }

    /**
    *    Base marker to anchor all staleness indicators near the request start so they remain readable.
    **/
    const Vector3 baseOrigin = job.request.start_origin;
    const float verticalHeight = 12.0f;
    const float lateralSpacing = 10.0f;

    if ( staleness.requires_revalidation ) {
        /**
        *    Revalidation overlay leans forward in Y so it stands apart from restart/resubmit indicators.
        **/
        SVG_Nav2_DebugDrawSnapshotFlag( baseOrigin, Vector3{ 0.0f, lateralSpacing, 0.0f }, verticalHeight, multicast_type );
    }
    if ( staleness.requires_restart ) {
        /**
        *    Restart overlay offsets to +X so it aligns with the job progressing through stages.
        **/
        SVG_Nav2_DebugDrawSnapshotFlag( baseOrigin, Vector3{ lateralSpacing, 0.0f, 0.0f }, verticalHeight, multicast_type );
    }
    if ( staleness.requires_resubmit ) {
        /**
        *    Resubmit overlay offsets to -X so operators can immediately distinguish it from other requests.
        **/
        SVG_Nav2_DebugDrawSnapshotFlag( baseOrigin, Vector3{ -lateralSpacing, 0.0f, 0.0f }, verticalHeight, multicast_type );
    }

    /**
    *    Draw a vertical compatibility stem at the request origin when any domain changed so serialization/staleness impact is obvious.
    **/
    if ( SVG_Nav2_CountSnapshotChanges( staleness ) > 0 ) {
        const Vector3 serializationStemStart = baseOrigin + Vector3{ 0.0f, 0.0f, 4.0f };
        const Vector3 serializationStemEnd = serializationStemStart + Vector3{ 0.0f, 0.0f, 18.0f };
        SVG_DebugDrawLine_TE( serializationStemStart, serializationStemEnd, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( serializationStemEnd, multicast_type, 2.5f );
    }

    /**
    *    Draw serialization-compatibility markers near the goal so mismatch domains are visible without reading textual traces.
    **/
    SVG_Nav2_DebugDrawSerializationCompatibilityOverlay( job, staleness, multicast_type );
}

/**
*    @brief    Draw serialization-compatibility marker overlays for one query job snapshot state.
*    @param    job             Query job whose goal anchor hosts compatibility markers.
*    @param    staleness       Snapshot comparison result to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     Marker layout is domain-stable so static/occupancy/mover/connector/model mismatches stay readable in-world.
**/
static void SVG_Nav2_DebugDrawSerializationCompatibilityOverlay( const nav2_query_job_t &job,
    const nav2_snapshot_staleness_t &staleness, const multicast_t multicast_type ) {
    /**
    *    Skip overlay work when no snapshot mismatch is present so the goal marker remains uncluttered.
    **/
    if ( SVG_Nav2_CountSnapshotChanges( staleness ) <= 0 ) {
        return;
    }

    /**
    *    Anchor compatibility markers near the request goal so mismatch diagnostics appear on the query destination side.
    **/
    const Vector3 base = job.request.goal_origin + Vector3{ 0.0f, 0.0f, 4.0f };
    const Vector3 stemTop = base + Vector3{ 0.0f, 0.0f, 12.0f };
    SVG_DebugDrawLine_TE( base, stemTop, multicast_type, false );

    /**
    *    Use a larger cap marker for hard incompatibility so resubmit-required states stand out immediately.
    **/
    SVG_Nav2_DebugDrawCrossMarker( stemTop, multicast_type, staleness.requires_resubmit ? 3.5f : 2.0f );

    /**
    *    Draw one domain marker per mismatch using stable offsets so repeated traces remain easy to interpret.
    **/
    if ( staleness.static_nav_changed ) {
        const Vector3 marker = stemTop + Vector3{ 7.0f, 0.0f, 0.0f };
        SVG_DebugDrawLine_TE( stemTop, marker, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( marker, multicast_type, 1.5f );
    }
    if ( staleness.occupancy_changed ) {
        const Vector3 marker = stemTop + Vector3{ 0.0f, 7.0f, 0.0f };
        SVG_DebugDrawLine_TE( stemTop, marker, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( marker, multicast_type, 1.5f );
    }
    if ( staleness.mover_changed ) {
        const Vector3 marker = stemTop + Vector3{ -7.0f, 0.0f, 0.0f };
        SVG_DebugDrawLine_TE( stemTop, marker, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( marker, multicast_type, 1.5f );
    }
    if ( staleness.connector_changed ) {
        const Vector3 marker = stemTop + Vector3{ 0.0f, -7.0f, 0.0f };
        SVG_DebugDrawLine_TE( stemTop, marker, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( marker, multicast_type, 1.5f );
    }
    if ( staleness.model_changed ) {
        const Vector3 marker = stemTop + Vector3{ 5.0f, 5.0f, 5.0f };
        SVG_DebugDrawLine_TE( stemTop, marker, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( marker, multicast_type, 1.75f );
    }
}

/**
*    @brief    Draw explicit endpoint commitment markers for one corridor segment.
*    @param    segment         Segment whose endpoint flags to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     This helper keeps start/goal endpoint commitments visible even when textual output is suppressed.
**/
static void SVG_Nav2_DebugDrawSegmentEndpointMarker( const nav2_corridor_segment_t &segment, const multicast_t multicast_type ) {
    /**
    *    Draw a short upward flag at the segment start when this segment carries corridor start endpoint semantics.
    **/
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_START_ENDPOINT ) != 0 ) {
        const Vector3 startTop = segment.start_anchor + Vector3{ 0.0f, 0.0f, 7.0f };
        SVG_DebugDrawLine_TE( segment.start_anchor, startTop, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( startTop, multicast_type, 2.0f );
    }

    /**
    *    Draw a short horizontal cap at the segment end when this segment carries corridor goal endpoint semantics.
    **/
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_GOAL_ENDPOINT ) != 0 ) {
        const Vector3 goalTop = segment.end_anchor + Vector3{ 0.0f, 0.0f, 9.0f };
        SVG_DebugDrawLine_TE( segment.end_anchor, goalTop, multicast_type, false );
        SVG_DebugDrawLine_TE( goalTop + Vector3{ -3.0f, 0.0f, 0.0f }, goalTop + Vector3{ 3.0f, 0.0f, 0.0f }, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( goalTop, multicast_type, 1.5f );
    }
}

/**
*    @brief    Draw policy-compatibility markers for one corridor segment.
*    @param    segment         Segment whose policy flags to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     Marker shape encodes compatible, soft-penalized, or would-reject policy state.
**/
static void SVG_Nav2_DebugDrawSegmentPolicyMarker( const nav2_corridor_segment_t &segment, const multicast_t multicast_type ) {
    /**
    *    Use a midpoint marker as the policy-state anchor so per-segment policy diagnostics remain easy to scan.
    **/
    const Vector3 mid = ( segment.start_anchor + segment.end_anchor ) * 0.5f;
    const Vector3 policyTop = mid + Vector3{ 0.0f, 0.0f, 4.0f };

    /**
    *    Draw the strongest policy marker first so rejection cases stand out in-world.
    **/
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT ) != 0 ) {
        SVG_DebugDrawLine_TE( mid + Vector3{ -3.0f, 0.0f, 0.0f }, mid + Vector3{ 3.0f, 0.0f, 0.0f }, multicast_type, false );
        SVG_DebugDrawLine_TE( policyTop + Vector3{ 0.0f, -3.0f, 0.0f }, policyTop + Vector3{ 0.0f, 3.0f, 0.0f }, multicast_type, false );
        SVG_Nav2_DebugDrawCrossMarker( policyTop, multicast_type, 2.5f );
        return;
    }

    /**
    *    Draw a small top bar when the segment is soft-penalized so tuning passes can spot penalty hot-spots.
    **/
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED ) != 0 ) {
        SVG_DebugDrawLine_TE( mid, policyTop, multicast_type, false );
        SVG_DebugDrawLine_TE( policyTop + Vector3{ -3.0f, 0.0f, 0.0f }, policyTop + Vector3{ 3.0f, 0.0f, 0.0f }, multicast_type, false );
        return;
    }

    /**
    *    Draw a simple policy stem for compatible segments so positive matches remain visible without clutter.
    **/
    if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE ) != 0 ) {
        SVG_DebugDrawLine_TE( mid, policyTop, multicast_type, false );
    }
}

/**
*    @brief    Return a stable short label used by stage-state overlay debugging.
*    @param    stage    Current query stage.
*    @return    Compact label used in in-world and text diagnostics.
**/
static const char *SVG_Nav2_QueryStageOverlayName( const nav2_query_stage_t stage ) {
    /**
    *    Reuse the budget stage name to keep stage vocabulary consistent across overlays and textual diagnostics.
    **/
    return SVG_Nav2_Budget_StageName( stage );
}

/**
*    @brief    Draw one stage-state overlay marker near the request start for scheduler diagnostics.
*    @param    job             Scheduler job to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
**/
static void SVG_Nav2_DebugDrawStageOverlay( const nav2_query_job_t &job, const multicast_t multicast_type ) {
    /**
    *    Offset this marker slightly from the request origin so it does not overlap the start anchor cross.
    **/
    const Vector3 stageBase = job.request.start_origin + Vector3{ 6.0f, -6.0f, 2.0f };
    const Vector3 stageTop = stageBase + Vector3{ 0.0f, 0.0f, 10.0f };
    SVG_DebugDrawLine_TE( stageBase, stageTop, multicast_type, false );

    /**
    *    Render a small cap marker so stage overlays stay visible from multiple camera angles.
    **/
    SVG_Nav2_DebugDrawCrossMarker( stageTop, multicast_type, 2.0f );

    /**
    *    Emit a compact textual note so the world marker is paired with an explicit stage name.
    **/
    gi.dprintf( "[NAV2][DebugDraw][JobOverlay] stage_marker=%s job=%llu\n",
        SVG_Nav2_QueryStageOverlayName( job.state.stage ),
        ( unsigned long long )job.job_id );
}

/**
*    @brief    Draw semantic overlay geometry for one corridor segment type.
*    @param    segment         Segment whose type semantics to visualize.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
**/
static void SVG_Nav2_DebugDrawSegmentTypeMarker( const nav2_corridor_segment_t &segment, const multicast_t multicast_type ) {
    /**
    *    Use the midpoint as a stable semantic marker anchor for this segment.
    **/
    const Vector3 mid = ( segment.start_anchor + segment.end_anchor ) * 0.5f;

    /**
    *    Draw semantic markers by type to differentiate ladders, stairs, portals, and mover traversal phases.
    **/
    switch ( segment.type ) {
    case nav2_corridor_segment_type_t::PortalTransition: {
        SVG_Nav2_DebugDrawPortalTypeOverlay( mid, multicast_type );
        break;
    }
    case nav2_corridor_segment_type_t::LadderTransition: {
        SVG_Nav2_DebugDrawLadderTypeOverlay( mid, multicast_type );
        break;
    }
    case nav2_corridor_segment_type_t::StairBand: {
        SVG_Nav2_DebugDrawStairTypeOverlay( mid, multicast_type );
        break;
    }
    case nav2_corridor_segment_type_t::MoverBoarding:
    case nav2_corridor_segment_type_t::MoverRide:
    case nav2_corridor_segment_type_t::MoverExit: {
        SVG_Nav2_DebugDrawMoverTypeOverlay( mid, multicast_type );
        break;
    }
    default:
        break;
    }
}

/**
*    @brief    Draw a portal-specific segment marker around the supplied midpoint.
*    @param    mid             Stable midpoint used for the marker.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     The helper keeps portal markers visually distinct from ladders, stairs, and movers.
**/
static void SVG_Nav2_DebugDrawPortalTypeOverlay( const Vector3 &mid, const multicast_t multicast_type ) {
    const Vector3 portalTop = mid + Vector3{ 0.0f, 0.0f, 12.0f };
    SVG_DebugDrawLine_TE( mid, portalTop, multicast_type, false );
    SVG_Nav2_DebugDrawCrossMarker( portalTop, multicast_type, 3.0f );
}

/**
*    @brief    Draw a ladder-specific segment marker around the supplied midpoint.
*    @param    mid             Stable midpoint used for the marker.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     The helper uses a rung-like shape so ladder commitments remain easy to identify in live overlays.
**/
static void SVG_Nav2_DebugDrawLadderTypeOverlay( const Vector3 &mid, const multicast_t multicast_type ) {
    const Vector3 ladderTopA = mid + Vector3{ 2.5f, 0.0f, 10.0f };
    const Vector3 ladderTopB = mid + Vector3{ -2.5f, 0.0f, 10.0f };
    SVG_DebugDrawLine_TE( mid + Vector3{ 2.5f, 0.0f, 0.0f }, ladderTopA, multicast_type, false );
    SVG_DebugDrawLine_TE( mid + Vector3{ -2.5f, 0.0f, 0.0f }, ladderTopB, multicast_type, false );
    SVG_DebugDrawLine_TE( mid + Vector3{ -2.5f, 0.0f, 6.0f }, mid + Vector3{ 2.5f, 0.0f, 6.0f }, multicast_type, false );
}

/**
*    @brief    Draw a stair-specific segment marker around the supplied midpoint.
*    @param    mid             Stable midpoint used for the marker.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     The helper uses a stepped incline shape so stair bands are distinct from ladders and portals.
**/
static void SVG_Nav2_DebugDrawStairTypeOverlay( const Vector3 &mid, const multicast_t multicast_type ) {
    const Vector3 stairA = mid + Vector3{ -4.0f, 0.0f, 2.0f };
    const Vector3 stairB = mid + Vector3{ 0.0f, 0.0f, 6.0f };
    const Vector3 stairC = mid + Vector3{ 4.0f, 0.0f, 10.0f };
    SVG_DebugDrawLine_TE( stairA, stairB, multicast_type, false );
    SVG_DebugDrawLine_TE( stairB, stairC, multicast_type, false );
}

/**
*    @brief    Draw a mover-specific segment marker around the supplied midpoint.
*    @param    mid             Stable midpoint used for the marker.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @note     The helper uses a simple ride/exit glyph so mover traversal phases remain easy to separate.
**/
static void SVG_Nav2_DebugDrawMoverTypeOverlay( const Vector3 &mid, const multicast_t multicast_type ) {
    const Vector3 moverTop = mid + Vector3{ 0.0f, 0.0f, 8.0f };
    SVG_DebugDrawLine_TE( mid, moverTop, multicast_type, false );
    SVG_DebugDrawLine_TE( moverTop, moverTop + Vector3{ 4.0f, 0.0f, 0.0f }, multicast_type, false );
    SVG_DebugDrawLine_TE( moverTop, moverTop + Vector3{ -4.0f, 0.0f, 0.0f }, multicast_type, false );
}

/**
*    @brief    Draw a small cross at `origin` using the debug trail helper.
*    @param    origin          World origin to mark.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
*    @param    half_extent     Half the cross length along each axis (default is 6 units).
**/
static void SVG_Nav2_DebugDrawCrossMarker( const Vector3 &origin, const multicast_t multicast_type, const float half_extent ) {
    /**
    *    Draw the horizontal X-axis component of the cross.
    **/
    Vector3 start = origin;
    Vector3 end = origin;
    start.x -= half_extent;
    end.x += half_extent;
    SVG_DebugDrawLine_TE( start, end, multicast_type, false );

    /**
    *    Draw the Y-axis component of the cross.
    **/
    start = origin;
    end = origin;
    start.y -= half_extent;
    end.y += half_extent;
    SVG_DebugDrawLine_TE( start, end, multicast_type, false );

    /**
    *    Draw a short vertical stem to make the marker visible in 3D.
    **/
    start = origin;
    end = origin;
    start.z -= half_extent * 0.5f;
    end.z += half_extent * 0.5f;
    SVG_DebugDrawLine_TE( start, end, multicast_type, false );
}

/**
*    @brief    Draw a vertical flag line for a snapshot state indicator.
*    @param    base            Base position used for the indicator.
*    @param    offset          Offset applied before raising the flag.
*    @param    height          Vertical height of the indicator line.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
**/
static void SVG_Nav2_DebugDrawSnapshotFlag( const Vector3 &base, const Vector3 &offset, const float height, const multicast_t multicast_type ) {
    Vector3 start = base;
    start.x += offset.x;
    start.y += offset.y;
    start.z += offset.z + 2.0f;

    Vector3 end = start;
    end.z += height;

    SVG_DebugDrawLine_TE( start, end, multicast_type, false );
    SVG_Nav2_DebugDrawCrossMarker( start, multicast_type, 3.0f );
}

/**
*    @brief    Render bounded sparse span-grid overlays and summary diagnostics.
*    @param    grid             Span grid to visualize.
*    @param    limit_columns    Maximum number of sparse columns to draw.
*    @param    multicast_type   Temp-entity multicast channel used for the overlay.
*    @note     This is intentionally bounded and opt-in so live debugging stays low-noise.
**/
void SVG_Nav2_DebugDrawSpanGrid( const nav2_span_grid_t &grid, const int32_t limit_columns, const multicast_t multicast_type ) {
    /**
    *    Guard invalid limits so bounded overlays remain explicit and deterministic.
    **/
    if ( limit_columns <= 0 ) {
        return;
    }

    /**
    *    Pull latest build diagnostics when available so the textual summary mirrors builder-side outcomes.
    **/
    nav2_span_grid_build_stats_t stats = {};
    const bool hasStats = SVG_Nav2_GetLastSpanGridBuildStats( &stats );

    /**
    *    Emit one concise summary line before drawing column/span overlays.
    **/
    if ( hasStats ) {
        gi.dprintf( "[NAV2][DebugDraw][SpanGrid] columns=%d sampled=%d emitted_cols=%d emitted_spans=%d reject=(solid_point=%d floor_trace=%d floor_miss=%d slope=%d clearance=%d solid_box=%d) hazards=(water=%d slime=%d lava=%d)\n",
            ( int32_t )grid.columns.size(),
            stats.sampled_columns,
            stats.emitted_columns,
            stats.emitted_spans,
            stats.rejected_solid_point,
            stats.rejected_floor_trace,
            stats.rejected_floor_miss,
            stats.rejected_slope,
            stats.rejected_clearance,
            stats.rejected_solid_box,
            stats.spans_with_water,
            stats.spans_with_slime,
            stats.spans_with_lava );
    } else {
        gi.dprintf( "[NAV2][DebugDraw][SpanGrid] columns=%d sampled=unknown emitted=unknown\n", ( int32_t )grid.columns.size() );
    }

    /**
    *    Draw only a bounded number of sparse columns so overlays remain predictable under large maps.
    **/
    const int32_t concreteLimit = std::min( limit_columns, ( int32_t )grid.columns.size() );
    for ( int32_t columnIndex = 0; columnIndex < concreteLimit; ++columnIndex ) {
        const nav2_span_column_t &column = grid.columns[ columnIndex ];

        /**
        *    Rebuild the column center from tile coordinates and grid sizing metadata so overlay positions remain deterministic.
        **/
        const Vector3 columnCenter = {
            ( float )( ( ( double )column.tile_x + 0.5 ) * grid.cell_size_xy ),
            ( float )( ( ( double )column.tile_y + 0.5 ) * grid.cell_size_xy ),
            0.0f
        };

        /**
        *    Draw one marker per span in this sparse column.
        **/
        for ( const nav2_span_t &span : column.spans ) {
            SVG_Nav2_DebugDrawSpanMarker( span, columnCenter, multicast_type );
        }
    }

    /**
    *    Report suppressed columns so bounded debug output stays explainable.
    **/
    if ( concreteLimit < ( int32_t )grid.columns.size() ) {
        gi.dprintf( "[NAV2][DebugDraw][SpanGrid] suppressed_columns=%d\n", ( int32_t )grid.columns.size() - concreteLimit );
    }
}

/**
*    @brief    Draw one compact span marker inside a sparse column overlay.
*    @param    span            Span metadata to visualize.
*    @param    column_center   World-space center reconstructed for the owning column.
*    @param    multicast_type  Temp-entity multicast channel used for the overlay.
**/
static void SVG_Nav2_DebugDrawSpanMarker( const nav2_span_t &span, const Vector3 &column_center, const multicast_t multicast_type ) {
    /**
    *    Place the marker at the preferred standing height to reflect precision-layer traversal position.
    **/
    const Vector3 base = { column_center.x, column_center.y, ( float )span.preferred_z };
    const Vector3 top = { column_center.x, column_center.y, ( float )span.ceiling_z };
    SVG_DebugDrawLine_TE( base, top, multicast_type, false );

    /**
    *    Draw a compact cross at the standing base so individual spans are visible in dense columns.
    **/
    SVG_Nav2_DebugDrawCrossMarker( base, multicast_type, 1.75f );

    /**
    *    Add small hazard side markers when movement flags indicate water/slime/lava semantics.
    **/
    if ( ( span.movement_flags & NAV_FLAG_WATER ) != 0 ) {
        SVG_DebugDrawLine_TE( base, base + Vector3{ 2.5f, 0.0f, 0.0f }, multicast_type, false );
    }
    if ( ( span.movement_flags & NAV_FLAG_SLIME ) != 0 ) {
        SVG_DebugDrawLine_TE( base, base + Vector3{ 0.0f, 2.5f, 0.0f }, multicast_type, false );
    }
    if ( ( span.movement_flags & NAV_FLAG_LAVA ) != 0 ) {
        SVG_DebugDrawLine_TE( base, base + Vector3{ -2.5f, 0.0f, 0.0f }, multicast_type, false );
    }
}
