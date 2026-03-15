/****************************************************************************************************************************************
*
*
*    SVGame: Navigation Portal Helpers
*
*    Contains declarations for portal construction and validation helpers.
*
**********************************************************************************************************************************/
#pragma once

#include <vector>

struct nav_mesh_t;
	
/**
*	@brief	Build merged portal records from traversable cross-region tile boundaries.
*	@param	mesh	Navigation mesh containing finalized tile adjacency and region ids.
*	@param	sorted_tile_ids	Deterministically ordered canonical world-tile ids.
**/
void Nav_Hierarchy_BuildPortals( nav_mesh_t * mesh, const std::vector<int32_t> &sorted_tile_ids );

/**
*	@brief	Validate the merged portal graph produced from region boundaries.
*	@param	mesh	Navigation mesh containing freshly built portal records.
*	@return	True when the portal graph references remain internally consistent.
*	@note	This function performs several sanity checks:
*			- Ensures `mesh` is valid.
*			- Ensures every `nav_portal_t` has valid and distinct region endpoints.
*			- Detects duplicate merged portals for the same unordered region pair.
*			- Verifies each portal ID appears in both owning regions' portal reference ranges.
**/
bool Nav_Hierarchy_ValidatePortalGraph( const nav_mesh_t * mesh );

