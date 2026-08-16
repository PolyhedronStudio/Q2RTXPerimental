/********************************************************************
*
*
*	ServerGame: Monster Base Entity Class
*	File: svg_monster_base.h
*	Description:
*		Unified foundation class for all monster entities in the engine.
*		Encapsulates navigation mesh pathfinding, waypoint progression,
*		steering, grounding, and slide-move physics execution.
*
*
********************************************************************/
#pragma once

// Entity includes
#include "svgame/entities/svg_base_edict.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Navigation
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_cover_query.h"

/**
*	@brief	Base entity class for all monsters.
*	@details	Provides shared navigation pathfinding, waypoint progression,
*				active steering, physics grounding, and step slide-move execution.
**/
struct svg_monster_base_t : public svg_base_edict_t {
	//! Default constructor.
	svg_monster_base_t() = default;
	//! Constructor with entityDictionary pointer.
	svg_monster_base_t( const cm_entity_t *ed ) : Super( ed ) {}
	//! Destructor.
	virtual ~svg_monster_base_t() = default;

	/**
	*	Define as abstract base edict class.
	**/
	DefineAbstractClass( svg_monster_base_t, svg_base_edict_t );

	/**
	*
	*
	*	Core:
	*
	*
	**/
	//! Reconstructs the object, optionally retaining the entityDictionary.
	virtual void Reset( const bool retainDictionary = false ) override;
	//! Save the entity into a file using game_write_context.
	virtual void Save( struct game_write_context_t *ctx ) override;
	//! Restore the entity from a loadgame read context.
	virtual void Restore( struct game_read_context_t *ctx ) override;

	/**
	*
	*
	*	Generic Think Support Routines:
	*
	*
	**/
	/**
	*	@brief	Generic support routine taking care of the base logic that each onThink implementation relies on.
	*	@return	True if the caller should proceed with its specific think logic, or false if it should return early.
	**/
	const bool GenericThinkBegin();
	/**
	*	@brief	Generic support routine taking care of the finishing logic that each onThink implementation relies on.
	*	@param	processSlideMove	When true, performs the slide move and all associated logic for handling blocked/trapped results.
	*	@param	blockedMask			[out] The blockedMask result from the slide move.
	*	@return	False if trapped or failed to move, true otherwise.
	**/
	const bool GenericThinkFinish( const bool processSlideMove, int32_t &blockedMask );

	/**
	*
	*
	*	Physics Movement & Grounding:
	*
	*
	**/
	//! Current movement state for the monster.
	mm_move_t monsterMove = {};

	/**
	*	@brief	Performs SlideMove processing and updates the final origin if successful.
	*	@return	The blockedMask result from the slide move.
	**/
	const int32_t ProcessSlideMove();
	/**
	*	@brief	Recategorizes the entity's ground/liquid states.
	**/
	const void RecategorizeGroundAndLiquidState();
	/**
	*	@brief	Retrieves the feet-origin agent bounds for navigation queries.
	*	@param	out_mins	[out] Minimum bounding extent in feet-origin space.
	*	@param	out_maxs	[out] Maximum bounding extent in feet-origin space.
	**/
	void GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs );

	/**
	*
	*
	*	Navigation & Path State:
	*
	*
	**/
	//! State for tracking navigation metrics.
	struct PathNavigationState_t {
		struct goalNavigationState_t {
			//! Tracks the last goal position used to validate cached navigation paths.
			Vector3 origin = { 0.f, 0.f, 0.f };
			//! Tracks whether last_nav_goal_origin holds valid data.
			bool isValid = false;
			//! Tracks whether the goal was visible when the last nav goal was recorded.
			bool isVisible = false;
		} lastGoal = {};

		//! The navigation policy to use for A* pathfinding.
		nav_path_policy_t policy = {};
	} pathNavigationState = {};

	//! Cached navigation path for the current A* pursuit.
	std::vector<int32_t> navPath;
	//! Cached string-pulled path in high-precision Vector3DP.
	std::vector<Vector3DP> stringPulledPath;
	//! Per-waypoint constraints that preserve mandatory stair approaches and crossings.
	std::vector<bool> stringPulledWaypointForced;
	//! Current position in the navPath vector.
	size_t pathPos = 0;
	//! Current position in the stringPulledPath vector.
	size_t stringPathPos = 0;
	//! Last server time when the path was calculated.
	QMTime lastPathCalcTime = 0_ms;
	//! Counts consecutive blocked/trapped movement frames while actively navigating.
	int32_t consecutiveBlockedFrames = 0;
	//! Last server time when blocked/trapped movement was observed.
	QMTime lastBlockedFrameTime = 0_ms;
	//! Most recent wall-like contact normal captured from slide traces.
	Vector3 recentWallBlockNormal = { 0.0f, 0.0f, 0.0f };
	//! Whether recentWallBlockNormal contains a valid wall contact normal.
	bool hasRecentWallBlockNormal = false;
	//! Server time when recentWallBlockNormal was last updated.
	QMTime lastWallBlockTime = 0_ms;
	//! KD-Tree caching
	int32_t cachedLeaf = -1;
	int32_t cachedPoly = -1;

	/**
	*	@brief	Path evaluation result states.
	**/
	enum class PathComputeResult {
		Failed,                 // Path generation failed completely
		ReusedCached,           // Reused the existing valid path (debounced)
		NewPathGenerated        // Generated a brand new path successfully
	};

	/**
	*
	*
	*	Navigation API:
	*
	*
	**/
	/**
	*	@brief	Clear stale async nav request state when no navmesh is loaded.
	*	@return	True when navmesh is unavailable and caller should early-return.
	**/
	const bool GuardForNullNavMesh();
	/**
	*	@brief	Reset cached navigation path state.
	**/
	void ResetNavigationPath();
	/**
	*	@brief	Check if the path should be recalculated based on distance and time.
	*	@param	pos	Target destination position.
	**/
	const bool ShouldRecalcPath( const Vector3 &pos );
	/**
	*	@brief	Find the current KD-Tree polygon the entity is standing on.
	**/
	const int32_t FindCurrentPoly();
	/**
	*	@brief	Compute an A* path to the target origin.
	*	@param	target	Target destination world-space position.
	*	@param	force	When true, bypasses debouncing and recalculates immediately.
	**/
	PathComputeResult ComputePathTo( const Vector3 &target, const bool force = false );
	/**
	*	@brief	Progress along navigation path, advance active segments, and compute lookahead steering direction.
	*	@param	finalGoal		Destination goal in world space.
	*	@param	outMoveDir		[out] Normalized 2D horizontal movement direction.
	*	@param	outSpeedScale	[out] Speed scaling factor for smooth corner deceleration.
	*	@return	True if movement towards goal should continue, false if arrived at goal.
	**/
	const bool ComputePathSteering( const Vector3DP &finalGoal, Vector3DP *outMoveDir, double *outSpeedScale );
	/**
	*	@brief	Steer and move entity towards goal origin using navigation path steering.
	*	@param	goalOrigin	Target world-space position.
	*	@return	True if entity moved, false if arrived or stopped.
	**/
	const bool StepMoveToGoal( const Vector3 &goalOrigin );
	/**
	*	@brief	High-level navigation driver that recalculates paths when necessary and drives movement.
	*	@param	goalOrigin	Target world-space position.
	*	@param	force		When true, forces path recalculation.
	*	@return	True if movement was updated, false otherwise.
	**/
	const bool MoveAStarToOrigin( const Vector3 &goalOrigin, bool force = false );
	/**
	*	@brief	Get the next active waypoint from the navigation path in full double precision.
	*	@param	finalGoal	Destination goal in world space.
	**/
	const Vector3DP NextWaypoint( const Vector3DP &finalGoal );
	/**
	*	@brief	Get the next active waypoint from the navigation path (single precision convenience wrapper).
	*	@param	finalGoal	Destination goal in world space.
	**/
	const Vector3 NextWaypoint( const Vector3 &finalGoal );
	/**
	*	@brief	Update blocked/trapped recovery bookkeeping and force path refresh after sustained stalls.
	*	@param	blockedMask	Slide move blocked flags for the current think frame.
	**/
	void UpdateBlockedNavigationRecovery( const int32_t blockedMask );

	/**
	*
	*
	*	Animation Dispatch Interface:
	*
	*
	**/
	/**
	*	@brief	Pure virtual animation updater implemented by concrete monster subclasses.
	*	@param	animId	Model-specific animation identifier.
	**/
	virtual void UpdateAnim( const int32_t animId ) = 0;
};
