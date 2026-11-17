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

#include "svgame/nav/nav_query.h"
#include "svgame/nav/nav_actor_query.h"
#include "svgame/nav/nav_identity.h"

// Entity includes
#include "svgame/entities/svg_base_edict.h"

// Monster Move
#include "svgame/monsters/svg_mmove.h"
#include "svgame/monsters/svg_mmove_slidemove.h"

// Navigation
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_steering.h"
#include "svgame/nav/nav_cover_query.h"

// Forward declarations.
struct nav_sector_info_t;

//! Arrival radius for standard intermediate path waypoints.
static constexpr double MONSTER_NAV_WAYPOINT_REACH_RADIUS = 16.0;
//! Arrival radius squared for standard intermediate path waypoints.
static constexpr double MONSTER_NAV_WAYPOINT_REACH_RADIUS_SQR = MONSTER_NAV_WAYPOINT_REACH_RADIUS * MONSTER_NAV_WAYPOINT_REACH_RADIUS;

//! Tighter arrival radius for forced corner standoff waypoints to ensure rounding the apex.
static constexpr double MONSTER_NAV_SHARP_CORNER_REACH_RADIUS = 12.0;
//! Tighter arrival radius squared for forced corner standoff waypoints to ensure rounding the apex.
static constexpr double MONSTER_NAV_SHARP_CORNER_REACH_RADIUS_SQR = MONSTER_NAV_SHARP_CORNER_REACH_RADIUS * MONSTER_NAV_SHARP_CORNER_REACH_RADIUS;

//! Pure-pursuit lookahead distance along subsequent path segments for gentle turns.
static constexpr double MONSTER_NAV_LOOKAHEAD_DISTANCE = 24.0;

//! Outward normal bias factor added to wall-parallel sliding direction to steer around corner apexes.
static constexpr double MONSTER_NAV_CORNER_DEFLECTION_BIAS = 0.35;

//! Lateral corridor width tolerance buffer added to entity radius (corridorDist = R_agent * 2.0 + MARGIN).
static constexpr double MONSTER_NAV_CORRIDOR_MARGIN = 8.0;

//! Maximum vertical step-up tolerance allowed above NAV_MAX_STEP_HEIGHT for path continuity.
static constexpr double MONSTER_NAV_VERTICAL_STEP_TOLERANCE = 6.0;

//! Minimum vertical delta for an agent to treat a path waypoint as a vertical step-up or step-down.
static constexpr double MONSTER_NAV_STEP_MIN_DELTA = 0.5;

//! Vertical tolerance around target elevation to consider an agent having physically landed on a step surface.
static constexpr double MONSTER_NAV_STEP_ARRIVAL_TOLERANCE = 2.0;

//! Proximity deadband distance to active waypoint within which steering looks forward to the subsequent waypoint to prevent yaw jitter.
static constexpr double MONSTER_NAV_WAYPOINT_DEADBAND = 4.0;

//! Minimum squared length of a 2D direction vector to be considered non-degenerate.
static constexpr double MONSTER_NAV_DIR_EPS_SQR = 0.0001;

//! Minimum corridor width required for two agents to walk abreast without collision (3 * agent diameter = 96.0 units).
static constexpr double MONSTER_NAV_MIN_TWO_AGENT_ABREAST_WIDTH = CROWD_DEFAULT_AGENT_RADIUS * 2.0 * 3.0;
//! Minimum radial clearance from obstacle boundary required for two agents to walk abreast (48.0 units).
static constexpr double MONSTER_NAV_MIN_TWO_AGENT_ABREAST_CLEARANCE = MONSTER_NAV_MIN_TWO_AGENT_ABREAST_WIDTH * 0.5;

//! Dot product threshold between incoming and outgoing segment directions to qualify as a gentle turn (<= 30 degrees).
static constexpr double MONSTER_NAV_GENTLE_TURN_MIN_DOT = 0.866;

//! Maximum allowed vertical elevation difference to final goal position to consider arrived.
//! A nearby XY goal on another tread is not arrival; allow only small native support/slope offsets.
static constexpr double MONSTER_NAV_FINAL_GOAL_MAX_Z_DELTA = 6.0;

//! Angular deviation threshold in degrees beyond which speed scaling begins decelerating into turns.
static constexpr double MONSTER_NAV_CORNER_DECEL_THRESHOLD_DEG = 35.0;

//! Minimum speed scale floor during sharp corner turns.
static constexpr double MONSTER_NAV_CORNER_DECEL_MIN_SPEED_SCALE = 0.40;

//! Minimum time interval between full path recalculations when the goal is stationary.
static constexpr QMTime MONSTER_NAV_PATH_RECALC_MIN_INTERVAL = 400_ms;

//! Minimum time interval before retrying a failed path search to prevent exhaustive search thrashing.
static constexpr QMTime MONSTER_NAV_PATH_FAIL_RECALC_INTERVAL = 600_ms;

//! Clearance margin added to entity radius for sharp corner waypoint arrival checks.
static constexpr double MONSTER_NAV_CORNER_HULL_CLEARANCE_MARGIN = 4.0;

//! Maximum lateral displacement allowed when advancing past a forced doorway plane.
static constexpr double MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_LATERAL = 24.0;
//! Squared maximum lateral displacement allowed when advancing past a forced doorway plane.
static constexpr double MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_LATERAL_SQR = MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_LATERAL * MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_LATERAL;

//! Maximum distance to waypoint allowed when advancing past a forced doorway plane.
static constexpr double MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_DIST = 36.0;
//! Squared maximum distance to waypoint allowed when advancing past a forced doorway plane.
static constexpr double MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_DIST_SQR = MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_DIST * MONSTER_NAV_DOORWAY_PLANE_ADVANCE_MAX_DIST;

//! Maximum distance to a corner waypoint to allow advancing past when in contact with world geometry.
static constexpr double MONSTER_NAV_CORNER_BRUSH_CLEARANCE_DIST = 22.0;
//! Squared maximum distance to a corner waypoint to allow advancing past when in contact with world geometry.
static constexpr double MONSTER_NAV_CORNER_BRUSH_CLEARANCE_DIST_SQR = MONSTER_NAV_CORNER_BRUSH_CLEARANCE_DIST * MONSTER_NAV_CORNER_BRUSH_CLEARANCE_DIST;

//! Consecutive blocked frames required before triggering navigation recovery.
static constexpr int32_t MONSTER_NAV_STUCK_RECOVER_BLOCKED_FRAMES = 32;

//! Physical nudge distance outward along blocking wall normal to dislodge from corner creases.
static constexpr float MONSTER_NAV_STUCK_WALL_NUDGE_DIST = 2.0f;

//! Multiplier applied to agent radius to determine the distance threshold for proportional arc clearance.
static constexpr double MONSTER_NAV_ARC_CLEARANCE_DIST_MULT = 2.0;

//! Proportional clearance fraction tested for closely-spaced intermediate arc waypoints.
static constexpr double MONSTER_NAV_ARC_CLEARANCE_RATIO = 0.4;

//! Multiplier applied to agent hull radius when validating 2D line-of-sight clearance into the corner departure zone.
//! Guarantees that steering direction never switches to the next waypoint until the physical capsule has cleared the corner brush.
static constexpr double MONSTER_NAV_DEPARTURE_CLEARANCE_SCALE = 1.0;


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
	//! Authoritative owned semantic obligations, supporting faces and callback state per route point.
	svg_vector_t<svg_nav_waypoint_t, TAG_SVGAME_NAVMESH> navigationWaypoints;
	//! Transient slot incarnation changes on construction/reset independently of saved spawn_count.
	uint64_t navigationIncarnation = Nav_IssueIdentityRevisionMainThread();
	//! Behavioral revision invalidated before explicit state changes or route cancellation.
	uint64_t navigationStateRevision = 0;
	//! Actor and publication identity accepted with the current native route.
	nav_query_identity_t navigationAcceptedIdentity;
	//! Request-owned diagnostics copied on route completion for actor-local inspection.
	nav_path_diagnostics_t navigationDiagnostics;
	//! Exclusive owner-side workspaces retain warm capacities without sharing mutable query scratch.
	svg_nav_graph_workspace_t navigationGraphWorkspace;
	//! Native route scratch is transient, omitted from save descriptors and never used off the owner.
	svg_nav_route_context_t navigationRouteContext;
	/**
	*	@brief	Validate every actor/publication revision before installing a copied reply.
	**/
	bool SVG_Nav_IsIdentityCurrent( const nav_query_identity_t &identity ) const;
	/**
	*	@brief	Capture a pointer-free actor/order/publication identity on the owner thread.
	**/
	nav_query_identity_t SVG_Nav_CaptureIdentity();
	/**
	*	@brief	Owner-visible request lifecycle, distinct from physical route arrival.
	**/
	using NavigationQueryState = nav_actor_query_state_t;
	//! Pending/terminal status is transient and never restored from save data.
	NavigationQueryState navigationQueryState = NavigationQueryState::Idle;
	//! Monotonic timestamp immediately before the latest actual worker submission attempt.
	uint64_t navigationSubmittedNs = 0;
	//! Actual owner submission tick retained for independently asserted deterministic delivery gates.
	uint64_t navigationSubmittedTick = 0;
	//! Transport's earliest eligible owner tick, copied at submission rather than inferred from first capture.
	uint64_t navigationEarliestDeliveryTick = 0;
	//! Reader solve boundaries copied by the owner from a matched completion lease.
	uint64_t navigationGraphStartNs = 0, navigationGraphFinishNs = 0;
	//! Submission identity owning the timing fields; synchronous requests cannot reuse them.
	uint64_t navigationTimingSequence = 0;
	//! Timing namespace prevents a wrapped sequence from being mistaken for an earlier submission.
	uint64_t navigationTimingEpoch = 0;
	//! Last timed request whose commit or terminal disposition has already been counted.
	uint64_t navigationTerminalCountedSequence = 0;
	/**
	*	@brief	Set a terminal query state and count its latest worker disposition exactly once.
	*	@param	state	NoRoute, Stale, Cancelled or ResourceLimited terminal disposition.
	*	@param	cause	Explicit owner-side attachment cause; never changes state or acceptance.
	*	@note	Only the currently timed actual worker submission contributes to terminal metrics.
	**/
	void SVG_Nav_SetQueryTerminalState( const NavigationQueryState state,
		const nav_actor_terminal_cause_t cause = nav_actor_terminal_cause_t::Unspecified );
	//! Native policy and original target retained exclusively on the owner for the latest request.
	nav_path_policy_t navigationPendingPolicy;
	//! Physical limits accepted with the installed route, independent of a pending replacement.
	nav_path_policy_t navigationCommittedPolicy;
	Vector3 navigationPendingTarget = {};
	//! Submission reason retained for identical synchronous/worker performance accounting.
	int32_t navigationPendingReason = 0;
	/**
	*	@brief	Validate ownership of an installed route while a replacement goal is pending.
	**/
	bool SVG_Nav_IsCommittedRouteUsable() const;
	/**
	*	@brief	Consume copied terminal graph data after releasing the transport lease.
	**/
	void SVG_Nav_AcceptWorkerResult();
	//! Latest actual submission sequence; an older same-goal reply cannot replace a newer submission.
	uint64_t navigationRequestSequence = 0;
	//! Monotonic identity of accepted route installations and explicit route resets.
	uint64_t navigationRouteRevision = 0;
	//! Monotonic identity of requested target changes, separate from installed geometry.
	uint64_t navigationGoalRevision = 0;
	//! Goal identity accepted by the current route; obsolete targets cannot release waypoints.
	uint64_t navigationAcceptedGoalRevision = 0;
	//! Requested feet-space goal used to detect even small obsolete target changes.
	Vector3DP navigationRequestedGoal = {};
	//! Whether a requested target identity has been initialized.
	bool navigationRequestedGoalValid = false;
	//! Mesh generation of the accepted semantic route and supporting faces.
	uint64_t waypointFacesMeshGeneration = 0;
	//! Topology revision under which the currently committed route was accepted.
	uint64_t waypointTopologyRevision = 0;
	//! Prevent repeated final-goal callbacks while the accepted route remains installed.
	bool finalWaypointEventFired = false;
	//! Last physical command validation for diagnosis; contains no borrowed scratch pointers.
	svg_nav_movement_result_t lastMovementValidation;
	//! Complete static route installation result, including a rejected retained link.
	nav_route_validation_t lastRouteValidation;
	//! Last strict boundary result, separate from native collision/support validation.
	nav_segment_clearance_t lastGeometricValidation;
	//! Total candidate-native sweeps during this grounded navigation update.
	uint32_t navigationValidationTraceCount = 0;
	//! Hold an unsafe command until a route replan succeeds rather than repeatedly inching forward.
	bool navigationCommandRejected = false;
	//! Structured shared release reason and per-frame advancement budget.
	svg_nav_release_t lastWaypointRelease;
	uint64_t navigationReleaseFrame = UINT64_MAX;
	uint32_t navigationReleasesThisFrame = 0;
	//! Callback audit is scoped to one tick and stores at most two releases plus one final event.
	uint64_t navigationEventFrame = UINT64_MAX;
	//! Fired route/index pairs are copied before invoking gameplay code, which may replace navigation.
	std::array<uint64_t, 3> navigationEventRoutes = {};
	std::array<uint32_t, 3> navigationEventIndices = {};
	std::array<bool, 3> navigationEventFinal = {};
	//! Number of valid callback audit entries for navigationEventFrame.
	uint32_t navigationEventCount = 0;
	//! Bounded named steering stages and final candidate decisions.
	svg_nav_steering_t navigationSteering;
	//! Explicit fixture speed multiplier; ordinary monsters retain 1.0.
	double navigationFixtureSpeedScale = 1.0;
	//! Explicit console fixture bounds survive standing-animation selection; ordinary actors keep their authored hull.
	bool navigationFixtureProfile = false;
	//! Validated native fixture minima and maxima in bbox-origin coordinates.
	Vector3 navigationFixtureMins, navigationFixtureMaxs;
	//! Opt-in causal mode: 0 normal, 1 geometry hold, 2 direct frozen, 3 blend frozen, 4 separation frozen, 5 normal replan.
	uint32_t navigationFixtureMode = 0;
	//! Currently resolved standing polygon face.
	int32_t currentStandingFace = -1;
	//! Server time when currentStandingFace was last resolved.
	QMTime lastStandingFaceResolveTime = -1_ms;
	//! Feet origin in double precision when currentStandingFace was last resolved.
	Vector3DP lastStandingFaceFeetOrigin = { 0.0, 0.0, 0.0 };
	//! Snapshot marks of corridor faces for O(1) hysteresis lookup in OnNavEvaluateEdgeCost.
	std::vector<uint32_t> corridorMembershipMarks;
	//! Stamp identifying active corridor membership snapshot.
	uint32_t corridorMembershipStamp = 0;
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
	//! Whether the next slide move should suppress entity deflection while queueing behind a teammate.
	bool suppressEntityDeflectionThisFrame = false;
	//! Configurable tactical behavior and adaptive fallback bitflags (see crowd_tactical_flags_t).
	uint32_t crowdTacticalFlags = CROWD_TACTICAL_FLAG_DEFAULT;

	/**
	*	@brief	Check whether a specific tactical behavior bitflag is active for this monster.
	*	@param	flag	Flag bitmask to check (see crowd_tactical_flags_t).
	*	@return	True if the flag bit is set.
	**/
	inline const bool HasTacticalBehaviorFlag( const uint32_t flag ) const {
		return ( crowdTacticalFlags & flag ) != 0;
	}
	/**
	*	@brief	Enable or disable a specific tactical behavior bitflag for this monster.
	*	@param	flag	Flag bitmask to modify (see crowd_tactical_flags_t).
	*	@param	enabled	True to enable the flag, false to clear.
	**/
	inline void SetTacticalBehaviorFlag( const uint32_t flag, const bool enabled = true ) {
		if ( enabled ) {
			crowdTacticalFlags |= flag;
		} else {
			crowdTacticalFlags &= ~flag;
		}
	}

	//! KD-Tree caching
	int32_t cachedLeaf = -1;
	int32_t cachedPoly = -1;

	/**
	*	@brief	Path evaluation result states.
	**/
	enum class PathComputeResult : uint8_t {
		Failed,                 // Path generation failed completely
		ReusedCached,           // Reused the existing valid path (debounced)
		NewPathGenerated,       // Generated a brand new path successfully
		Pending                 // Request is outstanding; this is neither failure nor arrival
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
	*	@brief	Resolve current standing polygon face with local polygon and topological cache before KD-leaf lookup.
	*	@param	myFeetDP	Agent feet-origin position in Vector3DP.
	*	@return	Resolved face index or -1 if no face found.
	**/
	int32_t ResolveCurrentFace( const Vector3DP &myFeetDP );
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
	*	@brief	Shared owner-only funnel, native proof and revision-checked route installation.
	**/
	PathComputeResult SVG_Nav_InstallCorridor( svg_nav_result_t &candidate, const Vector3DP &feet,
		const Vector3DP &goal, const Vector3 &target, const nav_path_policy_t &policy, const int32_t reason );

	/**
	*	@brief	Read mandatory status only from the authoritative semantic route contract.
	*	@param	index	Waypoint index in the accepted route revision.
	*	@return	True for a mandatory intermediate obligation.
	**/
	bool SVG_Nav_IsWaypointMandatory( const size_t index ) const;
	/**
	*	@brief	Dispatch a waypoint callback once within its accepted route revision.
	*	@param	index	Reached point index; rejected proposals never call this helper.
	**/
	void SVG_Nav_FireWaypoint( const size_t index );
	/**
	*	@brief	Evaluate one shared release contract with structured reason and owned witnesses.
	*	@param	index	Proposed intermediate waypoint.
	*	@param	progressEligible	Radius/approach-plane proposal.
	*	@return	Owned decision; clear permits one commit within the two-per-frame budget.
	**/
	svg_nav_release_t SVG_Nav_QueryWaypointRelease( const size_t index, const bool progressEligible );
	/**
	*	@brief	Store the shared release decision and consume its budget only on success.
	*	@param	index	Proposed retained constraint.
	*	@param	progressEligible	Progress proposal, never itself authorization.
	*	@return	True when geometry, native support, revisions and budget authorize release.
	**/
	bool SVG_Nav_CanAdvanceWaypoint( const size_t index, const bool progressEligible );
	/**
	*	@brief	Return the shared physical arrival radius for this actor's current crowd or solo order.
	*	@return	Radius used by both path-following and crowd station keeping.
	*	@note	Serialized staging retains its tighter queue-cell radius; native radius affects small actors.
	**/
	double GetNavigationArrivalRadius() const;
	/**
	*	@brief	Commit the final event from realized supported arrival before a crowd can end its order.
	*	@note	Requires current accepted geometry and all intermediate obligations already released.
	**/
	void SVG_Nav_CheckFinalArrival();
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
	*	@brief	Invoked when an intermediate path waypoint or final destination is reached and advanced during navigation.
	*	@param	waypointIndex	Index of the reached waypoint in stringPulledPath.
	*	@param	waypointPos		World-space coordinates of the reached waypoint in Vector3DP.
	*	@param	isFinalGoal		True if the reached waypoint represents the final path destination.
	*	@note	Derived monster classes override this callback to trigger tactical state shifts,
	*			stare-halt mechanics, ambushes, cover queries, animation transitions, or custom environmental reactions.
	**/
	virtual void OnWaypointReached( const size_t waypointIndex, const Vector3DP &waypointPos, const bool isFinalGoal );
	/**
	*	@brief	Query assigned squad formation target origin and recommended pacing speed scale.
	*	@param	outTargetOrigin	[out] World-space formation slot origin in Vector3DP.
	*	@param	outSpeedScale	[out] Recommended proportional speed multiplier [0.0..1.5].
	*	@return	True if enrolled in an active squad, false otherwise.
	**/
	const bool QuerySquadFormationTarget( Vector3DP *outTargetOrigin, float *outSpeedScale );
	/**
	*	@brief	Query instantaneous topological sector awareness and upcoming portal geometry.
	*	@param	goalOrigin		Destination world position.
	*	@param	outSectorInfo	[out] Populated sector awareness snapshot.
	*	@return	True if current sector was resolved, false otherwise.
	**/
	const bool QuerySectorAwareness( const Vector3DP &goalOrigin, nav_sector_info_t *outSectorInfo );
	/**
	*	@brief	Query right-of-way precedence when traversing a bottleneck aperture or portal.
	*	@param	portalId			Portal index to evaluate.
	*	@param	outHasRightOfWay	[out] True if entity is clear to traverse, false if yielding to preceding rank.
	*	@return	True if query succeeded.
	**/
	const bool QueryBottleneckRightOfWay( const int32_t portalId, bool *outHasRightOfWay );
	/**
	*	@brief	Evaluate tactical actions and custom pursuit weighting upon reaching a waypoint.
	*	@param	waypointPos		Position of reached waypoint in Vector3DP.
	*	@param	waypointRadius	Acceptance radius.
	*	@param	isFinalGoal		True if final destination slot reached.
	**/
	virtual void EvaluateWaypointArrival( const Vector3DP &waypointPos, const float waypointRadius, const bool isFinalGoal );
	/**
	*	@brief	Invoked when a designated tactical cover point is successfully reached.
	*	@param	coverIndex	Index of the reached cover point in g_nav_cover_points.
	*	@param	coverPos	World-space coordinates of the reached cover point in Vector3.
	*	@note	Derived monster classes override this method to transition into crouched hiding,
	*			peek-shooting, or ambush states.
	**/
	virtual void OnCoverPointReached( const int32_t coverIndex, const Vector3 &coverPos );
	/**
	*	@brief	Custom edge cost evaluator for A* navigation pathfinding.
	*	@details Allows individual monster classes or states to bias path choices (e.g. preferring stairs/ramps,
	*			applying path commitment hysteresis to prevent bifurcation jitter, avoiding hazards).
	*	@param	fromFaceIdx	Source polygon index.
	*	@param	toFaceIdx	Target polygon index.
	*	@param	he			Half-edge connecting fromFace to toFace.
	*	@param	baseCost	Standard geometric cost (distance * slope * clearance).
	*	@return	Adjusted edge traversal cost.
	**/
	virtual double OnNavEvaluateEdgeCost( const int32_t fromFaceIdx, const int32_t toFaceIdx, const nav_halfedge_t &he, const double baseCost );
	/**
	*	@brief	Declare a pure copied representation of this dynamic class's edge-cost policy.
	*	@param	cost	Scalar preferences copied into an owned request before solving.
	*	@return	False keeps an unknown override explicitly synchronous; never infer purity from its base class.
	*	@note	Subclasses must opt in and represent all virtual edge-cost behavior, including current state.
	**/
	virtual bool BuildNavigationCostSnapshot( nav_query_cost_policy_t &cost ) const;
	/**
	*	@brief	Static bridge dispatching nav_path_policy_t edge cost callbacks to monster instances.
	**/
	static double NavEdgeCostCallbackBridge( int32_t fromFaceIdx, int32_t toFaceIdx, const nav_halfedge_t &he, double baseCost, svg_monster_base_t *monster );

	/**
	*	@brief	Safely cast an edict pointer to svg_monster_base_t using the TypeInfo system.
	*	@param	ent	Entity pointer to cast.
	*	@return	Pointer to svg_monster_base_t if ent is valid and derives from svg_monster_base_t, else nullptr.
	**/
	static inline svg_monster_base_t *FromEdict( svg_base_edict_t *ent ) {
		if ( ent != nullptr && ent->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
			return static_cast<svg_monster_base_t*>( ent );
		}
		return nullptr;
	}

	/**
	*	@brief	Safely cast a const edict pointer to const svg_monster_base_t using the TypeInfo system.
	*	@param	ent	Entity pointer to cast.
	*	@return	Const pointer to svg_monster_base_t if ent is valid and derives from svg_monster_base_t, else nullptr.
	**/
	static inline const svg_monster_base_t *FromEdict( const svg_base_edict_t *ent ) {
		if ( ent != nullptr && ent->GetTypeInfo()->IsSubClassType<svg_monster_base_t>() ) {
			return static_cast<const svg_monster_base_t*>( ent );
		}
		return nullptr;
	}

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
