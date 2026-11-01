#include "svgame/nav/nav_thread.h"
/********************************************************************
*
*
*	ServerGame: Topological NavSector Graph Implementation
*	File: nav_sector_graph.cpp
*	Description:
*		Topological sector graph precomputation, all-pairs shortest paths,
*		and discrete spatial transition queries using double precision.
*
*
********************************************************************/
#include "svgame/nav/nav_sector_graph.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/nav/nav_core.h"
#include "svgame/svg_local.h"
#include "svgame/memory/svg_containers.h"

#include <limits>
#include <algorithm>

//! Total number of spatial sectors in the active graph.
static int32_t s_navSectorCount = 0;

//! Precomputed all-pairs topological distance matrix (flattened N x N table).
static svg_vector_t<double, TAG_SVGAME_NAVMESH> s_navSectorDistanceMatrix;

//! Precomputed next-portal lookup table (flattened N x N table storing portal index).
static svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> s_navSectorNextPortalMatrix;

//! Precomputed next-sector lookup table (flattened N x N table storing adjacent sector index).
static svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> s_navSectorNextSectorMatrix;

/**
*	@brief	One copied directed sector adjacency edge, rebuilt when passability changes.
**/
struct nav_sector_edge_t {
	//! Neighbor sector and exact source portal index.
	int32_t sector = -1, portal = -1;
	//! Original centroid-to-portal-to-centroid traversal distance.
	double distance = 0.0;
};
/**
*	@brief	A queued distance candidate for a destination-rooted shortest-path tree.
**/
struct nav_sector_candidate_t {
	//! Candidate sector and distance; stale heap entries are ignored.
	int32_t sector = -1;
	double distance = 0.0;
};
//! Contiguous adjacency offsets avoid scanning every portal during shortest-path relaxation.
static svg_vector_t<size_t, TAG_SVGAME_NAVMESH> s_navSectorOffsets;
//! Passable directed adjacency copied from the authoritative portal records.
static svg_vector_t<nav_sector_edge_t, TAG_SVGAME_NAVMESH> s_navSectorEdges;
//! A column becomes valid only after its destination-rooted shortest-path tree completes.
static svg_vector_t<uint8_t, TAG_SVGAME_NAVMESH> s_navSectorColumnsValid;
//! Exclusive retained heap scratch; query-time routing never allocates.
static svg_vector_t<nav_sector_candidate_t, TAG_SVGAME_NAVMESH> s_navSectorHeap;

/**
*	@brief	Prepare sparse sector adjacency and invalidate cached destination trees.
*	@note	No all-pairs search runs at load or per-door updates; all query scratch is prepared here.
**/
void Nav_SectorGraph_Build( void ) {
	/**
	*	Retain capacity across door state changes; full release remains the clear/map-retirement operation.
	**/
	s_navSectorCount = static_cast<int32_t>( g_nav_rooms.size() );
	const size_t count = g_nav_rooms.size();
	s_navSectorColumnsValid.assign( count, 0 );
	s_navSectorOffsets.assign( count + 1, 0 );
	s_navSectorDistanceMatrix.resize( count * count );
	s_navSectorNextPortalMatrix.resize( count * count );
	s_navSectorNextSectorMatrix.resize( count * count );
	s_navSectorEdges.clear();
	s_navSectorHeap.clear();
	// Empty topology owns no route, even if retained capacity came from an earlier map.
	if ( count == 0 ) {
		return;
	}
	/**
	*	Copy finite nonnegative portal costs once, preserving portal order and parallel connections.
	**/
	const auto eligible = [&]( const nav_portal_t &portal ) {
		return portal.is_passable && portal.from_room_id >= 0 && portal.to_room_id >= 0 &&
			portal.from_room_id < s_navSectorCount && portal.to_room_id < s_navSectorCount &&
			portal.from_room_id != portal.to_room_id;
	};
	// Count directed adjacency slots before filling the contiguous edge array.
	for ( const auto &portal : g_nav_portals ) {
		if ( eligible( portal ) ) {
			++s_navSectorOffsets[ static_cast<size_t>( portal.from_room_id ) + 1 ];
			++s_navSectorOffsets[ static_cast<size_t>( portal.to_room_id ) + 1 ];
		}
	}
	// Prefix sums give each sector a fixed adjacency interval.
	for ( size_t index = 1; index <= count; ++index ) {
		s_navSectorOffsets[ index ] += s_navSectorOffsets[ index - 1 ];
	}
	s_navSectorEdges.resize( s_navSectorOffsets.back() );
	auto cursors = s_navSectorOffsets;
	// Each accepted portal contributes two symmetric edges with the original exact weight.
	for ( size_t index = 0; index < g_nav_portals.size(); ++index ) {
		const auto &portal = g_nav_portals[ index ];
		if ( !eligible( portal ) ) {
			continue;
		}
		const int32_t a = portal.from_room_id, b = portal.to_room_id;
		const double distance = QM_Vector3DistanceDP( g_nav_rooms[ a ].centroid, portal.center ) +
			QM_Vector3DistanceDP( portal.center, g_nav_rooms[ b ].centroid );
		// Nonfinite weights cannot relax a route; keep slots but mark them unusable.
		const double weight = std::isfinite( distance ) && distance >= 0.0 ? distance : std::numeric_limits<double>::infinity();
		s_navSectorEdges[ cursors[ a ]++ ] = { b, static_cast<int32_t>( index ), weight };
		s_navSectorEdges[ cursors[ b ]++ ] = { a, static_cast<int32_t>( index ), weight };
	}
	// At most one successful relaxation per directed edge plus the destination seed can be queued.
	s_navSectorHeap.reserve( s_navSectorEdges.size() + 1 );
	Nav_GenerationPrintf( "NavSectorGraph: Prepared sparse routing for %d sectors and %d portals (destination trees on demand).\n",
		s_navSectorCount, static_cast<int32_t>( g_nav_portals.size() ) );
}

/**
*	@brief	Compute and retain only the requested destination's shortest-path tree.
*	@param	destination	Valid destination sector index.
*	@note	Owner/generation-exclusive context; no allocations or live actor access. Strict improvements and
*			settled-parent ordering keep zero-cost portal chains acyclic. Equal routes use sector/portal order.
**/
static void Nav_SectorGraph_EnsureDestination( const int32_t destination ) {
	/**
	*	A completed column belongs to the current adjacency build and can be reused without search.
	**/
	if ( s_navSectorColumnsValid[ destination ] != 0 ) {
		return;
	}
	const size_t count = static_cast<size_t>( s_navSectorCount );
	const auto cell = [&]( const int32_t sector ) { return static_cast<size_t>( sector ) * count + static_cast<size_t>( destination ); };
	// Initialize only this destination column; other columns remain unread until separately completed.
	for ( int32_t sector = 0; sector < s_navSectorCount; ++sector ) {
		s_navSectorDistanceMatrix[ cell( sector ) ] = std::numeric_limits<double>::infinity();
		s_navSectorNextPortalMatrix[ cell( sector ) ] = -1;
		s_navSectorNextSectorMatrix[ cell( sector ) ] = -1;
	}
	s_navSectorDistanceMatrix[ cell( destination ) ] = 0.0;
	s_navSectorNextSectorMatrix[ cell( destination ) ] = destination;
	s_navSectorHeap.clear();
	const auto order = []( const nav_sector_candidate_t &a, const nav_sector_candidate_t &b ) {
		return a.distance != b.distance ? a.distance > b.distance : a.sector > b.sector;
	};
	const auto push = [&]( const int32_t sector, const double distance ) {
		Q_assert( s_navSectorHeap.size() < s_navSectorHeap.capacity() );
		s_navSectorHeap.push_back( { sector, distance } );
		std::push_heap( s_navSectorHeap.begin(), s_navSectorHeap.end(), order );
	};
	push( destination, 0.0 );
	/**
	*	Relax sparse adjacency toward the destination, retaining direct next hops and exact portal identities.
	**/
	while ( !s_navSectorHeap.empty() ) {
		std::pop_heap( s_navSectorHeap.begin(), s_navSectorHeap.end(), order );
		const auto current = s_navSectorHeap.back();
		s_navSectorHeap.pop_back();
		// An improved sector may leave an older queued distance; never expand that stale entry.
		if ( current.distance != s_navSectorDistanceMatrix[ cell( current.sector ) ] ) {
			continue;
		}
		// Each outgoing adjacency is symmetric, so its portal is also the neighbor's next hop toward current.
		for ( size_t index = s_navSectorOffsets[ current.sector ]; index < s_navSectorOffsets[ current.sector + 1 ]; ++index ) {
			const auto &edge = s_navSectorEdges[ index ];
			const double candidate = current.distance + edge.distance;
			const size_t target = cell( edge.sector );
			// Strict improvement matches shortest-distance semantics and avoids zero-cost parent cycles.
			if ( candidate < s_navSectorDistanceMatrix[ target ] ) {
				s_navSectorDistanceMatrix[ target ] = candidate;
				s_navSectorNextPortalMatrix[ target ] = edge.portal;
				s_navSectorNextSectorMatrix[ target ] = current.sector;
				push( edge.sector, candidate );
			}
		}
	}
	// Publication follows the entire tree; no query can consume a partly computed column.
	s_navSectorColumnsValid[ destination ] = 1;
}

/**
*	@brief	Clear all precalculated matrices and release graph memory.
**/
void Nav_SectorGraph_Clear( void ) {
	// Map retirement releases adjacency, cached-column validity and retained query scratch too.
	s_navSectorOffsets.clear();
	s_navSectorOffsets.shrink_to_fit();
	s_navSectorEdges.clear();
	s_navSectorEdges.shrink_to_fit();
	s_navSectorColumnsValid.clear();
	s_navSectorColumnsValid.shrink_to_fit();
	s_navSectorHeap.clear();
	s_navSectorHeap.shrink_to_fit();
	// Reset sector count to zero.
	s_navSectorCount = 0;
	// Release distance matrix memory.
	s_navSectorDistanceMatrix.clear();
	s_navSectorDistanceMatrix.shrink_to_fit();
	// Release next portal table memory.
	s_navSectorNextPortalMatrix.clear();
	s_navSectorNextPortalMatrix.shrink_to_fit();
	// Release next sector table memory.
	s_navSectorNextSectorMatrix.clear();
	s_navSectorNextSectorMatrix.shrink_to_fit();
}

/**
*	@brief	Identify the discrete topological sector index enclosing a given world position.
*	@param	pos	World position query in double precision.
*	@return	Sector index in [0, num_sectors - 1], or -1 if outside mesh.
**/
int32_t Nav_SectorGraph_GetSectorForPoint( const Vector3DP &pos ) {
	// Query the enclosing room from the nav mesh room partition.
	const nav_room_t *room = Nav_GetRoomForPoint( pos );
	// If a valid room was found, return its identifier.
	if ( room != nullptr ) {
		return room->room_id;
	}
	// Position is unassigned or outside the mesh.
	return -1;
}

/**
*	@brief	Query the immediate next portal to traverse from one sector toward another.
*	@param	fromSector	Source sector index.
*	@param	toSector	Destination sector index.
*	@return	Pointer to the target nav_portal_t, or nullptr if unreachable or same sector.
**/
const nav_portal_t *Nav_SectorGraph_GetNextPortal( const int32_t fromSector, const int32_t toSector ) {
	// Validate sector index bounds.
	if ( fromSector < 0 || fromSector >= s_navSectorCount || toSector < 0 || toSector >= s_navSectorCount ) {
		return nullptr;
	}
	// If source and destination are identical, no portal transition is required.
	if ( fromSector == toSector ) {
		return nullptr;
	}

	// Prepare only the destination tree requested by this query.
	Nav_SectorGraph_EnsureDestination( toSector );

	const size_t idx = static_cast<size_t>( fromSector ) * static_cast<size_t>( s_navSectorCount ) + static_cast<size_t>( toSector );
	const int32_t portalId = s_navSectorNextPortalMatrix[ idx ];

	// Validate portal identifier and return pointer from global portals array.
	if ( portalId >= 0 && portalId < static_cast<int32_t>( g_nav_portals.size() ) ) {
		return &g_nav_portals[ portalId ];
	}

	return nullptr;
}

/**
*	@brief	Query the cached topological path distance connecting two sectors.
*	@param	fromSector	Source sector index.
*	@param	toSector	Destination sector index.
*	@return	Distance in world units, or infinity if disconnected.
**/
double Nav_SectorGraph_GetSectorDistance( const int32_t fromSector, const int32_t toSector ) {
	// Validate sector index bounds.
	if ( fromSector < 0 || fromSector >= s_navSectorCount || toSector < 0 || toSector >= s_navSectorCount ) {
		return std::numeric_limits<double>::infinity();
	}

	// Prepare only the destination tree requested by this query.
	Nav_SectorGraph_EnsureDestination( toSector );

	const size_t idx = static_cast<size_t>( fromSector ) * static_cast<size_t>( s_navSectorCount ) + static_cast<size_t>( toSector );
	return s_navSectorDistanceMatrix[ idx ];
}

/**
*	@brief	Construct the complete sequential list of portal indices between two sectors.
*	@param	fromSector		Source sector index.
*	@param	toSector		Destination sector index.
*	@param	outPortalIds	[out] Sequence of portal indices to traverse.
*	@return	True if a valid connected topological route exists, false otherwise.
**/
bool Nav_SectorGraph_BuildRoute( const int32_t fromSector, const int32_t toSector, std::vector<int32_t> &outPortalIds ) {
	// Clear the output list.
	outPortalIds.clear();

	// Validate bounds.
	if ( fromSector < 0 || fromSector >= s_navSectorCount || toSector < 0 || toSector >= s_navSectorCount ) {
		return false;
	}

	// Trivial case: source is already destination.
	if ( fromSector == toSector ) {
		return true;
	}

	// Prepare only the destination tree requested by this query.
	Nav_SectorGraph_EnsureDestination( toSector );

	int32_t currentSector = fromSector;
	int32_t hops = 0;

	// Follow next-portal routing chain until destination sector is reached.
	while ( currentSector != toSector && hops < s_navSectorCount ) {
		const size_t idx = static_cast<size_t>( currentSector ) * static_cast<size_t>( s_navSectorCount ) + static_cast<size_t>( toSector );
		const int32_t portalId = s_navSectorNextPortalMatrix[ idx ];
		const int32_t nextSector = s_navSectorNextSectorMatrix[ idx ];

		// If no routing portal exists or next sector is invalid, path is broken.
		if ( portalId < 0 || nextSector < 0 || nextSector == currentSector ) {
			outPortalIds.clear();
			return false;
		}

		outPortalIds.push_back( portalId );
		currentSector = nextSector;
		hops++;
	}

	// Successfully constructed topological route if we arrived at destination sector.
	return ( currentSector == toSector );
}

/**
*	@brief	Query topological awareness for an entity at a given position headed toward a goal.
*	@param	currentPos		Current world position of the entity.
*	@param	goalPos			Target destination world position.
*	@param	outSectorInfo	[out] Populated sector awareness snapshot.
*	@return	True if the entity's current sector was resolved, false otherwise.
**/
bool Nav_SectorGraph_QueryAwareness( const Vector3DP &currentPos, const Vector3DP &goalPos, nav_sector_info_t *outSectorInfo ) {
	// Sanity check: ensure valid output pointer.
	if ( outSectorInfo == nullptr ) {
		return false;
	}

	// Initialize snapshot to defaults.
	*outSectorInfo = nav_sector_info_t{};

	// Resolve the sector enclosing current position.
	const int32_t currentSector = Nav_SectorGraph_GetSectorForPoint( currentPos );
	if ( currentSector < 0 || currentSector >= s_navSectorCount ) {
		return false;
	}

	// Populate basic sector properties.
	const nav_room_t &room = g_nav_rooms[ currentSector ];
	outSectorInfo->sector_id = currentSector;
	outSectorInfo->zone_type = room.zone_type;
	outSectorInfo->centroid = room.centroid;
	outSectorInfo->avg_elevation = room.avg_elevation;
	outSectorInfo->num_portals = static_cast<int32_t>( room.portal_indices.size() );

	// Resolve destination sector.
	const int32_t goalSector = Nav_SectorGraph_GetSectorForPoint( goalPos );
	if ( goalSector >= 0 && goalSector < s_navSectorCount ) {
		outSectorInfo->distance_to_goal = Nav_SectorGraph_GetSectorDistance( currentSector, goalSector );
		const nav_portal_t *nextPortal = Nav_SectorGraph_GetNextPortal( currentSector, goalSector );
		if ( nextPortal != nullptr ) {
			outSectorInfo->next_portal_id = nextPortal->portal_id;
		}
	}

	return true;
}

/**
*	@brief	Safely retrieve a room/sector record by index.
*	@param	sectorId	Sector index to query.
*	@return	Pointer to nav_room_t, or nullptr if index out of bounds.
**/
const nav_room_t *Nav_SectorGraph_GetSector( const int32_t sectorId ) {
	// Check index bounds.
	if ( sectorId >= 0 && sectorId < static_cast<int32_t>( g_nav_rooms.size() ) ) {
		return &g_nav_rooms[ sectorId ];
	}
	return nullptr;
}

/**
*	@brief	Safely retrieve a portal boundary record by index.
*	@param	portalId	Portal index to query.
*	@return	Pointer to nav_portal_t, or nullptr if index out of bounds.
**/
const nav_portal_t *Nav_SectorGraph_GetPortal( const int32_t portalId ) {
	// Check index bounds.
	if ( portalId >= 0 && portalId < static_cast<int32_t>( g_nav_portals.size() ) ) {
		return &g_nav_portals[ portalId ];
	}
	return nullptr;
}

/**
*	@brief	Get total number of topological sectors.
*	@return	Sector count.
**/
int32_t Nav_SectorGraph_GetSectorCount( void ) {
	return s_navSectorCount;
}

/**
*	@brief	Get total number of transition portals.
*	@return	Portal count.
**/
int32_t Nav_SectorGraph_GetPortalCount( void ) {
	return static_cast<int32_t>( g_nav_portals.size() );
}

/**
*	@brief	Update passability state for a transition portal (e.g. when a door opens or locks).
*	@param	portalId	Index of the portal to update.
*	@param	isPassable	New passability state.
**/
void Nav_SectorGraph_SetPortalPassable( const int32_t portalId, const bool isPassable ) {
	// Sanity check portal index.
	if ( portalId < 0 || portalId >= static_cast<int32_t>( g_nav_portals.size() ) ) {
		return;
	}

	// If passability did not change, nothing to update.
	if ( g_nav_portals[ portalId ].is_passable == isPassable ) {
		return;
	}

	// Update portal record.
	g_nav_portals[ portalId ].is_passable = isPassable;

	// Rebuild sparse adjacency and invalidate destination trees before any later query.
	Nav_SectorGraph_Build();
}
