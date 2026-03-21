/********************************************************************
*
*
*	ServerGame: Nav2 Runtime Ownership Seam
*
*
********************************************************************/
#pragma once

#include <string>
#include <tuple>

#include "svgame/svg_local.h"
#include "svgame/nav2/nav2_types.h"


/**
*
*
*	Nav2 Runtime Forward Declarations:
*
*
**/
/**
*
*
*	Nav2 Runtime Public API:
*
*
**/
/**
*	@brief	Return the currently published nav2-owned navigation mesh.
*	@return	Pointer to the active nav2-owned mesh, or `nullptr` when no mesh is available.
*	@note	This returns the nav2-owned runtime container rather than the `main mesh` owner.
**/
nav2_mesh_t *SVG_Nav2_Runtime_GetMesh( void );

/**
*	@brief	Load the staged navigation mesh for the current level through the nav2 ownership seam.
*	@param	levelName	Level name used to resolve the cached navigation asset path.
*	@return	Tuple containing success state and the resolved game-path filename used for the load attempt.
*	@note	The implementation still delegates to the current runtime loader while nav2 absorbs public lifecycle ownership.
**/
const std::tuple<const bool, const std::string> SVG_Nav2_Runtime_LoadMesh( const char *levelName );

/**
*	@brief	Serialize the currently published navigation mesh through the nav2 ownership seam.
*	@param	levelName	Level name used to resolve the cached navigation asset path.
*	@return	True when the current mesh was serialized successfully.
*	@note	The implementation still delegates to the current runtime serializer while nav2 absorbs public lifecycle ownership.
**/
const bool SVG_Nav2_Runtime_SaveMesh( const char *levelName );

/**
*	@brief	Release the currently published navigation mesh through the nav2 ownership seam.
*	@note	The implementation still delegates to the current runtime free path while nav2 absorbs public lifecycle ownership.
**/
void SVG_Nav2_Runtime_FreeMesh( void );

/**
*	@brief	Initialize the staged navigation runtime through the nav2 ownership seam.
*	@note	The implementation still delegates to the existing runtime while nav2 absorbs lifecycle ownership.
**/
void SVG_Nav2_Runtime_Init( void );

/**
*	@brief	Shutdown the staged navigation runtime through the nav2 ownership seam.
*	@note	The implementation still delegates to the existing runtime while nav2 absorbs lifecycle ownership.
**/
void SVG_Nav2_Runtime_Shutdown( void );
