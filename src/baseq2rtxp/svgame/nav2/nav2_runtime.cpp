/********************************************************************
*
*
*	ServerGame: Nav2 Runtime Ownership Seam - Implementation
*
*
********************************************************************/
#include "svgame/nav2/nav2_runtime.h"
#include "svgame/nav2/nav2_format.h"

//! Nav2-owned runtime mesh published through the runtime seam.
nav2_mesh_raii_t g_nav_mesh = {};

/**
*	@brief	Return the currently published nav2-owned navigation mesh.
*	@return	Pointer to the active nav2-owned mesh, or `nullptr` when no mesh is available.
*	@note	This returns the nav2-owned runtime container rather than the `main mesh` owner.
**/
nav2_mesh_t *SVG_Nav2_Runtime_GetMesh( void ) {
	// Return the currently published nav2-owned runtime mesh.
	return g_nav_mesh.get();
}

/**
*	@brief	Release the currently published navigation mesh through the nav2 ownership seam.
*	@note	This clears only nav2-owned mesh state.
**/
void SVG_Nav2_Runtime_FreeMesh( void ) {
	// Reset the nav2-owned mesh placeholder and release its owned storage.
	if ( g_nav_mesh || !g_nav_mesh ) {
		g_nav_mesh = nav2_mesh_raii_t{};
	}
}


/**
*	@brief	Initialize the staged navigation runtime through the nav2 ownership seam.
*	@note	This currently ensures the nav2-owned mesh placeholder exists.
**/
void SVG_Nav2_Runtime_Init( void ) {
	// Allocate the nav2-owned mesh placeholder if it does not exist yet.
    if ( g_nav_mesh.get() == nullptr ) {
		g_nav_mesh.create( TAG_SVGAME_NAVMESH );
	}
}

/**
*	@brief	Shutdown the staged navigation runtime through the nav2 ownership seam.
*	@note	This currently tears down only nav2-owned mesh state.
**/
void SVG_Nav2_Runtime_Shutdown( void ) {
	// Release nav2-owned mesh state during shutdown.
	SVG_Nav2_Runtime_FreeMesh();
}

/**
*	@brief	Load the staged navigation mesh for the current level through the nav2 ownership seam.
*	@param	levelName	Level name used to resolve the cached navigation asset path.
*	@return	Tuple containing success state and the resolved game-path filename used for the load attempt.
*	@note	This is currently a temporary nav2 ownership seam and does not depend on the oldnav folder.
**/
const std::tuple<const bool, const std::string> SVG_Nav2_Runtime_LoadMesh( const char *levelName ) {
	// Validate the caller-provided level name before reporting a load attempt.
	if ( !levelName || levelName[ 0 ] == '\0' ) {
		return std::make_tuple( false, std::string() );
	}

	// Create a nav2-owned mesh placeholder so the ownership seam is exercised without legacy dependencies.
	if ( !g_nav_mesh ) {
		g_nav_mesh.create( TAG_SVGAME_NAVMESH );
	}

	// Mark the placeholder mesh as loaded so query callers have a stable owned object to bind to.
	g_nav_mesh->loaded = true;
	g_nav_mesh->serialized_format_version = NAV_SERIALIZED_FORMAT_VERSION;
	return std::make_tuple( true, std::string( levelName ) );
}

/**
*	@brief	Serialize the currently published navigation mesh through the nav2 ownership seam.
*	@param	levelName	Level name used to resolve the cached navigation asset path.
*	@return	True when the current mesh was serialized successfully.
*	@note	This temporary seam only reports success when a nav2-owned mesh exists.
**/
const bool SVG_Nav2_Runtime_SaveMesh( const char *levelName ) {
	// Validate the caller-provided level name and require an owned mesh to exist before saving.
    if ( !levelName || levelName[ 0 ] == '\0' || g_nav_mesh.get() == nullptr ) {
		return false;
	}

	// The real persistence path will be filled in after the nav2 data model replaces the scaffold.
	return true;
}
