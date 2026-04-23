/********************************************************************
*
*
*	ServerGame: Nav2 Span Adjacency - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_span_adjacency.h"

#include <cmath>
#include <unordered_map>


/**
*
*
*	Nav2 Span Adjacency Local Helpers:
*
*
**/
//! Maximum horizontal neighbor offsets evaluated for local sparse-column adjacency.
static constexpr int32_t NAV2_SPAN_ADJACENCY_NEIGHBOR_RADIUS = 1;
//! Base horizontal movement cost for orthogonal span-to-span traversal.
static constexpr double NAV2_SPAN_ADJACENCY_CARDINAL_COST = 1.0;
//! Base horizontal movement cost for diagonal span-to-span traversal.
static constexpr double NAV2_SPAN_ADJACENCY_DIAGONAL_COST = 1.41421356237;
//! Additional cost applied to controlled walk-off / drop transitions.
static constexpr double NAV2_SPAN_ADJACENCY_DROP_COST_BIAS = 0.5;
//! Additional cost applied to liquid transitions.
static constexpr double NAV2_SPAN_ADJACENCY_LIQUID_COST_BIAS = 0.25;
//! Additional cost applied when a transition crosses BSP partitions.
static constexpr double NAV2_SPAN_ADJACENCY_PARTITION_CROSS_COST_BIAS = 0.15;
//! Additional cost applied when a transition is diagonal and near a boundary.
static constexpr double NAV2_SPAN_ADJACENCY_DIAGONAL_CAUTION_COST_BIAS = 0.10;
//! Additional cost applied when a transition hugs a likely ledge boundary.
static constexpr double NAV2_SPAN_ADJACENCY_LEDGE_COST_BIAS = 0.30;
//! Additional cost applied when a transition hugs a likely wall boundary.
static constexpr double NAV2_SPAN_ADJACENCY_WALL_COST_BIAS = 0.20;

/**
*	@brief	Resolve the runtime-origin lift needed to convert a feet-level span sample into the collision origin used by the default stand hull.
*	@return	Positive Z lift for center-origin defaults, or 0 for feet-origin defaults.
*	@note	Span preferred Z values are currently stored in feet/floor space. Adjacency legality probes that use full hull bounds must therefore
*			lift those endpoints into origin space before tracing or querying contents, otherwise center-origin hulls are validated too low.
**/
static inline float SVG_Nav2_SpanAdjacency_GetFeetToOriginOffsetZ( void ) {
	return PHYS_DEFAULT_BBOX_STANDUP_MINS.z < 0.0f ? -PHYS_DEFAULT_BBOX_STANDUP_MINS.z : 0.0f;
}

/**
*	@brief	Build the conservative validation hull mins used by adjacency trace and contents probes.
*	@param	halfExtentXY	Resolved XY half-extent for the local sparse-cell probe.
*	@return	Validation hull minimum bounds in local origin space.
**/
static inline Vector3 SVG_Nav2_SpanAdjacency_GetValidationHullMins( const float halfExtentXY ) {
	return { -halfExtentXY, -halfExtentXY, PHYS_DEFAULT_BBOX_STANDUP_MINS.z };
}

/**
*	@brief	Build the conservative validation hull maxs used by adjacency trace and contents probes.
*	@param	halfExtentXY	Resolved XY half-extent for the local sparse-cell probe.
*	@return	Validation hull maximum bounds in local origin space.
**/
static inline Vector3 SVG_Nav2_SpanAdjacency_GetValidationHullMaxs( const float halfExtentXY ) {
	return { halfExtentXY, halfExtentXY, PHYS_DEFAULT_BBOX_STANDUP_MAXS.z };
}

/**
*	@brief	Trace/contents validation result for one directed adjacency transition.
**/
struct nav2_span_transition_validation_t {
	//! True when trace-based validation marked the edge hard-invalid.
	bool trace_invalid = false;
	//! True when contents-based validation marked the edge hard-invalid.
	bool contents_invalid = false;
};

/**
*	@brief	Stable lookup key used to resolve sparse columns by their published XY coordinates.
*	@note	This keeps adjacency construction deterministic without relying on vector scans for every neighbor probe.
**/
struct nav2_span_column_lookup_key_t {
	//! Published sparse-column X coordinate.
	int32_t tile_x = 0;
	//! Published sparse-column Y coordinate.
	int32_t tile_y = 0;

	/**
	*	@brief	Compare two sparse-column lookup keys for exact XY equality.
	*	@param	other	Other sparse-column lookup key to compare against.
	*	@return	True when both coordinates match exactly.
	**/
	bool operator==( const nav2_span_column_lookup_key_t &other ) const {
		return tile_x == other.tile_x && tile_y == other.tile_y;
	}
};

/**
*	@brief	Hash functor for sparse-column lookup keys.
*	@note	Uses the same coordinate-mixing pattern as other nav2 tile-key helpers so hash iteration remains stable enough for deterministic build order.
**/
struct nav2_span_column_lookup_key_hash_t {
	/**
	*	@brief	Compute the hash value for one sparse-column lookup key.
	*	@param	key	Sparse-column key to hash.
	*	@return	Combined hash of both XY coordinates.
	**/
	size_t operator()( const nav2_span_column_lookup_key_t &key ) const {
		size_t seed = std::hash<int32_t>{}( key.tile_x );
		seed ^= std::hash<int32_t>{}( key.tile_y ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
		return seed;
	}
};

/**
*	@brief	Conservative local boundary semantics derived for one span in one neighbor direction.
*	@note	This keeps ledge- and wall-adjacency tagging explicit without requiring richer cell-edge metadata from the span-grid builder yet.
**/
struct nav2_span_edge_boundary_semantics_t {
	//! True when the source/destination relationship suggests a nearby ledge or unsupported walk-off.
	bool is_ledge_adjacent = false;
	//! True when the source/destination relationship suggests a nearby wall or blocked side.
	bool is_wall_adjacent = false;
	//! True when the transition should remain traversable but carry a soft-penalty cost.
	bool apply_soft_penalty = false;
	//! True when the transition should be treated as hard-invalid.
	bool hard_invalid = false;
	//! Additional cost bias derived from the discovered boundary semantics.
	double soft_penalty_cost = 0.0;
};

/**
*	@brief	Return whether the span participates in liquid traversal semantics.
*	@param	span	Span to inspect.
*	@return	True when the span is marked as water, lava, or slime.
**/
static const bool SVG_Nav2_SpanAdjacency_IsLiquidSpan( const nav2_span_t &span ) {
	/**
	*    Treat any of the explicit liquid movement flags as liquid traversal space.
	**/
	return ( span.movement_flags & ( NAV_FLAG_WATER | NAV_FLAG_LAVA | NAV_FLAG_SLIME ) ) != 0;
}

/**
*	@brief	Return whether two spans overlap vertically enough for direct same-band or step-based traversal.
*	@param	from_span	Source span.
*	@param	to_span	Destination span.
*	@return	True when the spans share a usable vertical overlap interval.
**/
static const bool SVG_Nav2_SpanAdjacency_HasVerticalOverlap( const nav2_span_t &from_span, const nav2_span_t &to_span ) {
	/**
	*    Use the min/max overlap interval so the conservative first pass can reject obviously disconnected stacked spans.
	**/
	const double overlapMin = std::max( from_span.floor_z, to_span.floor_z );
	const double overlapMax = std::min( from_span.ceiling_z, to_span.ceiling_z );
	return overlapMax >= overlapMin;
}

/**
*	@brief	Resolve the base movement cost for one local XY neighbor offset.
*	@param	delta_x	Signed X offset between sparse columns.
*	@param	delta_y	Signed Y offset between sparse columns.
*	@return	Orthogonal or diagonal base traversal cost.
**/
static double SVG_Nav2_SpanAdjacency_BaseHorizontalCost( const int32_t delta_x, const int32_t delta_y ) {
	/**
	*    Diagonal moves are slightly more expensive than orthogonal moves so later fine A* has a stable geometric bias.
	**/
	return ( delta_x != 0 && delta_y != 0 ) ? NAV2_SPAN_ADJACENCY_DIAGONAL_COST : NAV2_SPAN_ADJACENCY_CARDINAL_COST;
}

/**
*	@brief	Return whether the transition crosses any BSP partition boundary worth soft-penalizing.
*	@param	from_span	Source span.
*	@param	to_span	Destination span.
*	@return	True when the spans cross leaf, cluster, or area partitions.
**/
static const bool SVG_Nav2_SpanAdjacency_CrossesPartitionBoundary( const nav2_span_t &from_span, const nav2_span_t &to_span ) {
	/**
	*    Treat any explicit leaf, cluster, or area boundary crossing as a soft caution signal for later refinement.
	**/
	return ( from_span.leaf_id >= 0 && to_span.leaf_id >= 0 && from_span.leaf_id != to_span.leaf_id )
		|| ( from_span.cluster_id >= 0 && to_span.cluster_id >= 0 && from_span.cluster_id != to_span.cluster_id )
		|| ( from_span.area_id >= 0 && to_span.area_id >= 0 && from_span.area_id != to_span.area_id );
}

/**
*	@brief	Derive conservative boundary semantics for one directed transition.
*	@param	from_span	Source span.
*	@param	to_span	Destination span.
*	@param	delta_x	Signed X offset between sparse columns.
*	@param	delta_y	Signed Y offset between sparse columns.
*	@return	Boundary semantics describing ledge/wall caution or invalidation.
*	@note	This first pass uses only current sparse span metadata and preferred-height relationships. It intentionally errs on the side of caution until richer per-cell edge metadata exists.
**/
static nav2_span_edge_boundary_semantics_t SVG_Nav2_SpanAdjacency_BuildBoundarySemantics( const nav2_span_t &from_span, const nav2_span_t &to_span,
	const int32_t delta_x, const int32_t delta_y ) {
	/**
	*    Start from an empty conservative result and widen it as the relationship demands.
	**/
	nav2_span_edge_boundary_semantics_t semantics = {};
	const double verticalDelta = to_span.preferred_z - from_span.preferred_z;
	const double absoluteVerticalDelta = std::fabs( verticalDelta );
	const bool isDiagonal = ( delta_x != 0 && delta_y != 0 );
	const bool hasVerticalOverlap = SVG_Nav2_SpanAdjacency_HasVerticalOverlap( from_span, to_span );

	/**
	*    Treat missing vertical overlap as a likely ledge-style boundary. Small gaps remain soft-cautioned while large upward or over-cap downward gaps become hard-invalid.
	**/
	if ( !hasVerticalOverlap ) {
		semantics.is_ledge_adjacent = true;
		semantics.apply_soft_penalty = true;
		semantics.soft_penalty_cost += NAV2_SPAN_ADJACENCY_LEDGE_COST_BIAS;
		if ( verticalDelta > PHYS_STEP_MAX_SIZE ) {
			semantics.hard_invalid = true;
		}
		if ( verticalDelta < 0.0 && absoluteVerticalDelta > NAV_DEFAULT_MAX_DROP_HEIGHT ) {
			semantics.hard_invalid = true;
		}
	}

	/**
	*    Treat very tall enclosing spans relative to the current support height as likely wall-adjacent transitions because they suggest a strong vertical barrier near the evaluated edge.
	**/
	if ( to_span.floor_z > ( from_span.preferred_z + PHYS_STEP_MAX_SIZE ) ) {
		semantics.is_wall_adjacent = true;
		semantics.apply_soft_penalty = true;
		semantics.soft_penalty_cost += NAV2_SPAN_ADJACENCY_WALL_COST_BIAS;
		if ( !hasVerticalOverlap ) {
			semantics.hard_invalid = true;
		}
	}

	/**
	*    Diagonal transitions near any boundary remain legal for now but deserve additional caution because they are more likely to clip corners or skim unsupported ledges.
	**/
	if ( isDiagonal && ( semantics.is_ledge_adjacent || semantics.is_wall_adjacent ) ) {
		semantics.apply_soft_penalty = true;
		semantics.soft_penalty_cost += NAV2_SPAN_ADJACENCY_DIAGONAL_CAUTION_COST_BIAS;
	}

	/**
	*    Cross-partition moves remain traversable in this pass but carry a modest penalty because they are more likely to need later legality confirmation.
	**/
	if ( SVG_Nav2_SpanAdjacency_CrossesPartitionBoundary( from_span, to_span ) ) {
		semantics.apply_soft_penalty = true;
		semantics.soft_penalty_cost += NAV2_SPAN_ADJACENCY_PARTITION_CROSS_COST_BIAS;
	}

	/**
	*    A hard-invalid result overrides any softer interpretation because later search should not expand this edge at all.
	**/
	if ( semantics.hard_invalid ) {
		semantics.apply_soft_penalty = false;
		semantics.soft_penalty_cost = 0.0;
	}
	return semantics;
}

/**
*	@brief	Validate one directed span transition with conservative trace and contents probes.
*	@param	from_span	Source span metadata.
*	@param	to_span	Destination span metadata.
*	@param	from_column	Source sparse-column metadata.
*	@param	to_column	Destination sparse-column metadata.
*	@param	grid	Resolved span-grid sizing metadata.
*	@return	Validation result carrying trace/contents invalidation flags.
**/
static nav2_span_transition_validation_t SVG_Nav2_ValidateSpanTransition( const nav2_span_t &from_span, const nav2_span_t &to_span,
	const nav2_span_column_t &from_column, const nav2_span_column_t &to_column, const nav2_span_grid_t &grid ) {
	/**
	*    Start from a pass-through result and only mark explicit invalidation signals when probes fail.
	**/
	nav2_span_transition_validation_t validation = {};

	/**
	*    Require collision imports and valid grid sizing before attempting any runtime legality probes.
	**/
	if ( !gi.trace || !gi.CM_BoxContents || grid.cell_size_xy <= 0.0 ) {
		return validation;
	}

	/**
	*    Reconstruct conservative source and destination world points from sparse-column coordinates and preferred heights.
	**/
	const Vector3 fromPoint = {
		( float )( ( ( double )from_column.tile_x + 0.5 ) * grid.cell_size_xy ),
		( float )( ( ( double )from_column.tile_y + 0.5 ) * grid.cell_size_xy ),
		( float )from_span.preferred_z
	};
	const Vector3 toPoint = {
		( float )( ( ( double )to_column.tile_x + 0.5 ) * grid.cell_size_xy ),
		( float )( ( ( double )to_column.tile_y + 0.5 ) * grid.cell_size_xy ),
		( float )to_span.preferred_z
	};

	/**
	*    Run a conservative hull trace between span centers to reject transitions that are blocked by playersolid geometry.
	**/
	const float halfExtentXY = ( float )std::max( 8.0, grid.cell_size_xy * 0.25 );
	const float feetToOriginOffsetZ = SVG_Nav2_SpanAdjacency_GetFeetToOriginOffsetZ();
	const Vector3 hullMins = SVG_Nav2_SpanAdjacency_GetValidationHullMins( halfExtentXY );
	const Vector3 hullMaxs = SVG_Nav2_SpanAdjacency_GetValidationHullMaxs( halfExtentXY );
	Vector3 fromTraceOrigin = fromPoint;
	fromTraceOrigin.z += feetToOriginOffsetZ;
	Vector3 toTraceOrigin = toPoint;
	toTraceOrigin.z += feetToOriginOffsetZ;
	const cm_trace_t transitionTrace = gi.trace( &fromTraceOrigin, &hullMins, &hullMaxs, &toTraceOrigin, nullptr, CM_CONTENTMASK_PLAYERSOLID );
	if ( transitionTrace.startsolid || transitionTrace.allsolid || transitionTrace.fraction < 1.0f ) {
		validation.trace_invalid = true;
	}

	/**
	*    Sample destination standing contents so obviously solid end volumes are rejected even when the center-to-center trace succeeds.
	**/
	const Vector3 destinationOrigin = {
		toPoint.x,
		toPoint.y,
		toPoint.z + feetToOriginOffsetZ
	};
	const Vector3 destinationMins = QM_Vector3Add( destinationOrigin, hullMins );
	const Vector3 destinationMaxs = QM_Vector3Add( destinationOrigin, hullMaxs );
	cm_contents_t destinationContents = CONTENTS_NONE;
	mleaf_t *leafList[ 8 ] = {};
	mnode_t *topNode = nullptr;
	const vec3_t destinationMinsVec = { destinationMins.x, destinationMins.y, destinationMins.z };
	const vec3_t destinationMaxsVec = { destinationMaxs.x, destinationMaxs.y, destinationMaxs.z };
	(void)gi.CM_BoxContents( destinationMinsVec, destinationMaxsVec, &destinationContents, leafList, 8, &topNode );
	if ( ( destinationContents & CONTENTS_SOLID ) != 0 ) {
		validation.contents_invalid = true;
	}

	return validation;
}

/**
*	@brief	Append a deterministic adjacency edge into the output buffer.
*	@param	from_column_index	Source sparse-column index.
*	@param	from_span_index	Source span index within the source sparse column.
*	@param	from_span		Source span metadata.
*	@param	to_column_index	Destination sparse-column index.
*	@param	to_span_index	Destination span index within the destination sparse column.
*	@param	to_span		Destination span metadata.
*	@param	delta_x		Signed X offset between sparse columns.
*	@param	delta_y		Signed Y offset between sparse columns.
*	@param	out_adjacency	[in,out] Adjacency output receiving the new edge.
**/
static void SVG_Nav2_SpanAdjacency_AppendEdge( const int32_t from_column_index, const int32_t from_span_index, const nav2_span_t &from_span,
	const int32_t to_column_index, const int32_t to_span_index, const nav2_span_t &to_span, const nav2_span_column_t &from_column,
	const nav2_span_column_t &to_column, const nav2_span_grid_t &grid, const int32_t delta_x, const int32_t delta_y,
	nav2_span_adjacency_t *out_adjacency ) {
	/**
	*    Require output storage before mutating the adjacency buffer.
	**/
	if ( !out_adjacency ) {
		return;
	}

	/**
	*    Derive the signed vertical delta from the preferred standing heights because that is the clearest future-facing movement metric for fine search.
	**/
	const double verticalDelta = to_span.preferred_z - from_span.preferred_z;
	const double absoluteVerticalDelta = std::fabs( verticalDelta );
	const nav2_span_edge_boundary_semantics_t boundarySemantics = SVG_Nav2_SpanAdjacency_BuildBoundarySemantics( from_span, to_span, delta_x, delta_y );
	const nav2_span_transition_validation_t transitionValidation = SVG_Nav2_ValidateSpanTransition( from_span, to_span, from_column, to_column, grid );

	/**
	*    Start from a deterministic empty edge and then widen it with the discovered movement semantics.
	**/
	nav2_span_edge_t edge = {};
	edge.edge_id = ( int32_t )out_adjacency->edges.size();
	edge.from_span_id = from_span.span_id;
	edge.to_span_id = to_span.span_id;
	edge.from_column_index = from_column_index;
	edge.to_column_index = to_column_index;
	edge.from_span_index = from_span_index;
	edge.to_span_index = to_span_index;
	edge.delta_x = delta_x;
	edge.delta_y = delta_y;
	edge.vertical_delta = verticalDelta;
	edge.cost = SVG_Nav2_SpanAdjacency_BaseHorizontalCost( delta_x, delta_y );
	edge.soft_penalty_cost = 0.0;
	edge.movement = nav2_span_edge_movement_t::HorizontalPass;
	edge.legality = nav2_span_edge_legality_t::Passable;
	edge.feature_bits = NAV_EDGE_FEATURE_PASSABLE;
	edge.flags = NAV2_SPAN_EDGE_FLAG_PASSABLE;

	/**
	*    Mark diagonal edges explicitly because later fine search and LOS shortcuts may want to treat them more cautiously.
	**/
	if ( delta_x != 0 && delta_y != 0 ) {
		edge.flags |= NAV2_SPAN_EDGE_FLAG_DIAGONAL;
	}

	/**
	*    Promote trace/contents invalidation after semantic classification so blocked transitions become explicit hard-invalid edges.
	**/
	if ( transitionValidation.trace_invalid || transitionValidation.contents_invalid ) {
		edge.legality = nav2_span_edge_legality_t::HardInvalid;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_HARD_INVALID;
		edge.flags &= ~NAV2_SPAN_EDGE_FLAG_PASSABLE;
		if ( transitionValidation.trace_invalid ) {
			edge.feature_bits |= NAV_TRAVERSAL_FEATURE_TRACE_BLOCKED;
		}
		if ( transitionValidation.contents_invalid ) {
			edge.feature_bits |= NAV_TRAVERSAL_FEATURE_CONTENTS_BLOCKED;
		}
	}

	/**
	*    Mirror topology boundary crossings into the edge flags so future corridor-aware refinement can penalize or audit them without re-deriving the relationship.
	**/
	if ( from_span.leaf_id >= 0 && to_span.leaf_id >= 0 && from_span.leaf_id != to_span.leaf_id ) {
		edge.flags |= NAV2_SPAN_EDGE_FLAG_CROSSES_LEAF;
	}
	if ( from_span.cluster_id >= 0 && to_span.cluster_id >= 0 && from_span.cluster_id != to_span.cluster_id ) {
		edge.flags |= NAV2_SPAN_EDGE_FLAG_CROSSES_CLUSTER;
	}
	if ( from_span.area_id >= 0 && to_span.area_id >= 0 && from_span.area_id != to_span.area_id ) {
		edge.flags |= NAV2_SPAN_EDGE_FLAG_CROSSES_AREA;
	}

	/**
	*    Publish conservative ledge and wall-adjacency semantics before movement classification so later fine search can reason about boundary-hugging transitions explicitly.
	**/
	if ( boundarySemantics.is_ledge_adjacent ) {
		edge.flags |= NAV2_SPAN_EDGE_FLAG_SOURCE_LEDGE_ADJACENT;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_DEST_LEDGE_ADJACENT;
		edge.feature_bits |= NAV_TRAVERSAL_FEATURE_LEDGE_ADJACENCY;
	}
	if ( boundarySemantics.is_wall_adjacent ) {
		edge.flags |= NAV2_SPAN_EDGE_FLAG_SOURCE_WALL_ADJACENT;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_DEST_WALL_ADJACENT;
		edge.feature_bits |= NAV_TRAVERSAL_FEATURE_WALL_ADJACENCY;
	}

	/**
	*    Classify liquid transitions first because they can coexist with vertical movement but still deserve explicit semantic tagging.
	**/
	if ( SVG_Nav2_SpanAdjacency_IsLiquidSpan( from_span ) || SVG_Nav2_SpanAdjacency_IsLiquidSpan( to_span ) ) {
		edge.movement = nav2_span_edge_movement_t::LiquidTransition;
		edge.cost += NAV2_SPAN_ADJACENCY_LIQUID_COST_BIAS;
		if ( ( to_span.movement_flags & NAV_FLAG_WATER ) != 0 ) {
			edge.flags |= NAV2_SPAN_EDGE_FLAG_ENTERS_WATER;
			edge.feature_bits |= NAV_EDGE_FEATURE_ENTERS_WATER;
		}
		if ( ( to_span.movement_flags & NAV_FLAG_LAVA ) != 0 ) {
			edge.flags |= NAV2_SPAN_EDGE_FLAG_ENTERS_LAVA;
			edge.feature_bits |= NAV_EDGE_FEATURE_ENTERS_LAVA;
		}
		if ( ( to_span.movement_flags & NAV_FLAG_SLIME ) != 0 ) {
			edge.flags |= NAV2_SPAN_EDGE_FLAG_ENTERS_SLIME;
			edge.feature_bits |= NAV_EDGE_FEATURE_ENTERS_SLIME;
		}
	}

	/**
	*    Map conservative vertical deltas onto horizontal pass, stair-step, or controlled-drop semantics.
	**/
	if ( absoluteVerticalDelta < PHYS_STEP_MIN_SIZE ) {
		/**
		*    Keep the default horizontal pass semantics when the spans are effectively on the same band.
		**/
	} else if ( verticalDelta > 0.0 && absoluteVerticalDelta <= PHYS_STEP_MAX_SIZE ) {
		/**
		*    Treat modest upward vertical deltas as stair-step-up traversal.
		**/
		edge.movement = nav2_span_edge_movement_t::StepUp;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_STEP_UP;
		edge.feature_bits |= NAV_EDGE_FEATURE_STAIR_STEP_UP;
	} else if ( verticalDelta < 0.0 && absoluteVerticalDelta <= PHYS_STEP_MAX_SIZE ) {
		/**
		*    Treat modest downward vertical deltas as stair-step-down traversal.
		**/
		edge.movement = nav2_span_edge_movement_t::StepDown;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_STEP_DOWN;
		edge.feature_bits |= NAV_EDGE_FEATURE_STAIR_STEP_DOWN;
	} else if ( verticalDelta < 0.0 && absoluteVerticalDelta <= NAV_DEFAULT_MAX_DROP_HEIGHT ) {
		/**
		*    Allow larger downward transitions only as explicit controlled-drop / walk-off edges.
		**/
		edge.movement = nav2_span_edge_movement_t::ControlledDrop;
		edge.cost += NAV2_SPAN_ADJACENCY_DROP_COST_BIAS;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_CONTROLLED_DROP;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_OPTIONAL_WALK_OFF;
		edge.feature_bits |= NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
	} else {
		/**
		*    Mark anything steeper than the conservative drop cap as forbidden for the current first-pass adjacency rules.
		**/
		edge.flags &= ~NAV2_SPAN_EDGE_FLAG_PASSABLE;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_FORBIDDEN_WALK_OFF;
		edge.feature_bits |= NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF;
		edge.legality = nav2_span_edge_legality_t::HardInvalid;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_HARD_INVALID;
	}

	/**
	*    Promote boundary-driven hard invalidation after the coarse movement class is assigned so unsupported climbs and blocked ledges cannot slip through as passable edges.
	**/
	if ( boundarySemantics.hard_invalid ) {
		edge.legality = nav2_span_edge_legality_t::HardInvalid;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_HARD_INVALID;
		edge.flags &= ~NAV2_SPAN_EDGE_FLAG_PASSABLE;
		if ( ( edge.flags & NAV2_SPAN_EDGE_FLAG_OPTIONAL_WALK_OFF ) != 0 ) {
			edge.flags &= ~NAV2_SPAN_EDGE_FLAG_OPTIONAL_WALK_OFF;
			edge.flags |= NAV2_SPAN_EDGE_FLAG_FORBIDDEN_WALK_OFF;
			edge.feature_bits &= ~NAV_EDGE_FEATURE_OPTIONAL_WALK_OFF;
			edge.feature_bits |= NAV_EDGE_FEATURE_FORBIDDEN_WALK_OFF;
		}
	}

	/**
	*    Apply soft penalties only when the edge remains legal, making the distinction between passable-but-costly and fully invalid transitions explicit.
	**/
	if ( edge.legality != nav2_span_edge_legality_t::HardInvalid && boundarySemantics.apply_soft_penalty ) {
		edge.legality = nav2_span_edge_legality_t::SoftPenalized;
		edge.flags |= NAV2_SPAN_EDGE_FLAG_SOFT_PENALIZED;
		edge.soft_penalty_cost += boundarySemantics.soft_penalty_cost;
	}

	/**
	*    Fold the accumulated soft penalty into the published traversal cost only after every penalty source has been processed.
	**/
	if ( edge.legality == nav2_span_edge_legality_t::SoftPenalized ) {
		edge.cost += edge.soft_penalty_cost;
	}

	/**
	*    Publish the deterministic edge after all semantics have been derived.
	**/
	out_adjacency->edges.push_back( edge );
}


/**
*
*
*	Nav2 Span Adjacency Public API:
*
*
**/
/**
*	@brief	Build local span adjacency from a sparse span grid.
*	@param	grid	Sparse span grid to analyze.
*	@param	out_adjacency	[out] Adjacency output.
*	@return	True when adjacency was built successfully.
*	@note	This Milestone 4 pass is intentionally conservative: it derives deterministic 8-neighbor local connectivity from span height overlap, legality tiers, and ledge/wall boundary semantics without consulting oldnav or live-world mutation paths.
**/
const bool SVG_Nav2_BuildSpanAdjacency( const nav2_span_grid_t &grid, nav2_span_adjacency_t *out_adjacency ) {
	/**
	*    Sanity check: require output storage before mutating the adjacency result.
	**/
	if ( !out_adjacency ) {
		return false;
	}

	/**
	*    Reset adjacency so future work can append validated edges without stale state.
	**/
	*out_adjacency = {};
	if ( !SVG_Nav2_SpanGridHasColumns( grid ) ) {
		return true;
	}

	/**
	*    Build a deterministic sparse-column lookup table so local XY neighbors can be resolved without repeated linear scans.
	**/
	std::unordered_map<nav2_span_column_lookup_key_t, int32_t, nav2_span_column_lookup_key_hash_t> columnIndexOf = {};
	columnIndexOf.reserve( grid.columns.size() );
	for ( int32_t columnIndex = 0; columnIndex < ( int32_t )grid.columns.size(); columnIndex++ ) {
		// Publish the sparse column under its explicit XY coordinate pair.
		const nav2_span_column_t &column = grid.columns[ ( size_t )columnIndex ];
		columnIndexOf[ { column.tile_x, column.tile_y } ] = columnIndex;
	}

	/**
	*    Walk every sparse column in published order so edge generation remains deterministic for serialization and compare-mode diagnostics.
	**/
	for ( int32_t fromColumnIndex = 0; fromColumnIndex < ( int32_t )grid.columns.size(); fromColumnIndex++ ) {
		// Resolve the current source sparse column once for the nested span and neighbor loops.
		const nav2_span_column_t &fromColumn = grid.columns[ ( size_t )fromColumnIndex ];

		/**
		*    Iterate each source span so local connectivity is evaluated per vertical band rather than per column only.
		**/
		for ( int32_t fromSpanIndex = 0; fromSpanIndex < ( int32_t )fromColumn.spans.size(); fromSpanIndex++ ) {
			// Resolve the current source span for neighbor comparisons.
			const nav2_span_t &fromSpan = fromColumn.spans[ ( size_t )fromSpanIndex ];
			if ( !SVG_Nav2_SpanIsValid( fromSpan ) ) {
				continue;
			}

			/**
			*    Evaluate the 8 neighboring sparse columns around the current source column.
			**/
			for ( int32_t deltaY = -NAV2_SPAN_ADJACENCY_NEIGHBOR_RADIUS; deltaY <= NAV2_SPAN_ADJACENCY_NEIGHBOR_RADIUS; deltaY++ ) {
				for ( int32_t deltaX = -NAV2_SPAN_ADJACENCY_NEIGHBOR_RADIUS; deltaX <= NAV2_SPAN_ADJACENCY_NEIGHBOR_RADIUS; deltaX++ ) {
					// Skip the zero offset because a span does not need a self-edge in this first local adjacency pass.
					if ( deltaX == 0 && deltaY == 0 ) {
						continue;
					}

					// Resolve the neighboring sparse column through the XY lookup map and skip empty space quickly.
					const nav2_span_column_lookup_key_t neighborKey = { fromColumn.tile_x + deltaX, fromColumn.tile_y + deltaY };
					const auto neighborColumnIt = columnIndexOf.find( neighborKey );
					if ( neighborColumnIt == columnIndexOf.end() ) {
						continue;
					}

					// Resolve the neighboring sparse column once and compare every destination span against the source span.
					const int32_t toColumnIndex = neighborColumnIt->second;
					const nav2_span_column_t &toColumn = grid.columns[ ( size_t )toColumnIndex ];
					for ( int32_t toSpanIndex = 0; toSpanIndex < ( int32_t )toColumn.spans.size(); toSpanIndex++ ) {
						// Resolve the candidate destination span and reject invalid spans immediately.
						const nav2_span_t &toSpan = toColumn.spans[ ( size_t )toSpanIndex ];
						if ( !SVG_Nav2_SpanIsValid( toSpan ) ) {
							continue;
						}

						/**
						*    Reject span pairs that do not overlap vertically at all because they cannot support direct same-band or step-based traversal.
						**/
						if ( !SVG_Nav2_SpanAdjacency_HasVerticalOverlap( fromSpan, toSpan ) ) {
							// Permit deliberate drop-style links when the destination is safely below but not when it is above with no overlap.
							const double downwardDelta = fromSpan.preferred_z - toSpan.preferred_z;
							if ( downwardDelta <= PHYS_STEP_MAX_SIZE || downwardDelta > NAV_DEFAULT_MAX_DROP_HEIGHT ) {
								continue;
							}
						}

						/**
						*    Append the deterministic adjacency edge after conservative local movement semantics are derived.
						**/
                        SVG_Nav2_SpanAdjacency_AppendEdge( fromColumnIndex, fromSpanIndex, fromSpan, toColumnIndex, toSpanIndex, toSpan,
							fromColumn, toColumn, grid, deltaX, deltaY, out_adjacency );
					}
				}
			}
		}
	}

	/**
	*    The adjacency pass completed successfully, even if the conservative rules produced zero edges for the current span grid.
	**/
	return true;
}

/**
*	@brief	Build a bounded summary for one adjacency result.
*	@param	adjacency	Adjacency output to summarize.
*	@param	out_summary	[out] Receives the compact summary statistics.
*	@return	True when the summary was produced.
**/
const bool SVG_Nav2_BuildSpanAdjacencySummary( const nav2_span_adjacency_t &adjacency, nav2_span_adjacency_summary_t *out_summary ) {
	/**
	*    Require output storage before emitting summary counters.
	**/
	if ( !out_summary ) {
		return false;
	}
	*out_summary = {};

	/**
	*    Walk generated edges once and publish bounded legality/movement statistics.
	**/
	out_summary->edge_count = ( int32_t )adjacency.edges.size();
	for ( const nav2_span_edge_t &edge : adjacency.edges ) {
		if ( edge.legality == nav2_span_edge_legality_t::Passable ) {
			out_summary->passable_count++;
		} else if ( edge.legality == nav2_span_edge_legality_t::SoftPenalized ) {
			out_summary->soft_penalized_count++;
		} else if ( edge.legality == nav2_span_edge_legality_t::HardInvalid ) {
			out_summary->hard_invalid_count++;
		}

		if ( ( edge.feature_bits & NAV_TRAVERSAL_FEATURE_TRACE_BLOCKED ) != 0 ) {
			out_summary->trace_invalidated_count++;
		}
		if ( ( edge.feature_bits & NAV_TRAVERSAL_FEATURE_CONTENTS_BLOCKED ) != 0 ) {
			out_summary->contents_invalidated_count++;
		}
		if ( edge.movement == nav2_span_edge_movement_t::ControlledDrop ) {
			out_summary->controlled_drop_count++;
		}
		if ( edge.movement == nav2_span_edge_movement_t::LiquidTransition ) {
			out_summary->liquid_transition_count++;
		}
	}

	return true;
}
