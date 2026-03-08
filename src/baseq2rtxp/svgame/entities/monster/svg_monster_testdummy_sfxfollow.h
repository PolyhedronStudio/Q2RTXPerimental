/********************************************************************
*
*
*    ServerGame: TestDummy Sound-Follow NPC Edict
*    File: svg_monster_testdummy_sfxfollow.h
*    Description:
*		Lightweight debug variant of the TestDummy that always attempts
*		asynchronous A* to a last heard nearby sound.
*		Useful for isolating pathfinding failures without other AI behavior interfering.
*
*
********************************************************************/
#pragma once

// Navigation policy.
#include "svgame/nav/svg_nav.h"
// Navigation and pathfinding.
#include "svgame/nav/svg_nav_path_process.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"
//#include "svgame/entities/monster/svg_monster_testdummy.h"



/**
 *    Sound-follow TestDummy Entity: investigate the most recently heard nearby sound
 **/
struct svg_monster_testdummy_sfxfollow_t : public svg_base_edict_t {
	//! Default constructor.
	svg_monster_testdummy_sfxfollow_t() = default;
		//! Constructor for use with constructing for an cm_entity_t *entityDictionary.
	svg_monster_testdummy_sfxfollow_t( const cm_entity_t *ed ) : Super( ed ) {};
	//! Destructor.
	virtual ~svg_monster_testdummy_sfxfollow_t() = default;

	/**
	*    Register this spawn class as a world-spawnable entity.
	**/
	DefineWorldSpawnClass(
		"monster_testdummy_sfxfollow", svg_monster_testdummy_sfxfollow_t, svg_base_edict_t,/*svg_monster_testdummy_t,*/
		EdictTypeInfo::TypeInfoFlag_WorldSpawn | EdictTypeInfo::TypeInfoFlag_GameSpawn,
		svg_monster_testdummy_sfxfollow_t::onSpawn
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
	DECLARE_MEMBER_CALLBACK_SPAWN( svg_monster_testdummy_sfxfollow_t, onSpawn );
	/**
	*   @brief  Post-Spawn.
	**/
	DECLARE_MEMBER_CALLBACK_POSTSPAWN( svg_monster_testdummy_sfxfollow_t, onPostSpawn );
	/**
	*   @brief  Thinking.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink );

	/**
	*   @brief  Touched.
	**/
	DECLARE_MEMBER_CALLBACK_TOUCH( svg_monster_testdummy_sfxfollow_t, onTouch );
	/**
	*   @brief
	**/
	DECLARE_MEMBER_CALLBACK_USE( svg_monster_testdummy_sfxfollow_t, onUse );
	/**
	*   @brief
	**/
	DECLARE_MEMBER_CALLBACK_PAIN( svg_monster_testdummy_sfxfollow_t, onPain );
	/**
	*   @brief
	**/
	DECLARE_MEMBER_CALLBACK_DIE( svg_monster_testdummy_sfxfollow_t, onDie );



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
	*	@brief		Deactivated idle thinker.
	*	@details	Keeps the sound-follow NPC stationary until it is explicitly activated by use.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_Idle );
	/**
	*	@brief		Activated lookout thinker.
	*
	*	@details	Sweeps in place while monitoring for fresh audible player-noise events.
	*				Transitions into `onThink_PursueSoundInvestigation` once a valid sound target is acquired.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_IdleLookout );
	/**
	*   @brief		Pursue the cached sound investigation target with async A*.
	*   @details	Keeps following the current sound origin until it is reached, expires, or a newer sound event replaces it.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_PursueSoundInvestigation );
	/**
	*	@brief	Set when dead. Does nothing.
	**/
	DECLARE_MEMBER_CALLBACK_THINK( svg_monster_testdummy_sfxfollow_t, onThink_Dead );


	//=============================================================================================
	//=============================================================================================


	/**
	*
	*
	*
	*		Generic Core (Think-) Support Routines:
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
	//! Tracks whether the NPC has been enabled(By use, with the intend for it to A* to sound locations) by the player.
	bool isActivated = false;
	//! Last time we heard a sound from the player.
	QMTime aiLastSoundHeard = 0_ms;
	//! Last server time when the activator was confirmed visible.
	QMTime aiLastPlayerSeen = 0_ms;

	/**
	*   Explicit AI states.
	**/
	enum class AIThinkState {
		//! Idle when deactivated.
		Idle,
		//! Idle lookout for sound events, with occasional activator checks.
		IdleLookout,
		//! Pursue the sound source with async A*, but do not update the goal unless we get a new sound event.
		PursueSoundInvestigation
	};
	//! Determines the thinking state callback to fire for the frame.
	AIThinkState thinkAIState = AIThinkState::Idle;

	/**
	*	@brief	NPC Goal Z Blend Policy Adjustment Helper:
	**/
	void DetermineGoalZBlendPolicyState( const Vector3 &goalOrigin );



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
	mm_move_t monsterMoveState = {};

	///**
	//*	AI SlideMove Flags for the current frame, which is used to tune the behavior 
	//*	of the slide move process in the GenericThinkFinish routine.
	//**/
	//enum mm_ai_move_flags_t {
	//	//! Should simply not be the case.
	//	NONE	= 0,
	//	//! Moved successfully with no obstructions.
	//	MOVED	= BIT( 0 ),
	//	//! Move hit into an obstruction, does not imply we did not slide at all.
	//	HIT_OBSTRUCTION = BIT( 1 ),
	//	//! Move is completely blocked and we did not slide at all.
	//	TRAPPED = BIT( 2 )
	//} aiMoveFlags = mm_ai_move_flags_t::NONE;

	/**
	*	@brief	Performs the actual SlideMove processing and updates the final origin if successful.
	*	@return	The blockedMask result from the slide move, which is important for the caller to determine if we should do any special handling for being trapped (e.g., try to jump, pick a new path, etc).
	**/
	const mm_slide_move_flags_t ProcessSlideMove();

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
	*	Internal Logic APIs:
	*		- Navigation Agent Bounds Retrieval
	*		- Navigation Queueing
	*		- Path Following
	*
	*
	*
	**/
	/**
	*	@brief	Retrieve the appropriate navigation agent bounds for the entity, prioritizing navmesh-defined bounds, then nav-agent-profile-defined bounds, and finally falling back to entity-defined bounds if necessary.
	**/
	void GetNavigationAgentBounds( Vector3 *out_mins, Vector3 *out_maxs );

	/**
	*   @brief	Clear stale async nav request state when no navmesh is loaded.
	*   @param	self	Sound-follow testdummy owning the async path process.
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
	*	@param	force	When true, bypass throttles/heuristics and rebuild immediately.
	*	@return	True if the queue accepted the request or already had one pending.
	*	@note	When this returns true the path process relies on the queued rebuild instead
	*			of immediate synchronous execution so we do not spam blocking calls.
	**/
	const bool TryRebuildNavigationInQueue( const Vector3 &start_origin,
		const Vector3 &goal_origin, const svg_nav_path_policy_t &policy, const Vector3 &agent_mins,
		const Vector3 &agent_maxs, const bool force = false );
	/**
	*    @brief	Reset cached navigation path state for the test dummy.
	*    @param	self	NPC whose path state should be cleared.
	*    @note	Cancels any queued async request and clears cached path buffers.
	**/
	void ResetNavigationPath();

	/**
	*	@brief	Member wrapper that forwards to the TU-local AdjustGoalZBlendPolicy helper.
	*	@param	goalOrigin	World-space feet-origin goal position used to bias layer selection.
	*	@note	Called each think after `GenericThinkBegin()` to keep `pathNavigationState.policy`
	*			tuned to current pursuit conditions (distance, vertical delta, failures, visibility).
	**/
	void AdjustGoalZBlendPolicy( const Vector3 &goalOrigin );


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
	struct PathNavigationState_t {
		//! Current state of the async nav path process, which is used by the A* pursuit thinkers and the generic think finish routine.
		svg_nav_path_process_t process = {};
		//! Current policy settings for the async nav path process, which is used by the A* pursuit thinkers and the generic think finish routine.
		svg_nav_path_policy_t policy = {};
		//! Rate limit for queue-status logging so we do not spam the console repeatedly.
		QMTime nextQueueStatusLogTime = 0_ms;

		/**
		*	@brief	Last known valid navigation fallback point (entity feet-origin space).
		*	@details	Used as a conservative fallback when async A* hasn't produced a path yet,
		*				so the monster can at least attempt to move toward the player instead of just standing still.
		*/
		struct goalNavigationState_t {
			//! Tracks the last goal position used to validate cached navigation paths.
			Vector3 origin = { 0.f, 0.f, 0.f };
			//! Tracks whether last_nav_goal_origin holds valid data.
			bool isValid = false;
			//! Tracks whether the goal was visible when the last nav goal was recorded.
			bool isVisible = false;
		} lastGoal = {};
	} pathNavigationState = {};



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
	*			Used for the `onThink_IdleLookout` state while the NPC is actively listening for new sounds.
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
		double targetYaw = 0.0;
	} stateIdleScan = {};

	/**
	*	@brief	Information about the most recently heard sound event that we are investigating.
    *			Used for the `onThink_PursueSoundInvestigation` state.
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
	static constexpr const Vector3 DUMMY_BBOX_STANDUP_MINS = PHYS_DEFAULT_BBOX_STANDUP_MINS;
	static constexpr const Vector3 DUMMY_BBOX_STANDUP_MAXS = PHYS_DEFAULT_BBOX_STANDUP_MAXS;
	static constexpr const double DUMMY_VIEWHEIGHT_STANDUP = PHYS_DEFAULT_VIEWHEIGHT_STANDUP;
	//! For when dummy is crouching.
	static constexpr const Vector3 DUMMY_BBOX_DUCKED_MINS = PHYS_DEFAULT_BBOX_DUCKED_MINS;
	static constexpr const Vector3 DUMMY_BBOX_DUCKED_MAXS = PHYS_DEFAULT_BBOX_DUCKED_MAXS;
	static constexpr const double DUMMY_VIEWHEIGHT_DUCKED = PHYS_DEFAULT_VIEWHEIGHT_DUCKED;
	//! For when dummy is dead.
	static constexpr const Vector3 DUMMY_BBOX_DEAD_MINS = { -16., -16., -36. };
	static constexpr const Vector3 DUMMY_BBOX_DEAD_MAXS = { 16., 16., 8. };
	static constexpr double	DUMMY_VIEWHEIGHT_DEAD = 8.;
};
