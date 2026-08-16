#pragma once

#include "svgame/nav/nav_cover_types.h"

/**
*	@brief	Generate tactical cover points for all boundary edges in the compiled navmesh.
*	@note	Invoked during navmesh generation after half-edges and faces are constructed.
**/
void Nav_GenerateCoverPoints( void );

/**
*	@brief	Clear all generated cover points from memory.
**/
void Nav_ClearCoverPoints( void );
