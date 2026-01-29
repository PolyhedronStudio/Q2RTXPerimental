/********************************************************************
*
*
*	SVGame: Navigation Voxelmesh Debugging
*
*
********************************************************************/
#pragma once



/**
*
*
*
*	Navigation Debug Drawing - State and Helpers:
*
*
*
**/
// Cached debug paths (last computed traversal paths for the current server frame).
static constexpr QMTime NAV_DEBUG_PATH_RETENTION = QMTime::FromMilliseconds( 1500 );
/**
*  @brief  Cached debug path structure.
**/
struct nav_debug_cached_path_t {
	//! Cached traversal path.
	nav_traversal_path_t path;
	//! Level Time at which this cached path expires.
	QMTime expireTime;
};
//! Cached debug paths.
extern std::vector<nav_debug_cached_path_t> s_nav_debug_cached_paths;

/**
*	@brief  Navigation debug drawing state
***/
typedef struct nav_debug_draw_state_s {
	int64_t frame = -1;
	int32_t segments_used = 0;
} nav_debug_draw_state_t;
//! Global debug draw state.
extern nav_debug_draw_state_t s_nav_debug_draw_state;

/**
* 	@brief  Purge expired cached debug paths.
**/
void NavDebug_PurgeCachedPaths( void );
/**
*	@brief  Clear all cached debug paths immediately.
**/
void NavDebug_ClearCachedPaths( void );



/**
*
*
*
*	Navigation Debug Drawing - Runtime Draw:
*
*
*
**/
/**
*	@brief	Prepare for a new frame of debug drawing.
**/
inline void NavDebug_NewFrame( void );
/**
*	@brief	Determine if we can emit the requested number of segments.
**/
inline bool NavDebug_CanEmitSegments( const int32_t count );
/**
*	@brief	Consume the given number of emitted segments.
**/
inline void NavDebug_ConsumeSegments( const int32_t count );
/**
*	@brief	Check if navigation debug drawing is enabled.
**/
inline const bool NavDebug_Enabled( void );
/**
*	@brief	Get the current view origin for distance filtering.
**/
inline Vector3 NavDebug_GetViewOrigin( void );
/**
*	@brief	Check if a position passes the distance filter.
* 	@param	pos	Position to check.
**/
inline const bool NavDebug_PassesDistanceFilter( const Vector3 &pos );
/**
*	@return	Returns true if a tile filter is specified and outputs the wanted tile coordinates.
**/
const bool NavDebug_ParseTileFilter( int32_t *outTileX, int32_t *outTileY );
/**
*	@brief	Filter by leaf index.
* 	@return	Returns true if the given leaf index passes the filter.
**/
inline const bool NavDebug_FilterLeaf( const int32_t leafIndex );
/**
*	@brief	Will filter by tile coordinates.
*	@return	Returns true if the given tile coordinates pass the filter.
**/
inline const bool NavDebug_FilterTile( const int32_t tileX, const int32_t tileY );
/**
*	@brief	Draw a vertical tick at the given position.
**/
void NavDebug_DrawSampleTick( const Vector3 &pos, const double height );
/**
*	@brief	Draw the bounding box of a tile.
*	@param	mesh	The navigation mesh.
*	@param	tile	The tile to draw.
**/
void NavDebug_DrawTileBBox( const nav_mesh_t *mesh, const nav_tile_t *tile );