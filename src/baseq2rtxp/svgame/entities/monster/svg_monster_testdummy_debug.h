/********************************************************************
*
*
*    ServerGame: TestDummy Debug NPC Edict (A* only)
*    File: svg_monster_testdummy_debug.h
*    Description:
*        Lightweight debug variant of the TestDummy that always attempts
*        asynchronous A* to its activator. Useful for isolating pathfinding
*        failures without other AI behavior interfering.
*
*
********************************************************************/
#pragma once


// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"
//#include "svgame/entities/monster/svg_monster_testdummy.h"

// Navigation
#include "svgame/nav/nav_path.h"



/**
 *    Debug TestDummy Entity: always A* to activator
 **/
struct svg_monster_testdummy_debug_t : public svg_base_edict_t {
	//! Default constructor.
	svg_monster_testdummy_debug_t() = default;
        //! Constructor for use with constructing for an cm_entity_t *entityDictionary.
	svg_monster_testdummy_debug_t( const cm_entity_t *ed ) : Super( ed ) {};
	//! Destructor.
	virtual ~svg_monster_testdummy_debug_t() = default;

	/**
	*    Register this spawn class as a world-spawnable entity.
	**/
	DefineWorldSpawnClass(
		"monster_testdummy_debug", svg_monster_testdummy_debug_t, svg_base_edict_t,/*svg_monster_testdummy_t,*/
		EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
		svg_monster_testdummy_debug_t::onSpawn
	);



	/**
	*
	*
	*
	*		Entity Callbacks.
	*
	*
	*
	**/
	/**
	*   @brief  Spawn.
	**/
	DECLARE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_debug_t, onSpawn );
	/**
	*   @brief  Post-Spawn.
	**/
	DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_debug_t, onPostSpawn );
	/**
	*   @brief  Thinking.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink );

	/**
	*   @brief  Touched.
	**/
	DECLARE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_debug_t, onTouch );
	/**
	*   @brief
	**/
	DECLARE_MEMBER_CALLBACK_USE( svg_monster_testdummy_debug_t, onUse );
	/**
	*   @brief
	**/
	DECLARE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_debug_t, onPain );
	/**
	*   @brief
	**/
	DECLARE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_debug_t, onDie );



	/**
	*
	*
	*
	*		Entity 'onThink' State Routines:
	*			- Each of these is set as the nextThink for the onThink callback, 
	*			and thus determines the behavior of the next think frame.
	*
	*
	*
	**/
	/**
	*	@brief	A* specific thinker: always attempt async A* to activator if present(and if it goes LOS, sets think to onThink_AStarPursuitTrail.), otherwise go idle.
	*
	*	@details	Will always check for player presence first, and if not present will check for trail presence.
	*				If player is present, will attempt async A* to player.
	*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
	*				If neither are present, will set nextThink to onThink_Idle.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarToPlayer );
	/**
	*	@brief	A* specific thinker: always attempt async A* to activator trail if present, and otherwise go idle.
	* 
	*	@details	Will always check for player presence first, and if not present will check for trail presence. 
	*				If trail is present, will attempt async A* to the trail's last known position. 
	*				If player is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player. 
	*				If neither are present, will set nextThink to onThink_Idle.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_AStarPursuitTrail );
	/**
	*   @brief	Investigate the most recently heard player-noise position, then return to idle/pursuit.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_InvestigateSound );

	/**
	*	@brief	Always looks for activator presence, or its trail, and otherwise does nothing.
	* 
	*	@details	If we are in idle state, it means we failed to find a valid target to pursue in the previous think.
	*				If activator is present, sets nextThink to onThink_AStarToPlayer and attempt async A* to player.
	*				If trail is present, sets nextThink to onThink_AStarPursuitTrail and attempt async A* to the trail's last known position.
	*				If neither are present, will keep nextThink as onThink_Idle and do nothing.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Idle );
	/**
	*	@brief	Set when dead. Does nothing.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_debug_t, onThink_Dead );


	//=============================================================================================
	//=============================================================================================
	

	/**
	*
	*
	*
	*		(Generic-) NPC Entity Think Support Routines:
	*
	*
	*
	**/
	/**
	*	@brief	Generic support routine taking care of the base logic that each onThink implementation relies on.
	*			( Setup navPolicy, recategorize ground and liquid information,  check for being alive, 
	*			  check for activator presence, etc).
	*	@return	True if the caller should proceed with its specific think logic, or false if it should return early and skip the specific think logic.
	**/
	const bool GenericThinkBegin();
	/**
	*	@brief	Generic support routine taking care of the finishing logic that each onThink implementation relies on.
	*			( Deal with the slidemove process, stepping stairs, jumping over obstructions, crouching under obstructions. ).
	*	@param	processSlideMove	When true, will perform the slide move and all the associated logic for handling blocked/trapped results.
	*								When false, will skip the slide move and just return false so the caller can handle it in its specific way
	*								(e.g., the caller may want to try a different movement approach if we are blocked, or may want to ignore being blocked if we are just trying to adjust our position to better see the player).
	*	@param	blockedMask			The blockedMask result from the slide move, which is important for the caller to determine if we should do any special handling
	*								for being trapped (e.g., try to jump, pick a new path, etc).
	*	@return	False if we didn't move, true if we did.
	**/
	const bool GenericThinkFinish( const bool processSlideMove, int32_t &blockedMask );


	//=============================================================================================
	//=============================================================================================


	/**
	*
	*
	*
	*
	*	Explicit NPC State Management:
	*
	*
	*
	*
	**/
	//! Tracks whether the NPC has been enabled(By use, with the intend for it to follow the player) by the player.
	bool isActivated = false;
	//! Last server time when the activator was confirmed visible.
	QMTime lastPlayerVisibleTime = 0_ms;

	/**
	*   Explicit AI states.
	**/
	enum class AIThinkState {
		IdleLookout,
		PursuePlayer,
		PursueBreadcrumb,
		InvestigateSound
	};
	//! Determines the thinking state callback to fire for the frame.
	AIThinkState thinkAIState = AIThinkState::IdleLookout;



	/**
	*
	*
	*
	*	Animation Processing Work:
	*
	*
	*
	**/
	skm_rootmotion_set_t *rootMotionSet = nullptr;


	//=============================================================================================
	//=============================================================================================
	
	/**
	*
	*
	*
	*	Basic AI Physics Movement(-State):
	*
	*
	*
	**/
	//! Current movement state for the monster, used for both the main think dispatcher and the specific A* pursuit thinkers.
	mm_move_t monsterMove = {};

	/**
	*	@brief	Performs the actual SlideMove processing and updates the final origin if successful.
	*	@return	The blockedMask result from the slide move, which is important for the caller to determine if we should do any special handling for being trapped (e.g., try to jump, pick a new path, etc).
	**/
	const int32_t ProcessSlideMove();

	/**
	*    @brief    Slerp direction helper. (Local to this TU to avoid parent dependency).
	**/
	const Vector3 SlerpDirectionVector3( const Vector3 &from, const Vector3 &to, float t );

	/**
	*	@brief	Recategorize the entity's ground/liquid and ground states.
	**/
	const void RecategorizeGroundAndLiquidState();


	//=============================================================================================
	//=============================================================================================


	/**
	*
	*
	*
	*	Internal Navigation Queueing and Path Following Logic API:
	*
	*
	*
	**/

	void GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs );

	/**
	*   @brief	Clear stale async nav request state when no navmesh is loaded.
	*   @param	self	Debug testdummy owning the async path process.
	*   @return	True when navmesh is unavailable and caller should early-return.
	*   @note	Prevents repeated queue refresh/debounce loops on maps without navmesh.
	**/
	const bool GuardForNullNavMesh();

	/**
	*    @brief    Attempt A* navigation to a target origin. (Local to this TU to avoid parent dependency).
	*    @param    goalOrigin      World-space destination used for the rebuild request.
	*    @param    force           When true, cancel any pending rebuild and enqueue immediately.
	*    @return   True if movement/animation was updated.
	**/
	const bool MoveAStarToOrigin( const Vector3 &goalOrigin, bool force = false );
	
	/**
	*	@brief	Enqueue a navigation rebuild request when the async queue is enabled.
	*	@param	self	NPC owning the path process state.
	*	@param	start_origin	Current feet-origin start position.
	*	@param	goal_origin	Desired feet-origin goal position.
	*	@param	policy	Path-follow policy tuning rebuild heuristics.
	*	@param	agent_mins	Feet-origin agent bbox minimums.
	*	@param	agent_maxs	Feet-origin agent bbox maximums.
	*    @brief	Reset cached navigation path state for the test dummy.
	*    @param	self	NPC whose path state should be cleared.
	*    @note	Cancels any queued async request and clears cached path buffers.
	**/
	void ResetNavigationPath();
	/**
	*	@brief	Update blocked/trapped recovery bookkeeping and force path refresh after sustained stalls.
	*	@param	blockedMask	Slide move blocked flags for the current think frame.
	**/
	void UpdateBlockedNavigationRecovery( const int32_t blockedMask );




	//=============================================================================================
	//=============================================================================================


	/**
	* 
	* 
	* 
	*	Path Navigation State:
	* 
	* 
	* 
	**/
	/**
	*	
	**/
	/**
	*	@brief	State for tracking navigation metrics.
	**/
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

	/**
	*	@brief	Check if the path should be recalculated based on distance and time.
	**/
	const bool ShouldRecalcPath( const Vector3 &pos );

	/**
	*	@brief	Find the current KD-Tree polygon the entity is standing on.
	**/
	const int32_t FindCurrentPoly();

	/**
	*	@brief	Compute an A* path to the target origin.
	**/
	const bool ComputePathTo( const Vector3 &target, const bool force = false );

	/**
	*    @brief    Get the next waypoint from the navigation path.
	**/
	const Vector3 NextWaypoint( const Vector3 &finalGoal );
	/**
	*	@brief	Stabilize per-frame waypoint targets to avoid portal-side flip oscillation.
	*	@param	candidateWaypoint	Raw waypoint candidate produced by portal logic.
	*	@param	isFinalGoalWaypoint	True when candidateWaypoint is the final-goal fallback.
	*	@return	A smoothed/sticky waypoint suitable for stable yaw and movement steering.
	**/
	const Vector3 StabilizeWaypointTarget( const Vector3 &candidateWaypoint, const bool isFinalGoalWaypoint );

	//! KD-Tree caching
	int32_t cachedLeaf = -1;
	int32_t cachedPoly = -1;

	/**
	*	@brief	Checks for audible sounds and transitions to InvestigateSound if a fresh one is found.
	*	@return	True if a sound was heard and state changed, false otherwise.
	**/
	bool CheckForAudibleSounds();

	/**
	* 
	* 
	* 
	*	A *Path State
	* 
	* 
	* 
	**/
	//! Cached navigation path for the current A* pursuit.
	std::vector<int32_t> navPath;
	//! Cached string-pulled path.
	std::vector<Vector3> stringPulledPath;
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
	//! Last stable horizontal navigation move direction used for continuity smoothing.
	Vector3 lastNavigationMoveDir = { 0.0f, 0.0f, 0.0f };
	//! Whether lastNavigationMoveDir contains valid data.
	bool hasLastNavigationMoveDir = false;
	//! Last stable facing direction used to smooth yaw against rapid 180-degree target flips.
	Vector3 lastFacingDirection = { 0.0f, 0.0f, 0.0f };
	//! Whether lastFacingDirection contains valid data.
	bool hasLastFacingDirection = false;
	//! Last stabilized waypoint used for per-segment target hysteresis.
	Vector3 lastWaypointTarget = { 0.0f, 0.0f, 0.0f };
	//! Whether lastWaypointTarget contains valid data.
	bool hasLastWaypointTarget = false;
	//! Path segment index associated with lastWaypointTarget.
	size_t lastWaypointPathPos = 0;
	//! Server time when lastWaypointTarget was updated.
	QMTime lastWaypointUpdateTime = 0_ms;



	/**
	*
	*
	*
	*	Behavioral NPC States:
	*
	*
	*
	**/
	/**
	*	@brief	State information for the idle lookout scanning behavior.
	*			Used for the onThink_Idle state, which is the default fallback state when we have no target to pursue.
	*
	*	@note	In a more complex implementation, we would probably want to have this support multiple simultaneous sound investigations with a proper queue,
	*			and we would want the idle scan state to also keep track of which discrete heading we are currently looking at and when
	*			to flip to the next one.
	*
	*			But for the sake of this simple debug implementation, we'll just keep track of the most recent sound event and have
	*			a simple idle scan that flips direction every few seconds and steps through discrete headings every few seconds.
	**/
	struct StateIdleScan_t {
		//! Idle scan yaw direction (+1 / -1) used for lookout sweeping.
		double yawScanDirection = 1.0;
		//! Time when idle scan should flip direction.
		QMTime nextFlipTime = 0_ms;
		//! Current discrete idle heading index (0..7) used for 45deg stepped scanning.
		int32_t headingIndex = 0;
		//! Target yaw degrees for the current idle heading. We lerp ideal_yaw toward this to smooth turns.
		float targetYaw = 0.0;
	} stateIdleScan = {};

	/**
	*	@brief	Navigation state for when we are pursuing the activator's breadcrumb trail instead of the activator itself.
	**/
	struct StateNavigationTrail_t {
		//! Current breadcrumb we are attempting to chase when following the trail.
		svg_base_edict_t *targetEntity = nullptr;

		//! Time marker used by the player-trail system. Monsters set this to indicate
		//! which trail timestamp they are currently following. Defaults to 0 (no trail).
		QMTime   trailTimeStamp = 0_ms;
	} stateNavigationTrail = {};
	
	/**
	*	@brief	Information about the most recently heard sound event that we are investigating. 
	*			Used for the onThink_InvestigateSound state.
	**/
	struct StateSoundScan_t {
		//! The cached investigation target origin of the sound event that we took note of.
		//! Cached investigation target from the latest heard sound event.
		Vector3 origin = { 0.0f, 0.0f, 0.0f };
		//! Whether stateSoundCan.origin currently contains a valid target.
		bool hasOrigin = false;
		//! Last processed sound timestamp so we do not re-investigate the same event.
		QMTime lastTime = 0_ms;
	} stateSoundCan = {};



	/**
	*
	*   Const Expressions:
	*
	**/
	//! For when dummy is standing straight up.
	static constexpr const Vector3 DUMMY_BBOX_STANDUP_MINS	= PHYS_DEFAULT_BBOX_STANDUP_MINS;
	static constexpr const Vector3 DUMMY_BBOX_STANDUP_MAXS	= PHYS_DEFAULT_BBOX_STANDUP_MAXS;
	static constexpr const double DUMMY_VIEWHEIGHT_STANDUP	= PHYS_DEFAULT_VIEWHEIGHT_STANDUP;
	//! For when dummy is crouching.
	static constexpr const Vector3 DUMMY_BBOX_DUCKED_MINS	= PHYS_DEFAULT_BBOX_DUCKED_MINS;
	static constexpr const Vector3 DUMMY_BBOX_DUCKED_MAXS	= PHYS_DEFAULT_BBOX_DUCKED_MAXS;
	static constexpr const double DUMMY_VIEWHEIGHT_DUCKED	= PHYS_DEFAULT_VIEWHEIGHT_DUCKED;
	//! For when dummy is dead.
	static constexpr const Vector3 DUMMY_BBOX_DEAD_MINS		= { -16., -16., -36. };
	static constexpr const Vector3 DUMMY_BBOX_DEAD_MAXS		= { 16., 16., 8. };
	static constexpr double	DUMMY_VIEWHEIGHT_DEAD			= 8.;
};
