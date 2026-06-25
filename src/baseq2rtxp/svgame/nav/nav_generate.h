/********************************************************************
*
*
*	ServerGame: Navigation edge-mesh generation and KD-tree construction.
*
*
********************************************************************/
#pragma once

#include "nav_core.h"
#include "nav_types.h"
#include "nav_containers.h"
#include <vector>

//! Temporary polygon data produced during extraction.
extern nav_vector_t<nav_poly_t> g_nav_polys;
//! Packed vertex array used by the half-edge mesh.
extern std::vector<Vector3> g_nav_vertices;
//! Packed half-edge array used by the half-edge mesh.
extern std::vector<nav_halfedge_t> g_nav_halfedges;
//! Packed face array used by the half-edge mesh.
extern std::vector<nav_face_t> g_nav_faces;

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
*	@brief	Build the KD-tree used for spatial nav queries.
**/
void Nav_BuildKDTree();
