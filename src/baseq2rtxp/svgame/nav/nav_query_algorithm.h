/********************************************************************
*
*	ServerGame: Immutable Navigation Query Algorithms
*	Pure geometry/search over copied data and caller-owned, preallocated buffers.
*	No engine imports, entity callbacks, global scratch, allocation or logging.
*
********************************************************************/
#pragma once
#include "nav_query_control.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <span>

/**
*	@brief	Pointer-free double-precision feet-space coordinate used in copied query data.
**/
struct nav_query_point_t {
	//! Cartesian world-space coordinates.
	double x = 0.0, y = 0.0, z = 0.0;
};

/**
*	@brief	Copied face geometry and policy-independent island identity.
**/
struct nav_query_face_t {
	//! First contiguous half-edge and the number of polygon sides.
	int32_t first_edge = 0, num_edges = 0;
	//! Query geometry; no pointers to source faces or entities.
	nav_query_point_t center = {}, normal = {};
	//! Policy-independent reachability component, or -1 when unavailable.
	int32_t component = -1;
	//! Material exclusion copied before publication.
	bool excluded = false;
};

/**
*	@brief	Copied half-edge with owner-precomputed portal overlap and dynamic state.
**/
struct nav_query_edge_t {
	//! Source vertex, next half-edge, and adjacent face; -1 means no neighbor.
	int32_t vertex = -1, next = -1, neighbor = -1;
	//! Upward rise, physical aperture width and copied portal endpoints.
	double rise = 0.0, width = 0.0;
	nav_query_point_t left = {}, right = {};
	//! Both sides' disabled flags are combined by the owner before publication.
	bool disabled = false;
	//! True only when the owner found a nondegenerate native-topology portal overlap.
	bool has_portal = false;
};

/**
*	@brief	Pointer-free KD node preserving existing split-coordinate and leaf semantics.
**/
struct nav_query_node_t {
	//! Leaf face span, or -1 for an internal node.
	int32_t first_face = -1, num_faces = 0;
	//! Child indices and authoritative split axis/position.
	int32_t left = -1, right = -1, axis = 0;
	double split = 0.0;
};

/**
*	@brief	Read-only view borrowed from an owner-pinned immutable snapshot.
*	@note	Only the designated owner may publish, pin/release or reclaim backing storage.
**/
struct nav_query_mesh_view_t {
	//! Snapshot identities; face/edge indices have meaning only within this pair.
	uint64_t generation = 0, topology = 0;
	//! Immutable geometry with no live entity/collision dependencies.
	std::span<const nav_query_point_t> vertices;
	std::span<const nav_query_face_t> faces;
	std::span<const nav_query_edge_t> edges;
	std::span<const nav_query_node_t> nodes;
};

/**
*	@brief	Copied cost preferences supported by the current base and debug-dummy callers.
*	@note	Unknown virtual policies stay in an explicit synchronous compatibility phase.
**/
struct nav_query_cost_policy_t {
	//! Existing committed-corridor discount, applied only to owned membership bytes.
	double corridor_multiplier = 0.85;
	//! Existing scared-dummy stair discount and its inclusive rise interval.
	double stair_multiplier = 1.0, stair_min = 4.0, stair_max = 18.25;
};

/**
*	@brief	Full scalar request identity; transient queries are never serialized with entities.
**/
struct nav_query_identity_t {
	//! Monotonic sequence and slot incarnation are independent of visitation stamps.
	uint64_t sequence = 0, incarnation = 0, epoch = 0;
	//! Revisions checked by the owner before a future route commit.
	uint64_t goal = 0, state = 0, crowd = 0, generation = 0, topology = 0;
	//! Entity slot; -1 denotes a synchronous diagnostic query with no entity commit.
	int32_t entity = -1;
};

/**
*	@brief	Structured search outcome distinguishes resource exhaustion from infeasibility.
**/
enum class nav_query_status_t : uint8_t {
	invalid_input, stale_snapshot, no_route, resource_limited, found, cancelled
};

/**
*	@brief	Owned request payload; buffer storage must be prepared on the engine-safe owner.
*	@tparam	Buffer	Tagged production buffer or instrumented standalone test buffer.
**/
template<template<typename> class Buffer>
struct nav_query_request_t {
	//! Exact requested feet endpoints and caller-selected face hints.
	nav_query_point_t start = {}, goal = {};
	int32_t start_face = -1, goal_face = -1;
	//! Copied physical hull, shape and movement policy needed by later native validation.
	nav_query_point_t mins = { -16.0, -16.0, -36.0 }, maxs = { 16.0, 16.0, 36.0 };
	int32_t shape = 1;
	double radius = 16.0, step = 18.25, drop = 128.0, min_normal = 0.7;
	//! Explicit topology policy and copied cost scalars; no virtual/function/entity pointers.
	bool ignore_disabled = false;
	nav_query_cost_policy_t cost;
	nav_query_identity_t identity;
	//! Indexed, owned corridor membership; an empty buffer means no prior preference.
	Buffer<uint8_t> prior_faces;
	//! Hard search work budgets; M6 must derive service limits from measured high-water marks.
	uint64_t max_expansions = std::numeric_limits<uint64_t>::max();
	uint64_t max_edge_tests = std::numeric_limits<uint64_t>::max();
};

/**
*	@brief	Query-local A* face state with constant-time ordinary stamp invalidation.
**/
struct nav_query_face_state_t {
	//! Wrapping query-local stamp; rollover clears only this context's state.
	uint32_t stamp = 0;
	double score = 0.0;
	int32_t predecessor = -1;
};

/**
*	@brief	Heap entry reproducing the synchronous reference's priority ordering.
**/
struct nav_query_heap_node_t {
	//! Face and weighted priority; stale entries are rejected against current scores.
	int32_t face = -1;
	double priority = 0.0;
};

/**
*	@brief	All mutable search state belongs exclusively to one context during a query.
*	@note	No growth occurs in the solver; the owner sizes states and reserves heap storage.
**/
template<template<typename> class Buffer>
struct nav_query_context_t {
	//! Retained face states and heap; sharing a snapshot does not share these writes.
	Buffer<nav_query_face_state_t> states;
	Buffer<nav_query_heap_node_t> heap;
	//! Independent visitation token and request-local telemetry.
	uint32_t stamp = 0;
	uint64_t expansions = 0, stale_pops = 0, edge_tests = 0, accepted = 0;
	uint64_t disabled = 0, narrow = 0, rises = 0, drops = 0, peak_heap = 0;
};

/**
*	@brief	Owned counters identify the graph work that produced each terminal reply.
**/
struct nav_query_diagnostics_t {
	//! Work includes checkpoints; expansion, stale-pop and edge counters preserve their original meanings.
	uint64_t work = 0, expansions = 0, stale_pops = 0, edge_tests = 0, accepted = 0, peak_heap = 0;
	//! Policy rejection counters survive reuse of the worker context.
	uint64_t disabled = 0, narrow = 0, rises = 0, drops = 0;
};

/**
*	@brief	Owned search result preserving request identity and exact requested endpoints.
**/
template<template<typename> class Buffer>
struct nav_query_result_t {
	//! Caller-owned output capacity; a limit leaves no partial corridor to mistake for success.
	Buffer<int32_t> corridor;
	//! Terminal status, original endpoints/identity and realized graph cost.
	nav_query_status_t status = nav_query_status_t::invalid_input;
	//! Limit reason and owned counters survive reuse of the exclusive search context.
	nav_query_limit_reason_t limit_reason = nav_query_limit_reason_t::none;
	nav_query_diagnostics_t diagnostics;
	nav_query_point_t start = {}, goal = {};
	nav_query_identity_t identity;
	double cost = 0.0;
};

/**
*	@brief	Measure reference-compatible Euclidean distance using copied double coordinates.
*	@return	Distance; invalid numerical input is rejected by request/snapshot preparation.
**/
inline double Nav_QueryDistance( const nav_query_point_t &a, const nav_query_point_t &b ) {
	// Preserve the original sum/order rather than changing search ties with a different norm.
	const double x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
	return std::sqrt( x * x + y * y + z * z );
}

/**
*	@brief	Validate finite coordinates before geometry or search arithmetic.
*	@return	True only when all components are finite.
**/
inline bool Nav_QueryFinite( const nav_query_point_t &point ) {
	// Nonfinite endpoints must never reach a KD split or heap comparator.
	return std::isfinite( point.x ) && std::isfinite( point.y ) && std::isfinite( point.z );
}

/**
*	@brief	Advance a service revision without silently reusing an ancient identity.
*	@param	revision	Owner's sequence/incarnation/revision counter.
*	@return	False requires a controlled service drain/reset before accepting new work.
**/
inline bool Nav_QueryAdvanceRevision( uint64_t &revision ) {
	// Unlike visitation stamps, service identities may not silently wrap and compare equal.
	if ( revision == std::numeric_limits<uint64_t>::max() ) {
		return false;
	}
	++revision;
	return true;
}

/**
*	@brief	Resolve a leaf with bounded traversal and no writes to snapshot nodes.
*	@param	mesh	Pinned immutable mesh view.
*	@param	point	Finite world/feet-space point.
*	@return	Leaf index or -1 for empty/invalid topology.
**/
inline int32_t Nav_QueryFindLeaf( const nav_query_mesh_view_t &mesh, const nav_query_point_t &point ) {
	/**
	*	Reject nonfinite input and bound traversal so a malformed node cycle cannot hang.
	**/
	if ( mesh.nodes.empty() || !Nav_QueryFinite( point ) ) {
		return -1;
	}
	int32_t index = 0;
	// A valid acyclic tree cannot visit more nodes than it owns.
	for ( size_t visits = 0; visits < mesh.nodes.size(); ++visits ) {
		if ( index < 0 || static_cast<size_t>( index ) >= mesh.nodes.size() ) {
			return -1;
		}
		const auto &node = mesh.nodes[ index ];
		if ( node.left == -1 && node.right == -1 ) {
			return index;
		}
		if ( node.axis < 0 || node.axis > 2 || !std::isfinite( node.split ) ) {
			return -1;
		}
		const double coordinate = node.axis == 0 ? point.x : ( node.axis == 1 ? point.y : point.z );
		// Preserve existing single-child and exact split-position selection.
		const bool left = node.left != -1 && ( node.right == -1 || coordinate <= node.split );
		index = left ? node.left : node.right;
	}
	return -1;
}

/**
*	@brief	Test convex XY containment at the existing half-unit edge-contact tolerance.
*	@param	mesh	Pinned immutable geometry.
*	@param	point	Finite feet-space point.
*	@param	face	Face index within that snapshot.
*	@return	True for a contained point; invalid topology fails closed.
**/
inline bool Nav_QueryInsideFace( const nav_query_mesh_view_t &mesh, const nav_query_point_t &point, const int32_t face ) {
	/**
	*	Validate indexed storage before reading polygon sides.
	**/
	if ( face < 0 || static_cast<size_t>( face ) >= mesh.faces.size() || !Nav_QueryFinite( point ) ) {
		return false;
	}
	const auto &polygon = mesh.faces[ face ];
	if ( polygon.first_edge < 0 || polygon.num_edges < 3 ||
		 static_cast<size_t>( polygon.first_edge ) + static_cast<size_t>( polygon.num_edges ) > mesh.edges.size() ) {
		return false;
	}
	double winding = 0.0;
	// Each side must agree in winding; genuine rounding contact retains the reference tolerance.
	for ( int32_t side = 0; side < polygon.num_edges; ++side ) {
		const auto &edge = mesh.edges[ polygon.first_edge + side ];
		if ( edge.vertex < 0 || edge.next < 0 || static_cast<size_t>( edge.next ) >= mesh.edges.size() ) {
			return false;
		}
		const int32_t next_vertex = mesh.edges[ edge.next ].vertex;
		if ( next_vertex < 0 || static_cast<size_t>( edge.vertex ) >= mesh.vertices.size() || static_cast<size_t>( next_vertex ) >= mesh.vertices.size() ) {
			return false;
		}
		const auto &a = mesh.vertices[ edge.vertex ], &b = mesh.vertices[ next_vertex ];
		const double dx = b.x - a.x, dy = b.y - a.y;
		const double cross = dx * ( point.y - a.y ) - dy * ( point.x - a.x );
		if ( std::fabs( cross ) <= 0.5 * std::sqrt( dx * dx + dy * dy ) ) {
			continue;
		}
		const double sign = cross > 0.0 ? 1.0 : -1.0;
		if ( winding == 0.0 ) {
			winding = sign;
		} else if ( sign != winding ) {
			return false;
		}
	}
	return true;
}

/**
*	@brief	Resolve strict/local or nearest fallback faces using copied geometry only.
*	@param	strict	True forbids global/nearest fallback, matching existing strict lookup.
*	@return	Snapshot face index, or -1 when no accepted candidate exists.
**/
inline int32_t Nav_QueryFindFace( const nav_query_mesh_view_t &mesh, const nav_query_point_t &point, const bool strict ) {
	/**
	*	Select a leaf span; malformed/absent spans fail strict queries but permit the reference global fallback.
	**/
	if ( !Nav_QueryFinite( point ) ) {
		return -1;
	}
	const int32_t leaf = Nav_QueryFindLeaf( mesh, point );
	const auto scan = [&]( const size_t first, const size_t count, const bool nearest ) {
		int32_t inside = -1, fallback = -1;
		double plane_best = 64.0, center_best = 999999.0 * 999999.0;
		// Contiguous candidates require no mesh stamps or shared deduplication writes.
		for ( size_t offset = 0; offset < count && first + offset < mesh.faces.size(); ++offset ) {
			const size_t index = first + offset;
			const auto &face = mesh.faces[ index ];
			if ( face.excluded || face.first_edge < 0 || static_cast<size_t>( face.first_edge ) >= mesh.edges.size() ) {
				continue;
			}
			const int32_t vertex = mesh.edges[ face.first_edge ].vertex;
			if ( vertex < 0 || static_cast<size_t>( vertex ) >= mesh.vertices.size() ) {
				continue;
			}
			const auto &origin = mesh.vertices[ vertex ];
			const double distance = std::fabs( ( point.x * face.normal.x + point.y * face.normal.y + point.z * face.normal.z ) -
				( origin.x * face.normal.x + origin.y * face.normal.y + origin.z * face.normal.z ) );
			if ( distance < plane_best && Nav_QueryInsideFace( mesh, point, static_cast<int32_t>( index ) ) ) {
				inside = static_cast<int32_t>( index );
				plane_best = distance;
			}
			const double dx = point.x - face.center.x, dy = point.y - face.center.y, dz = point.z - face.center.z;
			const double squared = dx * dx + dy * dy + dz * dz;
			if ( nearest && squared < center_best ) {
				fallback = static_cast<int32_t>( index );
				center_best = squared;
			}
		}
		return inside >= 0 ? inside : fallback;
	};
	if ( leaf >= 0 ) {
		const auto &node = mesh.nodes[ leaf ];
		if ( node.first_face >= 0 && node.num_faces >= 0 && static_cast<size_t>( node.first_face ) < mesh.faces.size() ) {
			const int32_t local = scan( static_cast<size_t>( node.first_face ), static_cast<size_t>( node.num_faces ), false );
			if ( local >= 0 || strict ) {
				return local;
			}
		}
	}
	return strict ? -1 : scan( 0, mesh.faces.size(), true );
}

/**
*	@brief	Extract and merge owner-precomputed portal segments without collision or cache mutation.
*	@return	True for a nondegenerate overlap; exact original topology endpoints are preserved.
**/
inline bool Nav_QueryPortal( const nav_query_mesh_view_t &mesh, const int32_t from, const int32_t to,
	nav_query_point_t *left, nav_query_point_t *right ) {
	/**
	*	Reject invalid outputs and face identities before scanning the directed face boundary.
	**/
	if ( left == nullptr || right == nullptr || from < 0 || to < 0 ||
		 static_cast<size_t>( from ) >= mesh.faces.size() || static_cast<size_t>( to ) >= mesh.faces.size() ) {
		return false;
	}
	const auto &face = mesh.faces[ from ];
	if ( face.first_edge < 0 || face.num_edges < 0 || static_cast<size_t>( face.first_edge ) + static_cast<size_t>( face.num_edges ) > mesh.edges.size() ) {
		return false;
	}
	bool found = false;
	nav_query_point_t base = {}, direction = {}, merged_left = {}, merged_right = {};
	double minimum = 0.0, maximum = 0.0;
	// Multiple collinear subportals retain the reference's directed union semantics.
	for ( int32_t side = 0; side < face.num_edges; ++side ) {
		const auto &edge = mesh.edges[ face.first_edge + side ];
		if ( edge.neighbor != to || !edge.has_portal ) {
			continue;
		}
		if ( !found ) {
			found = true;
			base = edge.right;
			merged_left = edge.left;
			merged_right = edge.right;
			const double length = Nav_QueryDistance( edge.left, edge.right );
			if ( length <= 0.0001 ) {
				return false;
			}
			const double inverse = 1.0 / length;
			direction = { ( edge.left.x - base.x ) * inverse, ( edge.left.y - base.y ) * inverse, ( edge.left.z - base.z ) * inverse };
			maximum = length;
		} else {
			const auto projection = [&]( const nav_query_point_t &point ) {
				return ( point.x - base.x ) * direction.x + ( point.y - base.y ) * direction.y + ( point.z - base.z ) * direction.z;
			};
			minimum = std::min( minimum, std::min( projection( edge.left ), projection( edge.right ) ) );
			maximum = std::max( maximum, std::max( projection( edge.left ), projection( edge.right ) ) );
			merged_right = { base.x + direction.x * minimum, base.y + direction.y * minimum, base.z + direction.z * minimum };
			merged_left = { base.x + direction.x * maximum, base.y + direction.y * maximum, base.z + direction.z * maximum };
		}
	}
	if ( found ) {
		*right = merged_right;
		*left = merged_left;
	}
	return found;
}

/**
*	@brief	Run reference-compatible weighted A* without allocation, imports or shared writes.
*	@param	mesh	Pinned copied mesh; generation/topology must match the request.
*	@param	context	Exclusive mutable state with owner-prepared storage/capacity.
*	@param	request	Owned scalar policy and prior-corridor membership.
*	@param	result	Exclusive owned result with face-count corridor capacity.
*	@return	Structured status; every failure clears output instead of exposing a partial route.
*	@note	This preserves weighted reference behavior; it does not assert an optimal/admissible heuristic.
**/
template<template<typename> class Buffer>
nav_query_status_t Nav_QueryFindPath( const nav_query_mesh_view_t &mesh, nav_query_context_t<Buffer> &context,
	const nav_query_request_t<Buffer> &request, nav_query_result_t<Buffer> &result, nav_query_execution_control_t *control = nullptr ) {
	/**
	*	Reset only caller-owned state and retain exact endpoint/request identity even on failure.
	**/
	result.corridor.clear();
	result.start = request.start;
	result.goal = request.goal;
	result.identity = request.identity;
	result.cost = 0.0;
	result.status = nav_query_status_t::invalid_input;
	context.heap.clear();
	context.expansions = context.stale_pops = context.edge_tests = context.accepted = 0;
	context.disabled = context.narrow = context.rises = context.drops = context.peak_heap = 0;
	result.limit_reason = nav_query_limit_reason_t::none;
	/**
	*	Copy terminal counters on every return without allocation or shared state.
	**/
	struct counters_scope_t {
		nav_query_context_t<Buffer> &context;
		nav_query_result_t<Buffer> &result;
		nav_query_execution_control_t *control;
		/**
		*	@brief	Publish final exclusive-context counters into owned result storage.
		**/
		~counters_scope_t() {
			// A null execution control represents an unlimited synchronous query.
			result.diagnostics = { control != nullptr ? control->work : 0, context.expansions, context.stale_pops,
				context.edge_tests, context.accepted, context.peak_heap, context.disabled, context.narrow, context.rises, context.drops };
		}
	} counters{ context, result, control };
	// Cancellation and time limits never expose an unfinished predecessor chain.
	const auto interrupted = [&]( const bool force = false ) {
		if ( !Nav_QueryCheckpoint( control, force ) ) {
			return false;
		}
		result.corridor.clear();
		result.status = control->cancelled ? nav_query_status_t::cancelled : nav_query_status_t::resource_limited;
		result.limit_reason = control->cancelled ? nav_query_limit_reason_t::none : control->reason;
		return true;
	};
	if ( interrupted( true ) ) {
		return result.status;
	}
	const int32_t start = request.start_face, goal = request.goal_face;
	if ( start < 0 || goal < 0 || static_cast<size_t>( start ) >= mesh.faces.size() || static_cast<size_t>( goal ) >= mesh.faces.size() ||
		 !Nav_QueryFinite( request.start ) || !Nav_QueryFinite( request.goal ) || !std::isfinite( request.radius ) ||
		 !Nav_QueryFinite( request.mins ) || !Nav_QueryFinite( request.maxs ) || !std::isfinite( request.min_normal ) ||
		 request.mins.x > request.maxs.x || request.mins.y > request.maxs.y || request.mins.z > request.maxs.z ||
		 request.radius < 0.0 || request.step < 0.0 || request.drop < 0.0 || request.min_normal < 0.0 || request.min_normal > 1.0 ||
		 !std::isfinite( request.step ) || !std::isfinite( request.drop ) || !std::isfinite( request.cost.corridor_multiplier ) ||
		 !std::isfinite( request.cost.stair_multiplier ) || !std::isfinite( request.cost.stair_min ) || !std::isfinite( request.cost.stair_max ) ||
		 request.cost.corridor_multiplier < 0.0 || request.cost.stair_multiplier < 0.0 ||
		 ( !request.prior_faces.empty() && request.prior_faces.size() != mesh.faces.size() ) ) {
		return result.status;
	}
	if ( request.identity.generation != mesh.generation || request.identity.topology != mesh.topology ) {
		result.status = nav_query_status_t::stale_snapshot;
		return result.status;
	}
	// Capacity checks precede all size-changing operations that could reach an allocator.
	if ( context.states.size() < mesh.faces.size() || result.corridor.capacity() < mesh.faces.size() || context.heap.capacity() == 0 ) {
		result.status = nav_query_status_t::resource_limited;
		result.limit_reason = nav_query_limit_reason_t::storage;
		return result.status;
	}
	if ( start == goal ) {
		result.corridor.push_back( start );
		result.status = nav_query_status_t::found;
		return result.status;
	}
	result.status = nav_query_status_t::no_route;
	if ( mesh.faces[ start ].component >= 0 && mesh.faces[ goal ].component >= 0 && mesh.faces[ start ].component != mesh.faces[ goal ].component ) {
		return result.status;
	}
	/**
	*	Advance the context-local stamp; rollover must clear only this context, never immutable faces.
	**/
	if ( ++context.stamp == 0 ) {
		for ( auto &state : context.states ) {
			// An interrupted rollover must retry the full clear before token one can be reused.
			if ( interrupted() ) {
				context.stamp = UINT32_MAX;
				return result.status;
			}
			state.stamp = 0;
		}
		context.stamp = 1;
	}
	context.states[ start ] = { context.stamp, 0.0, -1 };
	const auto heuristic = [&]( const int32_t face ) {
		const double distance = Nav_QueryDistance( mesh.faces[ face ].center, mesh.faces[ goal ].center );
		const double slope = std::clamp( mesh.faces[ face ].normal.z, 0.0, 1.0 );
		const double value = distance * ( 1.0 + ( 1.0 - slope ) * 4.0 );
		return std::isfinite( value ) && value >= 0.0 ? value : 0.0;
	};
	const auto heap_order = []( const nav_query_heap_node_t &a, const nav_query_heap_node_t &b ) {
		// Preserve the existing epsilon and deterministic polygon-index tie-break exactly.
		return std::fabs( a.priority - b.priority ) > 1e-6 ? a.priority > b.priority : a.face > b.face;
	};
	const auto push = [&]( const int32_t face, const double priority ) {
		if ( context.heap.size() == context.heap.capacity() ) {
			return false;
		}
		context.heap.push_back( { face, priority } );
		std::push_heap( context.heap.begin(), context.heap.end(), heap_order );
		context.peak_heap = std::max( context.peak_heap, static_cast<uint64_t>( context.heap.size() ) );
		return true;
	};
	push( start, heuristic( start ) );
	/**
	*	Explore immutable adjacency; every work/capacity limit has a distinct terminal status.
	**/
	while ( !context.heap.empty() ) {
		// Include stale pops in the bounded cancellation/work quantum.
		if ( interrupted() ) {
			return result.status;
		}
		std::pop_heap( context.heap.begin(), context.heap.end(), heap_order );
		const auto entry = context.heap.back();
		context.heap.pop_back();
		const int32_t current = entry.face;
		const auto &state = context.states[ current ];
		if ( entry.priority > state.score + heuristic( current ) + 1e-4 ) {
			++context.stale_pops;
			continue;
		}
		if ( context.expansions == request.max_expansions ) {
			result.status = nav_query_status_t::resource_limited;
			result.limit_reason = nav_query_limit_reason_t::expansions;
			return result.status;
		}
		++context.expansions;
		if ( current == goal ) {
			/**
			*	Reconstruct a bounded predecessor chain without allocation or partial-success ambiguity.
			**/
			int32_t predecessor = goal;
			for ( size_t count = 0; count < mesh.faces.size(); ++count ) {
				// Reconstruction is cancellable and cannot publish a partial found result.
				if ( interrupted() ) {
					return result.status;
				}
				if ( predecessor < 0 || static_cast<size_t>( predecessor ) >= mesh.faces.size() ) {
					break;
				}
				result.corridor.push_back( predecessor );
				if ( predecessor == start ) {
					// Reverse with bounded checkpoints, including exceptionally long corridors.
					for ( size_t left = 0, right = result.corridor.size() - 1; left < right; ++left, --right ) {
						if ( interrupted() ) {
							return result.status;
						}
						std::swap( result.corridor[ left ], result.corridor[ right ] );
					}
					result.cost = state.score;
					result.status = nav_query_status_t::found;
					return result.status;
				}
				predecessor = context.states[ predecessor ].predecessor;
			}
			result.corridor.clear();
			result.status = nav_query_status_t::invalid_input;
			return result.status;
		}
		const auto &face = mesh.faces[ current ];
		if ( face.first_edge < 0 || face.num_edges < 0 || static_cast<size_t>( face.first_edge ) + static_cast<size_t>( face.num_edges ) > mesh.edges.size() ) {
			result.status = nav_query_status_t::invalid_input;
			return result.status;
		}
		// Directed half-edges preserve rise/drop asymmetry and the legacy cost/tie order.
		for ( int32_t side = 0; side < face.num_edges; ++side ) {
			// Large polygons receive the same polling bound as repeated heap pops.
			if ( interrupted() ) {
				return result.status;
			}
			if ( context.edge_tests == request.max_edge_tests ) {
				result.status = nav_query_status_t::resource_limited;
				result.limit_reason = nav_query_limit_reason_t::edge_tests;
				return result.status;
			}
			++context.edge_tests;
			const auto &edge = mesh.edges[ face.first_edge + side ];
			const int32_t neighbor = edge.neighbor;
			if ( neighbor < 0 ) {
				continue;
			}
			if ( static_cast<size_t>( neighbor ) >= mesh.faces.size() ) {
				result.status = nav_query_status_t::invalid_input;
				return result.status;
			}
			if ( edge.disabled && !request.ignore_disabled ) {
				++context.disabled;
				continue;
			}
			const auto &next = mesh.faces[ neighbor ];
			if ( next.excluded ) {
				continue;
			}
			if ( edge.rise > request.step ) {
				++context.rises;
				continue;
			}
			if ( -edge.rise > request.drop ) {
				++context.drops;
				continue;
			}
			if ( edge.width < 20.0 && !request.ignore_disabled ) {
				++context.narrow;
				continue;
			}
			const double radius = request.radius > 0.0 ? request.radius : 16.0;
			const double desired = radius * 2.0 + 16.0, passable = radius * 2.0 - 4.0;
			double clearance_penalty = 1.0;
			if ( edge.width < passable && !request.ignore_disabled ) {
				clearance_penalty = 50.0 + ( ( passable - edge.width ) / passable ) * 150.0;
			} else if ( edge.width < desired && !request.ignore_disabled ) {
				clearance_penalty = 2.0 + ( ( desired - edge.width ) / desired ) * 10.0;
			}
			const double slope_penalty = 1.0 + ( 1.0 - std::clamp( next.normal.z, 0.0, 1.0 ) ) * 4.0;
			double cost = Nav_QueryDistance( face.center, next.center ) * slope_penalty * clearance_penalty;
			if ( !request.prior_faces.empty() && request.prior_faces[ neighbor ] ) {
				cost *= request.cost.corridor_multiplier;
			}
			const double rise = std::fabs( edge.rise );
			if ( rise >= request.cost.stair_min && rise <= request.cost.stair_max ) {
				cost *= request.cost.stair_multiplier;
			}
			if ( !std::isfinite( cost ) || cost < 0.0 ) {
				cost = 0.0;
			}
			const double candidate = state.score + cost;
			auto &neighbor_state = context.states[ neighbor ];
			if ( neighbor_state.stamp != context.stamp || candidate < neighbor_state.score ) {
				neighbor_state = { context.stamp, candidate, current };
				if ( !push( neighbor, candidate + heuristic( neighbor ) ) ) {
					result.status = nav_query_status_t::resource_limited;
					result.limit_reason = nav_query_limit_reason_t::heap;
					return result.status;
				}
				++context.accepted;
			}
		}
	}
	return result.status;
}
