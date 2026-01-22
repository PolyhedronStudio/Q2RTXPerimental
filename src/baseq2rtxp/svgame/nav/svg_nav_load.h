/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Load
*
*
********************************************************************/
#pragma once



/**
*	@brief	Load a navigation voxelmesh from file into `g_nav_mesh`.
*	@note	This is a development-time cache; format stability is not guaranteed.
*	@return	True on success, false on failure.
**/
bool SVG_Nav_LoadVoxelMesh( const char *filename );
