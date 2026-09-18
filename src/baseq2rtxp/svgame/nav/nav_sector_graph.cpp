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

#include <limits>
#include <algorithm>

//! Total number of spatial sectors in the active graph.
static int32_t s_navSectorCount = 0;

//! Precomputed all-pairs topological distance matrix (flattened N x N table).
static std::vector<double> s_navSectorDistanceMatrix;

//! Precomputed next-portal lookup table (flattened N x N table storing portal index).
static std::vector<int32_t> s_navSectorNextPortalMatrix;

//! Precomputed next-sector lookup table (flattened N x N table storing adjacent sector index).
static std::vector<int32_t> s_navSectorNextSectorMatrix;

/**
*	@brief	Initialize and precompute the topological NavSector graph from active rooms and portals.
**/
void Nav_SectorGraph_Build( void ) {
	/**
	*	Sanity checks: verify that spatial rooms and portals exist.
	**/
	// Clear any previous graph state before rebuilding.
	Nav_SectorGraph_Clear();

	// If no rooms have been decomposed, abort graph construction.
	if ( g_nav_rooms.empty() ) {
		return;
	}

	/**
	*	Allocate and initialize flattened all-pairs shortest path matrices.
	**/
	// Store the authoritative sector count.
	s_navSectorCount = static_cast<int32_t>( g_nav_rooms.size() );
	const int32_t N = s_navSectorCount;
	const size_t matrixSize = static_cast<size_t>( N ) * static_cast<size_t>( N );

	//! Infinity sentinel value for unlinked sectors.
	static constexpr double NAV_SECTOR_INFINITY_DIST = std::numeric_limits<double>::infinity();

	// Initialize distances to infinity, portals to -1, and next sectors to -1.
	s_navSectorDistanceMatrix.assign( matrixSize, NAV_SECTOR_INFINITY_DIST );
	s_navSectorNextPortalMatrix.assign( matrixSize, -1 );
	s_navSectorNextSectorMatrix.assign( matrixSize, -1 );

	// Self-distances along the diagonal are identically zero.
	for ( int32_t i = 0; i < N; i++ ) {
		const size_t diagIdx = static_cast<size_t>( i ) * static_cast<size_t>( N ) + static_cast<size_t>( i );
		s_navSectorDistanceMatrix[ diagIdx ] = 0.0;
		s_navSectorNextSectorMatrix[ diagIdx ] = i;
	}

	/**
	*	Populate direct 1-hop adjacency edges from active transition portals.
	**/
	// Iterate through all transition portals linking adjacent rooms.
	for ( size_t pIdx = 0; pIdx < g_nav_portals.size(); pIdx++ ) {
		const nav_portal_t &portal = g_nav_portals[ pIdx ];

		// Skip impassable portals (e.g. locked doors).
		if ( !portal.is_passable ) {
			continue;
		}

		const int32_t roomA = portal.from_room_id;
		const int32_t roomB = portal.to_room_id;

		// Validate that endpoint room indices are within bounds and distinct.
		if ( roomA < 0 || roomA >= N || roomB < 0 || roomB >= N || roomA == roomB ) {
			continue;
		}

		// Calculate 3D Euclidean distance from room A centroid to portal center, plus portal center to room B centroid.
		const double distAtoP = QM_Vector3DistanceDP( g_nav_rooms[ roomA ].centroid, portal.center );
		const double distPtoB = QM_Vector3DistanceDP( portal.center, g_nav_rooms[ roomB ].centroid );
		const double edgeDistance = distAtoP + distPtoB;

		const size_t idxAB = static_cast<size_t>( roomA ) * static_cast<size_t>( N ) + static_cast<size_t>( roomB );
		const size_t idxBA = static_cast<size_t>( roomB ) * static_cast<size_t>( N ) + static_cast<size_t>( roomA );

		// If multiple portals connect the same pair of rooms, retain the shortest traversal distance.
		if ( edgeDistance < s_navSectorDistanceMatrix[ idxAB ] ) {
			s_navSectorDistanceMatrix[ idxAB ] = edgeDistance;
			s_navSectorDistanceMatrix[ idxBA ] = edgeDistance;

			s_navSectorNextPortalMatrix[ idxAB ] = static_cast<int32_t>( pIdx );
			s_navSectorNextPortalMatrix[ idxBA ] = static_cast<int32_t>( pIdx );

			s_navSectorNextSectorMatrix[ idxAB ] = roomB;
			s_navSectorNextSectorMatrix[ idxBA ] = roomA;
		}
	}

	/**
	*	Execute Floyd-Warshall All-Pairs Shortest Path (APSP) algorithm.
	*	Given typical sector counts (N <= 128), N^3 is <= 2e6 cycles (< 1 millisecond).
	**/
	// Intermediate pivot sector k:
	for ( int32_t k = 0; k < N; k++ ) {
		const size_t kRow = static_cast<size_t>( k ) * static_cast<size_t>( N );

		// Source sector i:
		for ( int32_t i = 0; i < N; i++ ) {
			const size_t iRow = static_cast<size_t>( i ) * static_cast<size_t>( N );
			const double distIK = s_navSectorDistanceMatrix[ iRow + static_cast<size_t>( k ) ];

			// If source cannot reach pivot k, skip evaluation.
			if ( distIK >= NAV_SECTOR_INFINITY_DIST ) {
				continue;
			}

			// Destination sector j:
			for ( int32_t j = 0; j < N; j++ ) {
				const double distKJ = s_navSectorDistanceMatrix[ kRow + static_cast<size_t>( j ) ];

				// If pivot k cannot reach destination j, skip evaluation.
				if ( distKJ >= NAV_SECTOR_INFINITY_DIST ) {
					continue;
				}

				const double candidateDist = distIK + distKJ;
				const size_t idxIJ = iRow + static_cast<size_t>( j );

				// If path via pivot k improves cumulative distance, update shortest path and routing tables.
				if ( candidateDist < s_navSectorDistanceMatrix[ idxIJ ] ) {
					s_navSectorDistanceMatrix[ idxIJ ] = candidateDist;
					s_navSectorNextPortalMatrix[ idxIJ ] = s_navSectorNextPortalMatrix[ iRow + static_cast<size_t>( k ) ];
					s_navSectorNextSectorMatrix[ idxIJ ] = s_navSectorNextSectorMatrix[ iRow + static_cast<size_t>( k ) ];
				}
			}
		}
	}

	gi.dprintf( "NavSectorGraph: Precomputed topological APSP for %d sectors and %d portals.\n",
		s_navSectorCount, static_cast<int32_t>( g_nav_portals.size() ) );
}

/**
*	@brief	Clear all precalculated matrices and release graph memory.
**/
void Nav_SectorGraph_Clear( void ) {
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

	const size_t idx = static_cast<size_t>( fromSector ) * static_cast<size_t>( s_navSectorCount ) + static_cast<size_t>( toSector );
	const int32_t portalId = s_navSectorNextPortalMatrix[ idx ];

	// Validate portal identifier and return pointer from global portals array.
	if ( portalId >= 0 && portalId < static_cast<int32_t>( g_nav_portals.size() ) ) {
		return &g_nav_portals[ portalId ];
	}

	return nullptr;
}

/**
*	@brief	Query the precomputed topological path distance connecting two sectors.
*	@param	fromSector	Source sector index.
*	@param	toSector	Destination sector index.
*	@return	Distance in world units, or infinity if disconnected.
**/
double Nav_SectorGraph_GetSectorDistance( const int32_t fromSector, const int32_t toSector ) {
	// Validate sector index bounds.
	if ( fromSector < 0 || fromSector >= s_navSectorCount || toSector < 0 || toSector >= s_navSectorCount ) {
		return std::numeric_limits<double>::infinity();
	}

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

	// Rebuild graph matrices to reflect altered connectivity.
	Nav_SectorGraph_Build();
}
