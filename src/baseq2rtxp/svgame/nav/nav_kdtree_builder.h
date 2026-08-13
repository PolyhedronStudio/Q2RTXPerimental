/********************************************************************
*
*
*	ServerGame: KD-Tree Acceleration Structure Builder
*				using Adaptive Hybrid SAH (64-bin & Exact Edge-Events),
*				dynamic depth scaling, and exact leaf localization.
*
*
********************************************************************/
#pragma once

#include "nav_core.h"
#include "nav_types.h"
#include "nav_containers.h"
#include <vector>

/**
*	@brief	Compute surface area of an axis-aligned bounding box.
*	@param	mins	[in] Minimum extents corner.
*	@param	maxs	[in] Maximum extents corner.
*	@return	Total surface area (2 * (xy + yz + zx)).
**/
double SurfaceArea( const Vector3DP &mins, const Vector3DP &maxs );

/**
*	@brief	Build the KD-tree used for spatial nav queries.
*	@note	Populates g_nav_nodes, g_nav_leaf_links, and g_nav_leaf_poly_ids.
**/
void Nav_BuildKDTree();
