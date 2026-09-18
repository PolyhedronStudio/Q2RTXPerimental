/********************************************************************
*
*
*	ServerGame: Topological NavSector Graph
*	File: nav_sector_graph.h
*	Description:
*		Discrete topological graph representing exact, gapless spatial
*		sectors and transition portals decomposed from the navmesh.
*		Maintains precomputed all-pairs shortest paths, aperture metrics,
*		and O(1) topological route queries.
*
*
********************************************************************/
#pragma once

#include "svgame/nav/nav_types.h"
#include "shared/math/qm_vector3_dp.h"
#include <vector>

/**
*	@brief	Topological route record describing a multi-sector transit path.
**/
struct nav_sector_route_t {
	//! Starting sector index.
	int32_t from_sector_id = -1;
	//! Destination sector index.
	int32_t to_sector_id = -1;
	//! Precomputed cumulative Euclidean distance along portal centers.
	double total_topological_distance = 0.0;
	//! Sequence of portal indices to traverse in departure order.
	std::vector<int32_t> portal_ids;
};

/**
*	@brief	Instantaneous sector awareness snapshot for an autonomous agent.
**/
struct nav_sector_info_t {
	//! Sector index currently occupied by the agent.
	int32_t sector_id = -1;
	//! Semantic spatial classification of the sector.
	nav_zone_type_t zone_type = ZONE_TYPE_OPEN_SPACE;
	//! Area-weighted geometric floor centroid of the sector.
	Vector3DP centroid = {};
	//! Average floor elevation.
	double avg_elevation = 0.0;
	//! Number of transition portals opening into adjacent sectors.
	int32_t num_portals = 0;
	//! Index of the immediate next portal to traverse toward destination (-1 if arrived or unreachable).
	int32_t next_portal_id = -1;
	//! Precalculated topological distance from current sector to destination sector.
	double distance_to_goal = 0.0;
};

/**
*	@brief	Initialize and precompute the topological NavSector graph from active rooms and portals.
*	@note	Invoked automatically upon completion of Nav_BuildSpatialRegionsAndPortals().
**/
void Nav_SectorGraph_Build( void );

/**
*	@brief	Clear all precalculated matrices and release graph memory.
**/
void Nav_SectorGraph_Clear( void );

/**
*	@brief	Identify the discrete topological sector index enclosing a given world position.
*	@param	pos	World position query in double precision.
*	@return	Sector index in [0, num_sectors - 1], or -1 if outside mesh.
**/
int32_t Nav_SectorGraph_GetSectorForPoint( const Vector3DP &pos );

/**
*	@brief	Query the immediate next portal to traverse from one sector toward another.
*	@param	fromSector	Source sector index.
*	@param	toSector	Destination sector index.
*	@return	Pointer to the target nav_portal_t, or nullptr if unreachable or same sector.
**/
const nav_portal_t *Nav_SectorGraph_GetNextPortal( const int32_t fromSector, const int32_t toSector );

/**
*	@brief	Query the precomputed topological path distance connecting two sectors.
*	@param	fromSector	Source sector index.
*	@param	toSector	Destination sector index.
*	@return	Distance in world units, or infinity if disconnected.
**/
double Nav_SectorGraph_GetSectorDistance( const int32_t fromSector, const int32_t toSector );

/**
*	@brief	Construct the complete sequential list of portal indices between two sectors.
*	@param	fromSector		Source sector index.
*	@param	toSector		Destination sector index.
*	@param	outPortalIds	[out] Sequence of portal indices to traverse.
*	@return	True if a valid connected topological route exists, false otherwise.
**/
bool Nav_SectorGraph_BuildRoute( const int32_t fromSector, const int32_t toSector, std::vector<int32_t> &outPortalIds );

/**
*	@brief	Query topological awareness for an entity at a given position headed toward a goal.
*	@param	currentPos		Current world position of the entity.
*	@param	goalPos			Target destination world position.
*	@param	outSectorInfo	[out] Populated sector awareness snapshot.
*	@return	True if the entity's current sector was resolved, false otherwise.
**/
bool Nav_SectorGraph_QueryAwareness( const Vector3DP &currentPos, const Vector3DP &goalPos, nav_sector_info_t *outSectorInfo );

/**
*	@brief	Safely retrieve a room/sector record by index.
*	@param	sectorId	Sector index to query.
*	@return	Pointer to nav_room_t, or nullptr if index out of bounds.
**/
const nav_room_t *Nav_SectorGraph_GetSector( const int32_t sectorId );

/**
*	@brief	Safely retrieve a portal boundary record by index.
*	@param	portalId	Portal index to query.
*	@return	Pointer to nav_portal_t, or nullptr if index out of bounds.
**/
const nav_portal_t *Nav_SectorGraph_GetPortal( const int32_t portalId );

/**
*	@brief	Get total number of topological sectors.
*	@return	Sector count.
**/
int32_t Nav_SectorGraph_GetSectorCount( void );

/**
*	@brief	Get total number of transition portals.
*	@return	Portal count.
**/
int32_t Nav_SectorGraph_GetPortalCount( void );

/**
*	@brief	Update passability state for a transition portal (e.g. when a door opens or locks).
*	@param	portalId	Index of the portal to update.
*	@param	isPassable	New passability state.
**/
void Nav_SectorGraph_SetPortalPassable( const int32_t portalId, const bool isPassable );
