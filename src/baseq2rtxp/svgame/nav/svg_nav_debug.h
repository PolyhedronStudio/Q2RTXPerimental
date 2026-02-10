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
*	@brief	Reasons an A* edge rejection is worth visualizing.
**/
typedef enum nav_debug_reject_reason_s {
	NAV_DEBUG_REJECT_REASON_DROP_CAP = 0, //! Edge dropped for exceeding the allowed drop cap.
	NAV_DEBUG_REJECT_REASON_CLEARANCE = 1, //! Edge rejected because the mover lacked the clearance to climb.
	NAV_DEBUG_REJECT_REASON_SLOPE = 2, //! Edge rejected for exceeding the allowed slope.
} nav_debug_reject_reason_t;

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
*  	@brief  Purge expired cached debug paths.
**/
void NavDebug_PurgeCachedPaths( void );
/**
*	@brief  Clear all cached debug paths immediately.
**/
void NavDebug_ClearCachedPaths( void );

/**
*    CVars for navigation debug drawing exposed to tuning.
**/
//! Master on/off. 0 = off, 1 = on.
extern cvar_t *nav_debug_draw;
//! Only draw a specific BSP leaf index. -1 = any.
extern cvar_t *nav_debug_draw_leaf;
//! Only draw a specific tile (grid coords). Requires "x y". "*" disables.
extern cvar_t *nav_debug_draw_tile;
//! Budget in TE_DEBUG_TRAIL line segments per server frame.
extern cvar_t *nav_debug_draw_max_segments;
//! Max distance from current view player to draw debug (world units). 0 disables.
extern cvar_t *nav_debug_draw_max_dist;
//! Draw tile bboxes.
extern cvar_t *nav_debug_draw_tile_bounds;
//! Draw sample ticks.
extern cvar_t *nav_debug_draw_samples;
//! Draw final path segments from traversal path query.
extern cvar_t *nav_debug_draw_path;
//! Temporary cvar to enable printing of failed nav lookup diagnostics (tile/cell indices).
extern cvar_t *nav_debug_show_failed_lookups;
//! Toggle to draw/log edges rejected due to clearance or drop cap.
extern cvar_t *nav_debug_draw_rejects;


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
*	@param	pos	Position to check.
**/
inline const bool NavDebug_PassesDistanceFilter( const Vector3 &pos );
/**
*	@return	Returns true if a tile filter is specified and outputs the wanted tile coordinates.
**/
const bool NavDebug_ParseTileFilter( int32_t *outTileX, int32_t *outTileY );
/**
*	@brief	Filter by leaf index.
*	@return	Returns true if the given leaf index passes the filter.
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
/**
*	@brief	Record a rejected traversal edge for optional visualization.
*	@param	start	World-space start of the failed edge.
*	@param	end	World-space end of the failed edge.
*	@param	reason	Reason the edge was rejected.
**/
void NavDebug_RecordReject( const Vector3 &start, const Vector3 &end, nav_debug_reject_reason_t reason );
/**
*	@brief	Clear any previously recorded rejection events.
**/
void NavDebug_ClearRejects( void );