/********************************************************************
*
*
*	ServerGame: Navigation edge-mesh generation and pipeline orchestration.
*
*
********************************************************************/
#pragma once

#include "nav_core.h"
#include "nav_types.h"
#include "nav_containers.h"
#include "nav_csg.h"
#include "nav_kdtree_builder.h"
#include <vector>

//! Temporary polygon data produced during extraction.
extern nav_vector_t<nav_poly_t> g_nav_polys;
//! Packed vertex array used by the half-edge mesh.
extern std::vector<Vector3DP> g_nav_vertices;
//! Packed half-edge array used by the half-edge mesh.
extern std::vector<nav_halfedge_t> g_nav_halfedges;
//! Entity to half-edge mapping table for O(1) dynamic edge updates.
extern std::vector<std::vector<int32_t>> g_nav_entity_edges;
//! Packed face array used by the half-edge mesh.
extern std::vector<nav_face_t> g_nav_faces;
//! Topological room records decomposed from bottleneck apertures and portals.
extern std::vector<nav_room_t> g_nav_rooms;
//! Inter-zone transition portal records linking adjacent rooms and corridors.
extern std::vector<nav_portal_t> g_nav_portals;

//! KD-tree nodes generated for spatial queries.
extern nav_vector_t<nav_kdtree_node_t> g_nav_nodes;
//! BSP leaf to face-span mapping used during leaf-local lookups.
extern nav_vector_t<nav_leaf_link_t> g_nav_leaf_links;
//! Flattened face-id list referenced by the leaf link table.
extern nav_vector_t<int32_t> g_nav_leaf_poly_ids;

/**
*	@brief	Trigger navmesh generation from the console.
**/
void Nav_GenerateCommand();

/**
*	@brief	Print the current standalone nav generation status to the server console.
*	@note	This reports the active KD-tree build state without relying on nav2/nav3 runtime helpers.
**/
void Nav_StatusCommand( void );

/**
*	@brief	Clear all active navmesh data from memory.
**/
void Nav_Clear();

/**
*	@brief	Extract walkable surfaces from the current map collision model.
*	@note	Called from the asynchronous generation worker.
**/
void Nav_DoExtractionWork();

/**
*	@brief	Build the half-edge mesh from the extracted polygons.
**/
void Nav_BuildHalfEdgeMesh();

/**
*	@brief	Build topological spatial regions, corridors, alcoves, and transition portals across the navmesh.
*	@note	Called immediately after KD-tree construction and post-load deserialization for O(1) zone queries.
**/
void Nav_BuildSpatialRegionsAndPortals();

/**
*	@brief	Query the topological spatial room/zone record containing the given world position.
*	@param	pos	World position query.
*	@return	Pointer to the enclosing nav_room_t, or nullptr if unassigned/outside mesh.
**/
const nav_room_t *Nav_GetRoomForPoint( const Vector3DP &pos );

/**
*	@brief	Query the topological zone type of the given world position.
*	@param	pos	World position query.
*	@return	Topological zone type (e.g. ZONE_TYPE_ROOM_ENCLOSED, ZONE_TYPE_CORRIDOR_STAIRS, etc.).
**/
nav_zone_type_t Nav_GetZoneTypeForPoint( const Vector3DP &pos );

/**
*	@brief	Retrieve all boundary transition portals connected to the given room index.
*	@param	room_id		Room identifier.
*	@param	outPortals	[out] Vector to populate with pointers to connected nav_portal_t records.
**/
void Nav_GetRoomPortals( const int32_t room_id, std::vector<const nav_portal_t*> &outPortals );

/**
*	@brief	Check whether the given position is located inside an enclosed interior room.
*	@param	pos	World position query.
*	@return	True if pos lies within a ZONE_TYPE_ROOM_ENCLOSED zone.
**/
bool Nav_IsPointInEnclosedRoom( const Vector3DP &pos );

/**
*	@brief	Retrieve average riser step height for a staircase corridor zone.
*	@param	room_id	Room identifier.
*	@return	Average step height in world units (defaults to NAV_MAX_STEP_HEIGHT if flat).
**/
double Nav_GetStairCorridorStepHeight( const int32_t room_id );

/**
*	@brief	Validate half-edge and KD-tree ownership invariants.
*	@param	stage	Human-readable generation stage included in the bounded report.
*	@return	True when all checked topology invariants hold.
*	@note	This emits aggregate diagnostics and a capped set of failures only; it is
*			intended for deterministic diagnosis without per-edge log spam.
**/
bool Nav_ValidateTopology( const char *stage );
