/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Save
*
*
********************************************************************/
#pragma once



/**
*	@brief	Serialize the current navigation voxelmesh to a file.
*	@note	This is a development-time cache; format stability is not guaranteed.
*	@return	True on success, false on failure.
**/
bool SVG_Nav_SaveVoxelMesh( const char *filename );
