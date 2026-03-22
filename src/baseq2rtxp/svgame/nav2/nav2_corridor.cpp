/********************************************************************
*
*
*	ServerGame: Nav2 Corridor Commitments - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_corridor.h"

#include "svgame/nav2/nav2_query_iface_internal.h"
#include "svgame/nav2/nav2_span_adjacency.h"
#include "svgame/nav2/nav2_span_grid.h"

#include <algorithm>
#include <limits>

/**
*	@brief	Return whether one mover reference is compatible with the current corridor refinement policy.
*	@param	mover_ref	Candidate mover reference attached to the evaluated segment.
*	@param	policy		Nav2-owned refinement policy being tested.
*	@return	True when the policy has no mover constraints or the mover reference matches one of them.
**/
static const bool SVG_Nav2_IsMoverAllowedByPolicy( const nav2_corridor_mover_ref_t &mover_ref, const nav2_corridor_refine_policy_t &policy );


/**
*
*
*	Nav2 Corridor Local Helpers:
*
*
**/
/**
*	@brief	Resolve stable corridor topology metadata for a canonical nav2 node.
*	@param	mesh		Navigation mesh used to resolve nav2 query metadata.
*	@param	node		Canonical nav2 node reference to translate.
*	@param	out_topology	[out] Stable topology metadata output.
*	@return	True when at least one stable topology field was resolved.
*	@note	This helper keeps the Task 3.2 corridor path pointer-free while consuming one nav2-owned topology result
*			from the query seam for future corridor-aware refinement and diagnostics.
**/
static const bool SVG_Nav2_TryResolveCorridorTopologyFromNode( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node,
	nav2_corridor_topology_ref_t *out_topology ) {
	/**
  *	Sanity checks: require output storage before touching canonical node metadata.
	**/
	if ( !out_topology ) {
		return false;
	}
	*out_topology = {};
	nav2_query_topology_t topology = {};

	/**
   *	Resolve nav2-owned topology metadata first so corridor code consumes one stable query result instead of rebuilding topology locally.
	**/
 if ( !SVG_Nav2_QueryTryResolveNodeTopology( mesh, node, &topology ) ) {
		return false;
	}

	/**
  *	Mirror the nav2-owned topology metadata into the corridor persistence shape.
	**/
  out_topology->leaf_index = topology.leaf_index;
	out_topology->cluster_id = topology.cluster_id;
	out_topology->area_id = topology.area_id;
	out_topology->region_id = topology.region_id;
	out_topology->portal_id = topology.portal_id;
	out_topology->tile_id = topology.tile_location.tile_id;
	out_topology->tile_x = topology.tile_location.tile_key.tile_x;
	out_topology->tile_y = topology.tile_location.tile_key.tile_y;
	out_topology->cell_index = topology.tile_location.cell_index;
	out_topology->layer_index = node.key.layer_index;

	/**
	*	Return true when any useful stable topology identifier was recovered.
	**/
    return topology.IsValid();
}

/**
*	@brief	Build a conservative Z-band commitment spanning the start and goal nodes.
*	@param	start_node	Canonical start node.
*	@param	goal_node	Canonical goal node.
*	@param	out_band	[out] Resulting Z-band commitment.
*	@return	True when a valid Z-band was produced.
*	@note	This low-risk Task 3.2 band is intentionally conservative; later hierarchy work can
*			refine it into connector- or segment-specific constraints.
**/
static const bool SVG_Nav2_BuildCorridorZBandFromEndpoints( const nav2_query_node_t &start_node, const nav2_query_node_t &goal_node,
	nav2_corridor_z_band_t *out_band ) {
	/**
	*	Sanity check: require output storage for the resulting commitment band.
	**/
	if ( !out_band ) {
		return false;
	}

	/**
	*	Build a conservative vertical band that encloses both endpoints with modest slack.
	**/
	constexpr double endpointSlack = 32.0;
	const double startZ = start_node.worldPosition.z;
	const double goalZ = goal_node.worldPosition.z;
	out_band->min_z = std::min( startZ, goalZ ) - endpointSlack;
	out_band->max_z = std::max( startZ, goalZ ) + endpointSlack;
	out_band->preferred_z = goalZ;
	return out_band->IsValid();
}

/**
*	@brief	Append one compact integer value into a membership list only once.
*	@param	value	Candidate value to append.
*	@param	values	[in,out] Membership list receiving the unique value.
**/
static void SVG_Nav2_AppendUniqueMembershipValue( const int32_t value, std::vector<int32_t> *values ) {
	/**
	*	Ignore invalid values or missing output storage so membership collection stays defensive.
	**/
	if ( !values || value < 0 ) {
		return;
	}

	/**
	*	Keep the published membership list compact and deterministic by suppressing duplicates.
	**/
	for ( const int32_t existingValue : *values ) {
		if ( existingValue == value ) {
			return;
		}
	}
	values->push_back( value );
}

/**
*	@brief	Return whether one stable integer membership is allowed by the current corridor refinement policy.
*	@param	value		Candidate membership value being tested.
*	@param	allowed_values	Allowed stable membership list.
*	@return	True when the list is empty or the value appears in the allowed membership set.
**/
static const bool SVG_Nav2_IsMembershipAllowedByPolicy( const int32_t value, const std::vector<int32_t> &allowed_values ) {
	/**
	*	An empty allow-list means the policy does not currently constrain that membership class.
	**/
	if ( allowed_values.empty() ) {
		return true;
	}

	/**
	*	Reject invalid values when a concrete allow-list exists because the policy expects stable membership ids in that case.
	**/
	if ( value < 0 ) {
		return false;
	}

	/**
	*	Search the compact allow-list directly because these staged Task 3.2 policy sets remain small and deterministic.
	**/
	for ( const int32_t allowedValue : allowed_values ) {
		if ( allowedValue == value ) {
			return true;
		}
	}
	return false;
}

/**
*	@brief	Return whether one precision-layer span overlaps the published corridor Z band.
*	@param	span	Span being tested against the corridor commitment.
*	@param	z_band	Published corridor or segment Z band.
*	@return	True when the span overlaps the allowed vertical interval or when the band is not valid.
**/
static const bool SVG_Nav2_DoesSpanOverlapCorridorZBand( const nav2_span_t &span, const nav2_corridor_z_band_t &z_band ) {
	/**
	*	Treat invalid Z bands as unconstrained because callers use them only when explicit corridor commitment metadata exists.
	**/
	if ( !z_band.IsValid() ) {
		return true;
	}

	/**
	*	Accept the span when any part of its traversable interval overlaps the published corridor band.
	**/
	return span.ceiling_z >= z_band.min_z && span.floor_z <= z_band.max_z;
}

/**
*	@brief	Return whether one precision-layer span matches the explicit topology commitments on one corridor segment.
*	@param	span	Span being tested.
*	@param	segment	Corridor segment providing topology commitments.
*	@return	True when every explicit segment-level topology commitment matches the span metadata.
**/
static const bool SVG_Nav2_DoesSpanMatchSegmentTopology( const nav2_span_t &span, const nav2_corridor_segment_t &segment ) {
	/**
	*	Reject spans that drift outside any explicit segment-level topology ids because those ids represent the strongest current corridor commitment.
	**/
	if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_LEAF_MEMBERSHIP ) != 0 && segment.topology.leaf_index >= 0 && span.leaf_id != segment.topology.leaf_index ) {
		return false;
	}
	if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_CLUSTER_MEMBERSHIP ) != 0 && segment.topology.cluster_id >= 0 && span.cluster_id != segment.topology.cluster_id ) {
		return false;
	}
	if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_AREA_MEMBERSHIP ) != 0 && segment.topology.area_id >= 0 && span.area_id != segment.topology.area_id ) {
		return false;
	}
	if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID ) != 0 && segment.topology.region_id != NAV_REGION_ID_NONE
		&& span.region_layer_id != NAV_REGION_ID_NONE && span.region_layer_id != segment.topology.region_id ) {
		return false;
	}
	return true;
}

/**
*	@brief	Return whether one precision-layer span stays inside the broader corridor refinement-policy memberships.
*	@param	span	Span being tested.
*	@param	policy	Published nav2-owned refinement policy.
*	@return	True when the span satisfies all currently published membership allow-lists.
**/
static const bool SVG_Nav2_DoesSpanMatchCorridorPolicyTopology( const nav2_span_t &span, const nav2_corridor_refine_policy_t &policy ) {
	/**
	*	Apply the published membership allow-lists directly so staged refinement can reuse the same topology gates exposed by corridor diagnostics.
	**/
	return SVG_Nav2_IsMembershipAllowedByPolicy( span.leaf_id, policy.allowed_leaf_indices )
		&& SVG_Nav2_IsMembershipAllowedByPolicy( span.cluster_id, policy.allowed_cluster_ids )
		&& SVG_Nav2_IsMembershipAllowedByPolicy( span.area_id, policy.allowed_area_ids );
}

/**
*	@brief	Return whether one span already carries ladder traversal semantics.
*	@param	span	Span being inspected.
*	@return	True when the span advertises ladder movement or ladder tile summary metadata.
**/
static const bool SVG_Nav2_SpanHasLadderSemantics( const nav2_span_t &span ) {
	/**
	*	Use both movement and tile-summary metadata because the staged precision layer may publish either form first depending on the builder pass.
	**/
	return ( span.movement_flags & NAV_FLAG_LADDER ) != 0 || ( span.surface_flags & NAV_TILE_SUMMARY_LADDER ) != 0;
}

/**
*	@brief	Return whether one span already carries stair traversal semantics.
*	@param	span	Span being inspected.
*	@return	True when the span advertises stair tile-summary metadata.
**/
static const bool SVG_Nav2_SpanHasStairSemantics( const nav2_span_t &span ) {
	return ( span.surface_flags & NAV_TILE_SUMMARY_STAIR ) != 0;
}

/**
*	@brief	Return whether one corridor segment expects ladder traversal semantics from the precision layer.
*	@param	segment	Corridor segment being inspected.
*	@return	True when the segment carries explicit ladder commitments.
**/
static const bool SVG_Nav2_SegmentRequiresLadderSemantics( const nav2_corridor_segment_t &segment ) {
	return segment.type == nav2_corridor_segment_type_t::LadderTransition
		|| ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT ) != 0
		|| ( segment.traversal_feature_bits & ( NAV_TRAVERSAL_FEATURE_LADDER | NAV_TRAVERSAL_FEATURE_LADDER_STARTPOINT | NAV_TRAVERSAL_FEATURE_LADDER_ENDPOINT ) ) != 0;
}

/**
*	@brief	Return whether one corridor segment expects stair traversal semantics from the precision layer.
*	@param	segment	Corridor segment being inspected.
*	@return	True when the segment carries explicit stair commitments.
**/
static const bool SVG_Nav2_SegmentRequiresStairSemantics( const nav2_corridor_segment_t &segment ) {
	return segment.type == nav2_corridor_segment_type_t::StairBand
		|| ( segment.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0
		|| ( segment.edge_feature_bits & ( NAV_EDGE_FEATURE_STAIR_STEP_UP | NAV_EDGE_FEATURE_STAIR_STEP_DOWN ) ) != 0;
}

/**
*	@brief	Return whether one precision-layer span or edge satisfies the connector semantics published by one corridor segment.
*	@param	segment	Corridor segment providing connector commitments.
*	@param	from_span	Source precision-layer span.
*	@param	to_span	Destination precision-layer span.
*	@param	edge	Optional precision-layer edge metadata.
*	@return	True when the candidate satisfies any explicit ladder or stair commitment carried by the segment.
**/
static const bool SVG_Nav2_DoesCandidateMatchSegmentConnectorSemantics( const nav2_corridor_segment_t &segment,
	const nav2_span_t &from_span, const nav2_span_t &to_span, const nav2_span_edge_t *edge ) {
	/**
	*	Require ladder semantics when the corridor segment explicitly commits to ladder traversal.
	**/
	if ( SVG_Nav2_SegmentRequiresLadderSemantics( segment ) ) {
		const bool hasLadderSemantics = SVG_Nav2_SpanHasLadderSemantics( from_span )
			|| SVG_Nav2_SpanHasLadderSemantics( to_span )
			|| ( edge && ( edge->feature_bits & NAV_EDGE_FEATURE_LADDER_PASS ) != 0 );
		if ( !hasLadderSemantics ) {
			return false;
		}
	}

	/**
	*	Require stair semantics when the corridor segment explicitly commits to stair traversal.
	**/
	if ( SVG_Nav2_SegmentRequiresStairSemantics( segment ) ) {
		const bool hasStairSemantics = SVG_Nav2_SpanHasStairSemantics( from_span )
			|| SVG_Nav2_SpanHasStairSemantics( to_span )
			|| ( edge && ( edge->feature_bits & ( NAV_EDGE_FEATURE_STAIR_STEP_UP | NAV_EDGE_FEATURE_STAIR_STEP_DOWN ) ) != 0 );
		if ( !hasStairSemantics ) {
			return false;
		}
	}

	return true;
}
	
/**
*	@brief	Evaluate one precision-layer candidate against one explicit corridor segment.
*	@param	corridor	Owning corridor providing shared refinement policy defaults.
*	@param	segment	Explicit segment being tested.
*	@param	from_span	Source precision-layer span.
*	@param	to_span	Destination precision-layer span.
*	@param	edge	Optional precision-layer edge metadata.
*	@param	mover_ref	Optional mover reference for mover-local candidates.
*	@param	out_eval	[out] Segment-local compatibility result.
*	@return	True when the candidate remains compatible with this segment.
**/
static const bool SVG_Nav2_EvaluateCandidateAgainstCorridorSegment( const nav2_corridor_t &corridor, const nav2_corridor_segment_t &segment,
	const nav2_span_t &from_span, const nav2_span_t &to_span, const nav2_span_edge_t *edge,
	const nav2_corridor_mover_ref_t *mover_ref, nav2_corridor_refine_eval_t *out_eval ) {
	/**
	*	Require output storage because the staged adapter is intended to publish explicit reject and penalty reasons.
	**/
	if ( !out_eval ) {
		return false;
	}
	*out_eval = {};
	out_eval->matched_segment = true;
	out_eval->compatible = true;
	out_eval->matched_segment_id = segment.segment_id;
	out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_MATCHED_SEGMENT;

	/**
	*	Apply the strongest available Z-band commitment first so vertical intent remains a hard route constraint.
	**/
	const nav2_corridor_z_band_t &activeZBand = ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND ) != 0 )
		? segment.allowed_z_band
		: corridor.global_z_band;
	if ( !SVG_Nav2_DoesSpanOverlapCorridorZBand( to_span, activeZBand ) ) {
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_Z_MISMATCH;
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
		out_eval->compatible = false;
	}

	/**
	*	Apply explicit segment topology ids and then the broader corridor-policy allow-lists so incremental refinement can reject drift early.
	**/
	const bool segmentTopologyMatch = SVG_Nav2_DoesSpanMatchSegmentTopology( to_span, segment );
	const bool policyTopologyMatch = SVG_Nav2_DoesSpanMatchCorridorPolicyTopology( to_span, corridor.refine_policy );
	if ( !segmentTopologyMatch || !policyTopologyMatch ) {
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_TOPOLOGY_MISMATCH;
		if ( ( corridor.refine_policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_PENALIZE_TOPOLOGY_MISMATCH ) != 0 ) {
			out_eval->penalty += corridor.refine_policy.topology_mismatch_penalty;
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_SOFT_PENALIZED;
		}
		if ( !segmentTopologyMatch || ( corridor.refine_policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_TOPOLOGY_MISMATCH ) != 0 ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
			out_eval->compatible = false;
		}
	}

	/**
	*	Require connector semantics when the segment commits to ladders or stairs so the candidate does not silently bypass explicit traversal-mode choices.
	**/
	if ( !SVG_Nav2_DoesCandidateMatchSegmentConnectorSemantics( segment, from_span, to_span, edge ) ) {
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_CONNECTOR_MISMATCH;
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
		out_eval->compatible = false;
	}

	/**
	*	Honor explicit mover-stage commitments strictly while keeping broader mover-policy drift as a soft penalty when possible.
	**/
	const bool segmentRequiresMover = segment.mover_ref.IsValid()
		|| ( segment.flags & ( NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_BOARDING | NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_RIDE | NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_EXIT ) ) != 0;
	if ( segmentRequiresMover ) {
		const bool moverMatchesSegment = mover_ref
			&& mover_ref->owner_entnum == segment.mover_ref.owner_entnum
			&& mover_ref->model_index == segment.mover_ref.model_index;
		if ( !moverMatchesSegment ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_MOVER_MISMATCH;
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
			out_eval->compatible = false;
		}
	} else if ( mover_ref && ( corridor.refine_policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT ) != 0
		&& !SVG_Nav2_IsMoverAllowedByPolicy( *mover_ref, corridor.refine_policy ) ) {
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_MOVER_MISMATCH;
		out_eval->penalty += corridor.refine_policy.mover_state_penalty;
		out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_SOFT_PENALIZED;
	}

	/**
	*	Fold explicit precision-layer legality into the corridor result so future fine A* can consume one combined compatibility answer.
	**/
	if ( edge ) {
		if ( edge->legality == nav2_span_edge_legality_t::HardInvalid || ( edge->flags & NAV2_SPAN_EDGE_FLAG_HARD_INVALID ) != 0 ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
			out_eval->compatible = false;
		}
		if ( edge->legality == nav2_span_edge_legality_t::SoftPenalized || ( edge->flags & NAV2_SPAN_EDGE_FLAG_SOFT_PENALIZED ) != 0 ) {
			out_eval->penalty += edge->soft_penalty_cost;
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_SOFT_PENALIZED;
		}
	}

	return out_eval->compatible;
}

/**
*	@brief	Return whether one candidate evaluation should replace the currently selected best result.
*	@param	candidate	Candidate evaluation currently being considered.
*	@param	best	Current best evaluation.
*	@return	True when the candidate is a stronger match than the current best result.
**/
static const bool SVG_Nav2_ShouldPreferCorridorEvaluation( const nav2_corridor_refine_eval_t &candidate,
	const nav2_corridor_refine_eval_t &best ) {
	/**
	*	Always prefer compatible results over rejected ones so callers can stop at the first viable corridor match.
	**/
	if ( candidate.compatible != best.compatible ) {
		return candidate.compatible;
	}

	/**
	*	Prefer lower total penalty when both candidates share the same compatibility state.
	**/
	if ( candidate.penalty != best.penalty ) {
		return candidate.penalty < best.penalty;
	}

	/**
	*	Break ties by stable segment id so repeated evaluation stays deterministic.
	**/
	return candidate.matched_segment_id < best.matched_segment_id;
}

/**
*	@brief	Return whether one mover reference is compatible with the current corridor refinement policy.
*	@param	mover_ref	Candidate mover reference attached to the evaluated segment.
*	@param	policy		Nav2-owned refinement policy being tested.
*	@return	True when the policy has no mover constraints or the mover reference matches one of them.
**/
static const bool SVG_Nav2_IsMoverAllowedByPolicy( const nav2_corridor_mover_ref_t &mover_ref, const nav2_corridor_refine_policy_t &policy ) {
	/**
	*	If no mover commitments were published, the current staged policy does not constrain mover identity yet.
	**/
	if ( policy.mover_refs.empty() ) {
		return true;
	}

	/**
	*	Reject missing mover references only when the policy explicitly expects mover-aware corridor commitments.
	**/
	if ( !mover_ref.IsValid() ) {
		return false;
	}

	/**
	*	Search the stable mover list directly because Task 3.2 keeps these commitment sets compact and pointer-free.
	**/
	for ( const nav2_corridor_mover_ref_t &allowedMoverRef : policy.mover_refs ) {
		if ( allowedMoverRef.owner_entnum == mover_ref.owner_entnum && allowedMoverRef.model_index == mover_ref.model_index ) {
			return true;
		}
	}
	return false;
}

/**
*	@brief	Evaluate one explicit corridor segment against the current nav2-owned refinement policy.
*	@param	policy		Nav2-owned refinement policy to apply.
*	@param	segment		[in,out] Segment receiving policy-compatibility flags and soft-penalty data.
*	@return	True when the segment remains compatible with the staged policy evaluation.
*	@note	This is intentionally diagnostic-first: it publishes the same rejection/penalty semantics future fine search should adopt, without changing the active oldnav-owned solver yet.
**/
static const bool SVG_Nav2_EvaluateCorridorSegmentAgainstPolicy( const nav2_corridor_refine_policy_t &policy, nav2_corridor_segment_t *segment ) {
	/**
	*	Sanity check: require segment storage before publishing policy results.
	**/
	if ( !segment ) {
		return false;
	}

	/**
	*	Clear any previous policy-evaluation bits so the result always reflects the latest staged corridor analysis.
	**/
	segment->flags &= ~( NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE
		| NAV_CORRIDOR_SEGMENT_FLAG_POLICY_Z_MISMATCH
		| NAV_CORRIDOR_SEGMENT_FLAG_POLICY_TOPOLOGY_MISMATCH
		| NAV_CORRIDOR_SEGMENT_FLAG_POLICY_MOVER_MISMATCH
		| NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED
		| NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT );
	segment->policy_penalty = 0.0;

	/**
	*	Track whether any staged policy clause would reject the segment and whether any clause should publish only a soft penalty.
	**/
	bool wouldReject = false;
	bool hasSoftPenalty = false;

	/**
	*	Apply the global Z-band guard first so vertical-commitment mismatches become explicit before topology filtering.
	**/
	if ( ( policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_OUT_OF_BAND ) != 0
		&& ( segment->flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND ) != 0
		&& segment->allowed_z_band.IsValid() ) {
		const double anchorMinZ = std::min( ( double )segment->start_anchor.z, ( double )segment->end_anchor.z );
		const double anchorMaxZ = std::max( ( double )segment->start_anchor.z, ( double )segment->end_anchor.z );
		if ( anchorMaxZ < segment->allowed_z_band.min_z || anchorMinZ > segment->allowed_z_band.max_z ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_Z_MISMATCH;
			wouldReject = true;
		}
	}

	/**
	*	Apply topology-membership checks so the corridor can already publish which segments drift outside the preferred leaf/cluster/area sets.
	**/
	const bool leafAllowed = SVG_Nav2_IsMembershipAllowedByPolicy( segment->topology.leaf_index, policy.allowed_leaf_indices );
	const bool clusterAllowed = SVG_Nav2_IsMembershipAllowedByPolicy( segment->topology.cluster_id, policy.allowed_cluster_ids );
	const bool areaAllowed = SVG_Nav2_IsMembershipAllowedByPolicy( segment->topology.area_id, policy.allowed_area_ids );
	if ( !leafAllowed || !clusterAllowed || !areaAllowed ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_TOPOLOGY_MISMATCH;
		if ( ( policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_TOPOLOGY_MISMATCH ) != 0 ) {
			wouldReject = true;
		}
		if ( ( policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_PENALIZE_TOPOLOGY_MISMATCH ) != 0 ) {
			segment->policy_penalty += policy.topology_mismatch_penalty;
			hasSoftPenalty = true;
		}
	}

	/**
	*	Apply mover-commitment checks so the corridor exposes when a mover-tagged segment drifts away from the staged mover set.
	**/
	if ( ( policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT ) != 0 ) {
		const bool moverAllowed = SVG_Nav2_IsMoverAllowedByPolicy( segment->mover_ref, policy );
		if ( !moverAllowed ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_MOVER_MISMATCH;
			segment->policy_penalty += policy.mover_state_penalty;
			hasSoftPenalty = true;
		}
	}

	/**
	*	Publish the aggregate policy result bits used by current diagnostics and future fine-refinement adoption.
	**/
	if ( wouldReject ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_WOULD_REJECT;
	}
	if ( hasSoftPenalty && segment->policy_penalty > 0.0 ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_SOFT_PENALIZED;
	}
	if ( !wouldReject ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_POLICY_COMPATIBLE;
	}
	return !wouldReject;
}

/**
*	@brief	Apply nav2-owned refinement-policy evaluation across all explicit corridor segments.
*	@param	corridor	[in,out] Corridor receiving aggregate mismatch and penalty diagnostics.
*	@note	This staged Task 3.2 adoption point keeps policy evaluation inside nav2-owned corridor construction, making the semantics real and reusable before the active refinement pipeline adopts them.
**/
static void SVG_Nav2_ApplyCorridorRefinePolicyEvaluation( nav2_corridor_t *corridor ) {
	/**
	*	Sanity checks: require corridor storage and explicit segments before evaluating policy compatibility.
	**/
	if ( !corridor || corridor->segments.empty() ) {
		return;
	}

	/**
	*	Reset aggregate counters first so callers always observe the latest policy-evaluation state.
	**/
	corridor->policy_mismatch_count = 0;
	corridor->policy_penalty_total = 0.0;

	/**
	*	Evaluate each explicit segment in order and accumulate the staged mismatch/penalty summary.
	**/
	for ( nav2_corridor_segment_t &segment : corridor->segments ) {
		if ( !SVG_Nav2_EvaluateCorridorSegmentAgainstPolicy( corridor->refine_policy, &segment ) ) {
			corridor->policy_mismatch_count++;
		}
		corridor->policy_penalty_total += segment.policy_penalty;
	}
}

/**
*	@brief	Append one mover reference into a mover-membership list only once.
*	@param	mover_ref	Candidate mover reference to append.
*	@param	values		[in,out] Mover reference list receiving the unique value.
**/
static void SVG_Nav2_AppendUniqueMoverRef( const nav2_corridor_mover_ref_t &mover_ref, std::vector<nav2_corridor_mover_ref_t> *values ) {
	/**
	*	Ignore invalid references or missing output storage because mover commitments are optional.
	**/
	if ( !values || !mover_ref.IsValid() ) {
		return;
	}

	/**
	*	Keep mover references deterministic and pointer-free by suppressing duplicate model/entity pairs.
	**/
	for ( const nav2_corridor_mover_ref_t &existingValue : *values ) {
		if ( existingValue.owner_entnum == mover_ref.owner_entnum && existingValue.model_index == mover_ref.model_index ) {
			return;
		}
	}
	values->push_back( mover_ref );
}

/**
*	@brief	Apply explicit mover boarding / ride / exit stage segmentation to corridor segments.
*	@param	corridor	[in,out] Corridor whose mover-tagged segments should be refined into stage-specific commitments.
*	@note	This keeps Task 3.2 mover commitments persistence-friendly and diagnostic-only while avoiding any oldnav refinement rewrite.
**/
static void SVG_Nav2_SegmentMoverCorridorStages( nav2_corridor_t *corridor ) {
	/**
	*	Sanity checks: require corridor storage and at least one explicit segment before attempting mover-stage refinement.
	**/
	if ( !corridor || corridor->segments.empty() ) {
		return;
	}

	/**
	*	Track whether the previous segment was already on the same mover so stage transitions remain deterministic.
	**/
	nav2_corridor_mover_ref_t previousMoverRef = {};
	bool previousSegmentWasMover = false;

	/**
	*	Walk explicit segments in order so mover commitments can be promoted from generic overlap tagging into boarding, ride, and exit stages.
	**/
	for ( int32_t segmentIndex = 0; segmentIndex < ( int32_t )corridor->segments.size(); segmentIndex++ ) {
		nav2_corridor_segment_t &segment = corridor->segments[ segmentIndex ];

		// Skip segments that do not carry a stable mover reference because only mover-tagged segments participate in staged mover commitments.
		if ( ( segment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF ) == 0 || !segment.mover_ref.IsValid() ) {
			previousSegmentWasMover = false;
			previousMoverRef = {};
			continue;
		}

		/**
		*	Determine whether the next explicit segment remains on the same mover so this segment can become a boarding, ride, or exit stage.
		**/
		bool nextSegmentSharesMover = false;
		if ( segmentIndex + 1 < ( int32_t )corridor->segments.size() ) {
			const nav2_corridor_segment_t &nextSegment = corridor->segments[ segmentIndex + 1 ];
			nextSegmentSharesMover = ( ( nextSegment.flags & NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF ) != 0 )
				&& nextSegment.mover_ref.owner_entnum == segment.mover_ref.owner_entnum
				&& nextSegment.mover_ref.model_index == segment.mover_ref.model_index;
		}

		/**
		*	Clear any previous mover-stage flags before assigning the latest deterministic classification.
		**/
		segment.flags &= ~( NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_BOARDING
			| NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_RIDE
			| NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_EXIT );

		/**
		*	Classify the mover-tagged segment by comparing it with the adjacent explicit corridor segments.
		**/
		const bool continuesPreviousMover = previousSegmentWasMover
			&& previousMoverRef.owner_entnum == segment.mover_ref.owner_entnum
			&& previousMoverRef.model_index == segment.mover_ref.model_index;
		if ( !continuesPreviousMover && nextSegmentSharesMover ) {
			// This is the first segment entering a mover-backed run, so publish it as a boarding stage.
			segment.type = nav2_corridor_segment_type_t::MoverBoarding;
			segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_BOARDING;
		} else if ( continuesPreviousMover && nextSegmentSharesMover ) {
			// This segment remains inside the same mover-backed run, so keep it as a mover ride stage.
			segment.type = nav2_corridor_segment_type_t::MoverRide;
			segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_RIDE;
		} else if ( continuesPreviousMover && !nextSegmentSharesMover ) {
			// This is the final segment on the mover-backed run, so publish it as an exit stage.
			segment.type = nav2_corridor_segment_type_t::MoverExit;
			segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_EXIT;
		} else {
			// Single-segment mover overlaps are treated conservatively as ride stages until richer connector extraction exists.
			segment.type = nav2_corridor_segment_type_t::MoverRide;
			segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_MOVER_STAGE_RIDE;
		}

		/**
		*	Carry forward the current mover reference so the next segment can decide whether it remains on the same mover-backed run.
		**/
		previousSegmentWasMover = true;
		previousMoverRef = segment.mover_ref;
	}
}

/**
*	@brief	Try resolve a stable world-tile view from a nav2 tile reference.
*	@param	mesh		Navigation mesh owning canonical world tiles.
*	@param	tile_ref	Persistence-friendly tile reference.
*	@param	out_tile	[out] Canonical world-tile view when resolved.
*	@param	out_tile_id	[out] Stable tile id when resolved.
*	@return	True when a canonical world tile was found.
**/
static const bool SVG_Nav2_TryResolveWorldTileFromRef( const nav2_query_mesh_t *mesh, const nav2_corridor_tile_ref_t &tile_ref,
	const nav2_tile_t **out_tile, int32_t *out_tile_id ) {
	/**
	*	Sanity checks: require output slots and an active mesh before touching canonical tile storage.
	**/
	if ( !mesh || !out_tile || !out_tile_id ) {
		return false;
	}
	*out_tile = nullptr;
	*out_tile_id = -1;
    const nav2_query_tile_location_t tileLocation = { .tile_key = { .tile_x = tile_ref.tile_x, .tile_y = tile_ref.tile_y }, .tile_id = tile_ref.tile_id, .cell_index = -1 };

	/**
   *	Resolve the stable world-tile id first so only the final canonical tile view requires staged private lookup.
	**/
    int32_t tileId = tileLocation.tile_id;
	if ( tileId < 0 && !SVG_Nav2_QueryTryResolveTileIdByCoords( mesh, tile_ref.tile_x, tile_ref.tile_y, &tileId ) ) {
		return false;
	}

	/**
 *	Read the canonical staged tile view only after the stable tile id is known.
	**/
 const nav2_query_tile_view_t tileView = SVG_Nav2_QueryGetTileViewByCoords( mesh, tile_ref.tile_x, tile_ref.tile_y );
	if ( tileView.tile_x != tile_ref.tile_x || tileView.tile_y != tile_ref.tile_y ) {
		return false;
	}

  /**
	*	Mirror the resolved nav2-owned tile view into a private staged tile payload only for the remaining segment annotation code.
	**/
	static thread_local nav2_tile_t s_resolved_tile = {};
	s_resolved_tile.tile_x = tileView.tile_x;
	s_resolved_tile.tile_y = tileView.tile_y;
	s_resolved_tile.region_id = tileView.region_id;
	s_resolved_tile.traversal_summary_bits = tileView.traversal_summary_bits;
	s_resolved_tile.edge_summary_bits = tileView.edge_summary_bits;
	*out_tile = &s_resolved_tile;
	*out_tile_id = tileId;
	return true;
}

/**
*	@brief	Try resolve a mover reference for one tile coordinate by comparing it against inline-model tile storage.
*	@param	mesh		Navigation mesh owning inline-model runtime and tile metadata.
*	@param	tile_x		Tile X coordinate to probe.
*	@param	tile_y		Tile Y coordinate to probe.
*	@param	out_mover_ref	[out] Stable mover reference when a matching inline-model tile is found.
*	@return	True when a mover reference was resolved.
**/
static const bool SVG_Nav2_TryResolveMoverRefFromTileCoords( const nav2_query_mesh_t *mesh, const int32_t tile_x, const int32_t tile_y,
	nav2_corridor_mover_ref_t *out_mover_ref ) {
	/**
	*	Sanity checks: require mesh storage and output storage before searching inline-model metadata.
	**/
	if ( !mesh || !out_mover_ref ) {
		return false;
	}
	*out_mover_ref = {};

	/**
 *	Resolve nav2-owned inline-model membership directly through the public query seam.
	**/
 nav2_query_inline_model_membership_t membership = {};
	if ( !SVG_Nav2_QueryTryResolveInlineModelMembershipByTileCoords( mesh, tile_x, tile_y, &membership ) ) {
		return false;
	}

	/**
  *	Mirror the nav2-owned inline-model membership into the corridor mover reference shape.
	**/
  out_mover_ref->owner_entnum = membership.owner_entnum;
	out_mover_ref->model_index = membership.model_index;
	return true;
}

/**
*	@brief	Try resolve a mover reference for one canonical node by comparing the node's tile against inline-model tiles.
*	@param	mesh		Navigation mesh owning inline-model runtime and tile metadata.
*	@param	node		Canonical node whose tile membership should be checked.
*	@param	out_mover_ref	[out] Stable mover reference when a matching inline-model tile is found.
*	@return	True when a mover reference was resolved.
**/
static const bool SVG_Nav2_TryResolveMoverRefFromNode( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node,
	nav2_corridor_mover_ref_t *out_mover_ref ) {
	/**
	*	Sanity checks: require mesh storage, output storage, and a canonical tile before searching inline-model metadata.
	**/
   if ( !mesh || !out_mover_ref ) {
		return false;
	}
	*out_mover_ref = {};

    /**
	*	Resolve nav2-owned inline-model membership directly from the canonical node.
	**/
	nav2_query_inline_model_membership_t membership = {};
	if ( !SVG_Nav2_QueryTryResolveInlineModelMembershipForNode( mesh, node, &membership ) ) {
		return false;
	}
	out_mover_ref->owner_entnum = membership.owner_entnum;
	out_mover_ref->model_index = membership.model_index;
	return true;
}

/**
*	@brief	Collect nav2 refinement-policy memberships from the explicit tile corridor.
*	@param	mesh		Navigation mesh owning leaf and tile metadata.
*	@param	exact_tile_route	Persistence-friendly tile corridor mirrored into nav2 state.
*	@param	out_policy	[in,out] Refinement policy receiving additional memberships and mover references.
**/
static void SVG_Nav2_CollectRefinePolicyFromExactTileRoute( const nav2_query_mesh_t *mesh,
	const std::vector<nav2_corridor_tile_ref_t> &exact_tile_route, nav2_corridor_refine_policy_t *out_policy ) {
	/**
	*	Sanity checks: require mesh storage, output storage, and an actual tile route before expanding memberships.
	**/
	if ( !mesh || !out_policy || exact_tile_route.empty() ) {
		return;
	}
    const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( mesh );

	/**
	*	Scan leaf membership by matching each BSP leaf's canonical tile ids against the explicit corridor route.
	**/
  for ( int32_t leafIndex = 0; leafIndex < meshMeta.leaf_count; leafIndex++ ) {
		/**
		*	Accept this leaf when any of its canonical tiles appears inside the explicit corridor route.
		**/
		bool leafMatchesCorridor = false;

		// Test each explicit corridor tile against the current leaf through the nav2-owned membership seam.
		for ( const nav2_corridor_tile_ref_t &tileRef : exact_tile_route ) {
			if ( SVG_Nav2_QueryLeafContainsTile( mesh, leafIndex, tileRef.tile_id, tileRef.tile_x, tileRef.tile_y ) ) {
				leafMatchesCorridor = true;
				break;
			}
		}
		if ( !leafMatchesCorridor ) {
			continue;
		}

		/**
		*	Publish stable leaf, cluster, and area memberships for later nav2 fine-refinement adoption.
		**/
		SVG_Nav2_AppendUniqueMembershipValue( leafIndex, &out_policy->allowed_leaf_indices );
		if ( gi.GetCollisionModel ) {
			const cm_t *collisionModel = gi.GetCollisionModel();
			if ( collisionModel && collisionModel->cache && collisionModel->cache->leafs ) {
				SVG_Nav2_AppendUniqueMembershipValue( collisionModel->cache->leafs[ leafIndex ].cluster, &out_policy->allowed_cluster_ids );
				SVG_Nav2_AppendUniqueMembershipValue( collisionModel->cache->leafs[ leafIndex ].area, &out_policy->allowed_area_ids );
			}
		}
	}

	/**
	*	Seed mover references from any corridor tiles that overlap inline-model traversal space.
	**/
	for ( const nav2_corridor_tile_ref_t &tileRef : exact_tile_route ) {
		nav2_corridor_mover_ref_t moverRef = {};
		if ( SVG_Nav2_TryResolveMoverRefFromTileCoords( mesh, tileRef.tile_x, tileRef.tile_y, &moverRef ) ) {
			SVG_Nav2_AppendUniqueMoverRef( moverRef, &out_policy->mover_refs );
		}
	}

	/**
	*	Refresh the published refinement-policy flags to reflect the widened memberships gathered from the tile corridor.
	**/
	if ( !out_policy->allowed_leaf_indices.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_LEAF_MEMBERSHIP;
	}
	if ( !out_policy->allowed_cluster_ids.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_CLUSTER_MEMBERSHIP;
	}
	if ( !out_policy->allowed_area_ids.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_AREA_MEMBERSHIP;
	}
	if ( !out_policy->mover_refs.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT;
	}
}

/**
*	@brief	Annotate one tile-seed segment from canonical world-tile metadata.
*	@param	mesh		Navigation mesh owning world-tile metadata.
*	@param	tile_ref	Persistence-friendly tile reference mirrored from the corridor.
*	@param	segment		[in,out] Segment receiving tile-level commitment annotations.
*	@param	corridor_flags	[in,out] Corridor flags widened by any newly discovered commitment types.
**/
static void SVG_Nav2_AnnotateTileSegment( const nav2_query_mesh_t *mesh, const nav2_corridor_tile_ref_t &tile_ref, nav2_corridor_segment_t *segment,
	uint32_t *corridor_flags ) {
	/**
	*	Sanity checks: require segment storage before resolving tile-level commitment metadata.
	**/
	if ( !segment ) {
		return;
	}

	/**
	*	Resolve canonical tile storage so coarse stair and ladder summaries remain visible in nav2 corridor state.
	**/
	const nav2_tile_t *tile = nullptr;
	int32_t tileId = -1;
	if ( SVG_Nav2_TryResolveWorldTileFromRef( mesh, tile_ref, &tile, &tileId ) ) {
		segment->topology.tile_id = tileId;
		segment->topology.tile_x = tile->tile_x;
		segment->topology.tile_y = tile->tile_y;
		segment->topology.region_id = tile->region_id;
		segment->traversal_feature_bits = tile->traversal_summary_bits;
		segment->edge_feature_bits = tile->edge_summary_bits;
      if ( tile->traversal_summary_bits != NAV_TILE_SUMMARY_NONE ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES;
		}
       if ( tile->edge_summary_bits != NAV_TILE_SUMMARY_NONE ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		}
      if ( tile->region_id != NAV_REGION_ID_NONE ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID;
		}

		/**
		*	Promote coarse stair and ladder tile summaries into explicit segment taxonomy for Task 3.2 diagnostics.
		**/
        if ( ( tile->traversal_summary_bits & NAV_TILE_SUMMARY_LADDER ) != 0 ) {
			segment->type = nav2_corridor_segment_type_t::LadderTransition;
			if ( corridor_flags ) {
				*corridor_flags |= NAV_CORRIDOR_FLAG_HAS_LADDER_COMMITMENT;
			}
		}
     if ( ( tile->traversal_summary_bits & NAV_TILE_SUMMARY_STAIR ) != 0 ) {
			segment->type = nav2_corridor_segment_type_t::StairBand;
			if ( corridor_flags ) {
				*corridor_flags |= NAV_CORRIDOR_FLAG_HAS_STAIR_COMMITMENT;
			}
		}
	}

	/**
	*	Publish mover references when this tile overlaps inline-model traversal space.
	**/
	if ( SVG_Nav2_TryResolveMoverRefFromTileCoords( mesh, tile_ref.tile_x, tile_ref.tile_y, &segment->mover_ref ) ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF;
		if ( segment->type == nav2_corridor_segment_type_t::None || segment->type == nav2_corridor_segment_type_t::TileSeed ) {
			segment->type = nav2_corridor_segment_type_t::MoverRide;
		}
		if ( corridor_flags ) {
			*corridor_flags |= NAV_CORRIDOR_FLAG_HAS_MOVER_COMMITMENT;
		}
	}
}

/**
*	@brief	Build nav2-owned refinement-policy commitments directly from mesh and endpoint metadata.
*	@param	mesh		Navigation mesh used to resolve leaf, cluster, area, and inline-model memberships.
*	@param	start_node	Canonical start node of the corridor.
*	@param	goal_node	Canonical goal node of the corridor.
*	@param	out_policy	[out] Nav2-owned refinement policy commitments.
**/
static void SVG_Nav2_BuildRefinePolicyFromNodes( const nav2_query_mesh_t *mesh, const nav2_query_node_t &start_node, const nav2_query_node_t &goal_node,
	nav2_corridor_refine_policy_t *out_policy ) {
	/**
	*	Sanity checks: require output storage before publishing nav2-owned commitment metadata.
	**/
	if ( !out_policy ) {
		return;
	}
	*out_policy = {};

	/**
	*	Publish conservative default refinement semantics that later nav2 fine search can honor explicitly.
	**/
	out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_OUT_OF_BAND;
	out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_TOPOLOGY_MISMATCH;
	out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_PENALIZE_TOPOLOGY_MISMATCH;
	out_policy->topology_mismatch_penalty = 12.0;
	out_policy->mover_state_penalty = 16.0;

	/**
	*	Seed allowed topology memberships from both endpoints so the corridor keeps explicit route intent even before nav2 fine A* exists.
	**/
	const nav2_corridor_topology_ref_t endpointTopologies[ 2 ] = {
		{},
		{}
	};
	(void)endpointTopologies;

	nav2_corridor_topology_ref_t startTopology = {};
	nav2_corridor_topology_ref_t goalTopology = {};
	SVG_Nav2_TryResolveCorridorTopologyFromNode( mesh, start_node, &startTopology );
	SVG_Nav2_TryResolveCorridorTopologyFromNode( mesh, goal_node, &goalTopology );

	SVG_Nav2_AppendUniqueMembershipValue( startTopology.leaf_index, &out_policy->allowed_leaf_indices );
	SVG_Nav2_AppendUniqueMembershipValue( goalTopology.leaf_index, &out_policy->allowed_leaf_indices );
	SVG_Nav2_AppendUniqueMembershipValue( startTopology.cluster_id, &out_policy->allowed_cluster_ids );
	SVG_Nav2_AppendUniqueMembershipValue( goalTopology.cluster_id, &out_policy->allowed_cluster_ids );
	SVG_Nav2_AppendUniqueMembershipValue( startTopology.area_id, &out_policy->allowed_area_ids );
	SVG_Nav2_AppendUniqueMembershipValue( goalTopology.area_id, &out_policy->allowed_area_ids );

	/**
	*	Flag whichever topology memberships were actually recovered from the active endpoints.
	**/
	if ( !out_policy->allowed_leaf_indices.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_LEAF_MEMBERSHIP;
	}
	if ( !out_policy->allowed_cluster_ids.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_CLUSTER_MEMBERSHIP;
	}
	if ( !out_policy->allowed_area_ids.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_ALLOWED_AREA_MEMBERSHIP;
	}

	/**
	*	Seed any mover commitment directly from endpoint-local inline-model overlap when present.
	**/
	nav2_corridor_mover_ref_t moverRef = {};
	if ( SVG_Nav2_TryResolveMoverRefFromNode( mesh, start_node, &moverRef ) ) {
		SVG_Nav2_AppendUniqueMoverRef( moverRef, &out_policy->mover_refs );
	}
	if ( SVG_Nav2_TryResolveMoverRefFromNode( mesh, goal_node, &moverRef ) ) {
		SVG_Nav2_AppendUniqueMoverRef( moverRef, &out_policy->mover_refs );
	}
	if ( !out_policy->mover_refs.empty() ) {
		out_policy->flags |= NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT;
	}
}

/**
*	@brief	Annotate one nav2 corridor segment from a canonical node's layer and inline-model metadata.
*	@param	mesh		Navigation mesh used to resolve layer and mover metadata.
*	@param	node		Canonical node contributing metadata to the segment.
*	@param	segment		[in,out] Segment receiving the nav2-owned commitment annotations.
*	@param	corridor_flags	[in,out] Corridor flags widened by any newly discovered commitment types.
**/
static void SVG_Nav2_AnnotateSegmentFromNode( const nav2_query_mesh_t *mesh, const nav2_query_node_t &node, nav2_corridor_segment_t *segment,
	uint32_t *corridor_flags ) {
	/**
	*	Sanity checks: require a segment to annotate before reading mesh metadata.
	**/
	if ( !segment ) {
		return;
	}

	/**
	*	Publish node-level topology directly onto the segment so later debug visualization can reason about endpoint semantics.
	**/
	SVG_Nav2_TryResolveCorridorTopologyFromNode( mesh, node, &segment->topology );
	if ( segment->topology.leaf_index >= 0 ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LEAF_MEMBERSHIP;
	}
	if ( segment->topology.cluster_id >= 0 ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_CLUSTER_MEMBERSHIP;
	}
	if ( segment->topology.area_id >= 0 ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_AREA_MEMBERSHIP;
	}

	/**
	*	Annotate layer-level traversal features so nav2 owns ladder and stair commitment metadata directly.
	**/
  const nav2_query_layer_view_t layer = SVG_Nav2_QueryGetNodeLayerView( mesh, node );
	if ( layer.traversal_feature_bits != NAV_TRAVERSAL_FEATURE_NONE || layer.ladder_endpoint_flags != NAV_LADDER_ENDPOINT_NONE ) {
        segment->traversal_feature_bits = layer.traversal_feature_bits;
		segment->ladder_endpoint_flags = layer.ladder_endpoint_flags;
		if ( layer.traversal_feature_bits != NAV_TRAVERSAL_FEATURE_NONE ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TRAVERSAL_FEATURES;
		}
       if ( layer.ladder_endpoint_flags != NAV_LADDER_ENDPOINT_NONE ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_LADDER_ENDPOINT;
			segment->type = nav2_corridor_segment_type_t::LadderTransition;
			if ( corridor_flags ) {
				*corridor_flags |= NAV_CORRIDOR_FLAG_HAS_LADDER_COMMITMENT;
			}
		}

		/**
		*	Collapse the persisted horizontal edge features into one summary mask so stair-step semantics remain visible at the corridor layer.
		**/
       uint32_t edgeFeatureMask = NAV_EDGE_FEATURE_NONE;
     for ( int32_t edgeIndex = 0; edgeIndex < NAV_LAYER_EDGE_DIR_COUNT; edgeIndex++ ) {
			edgeFeatureMask |= layer.edge_feature_bits[ edgeIndex ];
		}
		segment->edge_feature_bits = edgeFeatureMask;
       if ( edgeFeatureMask != NAV_EDGE_FEATURE_NONE ) {
			segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_EDGE_FEATURES;
		}
      if ( ( edgeFeatureMask & ( NAV_EDGE_FEATURE_STAIR_STEP_UP | NAV_EDGE_FEATURE_STAIR_STEP_DOWN ) ) != 0
			|| ( layer.traversal_feature_bits & NAV_TRAVERSAL_FEATURE_STAIR_PRESENCE ) != 0 ) {
			segment->type = nav2_corridor_segment_type_t::StairBand;
			if ( corridor_flags ) {
				*corridor_flags |= NAV_CORRIDOR_FLAG_HAS_STAIR_COMMITMENT;
			}
		}
	}

	/**
	*	Publish mover references when the node appears to sit on inline-model-backed traversal space.
	**/
	if ( SVG_Nav2_TryResolveMoverRefFromNode( mesh, node, &segment->mover_ref ) ) {
		segment->flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_MOVER_REF;
		if ( segment->type == nav2_corridor_segment_type_t::None || segment->type == nav2_corridor_segment_type_t::TileSeed ) {
			segment->type = nav2_corridor_segment_type_t::MoverRide;
		}
		if ( corridor_flags ) {
			*corridor_flags |= NAV_CORRIDOR_FLAG_HAS_MOVER_COMMITMENT;
		}
	}
}

/**
*	@brief	Append one region-path segment into the explicit nav2 corridor segment list.
*	@param	region_id	Compact hierarchy region id referenced by the corridor.
*	@param	segments	Segment buffer receiving the commitment record.
**/
static void SVG_Nav2_AppendRegionCorridorSegment( const int32_t region_id, std::vector<nav2_corridor_segment_t> *segments ) {
	/**
	*	Sanity check: require output storage before appending explicit segment state.
	**/
	if ( !segments ) {
		return;
	}

	/**
	*	Append a persistence-friendly hierarchy region commitment segment.
	**/
	nav2_corridor_segment_t segment = {};
	segment.segment_id = ( int32_t )segments->size();
	segment.type = nav2_corridor_segment_type_t::RegionBand;
	segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_REGION_ID;
	segment.topology.region_id = region_id;
	segments->push_back( segment );
}

/**
*	@brief	Append one portal-path segment into the explicit nav2 corridor segment list.
*	@param	portal_id	Compact hierarchy portal id referenced by the corridor.
*	@param	segments	Segment buffer receiving the commitment record.
**/
static void SVG_Nav2_AppendPortalCorridorSegment( const int32_t portal_id, std::vector<nav2_corridor_segment_t> *segments ) {
	/**
	*	Sanity check: require output storage before appending explicit segment state.
	**/
	if ( !segments ) {
		return;
	}

	/**
	*	Append a persistence-friendly portal transition segment.
	**/
	nav2_corridor_segment_t segment = {};
	segment.segment_id = ( int32_t )segments->size();
	segment.type = nav2_corridor_segment_type_t::PortalTransition;
	segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_PORTAL_ID;
	segment.topology.portal_id = portal_id;
	segments->push_back( segment );
}

/**
*	@brief	Build a conservative world-space anchor from one persistence-friendly tile reference.
*	@param	mesh		Navigation mesh used to recover current tile sizing.
*	@param	tile_ref	Stable tile reference mirrored into the corridor.
*	@param	preferred_z	Preferred world-space Z to apply to the anchor.
*	@param	out_anchor	[out] Resulting world-space anchor when tile sizing is available.
*	@return	True when a stable tile-centered anchor was produced.
*	@note	This intentionally derives only a coarse tile-centered anchor so Task 3.2 can enrich connector payloads without depending on unfinished connector extraction.
**/
static const bool SVG_Nav2_TryBuildCorridorAnchorFromTileRef( const nav2_query_mesh_t *mesh, const nav2_corridor_tile_ref_t &tile_ref,
	const double preferred_z, Vector3 *out_anchor ) {
	/**
	*	Sanity checks: require mesh storage, output storage, and usable tile sizing before deriving a tile-centered anchor.
	**/
	if ( !mesh || !out_anchor ) {
		return false;
	}
	const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( mesh );
	if ( !meshMeta.HasTileSizing() ) {
		return false;
	}

	/**
	*	Build the world-space tile center so portal-transition segments can expose a persistence-friendly connector anchor without touching legacy portal internals.
	**/
	const double tileWorldSize = meshMeta.GetTileWorldSize();
	out_anchor->x = ( float )( ( ( double )tile_ref.tile_x + 0.5 ) * tileWorldSize );
	out_anchor->y = ( float )( ( ( double )tile_ref.tile_y + 0.5 ) * tileWorldSize );
	out_anchor->z = ( float )preferred_z;
	return true;
}

/**
*	@brief	Annotate portal-transition segments with conservative connector anchors derived from adjacent tile-route seeds.
*	@param	mesh		Navigation mesh used to recover tile sizing.
*	@param	start_node	Canonical start node used as a fallback anchor source.
*	@param	goal_node	Canonical goal node used as a fallback anchor source.
*	@param	corridor	[in,out] Corridor receiving enriched portal-segment anchor payloads.
*	@note	This is a low-risk Task 3.2 payload increment: it gives portal segments explicit anchor endpoints without requiring a fuller portal/connectors subsystem yet.
**/
static void SVG_Nav2_AnnotatePortalSegmentsFromExactTileRoute( const nav2_query_mesh_t *mesh, const nav2_query_node_t &start_node,
	const nav2_query_node_t &goal_node, nav2_corridor_t *corridor ) {
	/**
	*	Sanity checks: require corridor storage and at least one explicit segment before searching for portal-transition payloads.
	**/
	if ( !corridor || corridor->segments.empty() ) {
		return;
	}

	/**
	*	Track the number of portal segments seen so adjacent exact-tile seeds can be consumed in deterministic order.
	**/
	int32_t portalSegmentOrdinal = 0;
	for ( nav2_corridor_segment_t &segment : corridor->segments ) {
		// Only connector-like portal-transition segments need anchor enrichment during this staged Task 3.2 increment.
		if ( segment.type != nav2_corridor_segment_type_t::PortalTransition ) {
			continue;
		}

		/**
		*	Default both anchors to the query endpoints so portal payloads remain valid even when no exact tile route is available yet.
		**/
		segment.start_anchor = ( portalSegmentOrdinal <= 0 ) ? start_node.worldPosition : segment.start_anchor;
		segment.end_anchor = ( portalSegmentOrdinal >= ( int32_t )corridor->portal_path.size() - 1 ) ? goal_node.worldPosition : segment.end_anchor;

		/**
		*	Use adjacent exact-tile seeds when present because they provide the best current nav2-owned approximation of where a portal transition begins and ends.
		**/
		if ( portalSegmentOrdinal < ( int32_t )corridor->exact_tile_route.size() ) {
			Vector3 anchor = {};
			if ( SVG_Nav2_TryBuildCorridorAnchorFromTileRef( mesh, corridor->exact_tile_route[ portalSegmentOrdinal ],
				corridor->global_z_band.preferred_z, &anchor ) ) {
				segment.start_anchor = anchor;
			}
		}
		if ( portalSegmentOrdinal + 1 < ( int32_t )corridor->exact_tile_route.size() ) {
			Vector3 anchor = {};
			if ( SVG_Nav2_TryBuildCorridorAnchorFromTileRef( mesh, corridor->exact_tile_route[ portalSegmentOrdinal + 1 ],
				corridor->global_z_band.preferred_z, &anchor ) ) {
				segment.end_anchor = anchor;
			}
		}

		/**
		*	If the anchor pair collapsed to zero because tile sizing was unavailable, fall back to the nearest endpoint positions so the connector payload still remains explicit and readable in diagnostics.
		**/
		if ( segment.start_anchor == Vector3{} ) {
			segment.start_anchor = ( portalSegmentOrdinal <= 0 ) ? start_node.worldPosition : goal_node.worldPosition;
		}
		if ( segment.end_anchor == Vector3{} ) {
			segment.end_anchor = ( portalSegmentOrdinal + 1 >= ( int32_t )corridor->portal_path.size() ) ? goal_node.worldPosition : start_node.worldPosition;
		}

		portalSegmentOrdinal++;
	}
}

/**
*	@brief	Append one exact tile-route segment into the explicit nav2 corridor segment list.
*	@param	tile_ref	Stable tile reference mirrored from the legacy corridor.
*	@param	segments	Segment buffer receiving the commitment record.
**/
static void SVG_Nav2_AppendTileCorridorSegment( const nav2_corridor_tile_ref_t &tile_ref, std::vector<nav2_corridor_segment_t> *segments ) {
	/**
	*	Sanity check: require output storage before appending explicit segment state.
	**/
	if ( !segments ) {
		return;
	}

	/**
	*	Append a persistence-friendly tile-seed segment used by current fine-refinement filters.
	**/
	nav2_corridor_segment_t segment = {};
	segment.segment_id = ( int32_t )segments->size();
	segment.type = nav2_corridor_segment_type_t::TileSeed;
	segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_TILE_REF;
	segment.topology.tile_id = tile_ref.tile_id;
	segment.topology.tile_x = tile_ref.tile_x;
	segment.topology.tile_y = tile_ref.tile_y;
	segments->push_back( segment );
}

/**
*	@brief	Resolve canonical start and goal nodes for a world-space corridor query.
*	@param	mesh		Navigation mesh used for node lookup.
*	@param	start_origin	Feet-origin start point for the corridor query.
*	@param	goal_origin	Feet-origin goal point for the corridor query.
*	@param	agent_mins	Agent bounds minimums used for feet-to-center conversion.
*	@param	agent_maxs	Agent bounds maximums used for feet-to-center conversion.
*	@param	out_start_node	[out] Canonical start node used by the legacy corridor builder.
*	@param	out_goal_node	[out] Canonical goal node used by the legacy corridor builder.
*	@return	True when both endpoints resolve to canonical nodes.
**/
static const bool SVG_Nav2_TryResolveCorridorEndpointNodes( const nav2_query_mesh_t *mesh, const Vector3 &start_origin,
	const Vector3 &goal_origin, const Vector3 &agent_mins, const Vector3 &agent_maxs, nav2_query_node_t *out_start_node,
	nav2_query_node_t *out_goal_node ) {
	/**
	*	Sanity checks: require the mesh and both endpoint output slots.
	**/
	if ( !mesh || !out_start_node || !out_goal_node ) {
		return false;
	}

	/**
	*	Convert feet-origin endpoints into the nav-center space expected by canonical node lookup.
	**/
    return SVG_Nav2_QueryTryResolveNodeForFeetOrigin( mesh, start_origin, agent_mins, agent_maxs, out_start_node )
		&& SVG_Nav2_QueryTryResolveNodeForFeetOrigin( mesh, goal_origin, agent_mins, agent_maxs, out_goal_node );
}


/**
*
*
*	Nav2 Corridor Public API:
*
*
**/
/**
*	@brief	Build a nav2 corridor wrapper from the current legacy refine corridor result.
*	@param	mesh		Navigation mesh used to resolve canonical tile metadata.
*	@param	start_node	Canonical start node used to seed endpoint topology and Z-band commitments.
*	@param	goal_node	Canonical goal node used to seed endpoint topology and Z-band commitments.
*	@param	legacy_corridor	Legacy coarse corridor result to mirror into nav2.
*	@param	coarse_expansions	Coarse expansion count reported by the legacy builder.
*	@param	out_corridor	[out] Persistence-friendly nav2 corridor description.
*	@return	True when the legacy corridor was translated successfully.
**/
const bool SVG_Nav2_BuildCorridorFromQueryCoarseCorridor( const nav2_query_mesh_t *mesh, const nav2_query_node_t &start_node,
	const nav2_query_node_t &goal_node, const nav2_query_coarse_corridor_t &coarse_corridor, const int32_t coarse_expansions,
	nav2_corridor_t *out_corridor ) {
	/**
	*	Sanity check: require output storage before mirroring the legacy corridor.
	**/
	if ( !out_corridor ) {
		return false;
	}
	*out_corridor = {};

	/**
	*	Reject empty legacy corridors so callers keep fallback behavior explicit.
	**/
 if ( !coarse_corridor.HasExactTileRoute() && coarse_corridor.region_path.empty() && coarse_corridor.portal_path.empty() ) {
		return false;
	}

	/**
	*	Mirror the coarse source classification and basic route statistics first.
	**/
 switch ( coarse_corridor.source ) {
	case nav2_query_coarse_corridor_t::source_t::Hierarchy:
		out_corridor->source = nav2_corridor_source_t::Hierarchy;
		out_corridor->flags |= NAV_CORRIDOR_FLAG_HIERARCHY_DERIVED;
		break;
 case nav2_query_coarse_corridor_t::source_t::ClusterFallback:
		out_corridor->source = nav2_corridor_source_t::ClusterFallback;
		out_corridor->flags |= NAV_CORRIDOR_FLAG_CLUSTER_FALLBACK;
		break;
	default:
		out_corridor->source = nav2_corridor_source_t::None;
		break;
	}
	out_corridor->coarse_expansion_count = coarse_expansions;
    out_corridor->overlay_block_count = coarse_corridor.overlay_block_count;
	out_corridor->overlay_penalty_count = coarse_corridor.overlay_penalty_count;

	/**
	*	Resolve stable endpoint topology and a conservative vertical commitment band.
	**/
	SVG_Nav2_TryResolveCorridorTopologyFromNode( mesh, start_node, &out_corridor->start_topology );
	SVG_Nav2_TryResolveCorridorTopologyFromNode( mesh, goal_node, &out_corridor->goal_topology );
	if ( SVG_Nav2_BuildCorridorZBandFromEndpoints( start_node, goal_node, &out_corridor->global_z_band ) ) {
		out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_GLOBAL_Z_BAND;
	}
	SVG_Nav2_BuildRefinePolicyFromNodes( mesh, start_node, goal_node, &out_corridor->refine_policy );
	if ( out_corridor->refine_policy.flags != NAV_CORRIDOR_REFINE_POLICY_FLAG_NONE ) {
		out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_REFINEMENT_POLICY;
	}

	/**
	*	Mirror hierarchy region and portal paths into persistence-friendly storage.
	**/
    out_corridor->region_path = coarse_corridor.region_path;
	out_corridor->portal_path = coarse_corridor.portal_path;
	if ( !out_corridor->region_path.empty() ) {
		out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_REGION_PATH;
		for ( const int32_t regionId : out_corridor->region_path ) {
			SVG_Nav2_AppendRegionCorridorSegment( regionId, &out_corridor->segments );
		}
	}
	if ( !out_corridor->portal_path.empty() ) {
		out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_PORTAL_PATH;
		for ( const int32_t portalId : out_corridor->portal_path ) {
			SVG_Nav2_AppendPortalCorridorSegment( portalId, &out_corridor->segments );
		}
	}

	/**
	*	Mirror the exact tile route into stable tile references used by current fine refinement.
	**/
  if ( !coarse_corridor.exact_tile_route.empty() ) {
		out_corridor->flags |= NAV_CORRIDOR_FLAG_HAS_EXACT_TILE_ROUTE;
      out_corridor->exact_tile_route.reserve( coarse_corridor.exact_tile_route.size() );
		for ( const nav2_query_tile_ref_t &tileKey : coarse_corridor.exact_tile_route ) {
			nav2_corridor_tile_ref_t tileRef = {};
			tileRef.tile_x = tileKey.tile_x;
			tileRef.tile_y = tileKey.tile_y;

			/**
			*	Resolve the canonical world-tile id when the mesh is available.
			**/
           if ( mesh ) {
				nav2_query_tile_location_t tileLocation = {};
				const nav2_query_mesh_meta_t meshMeta = SVG_Nav2_QueryGetMeshMeta( mesh );
				const Vector3 tileCenter = {
					( float )( ( ( double )tileKey.tile_x + 0.5 ) * meshMeta.GetTileWorldSize() ),
					( float )( ( ( double )tileKey.tile_y + 0.5 ) * meshMeta.GetTileWorldSize() ),
					0.0f
				};

				/**
				*	Resolve the stable world-tile id through the nav2-owned tile-localization seam when mesh sizing metadata is usable.
				**/
				if ( meshMeta.HasTileSizing() && SVG_Nav2_QueryTryResolveTileLocation( mesh, tileCenter, &tileLocation ) ) {
					tileRef.tile_id = tileLocation.tile_id;
				}
			}

			out_corridor->exact_tile_route.push_back( tileRef );
			SVG_Nav2_AppendTileCorridorSegment( tileRef, &out_corridor->segments );
		}

		/**
		*	Widen refinement-policy memberships using the explicit tile corridor so nav2 keeps topology commitments independent of oldnav internals.
		**/
		SVG_Nav2_CollectRefinePolicyFromExactTileRoute( mesh, out_corridor->exact_tile_route, &out_corridor->refine_policy );
	}

	/**
	*	Mark the first and last explicit segments as corridor endpoints when available.
	**/
	if ( !out_corridor->segments.empty() ) {
		out_corridor->segments.front().flags |= NAV_CORRIDOR_SEGMENT_FLAG_START_ENDPOINT;
		out_corridor->segments.back().flags |= NAV_CORRIDOR_SEGMENT_FLAG_GOAL_ENDPOINT;

		/**
		*	Annotate tile-seed segments from the explicit corridor route before endpoint-specific overlaying so coarse tile commitments remain visible.
		**/
		for ( int32_t segmentIndex = 0, tileIndex = 0; segmentIndex < ( int32_t )out_corridor->segments.size(); segmentIndex++ ) {
			nav2_corridor_segment_t &segment = out_corridor->segments[ segmentIndex ];
			if ( segment.type != nav2_corridor_segment_type_t::TileSeed || tileIndex >= ( int32_t )out_corridor->exact_tile_route.size() ) {
				continue;
			}
			SVG_Nav2_AnnotateTileSegment( mesh, out_corridor->exact_tile_route[ tileIndex ], &segment, &out_corridor->flags );
			tileIndex++;
		}

		/**
		*	Overlay endpoint-local node semantics onto the first and last segments so ladder endpoint bits and endpoint topology remain explicit.
		**/
		SVG_Nav2_AnnotateSegmentFromNode( mesh, start_node, &out_corridor->segments.front(), &out_corridor->flags );
		SVG_Nav2_AnnotateSegmentFromNode( mesh, goal_node, &out_corridor->segments.back(), &out_corridor->flags );

		/**
		*	Enrich portal-transition segments with conservative connector anchors derived from adjacent exact-tile seeds and endpoint nodes.
		**/
		SVG_Nav2_AnnotatePortalSegmentsFromExactTileRoute( mesh, start_node, goal_node, out_corridor );

		for ( nav2_corridor_segment_t &segment : out_corridor->segments ) {
			if ( out_corridor->global_z_band.IsValid() ) {
				segment.allowed_z_band = out_corridor->global_z_band;
				segment.flags |= NAV_CORRIDOR_SEGMENT_FLAG_HAS_ALLOWED_Z_BAND;
			}
		}

		/**
		*	Promote generic mover overlap tagging into explicit boarding / ride / exit stages after endpoint and tile annotations have been applied.
		**/
		SVG_Nav2_SegmentMoverCorridorStages( out_corridor );

		/**
		*	Apply nav2-owned rejection/penalty evaluation now so current diagnostics and future refinement can observe real policy compatibility state from the corridor seam itself.
		**/
		SVG_Nav2_ApplyCorridorRefinePolicyEvaluation( out_corridor );
	}

	return out_corridor->HasExactTileRoute() || out_corridor->HasSegments();
}

/**
*	@brief	Build a nav2 corridor directly from world-space endpoints through the legacy coarse builder.
*	@param	mesh		Navigation mesh used for endpoint lookup and legacy corridor generation.
*	@param	start_origin	Feet-origin start point for the corridor query.
*	@param	goal_origin	Feet-origin goal point for the corridor query.
*	@param	policy		Optional traversal policy used to gate legacy coarse transitions.
*	@param	agent_mins	Agent bounds minimums used for feet-to-center conversion.
*	@param	agent_maxs	Agent bounds maximums used for feet-to-center conversion.
*	@param	out_corridor	[out] Persistence-friendly nav2 corridor description.
*	@return	True when a strict nav2 corridor could be built.
*	@note	The public nav2 path stays strict here; temporary compatibility fallback behavior must remain isolated
*          inside internal seam helpers and not be treated as the public answer.
**/
const bool SVG_Nav2_BuildCorridorForEndpoints( const nav2_query_mesh_t *mesh, const Vector3 &start_origin, const Vector3 &goal_origin,
	const nav2_query_policy_t *policy, const Vector3 &agent_mins, const Vector3 &agent_maxs, nav2_corridor_t *out_corridor ) {
	// Sanity checks: require output storage and a valid mesh before resolving endpoint nodes.
	if ( !mesh || !out_corridor ) {
		return false;
	}

	// Resolve canonical start and goal nodes through the existing query seam.
	nav2_query_node_t startNode = {};
	nav2_query_node_t goalNode = {};
	if ( !SVG_Nav2_TryResolveCorridorEndpointNodes( mesh, start_origin, goal_origin, agent_mins, agent_maxs, &startNode, &goalNode ) ) {
		return false;
	}

	// Build the staged coarse corridor through the compatibility layer so the public nav2 corridor can mirror explicit commitments only.
	nav2_query_coarse_corridor_t coarseCorridor = {};
	int32_t coarseExpansions = 0;
	if ( !SVG_Nav2_QueryBuildCoarseCorridor( mesh, startNode, goalNode, policy, &coarseCorridor, &coarseExpansions ) ) {
		return false;
	}

	// Reject empty coarse corridors so the public nav2 surface never accepts a fallback-only answer.
	if ( !coarseCorridor.HasExactTileRoute() && coarseCorridor.region_path.empty() && coarseCorridor.portal_path.empty() ) {
		return false;
	}

	// Mirror the staged coarse corridor into the explicit nav2 corridor representation.
	return SVG_Nav2_BuildCorridorFromQueryCoarseCorridor( mesh, startNode, goalNode, coarseCorridor, coarseExpansions, out_corridor );
}

/**
*	@brief	Evaluate one precision-layer span against the published nav2 corridor commitments.
*	@param	corridor	Corridor supplying Z-band, topology, and connector commitments.
*	@param	span	Precision-layer span being considered for incremental refinement.
*	@param	mover_ref	Optional mover reference for mover-local refinement candidates.
*	@param	out_eval	[out] Aggregate compatibility result.
*	@return	True when the candidate remains compatible with the published corridor semantics.
**/
const bool SVG_Nav2_EvaluateSpanForCorridor( const nav2_corridor_t &corridor, const nav2_span_t &span,
	const nav2_corridor_mover_ref_t *mover_ref, nav2_corridor_refine_eval_t *out_eval ) {
	/**
	*	Require output storage because the staged adapter is expected to publish explicit penalty and rejection reasons.
	**/
	if ( !out_eval ) {
		return false;
	}
	*out_eval = {};

	/**
	*	When no explicit segments exist yet, fall back to the published global policy only so callers can still consume corridor constraints incrementally.
	**/
	if ( corridor.segments.empty() ) {
		out_eval->compatible = true;
		if ( corridor.global_z_band.IsValid() && !SVG_Nav2_DoesSpanOverlapCorridorZBand( span, corridor.global_z_band ) ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_Z_MISMATCH;
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
			out_eval->compatible = false;
		}
		if ( !SVG_Nav2_DoesSpanMatchCorridorPolicyTopology( span, corridor.refine_policy ) ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_TOPOLOGY_MISMATCH;
			if ( ( corridor.refine_policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_PENALIZE_TOPOLOGY_MISMATCH ) != 0 ) {
				out_eval->penalty += corridor.refine_policy.topology_mismatch_penalty;
				out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_SOFT_PENALIZED;
			}
			if ( ( corridor.refine_policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_REJECT_TOPOLOGY_MISMATCH ) != 0 ) {
				out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
				out_eval->compatible = false;
			}
		}
		if ( mover_ref && ( corridor.refine_policy.flags & NAV_CORRIDOR_REFINE_POLICY_FLAG_HAS_MOVER_COMMITMENT ) != 0
			&& !SVG_Nav2_IsMoverAllowedByPolicy( *mover_ref, corridor.refine_policy ) ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_MOVER_MISMATCH;
			out_eval->penalty += corridor.refine_policy.mover_state_penalty;
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_SOFT_PENALIZED;
		}
		return out_eval->compatible;
	}

	/**
	*	Evaluate the span against every explicit segment and keep the strongest deterministic match for the caller.
	**/
	nav2_corridor_refine_eval_t bestEval = {};
	bestEval.penalty = std::numeric_limits<double>::infinity();
	bool hasBestEval = false;
	for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
		nav2_corridor_refine_eval_t segmentEval = {};
		SVG_Nav2_EvaluateCandidateAgainstCorridorSegment( corridor, segment, span, span, nullptr, mover_ref, &segmentEval );
		if ( !hasBestEval || SVG_Nav2_ShouldPreferCorridorEvaluation( segmentEval, bestEval ) ) {
			bestEval = segmentEval;
			hasBestEval = true;
		}
	}
	if ( hasBestEval ) {
		*out_eval = bestEval;
	}
	return out_eval->compatible;
}

/**
*	@brief	Evaluate one precision-layer adjacency edge against the published nav2 corridor commitments.
*	@param	corridor	Corridor supplying Z-band, topology, and connector commitments.
*	@param	from_span	Source span of the evaluated edge.
*	@param	to_span	Destination span of the evaluated edge.
*	@param	edge	Precision-layer adjacency edge being considered for incremental refinement.
*	@param	mover_ref	Optional mover reference for mover-local refinement candidates.
*	@param	out_eval	[out] Aggregate compatibility result.
*	@return	True when the edge remains compatible with the published corridor semantics.
**/
const bool SVG_Nav2_EvaluateSpanEdgeForCorridor( const nav2_corridor_t &corridor, const nav2_span_t &from_span, const nav2_span_t &to_span,
	const nav2_span_edge_t &edge, const nav2_corridor_mover_ref_t *mover_ref, nav2_corridor_refine_eval_t *out_eval ) {
	/**
	*	Require output storage because the staged adapter is expected to publish explicit penalty and rejection reasons.
	**/
	if ( !out_eval ) {
		return false;
	}
	*out_eval = {};

	/**
	*	When no explicit segments exist yet, reuse the span-only compatibility result and then fold in the edge legality semantics.
	**/
	if ( corridor.segments.empty() ) {
		SVG_Nav2_EvaluateSpanForCorridor( corridor, to_span, mover_ref, out_eval );
		if ( edge.legality == nav2_span_edge_legality_t::HardInvalid || ( edge.flags & NAV2_SPAN_EDGE_FLAG_HARD_INVALID ) != 0 ) {
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_WOULD_REJECT;
			out_eval->compatible = false;
		}
		if ( edge.legality == nav2_span_edge_legality_t::SoftPenalized || ( edge.flags & NAV2_SPAN_EDGE_FLAG_SOFT_PENALIZED ) != 0 ) {
			out_eval->penalty += edge.soft_penalty_cost;
			out_eval->flags |= NAV_CORRIDOR_REFINE_EVAL_FLAG_SOFT_PENALIZED;
		}
		return out_eval->compatible;
	}

	/**
	*	Evaluate the directed span edge against every explicit segment and keep the strongest deterministic match for the caller.
	**/
	nav2_corridor_refine_eval_t bestEval = {};
	bestEval.penalty = std::numeric_limits<double>::infinity();
	bool hasBestEval = false;
	for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
		nav2_corridor_refine_eval_t segmentEval = {};
		SVG_Nav2_EvaluateCandidateAgainstCorridorSegment( corridor, segment, from_span, to_span, &edge, mover_ref, &segmentEval );
		if ( !hasBestEval || SVG_Nav2_ShouldPreferCorridorEvaluation( segmentEval, bestEval ) ) {
			bestEval = segmentEval;
			hasBestEval = true;
		}
	}
	if ( hasBestEval ) {
		*out_eval = bestEval;
	}
	return out_eval->compatible;
}

/**
*	@brief	Return a short debug label for a corridor source classification.
*	@param	source	Corridor source classification to stringify.
*	@return	Stable debug label for the requested corridor source.
**/
const char *SVG_Nav2_CorridorSourceName( const nav2_corridor_source_t source ) {
	/**
	*	Translate the stable enum into a concise diagnostics label.
	**/
	switch ( source ) {
	case nav2_corridor_source_t::Hierarchy:
		return "Hierarchy";
	case nav2_corridor_source_t::ClusterFallback:
		return "ClusterFallback";
	default:
		return "None";
	}
}

/**
*	@brief	Return a short debug label for a corridor segment type.
*	@param	type	Corridor segment type to stringify.
*	@return	Stable debug label for the requested segment type.
**/
const char *SVG_Nav2_CorridorSegmentTypeName( const nav2_corridor_segment_type_t type ) {
	/**
	*	Translate the stable enum into a concise diagnostics label.
	**/
	switch ( type ) {
	case nav2_corridor_segment_type_t::RegionBand:
		return "RegionBand";
	case nav2_corridor_segment_type_t::PortalTransition:
		return "PortalTransition";
	case nav2_corridor_segment_type_t::TileSeed:
		return "TileSeed";
	case nav2_corridor_segment_type_t::LadderTransition:
		return "LadderTransition";
	case nav2_corridor_segment_type_t::StairBand:
		return "StairBand";
	case nav2_corridor_segment_type_t::MoverBoarding:
		return "MoverBoarding";
	case nav2_corridor_segment_type_t::MoverRide:
		return "MoverRide";
	case nav2_corridor_segment_type_t::MoverExit:
		return "MoverExit";
	case nav2_corridor_segment_type_t::GoalEndpoint:
		return "GoalEndpoint";
	default:
		return "None";
	}
}

/**
*	@brief	Emit a concise debug dump of the explicit nav2 corridor commitments.
*	@param	corridor	Corridor to report.
*	@note	Callers are expected to gate this behind their own debug policy to avoid log spam.
**/
void SVG_Nav2_DebugPrintCorridor( const nav2_corridor_t &corridor ) {
	/**
	*	Emit a compact header summarizing the mirrored coarse commitments first.
	**/
	gi.dprintf( "[NAV2][Corridor] source=%s flags=0x%08x regions=%d portals=%d tiles=%d segments=%d coarse=%d overlay=(%d/%d) z=[%.1f %.1f] pref=%.1f\n",
		SVG_Nav2_CorridorSourceName( corridor.source ),
		corridor.flags,
		( int32_t )corridor.region_path.size(),
		( int32_t )corridor.portal_path.size(),
		( int32_t )corridor.exact_tile_route.size(),
		( int32_t )corridor.segments.size(),
		corridor.coarse_expansion_count,
		corridor.overlay_block_count,
		corridor.overlay_penalty_count,
		corridor.global_z_band.min_z,
		corridor.global_z_band.max_z,
		corridor.global_z_band.preferred_z );
 gi.dprintf( "[NAV2][Corridor] policy mismatches=%d penalty_total=%.1f\n",
		corridor.policy_mismatch_count,
		corridor.policy_penalty_total );
	gi.dprintf( "[NAV2][Corridor] refine_policy flags=0x%08x leafs=%d clusters=%d mismatch_penalty=%.1f\n",
		corridor.refine_policy.flags,
		( int32_t )corridor.refine_policy.allowed_leaf_indices.size(),
		( int32_t )corridor.refine_policy.allowed_cluster_ids.size(),
		corridor.refine_policy.topology_mismatch_penalty );
	gi.dprintf( "[NAV2][Corridor] refine_policy areas=%d movers=%d mover_penalty=%.1f\n",
		( int32_t )corridor.refine_policy.allowed_area_ids.size(),
		( int32_t )corridor.refine_policy.mover_refs.size(),
		corridor.refine_policy.mover_state_penalty );

	/**
	*	Dump the explicit segment commitments in deterministic order for diagnostics.
	**/
	for ( const nav2_corridor_segment_t &segment : corridor.segments ) {
       gi.dprintf( "[NAV2][Corridor] segment=%d type=%s flags=0x%08x penalty=%.1f region=%d portal=%d tileId=%d tile=(%d,%d) cell=%d layer=%d leaf=%d cluster=%d area=%d move=(ent=%d model=%d) traverse=0x%08x edge=0x%08x ladder=0x%02x z=[%.1f %.1f] pref=%.1f\n",
			segment.segment_id,
			SVG_Nav2_CorridorSegmentTypeName( segment.type ),
			segment.flags,
         segment.policy_penalty,
			segment.topology.region_id,
			segment.topology.portal_id,
			segment.topology.tile_id,
			segment.topology.tile_x,
			segment.topology.tile_y,
			segment.topology.cell_index,
			segment.topology.layer_index,
			segment.topology.leaf_index,
			segment.topology.cluster_id,
			segment.topology.area_id,
           segment.mover_ref.owner_entnum,
			segment.mover_ref.model_index,
			segment.traversal_feature_bits,
			segment.edge_feature_bits,
			segment.ladder_endpoint_flags,
			segment.allowed_z_band.min_z,
			segment.allowed_z_band.max_z,
			segment.allowed_z_band.preferred_z );
	}
}
