/********************************************************************
*
*	ServerGame: Navigation Snapshot Ownership and Synchronous Query Bridge
*	Tagged owners remain on the engine thread. Only const views and exclusively
*	owned preallocated query buffers are candidates for the later M6 service.
*
********************************************************************/
#pragma once

#include "svgame/nav/nav_query_algorithm.h"
#include "svgame/nav/nav_waypoint.h"
#include "svgame/nav/nav_route_context.h"
#include "svgame/memory/svg_containers.h"

#include <span>

//! Navigation query backing storage remains accounted to the navigation zone tag.
template<typename T>
using svg_nav_query_buffer_t = svg_vector_t<T, TAG_SVGAME_NAVMESH>;
//! Concrete owned request with no live entity, callback or borrowed corridor storage.
using svg_nav_request_t = nav_query_request_t<svg_nav_query_buffer_t>;
//! Exclusive retained search state; later funnel/LOS/refinement state must join this owner.
using svg_nav_query_context_t = nav_query_context_t<svg_nav_query_buffer_t>;
//! Owned graph result used before the explicit main-thread route-validation phase.
using svg_nav_search_result_t = nav_query_result_t<svg_nav_query_buffer_t>;

/**
*	@brief	Exclusive graph-stage workspace explicitly owned by a synchronous caller.
*	@note	All capacity preparation and reclamation occur on the designated engine owner.
**/
struct svg_nav_graph_workspace_t {
	//! Pointer-free request, independent search scratch and owned terminal reply.
	svg_nav_request_t request;
	svg_nav_query_context_t context;
	svg_nav_search_result_t result;
};

struct nav_path_policy_t;

/**
*	@brief	Copied immutable graph owner with indexed geometry and owner-prepared portal data.
*	@note	Only Nav_BuildQuerySnapshotMainThread mutates private backing; never rebuild while pinned.
**/
class svg_nav_mesh_snapshot_t {
public:
	/**
	*	@brief	Borrow read-only geometry while its owner-side lease is held.
	*	@return	Const spans and generation/topology identity, never live mesh pointers.
	**/
	nav_query_mesh_view_t View() const {
		// The caller's owner-side pin, not the view itself, owns backing storage lifetime.
		return { generation, topology, vertices, faces, edges, nodes };
	}

	/**
	*	@brief	Borrow immutable copied route geometry while the owner lease remains held.
	**/
	nav_route_mesh_view_t RouteView() const {
		return { generation, topology, native_vertices, native_halfedges, native_faces, native_nodes, native_rooms,
			native_corners, native_solid_edges, native_edge_blocks_xy, native_components, native_widths,
			&native_grid_edges, &native_grid_corners, native_clearance_nodes, native_clearance_tree_ready };
	}
	/**
	*	@brief	Conservatively account copied snapshot storage, including nested bucket capacity.
	*	@note	Owner audit only; overhead headroom covers tagged/debug allocations beyond payload.
	**/
	uint64_t RetainedBytes() const {
		// Capacity, not size, determines retained backing after a smaller map replaces a large one.
		const auto bytes = []( const auto &buffer ) -> uint64_t {
			return buffer.capacity() * sizeof( typename std::decay_t<decltype( buffer )>::value_type ) + 1024;
		};
		uint64_t total = sizeof( *this ) + bytes( vertices ) + bytes( faces ) + bytes( edges ) + bytes( nodes ) +
			bytes( native_vertices ) + bytes( native_halfedges ) + bytes( native_faces ) + bytes( native_nodes ) +
			bytes( native_rooms ) + bytes( native_corners ) + bytes( native_solid_edges ) + bytes( native_edge_blocks_xy ) +
			bytes( native_components ) + bytes( native_widths ) + bytes( native_clearance_nodes ) + bytes( native_clearance_indices );
		// Nested maps and their allocator nodes remain private to this same snapshot lease.
		for ( const auto *grid : { &native_grid_edges, &native_grid_corners } ) {
			total += grid->bucket_count() * sizeof( void * ) + 1024;
			for ( const auto &bucket : *grid ) {
				total += bytes( bucket.second ) + 1024;
			}
		}
		return total;
	}
private:
	//! Private snapshot lifetime identities, assigned only after complete successful preparation.
	uint64_t generation = 0, topology = 0;
	//! Copied, pointer-free records; no spans refer to g_nav_* containers.
	svg_nav_query_buffer_t<nav_query_point_t> vertices;
	svg_nav_query_buffer_t<nav_query_face_t> faces;
	svg_nav_query_buffer_t<nav_query_edge_t> edges;
	svg_nav_query_buffer_t<nav_query_node_t> nodes;
	//! Copied native geometry and prepared classification; backing is private and owner-reclaimed.
	svg_nav_query_buffer_t<Vector3DP> native_vertices;
	svg_nav_query_buffer_t<nav_halfedge_t> native_halfedges;
	svg_nav_query_buffer_t<nav_face_t> native_faces;
	svg_nav_query_buffer_t<nav_kdtree_node_t> native_nodes;
	svg_nav_query_buffer_t<nav_route_room_t> native_rooms;
	svg_nav_query_buffer_t<nav_cached_corner_t> native_corners;
	svg_nav_query_buffer_t<std::pair<Vector3DP, Vector3DP>> native_solid_edges;
	svg_nav_query_buffer_t<uint8_t> native_edge_blocks_xy;
	svg_nav_query_buffer_t<int32_t> native_components;
	svg_nav_query_buffer_t<double> native_widths;
	//! Deep-copied immutable spatial buckets; nested allocations remain owner-only.
	nav_route_grid_t native_grid_edges, native_grid_corners;
	//! Immutable nearest-boundary tree and owner-only median-partition construction scratch.
	svg_nav_query_buffer_t<nav_clearance_tree_node_t> native_clearance_nodes;
	svg_nav_query_buffer_t<int32_t> native_clearance_indices;
	bool native_clearance_tree_ready = false;
	//! Explicit owner copy phase has access before publication, never while pinned.
	friend void Nav_CaptureRouteSnapshotMainThread( svg_nav_mesh_snapshot_t &snapshot );
	//! Owner-only builder has explicit access to private publication storage.
	friend bool Nav_BuildQuerySnapshotMainThread( svg_nav_mesh_snapshot_t &snapshot );
};

/**
*	@brief	Pin the current copied snapshot from its designated engine owner.
*	@return	Const owner pointer or nullptr during generation/unsupported publication.
*	@note	An outstanding old pin blocks replacement, not pure reads of its preserved geometry.
**/
const svg_nav_mesh_snapshot_t *Nav_AcquireQuerySnapshotMainThread( void );

/**
*	@brief	Release an owner-side snapshot pin; never call from a worker or entity destructor callback.
*	@param	snapshot	Exact owner returned by acquisition, or nullptr for no lease.
**/
void Nav_ReleaseQuerySnapshotMainThread( const svg_nav_mesh_snapshot_t *snapshot );

/**
*	@brief	Close acquisition before mesh mutation while retaining already pinned copied data.
*	@note	Pure readers of old copies remain valid; native/live readers still require the M5 lifecycle audit.
**/
void Nav_RetireQuerySnapshotMainThread( void );

/**
*	@brief	Snapshot immutable cost inputs then execute the pure solver synchronously.
*	@param	marks	Owner-only borrowed membership input, copied into the owned request before solving.
*	@param	stamp	Current owner membership stamp; zero never denotes committed membership.
*	@note	The reference policy's live callback is used only by explicit opt-in main-thread parity.
**/
bool Nav_FindPathSnapshotCostsMainThread( const int32_t start, const int32_t goal,
	std::vector<int32_t> &output, const nav_path_policy_t &policy, const nav_query_cost_policy_t &cost,
	const std::span<const uint32_t> marks = {}, const uint32_t stamp = 0,
	const Vector3DP *start_feet = nullptr, const Vector3DP *goal_feet = nullptr,
	svg_nav_graph_workspace_t *workspace = nullptr, svg_nav_result_t *route_result = nullptr );

/**
*	@brief	Enable/disable and report opt-in synchronous reference parity checks.
*	@note	Normal gameplay has no parity double-search or diagnostic output.
**/
void Nav_QueryParityCommand( void );

/**
*	@brief	Run opt-in copied-snapshot pin/retirement/publication lifecycle invariants.
*	@note	Changes only diagnostic topology/cache revision and leaves actual geometry untouched.
**/
void Nav_QuerySnapshotCheckCommand( void );

/**
*	@brief	Copy prepared native route geometry before publishing snapshot identities.
**/
void Nav_CaptureRouteSnapshotMainThread( svg_nav_mesh_snapshot_t &snapshot );

/**
*	@brief	Run opt-in production actor revision and controlled identity-overflow contracts.
*	@note	Invalidates the selected actor's route; use only in an isolated diagnostic fixture.
**/
void Nav_QueryIdentityCheckCommand( void );

/**
*	@brief	Prepare pointer-free scalar policy identically for owner and worker graph execution.
*	@note	The caller retains the immutable publication while copying indexed membership.
**/
bool Nav_PreparePathRequestMainThread( const nav_query_mesh_view_t &mesh, svg_nav_request_t &request,
	const int32_t start, const int32_t goal, const nav_path_policy_t &policy, const nav_query_cost_policy_t &cost,
	const std::span<const uint32_t> marks, const uint32_t stamp,
	const Vector3DP *start_feet, const Vector3DP *goal_feet, const nav_query_identity_t &identity );
