/********************************************************************
*
*
*	ServerGame: Crowd & Crew Navigation Manager
*	File: svg_crowd_manager.h
*	Description:
*		Central manager for tracking crowd groups, allocating formation
*		slots, assigning tactical cover, dispatching A* navigation routes,
*		and managing crowd lifecycle using Vector3DP.
*
*
********************************************************************/
#pragma once

#include "svgame/crowd/svg_crowd_types.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_cover_query.h"
#include "svgame/memory/svg_zone_allocator.h"
#include "shared/math/qm_vector3_dp.h"

#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations.
struct svg_base_edict_t;

//! Common staging/entry tolerance shared by crowd arrival and monster station keeping.
static constexpr double CROWD_INGRESS_ARRIVAL_RADIUS = 8.0;

/**
*	@brief	Lifecycle phases for deterministic single-aperture crowd ingress.
**/
enum class crowd_ingress_phase_t : uint8_t {
	//! No serialized doorway operation is active.
	INACTIVE = 0,
	//! Every queued member is moving to a distinct exterior wall-adjacent staging slot.
	MARSHALLING,
	//! One queue member at a time owns the doorway and moves to its immutable final slot.
	ADMISSION
};

/**
*	@brief	Active crowd/crew group coordination record.
**/
/**
*	@brief	Active crowd/crew group coordination record.
**/
struct svg_crowd_group_t {
	//! Crowd identifier (0 = neutral NPC crowd, > 0 = tactical squad).
	int32_t crowdID = -1;
	//! Monotonic identity of issued crowd orders, including synchronous retargeting.
	uint64_t orderRevision = 0;
	//! Active chase/formation style.
	crowd_chase_target_type_t style = crowd_chase_target_type_t::CROWD_STYLE_ARROW;
	//! Configuration parameters for spacing, tactical cover, etc.
	svg_crowd_params_t params = {};
	//! Static world-space destination origin in double precision (valid when targetEntityNumber == ENTITYNUM_NONE).
	Vector3DP destinationOrigin = { 0.0, 0.0, 0.0 };
	//! Entity number of the target being followed (ENTITYNUM_NONE if navigating to static destinationOrigin).
	int32_t targetEntityNumber = ENTITYNUM_NONE;
	//! Pool lifetime captured with the followed target number; a reused slot cannot inherit the order.
	int32_t targetSpawnCount = -1;
	//! Entity number of the designated squad leader establishing the formation path (ENTITYNUM_NONE if unassigned).
	int32_t leaderEntityNumber = ENTITYNUM_NONE;
	//! Pool lifetime captured with the designated leader number.
	int32_t leaderSpawnCount = -1;
	//! Last recorded origin of targetEntity (used to detect when to rebuild formation).
	Vector3DP lastTargetEntityOrigin = { 0.0, 0.0, 0.0 };
	//! Forward heading in degrees used for the active formation orientation.
	double currentHeadingYaw = 0.0;
	//! Dynamic corridor squeeze scale factor [0.2..1.0] applied to lateral spacing.
	double dynamicSqueezeFactor = 1.0;
	//! Cached polyline waypoints from the leader's primary path for followers to share.
	std::vector<Vector3DP> sharedLeaderPath = {};
	//! Calculated formation slots (in world space).
	std::vector<svg_crowd_slot_t> slots = {};
	//! Normalized horizontal approach vector towards destinationOrigin.
	Vector3DP ingressDirection = { 0.0, 0.0, 0.0 };
	//! Center of the constrained doorway currently controlled by serialized ingress.
	Vector3DP ingressPortalOrigin = { 0.0, 0.0, 0.0 };
	//! Unit normal pointing from the doorway into the destination room.
	Vector3DP ingressPortalInward = { 0.0, 0.0, 0.0 };
	//! Half-width of the finite doorway aperture used for exact lateral hull-clearance tests.
	double ingressPortalHalfWidth = 0.0;
	//! Frozen front-to-back entity order for the current doorway reservation queue.
	std::vector<int32_t> ingressQueueEntityNumbers = {};
	//! Incarnation of each immutable ingress owner; swaps carry this key with the entity number.
	svg_vector_t<uint64_t, TAG_SVGAME_NAVMESH> ingressQueueIncarnations = {};
	//! Pool spawn counts paired with queued entity numbers; a reused slot cannot inherit ownership.
	svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> ingressQueueSpawnCounts = {};
	//! Exterior staging position indexed directly by ingress queue rank.
	std::vector<Vector3DP> ingressStagingPositions = {};
	//! Owned static native routes from each exterior cell to its immediately preceding cell.
	std::vector<std::vector<Vector3DP>> ingressStagingTransitRoutes = {};
	//! Immutable geometric row for plotting; ownership recovery may reorder the reservation records.
	std::vector<Vector3DP> ingressStagingLine = {};
	//! O(1) index of the member currently owning the doorway reservation.
	int32_t ingressQueueHead = 0;
	//! Adjacent exterior cell currently approached by the active head; admission requires cell zero.
	int32_t ingressHeadStagingCell = 0;
	/**
	*	Reversible approach reservations; final seats and queue ranks are independent.
	**/
	struct ingress_yield_t {
		//! Follower holding outside the current transit.
		int32_t entityNumber = ENTITYNUM_NONE;
		//! Incarnation preventing entity-slot reuse from inheriting this claim.
		uint64_t incarnation = 0;
		//! Pool lifetime of this follower, checked before restoring any saved goal.
		int32_t spawnCount = 0;
		//! Owning head rank; completion releases this holding obligation.
		int32_t head = -1;
		//! Original staging destination restored after that head completes admission.
		Vector3 resumeGoal = {};
		//! Supported holding destination reserved against other temporary claims.
		Vector3 holdingGoal = {};
		//! Full native hull radius used for holding-point separation.
		double radius = 0.0;
	};
	//! At most one claim per queued follower; storage is reserved during order construction.
	svg_vector_t<ingress_yield_t, TAG_SVGAME_NAVMESH> ingressYieldClaims = {};
	//! Earliest retry for the bounded native holding-point probes.
	QMTime ingressYieldNextProbeTime = 0_ms;
	//! O(1) cursor retained for ingress phase bookkeeping while admission is queue-head gated.
	int32_t ingressMarshalCursor = 0;
	//! Server timestamp when the current queue head began waiting for its staging cell or final room slot.
	QMTime ingressQueueHeadStartTime = 0_ms;
	//! Signed interior depth that completely clears an agent hull beyond the portal plane.
	double ingressReleaseDepth = 0.0;
	//! Current two-phase doorway protocol state.
	crowd_ingress_phase_t ingressPhase = crowd_ingress_phase_t::INACTIVE;
	//! Entity numbers of members actively registered to this group.
	std::vector<int32_t> memberEntityNumbers = {};
	//! Server timestamp when current order began.
	QMTime orderStartTime = 0_ms;
	//! Server timestamp of next scheduled formation/path update tick.
	QMTime nextUpdateTick = 0_ms;
	//! Server timestamp of last target entity follow update.
	QMTime lastTargetEntityUpdateTime = 0_ms;
	//! Whether the crowd is actively moving towards orders.
	bool isMoving = false;
	//! True while the destination doorway is being consumed as a single-capacity resource.
	bool hasSerializedIngress = false;
	//! Center of the constrained doorway currently controlled by serialized room egress.
	Vector3DP egressPortalOrigin = { 0.0, 0.0, 0.0 };
	//! Unit normal pointing from inside the origin room outward through the exit doorway.
	Vector3DP egressPortalOutward = { 0.0, 0.0, 0.0 };
	//! Half-width of the exit doorway aperture.
	double egressPortalHalfWidth = 0.0;
	//! Ordered entity numbers of crowd members queued for egress (closest to exit door first).
	std::vector<int32_t> egressQueueEntityNumbers = {};
	//! Identity namespaces owning the physical queues; revision rollover cancels these reservations.
	uint64_t ingressQueueIdentityEpoch = 0;
	//! Identity namespace owning origin aperture clearance and its incarnation records.
	uint64_t egressQueueIdentityEpoch = 0;
	//! Incarnation of each frozen egress owner; recycled entity slots cannot inherit clearance.
	svg_vector_t<uint64_t, TAG_SVGAME_NAVMESH> egressQueueIncarnations = {};
	//! Pool spawn counts paired with queued entity numbers; a reused slot cannot inherit ownership.
	svg_vector_t<int32_t, TAG_SVGAME_NAVMESH> egressQueueSpawnCounts = {};
	//! Mesh publication and topology that own this physical aperture reservation.
	uint64_t egressMeshGeneration = 0;
	//! Dynamic topology revision that owns the retained aperture geometry.
	uint64_t egressTopologyRevision = 0;
	//! O(1) index of the member currently at the head of the egress release queue.
	int32_t egressQueueHead = 0;
	//! Server timestamp when the current queue head began waiting to clear the exit doorway.
	QMTime egressQueueHeadStartTime = 0_ms;
	//! Server timestamp when the next egress queue member is eligible for rapid release.
	QMTime egressNextReleaseTime = 0_ms;
	//! True while the origin room doorway is being exited via serialized in-order egress.
	bool hasSerializedEgress = false;

	/**
	*	@brief	Safely resolve the target entity being followed.
	*	@return	Pointer to target edict if active and alive, nullptr otherwise.
	**/
	svg_base_edict_t *GetTargetEntity( void ) const;

	/**
	*	@brief	Safely resolve the designated squad leader entity.
	*	@return	Pointer to leader edict if active and alive, nullptr otherwise.
	**/
	svg_base_edict_t *GetLeaderEntity( void ) const;
};

/**
*	@brief	Initialize crowd management subsystems and cvars.
**/
void SVG_Crowd_Init( void );

/**
*	@brief	Shutdown crowd management subsystems and release all reservations.
**/
void SVG_Crowd_Shutdown( void );

/**
*	@brief	Synchronize crowd group registries with active entities in the edict pool.
*	@note	Invoked after map spawn and savegame load to rebuild crowd group records
*			from entity states without wiping individual member assignments.
**/
void SVG_Crowd_SyncFromEntities( void );

/**
*	@brief	Execute per-frame crowd coordination, slot updates, and staggered pathing.
**/
void SVG_Crowd_Frame( void );

/**
*	@brief	Command a crowd to navigate to a world origin in Vector3DP with the specified formation style.
*	@param	crowdID	Crowd group identifier.
*	@param	origin	World-space destination origin in Vector3DP.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarCrowdOrigin( const int32_t crowdID, const Vector3DP &origin, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Command a crowd to navigate to a world origin in Vector3 with the specified formation style.
*	@param	crowdID	Crowd group identifier.
*	@param	origin	World-space destination origin in Vector3.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarCrowdOrigin( const int32_t crowdID, const Vector3 &origin, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Command a crowd to follow an entity by entity number in the specified formation style.
*	@param	crowdID				Crowd group identifier.
*	@param	targetEntityNumber	Target entity number to follow.
*	@param	style				Formation / chase style.
*	@param	params				Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarFollowEntity( const int32_t crowdID, const int32_t targetEntityNumber, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Command a crowd to follow an entity pointer in the specified formation style (convenience overload).
*	@param	crowdID	Crowd group identifier.
*	@param	entity	Target entity to follow.
*	@param	style	Formation / chase style.
*	@param	params	Formation parameters (spacing, cover distance, etc.).
*	@return	True when orders were successfully dispatched to one or more crowd members.
**/
bool MoveAStarFollowEntity( const int32_t crowdID, svg_base_edict_t *entity, const crowd_chase_target_type_t style, const svg_crowd_params_t &params = {} );

/**
*	@brief	Designate a squad member as the squad leader by entity number.
*	@param	crowdID				Crowd identifier.
*	@param	leaderEntityNumber	Entity number to set as leader (or ENTITYNUM_NONE to clear).
**/
void SVG_Crowd_SetLeader( const int32_t crowdID, const int32_t leaderEntityNumber );

/**
*	@brief	Designate a squad member as the squad leader by edict pointer.
*	@param	crowdID		Crowd identifier.
*	@param	leaderEnt	Entity to set as leader.
**/
void SVG_Crowd_SetLeader( const int32_t crowdID, svg_base_edict_t *leaderEnt );

/**
*	@brief	Register an entity as a member of a specific crowd group by entity number.
*	@param	entityNumber	Entity number to register.
*	@param	crowdID			Crowd identifier.
**/
void SVG_Crowd_RegisterMember( const int32_t entityNumber, const int32_t crowdID );

/**
*	@brief	Register an entity as a member of a specific crowd group by edict pointer.
*	@param	ent		Entity to register.
*	@param	crowdID	Crowd identifier.
**/
void SVG_Crowd_RegisterMember( svg_base_edict_t *ent, const int32_t crowdID );

/**
*	@brief	Unregister an entity from its active crowd group by entity number.
*	@param	entityNumber	Entity number to unregister.
**/
void SVG_Crowd_UnregisterMember( const int32_t entityNumber );

/**
*	@brief	Unregister an entity from its active crowd group by edict pointer.
*	@param	ent	Entity to unregister.
**/
void SVG_Crowd_UnregisterMember( svg_base_edict_t *ent );

/**
*	@brief	Retrieve entity numbers of all living members in a specific crowd.
*	@param	crowdID			Crowd identifier.
*	@param	outEntityNums	[out] Vector to receive member entity numbers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, std::vector<int32_t> &outEntityNums );

/**
*	@brief	Retrieve living member edicts of a specific crowd.
*	@param	crowdID		Crowd identifier.
*	@param	outMembers	[out] Vector to receive member entity pointers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, std::vector<svg_base_edict_t*> &outMembers );

/**
*	@brief	Retrieve living member edicts of a specific crowd (zone-allocated vector overload).
*	@param	crowdID		Crowd identifier.
*	@param	outMembers	[out] Vector to receive member entity pointers.
**/
void SVG_Crowd_GetCrowdMembers( const int32_t crowdID, svg_vector_t<svg_base_edict_t*, TAG_SVGAME_NAVMESH> &outMembers );

/**
*	@brief	Update the formation style of an active crowd group.
*	@param	crowdID	Crowd identifier.
*	@param	style	New formation style.
**/
void SVG_Crowd_SetCrowdStyle( const int32_t crowdID, const crowd_chase_target_type_t style );

/**
*	@brief	Update the configuration parameters of an active crowd group.
*	@param	crowdID	Crowd identifier.
*	@param	params	New configuration parameters.
**/
void SVG_Crowd_SetCrowdParams( const int32_t crowdID, const svg_crowd_params_t &params );

/**
*	@brief	Halt and clear movement orders for an active crowd group.
*	@param	crowdID	Crowd identifier.
**/
void SVG_Crowd_StopCrowd( const int32_t crowdID );

/**
*	@brief	Retrieve a pointer to an active crowd group record.
*	@param	crowdID	Crowd identifier.
*	@return	Pointer to crowd group record, or nullptr if none exists.
**/
svg_crowd_group_t *SVG_Crowd_GetGroup( const int32_t crowdID );

/**
*	@brief	Compute mutual separation steering push force from fellow crowd members by entity number.
*	@param	entityNumber		Query entity number.
*	@param	outSeparationForce	[out] Vector to receive 2D repulsive displacement vector in Vector3DP.
*	@return	True if a non-zero separation force was calculated.
**/
bool SVG_Crowd_ComputeMutualSeparation( const int32_t entityNumber, Vector3DP *outSeparationForce );

/**
*	@brief	Compute mutual separation steering push force from fellow crowd members by edict pointer.
*	@param	ent					Query entity.
*	@param	outSeparationForce	[out] Vector to receive 2D repulsive displacement vector in Vector3DP.
*	@return	True if a non-zero separation force was calculated.
**/
bool SVG_Crowd_ComputeMutualSeparation( const svg_base_edict_t *ent, Vector3DP *outSeparationForce );

/**
*	@brief		Compute speed throttling scale for trailing squad members to yield to leading teammates in narrow corridors.
*	@param	entityNumber			Query entity number.
*	@param	moveDir				Normalized 2D horizontal movement direction towards active waypoint.
*	@param	outSpeedScale			[out] Multiplier applied to frame velocity [0.0..1.0] to maintain following distance.
*	@param	outStrictQueueing		[out] Optional flag set when throttling was caused by strict bottleneck/doorway single-file queueing.
*	@return	True if a leading teammate was found directly ahead in the travel corridor.
**/
bool SVG_Crowd_ComputeTeammateFollowSpeedScale( const int32_t entityNumber, const Vector3DP &moveDir, double *outSpeedScale, bool *outStrictQueueing = nullptr );

/**
*	@brief	Dynamically optimize slot assignments among crowd members to eliminate crossing trajectories.
*	@param	group	Active crowd coordination group.
*	@param	members	List of active squad member entities.
**/
void SVG_Crowd_OptimizeSlotAssignments( svg_crowd_group_t &group, const std::vector<svg_base_edict_t*> &members );

/**
*	@brief	Dynamically optimize slot assignments among crowd members to eliminate crossing trajectories (zone-allocated overload).
*	@param	group	Active crowd coordination group.
*	@param	members	List of active squad member entities.
**/
void SVG_Crowd_OptimizeSlotAssignments( svg_crowd_group_t &group, const svg_vector_t<svg_base_edict_t*, TAG_SVGAME_NAVMESH> &members );

/**
*	@brief	Render debug visualization for all active crowd groups and formation slots.
**/
void SVG_Crowd_DebugDraw( void );

/**
*	@brief	Check the current pool/navigation lifetime against the physically cleared origin queue prefix.
*	@param	member	Actor whose clearance token is queried.
*	@return	True only while the same live queued owner has cleared an active origin aperture.
*	@note	Read-only, constant-time and owner-thread only; entity number alone cannot grant clearance.
**/
bool SVG_Crowd_HasClearedEgress( const svg_base_edict_t *member );

/**
*	@brief	Test current ingress-head ownership using the retained pool and navigation lifetime.
*	@param	member	Potential current staging head.
*	@return	True only for the active namespace's exact queued owner.
*	@note	Read-only owner-thread lookup; a matching entity number or rank alone is insufficient.
**/
bool SVG_Crowd_IsCurrentIngressHead( const svg_base_edict_t *member );

/**
*	@brief	Test exact current holding-mover ownership using pool lifetime, incarnation and unchanged intent.
*	@param	member	Potential follower carrying a reversible holding assignment.
*	@return	True only for a current active-head claim and the exact queued owner.
*	@note	Read-only bounded lookup; no admission, rank, role or goal changes occur.
**/
bool SVG_Crowd_IsCurrentApproachYieldMember( const svg_base_edict_t *member );

/**
*	@brief	Test whether the active approach head must wait for its current follower holding move.
*	@param	member	Current potential ingress owner.
*	@return	True until the exact pool/incarnation-owned holding assignment physically arrives.
*	@note	Read-only bounded lookup; stale claims cannot pause a replacement actor.
**/
bool SVG_Crowd_IsWaitingForApproachYield( const svg_base_edict_t *member );

/**
*	@brief	Compute the exact full-footprint depth required to clear the origin aperture.
*	@param	member	Live native hull whose footprint is retained.
*	@return	Required outward center depth, or infinity for a missing actor.
*	@note	Uses the existing clearance scale; no radius or release policy is reduced.
**/
double SVG_Crowd_GetEgressClearanceDepth( const svg_base_edict_t *member );
