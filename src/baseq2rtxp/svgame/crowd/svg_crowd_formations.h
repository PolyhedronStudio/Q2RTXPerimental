/********************************************************************
*
*
*	ServerGame: Crowd Formation Geometry & Slot Allocators
*	File: svg_crowd_formations.h
*	Description:
*		Formation pattern generators, local-to-world coordinate
*		transforms, navmesh projection/snapping, and anti-crossover
*		member-to-slot assignment algorithms using Vector3DP.
*
*
********************************************************************/
#pragma once

#include "svgame/crowd/svg_crowd_types.h"
#include <vector>

/**
* 
* 
* 
*	Formation Slot Generation Functions:
* 
* 
* 
**/
/**
*	@brief	Generate local slot offsets for an abreast line formation.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateLineSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for an arrow/wedge (V-formation).
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateArrowSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a filled concentric circle pattern.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateCircleFilledSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a dashed/staggered echelon line.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateDashedLineSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a surround perimeter circle.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GeneratePerimeterSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a single-file column march.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateColumnSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a double-column staggered patrol march.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateStaggeredColumnSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a 4-point diamond / 5-point box formation.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateBoxDiamondSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Generate local slot offsets for a slanted echelon formation.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	leftFlank	True for echelon left, false for echelon right.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateEchelonSlots( const size_t memberCount, const svg_crowd_params_t &params, const bool leftFlank, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	@brief	Compute walkable corridor clearance width around a world position on the navmesh.
*	@param	worldOrigin		Query position in world space.
*	@param	desiredWidth	Default unconstrained formation width.
*	@return	Constrained width allowed by navmesh boundaries (at least 24 units).
**/
double SVG_Crowd_ComputeCorridorClearance( const Vector3DP &worldOrigin, const double desiredWidth );

/**
*	@brief	Master dispatcher: generate local slots for any given crowd style.
*	@param	style		Formation style identifier.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateFormationSlots( const crowd_chase_target_type_t style, const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );



/**
* 
* 
* 
* 
*	Spatial Transformation & Navmesh Snapping:
* 
* 
* 
* 
**/
/**
*	@brief		Transform local formation slot offsets into world-space coordinates.
*	@param	anchorOrigin	World-space center/destination in Vector3DP.
*	@param	forwardYawDeg	Heading angle in degrees for the formation forward direction.
*	@param	slots			[in/out] Slots whose world positions will be updated.
**/
void SVG_Crowd_TransformLocalSlotsToWorld( const Vector3DP &anchorOrigin, const double forwardYawDeg, std::vector<svg_crowd_slot_t> &slots );

/**
*	@brief		Transform local formation slot offsets into world-space coordinates (Vector3 overload).
*	@param	anchorOrigin	World-space center/destination in Vector3.
*	@param	forwardYawDeg	Heading angle in degrees for the formation forward direction.
*	@param	slots			[in/out] Slots whose world positions will be updated.
**/
void SVG_Crowd_TransformLocalSlotsToWorld( const Vector3 &anchorOrigin, const double forwardYawDeg, std::vector<svg_crowd_slot_t> &slots );

/**
*	@brief		Project and snap all formation slot world positions onto valid walkable navmesh polygons.
*	@param	slots			[in/out] Formation slots to clamp/project.
*	@param	anchorOrigin	World-space formation center used for geometric line-of-sight rejection
*							of slots that project through solid brush walls.
*	@param	agentRadius		Radius of agents for wall standoff checking.
**/
void SVG_Crowd_SnapSlotsToNavMesh( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double agentRadius = CROWD_DEFAULT_AGENT_RADIUS );

/**
*	@brief	Interpolate a point along a navigation guide path in reverse from destination by target distance.
*	@param	path			Navigation path vertices from start to destination.
*	@param	targetDistBack	Distance to traverse backwards from path.back().
*	@param	outPos			[out] Interpolated 3D position along the path.
*	@param	outTangent		[out] Forward corridor tangent vector at the sample point.
*	@return	True if a sample was obtained from the path.
**/
bool SVG_Crowd_SampleGuidePathInReverse( const std::vector<Vector3DP> &path, const double targetDistBack, Vector3DP *outPos, Vector3DP *outTangent );

/**
*	@brief	Perform bounded O(1) topological BFS flood-fill across adjacent NavMesh faces to pack slots into an enclosed room.
*	@param	slots				[in/out] Formation slots to validate and pack.
*	@param	anchorOrigin		Formation anchor in Vector3DP.
*	@param	minPackingSepSqr	Minimum allowable separation squared between slots.
*	@param	agentRadius			Agent collision hull radius.
*	@param	acceptedPositions	[in/out] Array of already-accepted valid slot positions.
*	@param	maxRoomRadius		Maximum radial bound in units from anchorOrigin to enclose packing within room walls.
*	@return	True if all invalid slots were successfully resolved.
**/
bool SVG_Crowd_FloodFillRoomPackingO1( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double minPackingSepSqr, const double agentRadius, std::vector<Vector3DP> &acceptedPositions, const double maxRoomRadius = 999999.0, const Vector3DP *portalOrigin = nullptr, const Vector3DP *ingressDir = nullptr );

/**
*	@brief	Fit circular / perimeter defense rings to enclosed room geometry.
*	@details	When the anchor resolves to an enclosed room / alcove, slots are plotted
*				directly from the room's walkable polygons: every convex room face is
*				eroded inward by (agentRadius + margin), an encirclement contour
*				(circle for round rooms, rounded rectangle for rectangular rooms) is
*				fitted into the eroded region, angular keep-out lanes are carved around
*				each passable doorway portal, and each contour sample is validated with
*				exact eroded-polygon containment plus a step-aware SVG_MMove_StepProbe
*				from the anchor, sliding along the contour to relocate blocked samples
*				instead of rejecting them. Open-terrain / unclassified destinations fall
*				back to the legacy ray-based fitting path. Slots that still cannot be
*				placed are marked isNavmeshValid = false for phase-2 queue overflow.
*	@param	slots			[in/out] Formation slots to fit inside room walls.
*	@param	anchorOrigin	Center anchor origin in Vector3DP.
*	@param	agentRadius		Agent collision hull radius in world units.
*	@param	portalOrigin	Optional bottleneck portal origin for doorway aperture clearance.
*	@param	ingressDir		Optional ingress approach direction vector.
**/
void SVG_Crowd_FitCircularFormationToRoom( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double agentRadius, const Vector3DP *portalOrigin = nullptr, const Vector3DP *ingressDir = nullptr );

/**
*	@brief		Resolve off-mesh, in-wall, and mutually colliding formation slots into distinct column ranks.
*	@details	Guarantees that every slot is on a valid walkable navmesh surface and no two slots share
*				the same coordinate or violate mutual separation distance.
*	@param	slots			[in/out] Formation slots to validate and space out.
*	@param	anchorOrigin	Formation center anchor in Vector3DP.
*	@param	headingYawDeg	Forward movement heading in degrees.
*	@param	minSeparation	Minimum physical separation distance required between distinct slot centers.
*	@param	agentRadius		Radius of agents for wall standoff checking.
*	@param	guidePath		Optional navigation guide path from squad approach to destination used
*							to curve trailing column slots along curved corridors, ramps, and staircases.
*	@param	tacticalFlags	Configurable tactical behavior and fallback bitflags (see crowd_tactical_flags_t).
**/
void SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double headingYawDeg, const double minSeparation = CROWD_DEFAULT_SEPARATION_RADIUS, const double agentRadius = CROWD_DEFAULT_AGENT_RADIUS, const std::vector<Vector3DP> *guidePath = nullptr, const uint32_t tacticalFlags = CROWD_TACTICAL_FLAG_DEFAULT, const Vector3DP *explicitPortalOrigin = nullptr );

/**
*	@brief		Sort formation slots so that slots deepest along the ingress vector are indexed first.
*	@details	Enforces that the earliest-arriving agents navigate to the back of an enclosed area
*				(bunker, room, corridor) so they never block subsequent incoming agents.
*	@param	slots			[in/out] Formation slots to order by ingress depth.
*	@param	destOrigin		Destination center origin in Vector3DP.
*	@param	ingressDir		Normalized approach direction vector in Vector3DP (from squad towards destination).
**/
void SVG_Crowd_SortSlotsByIngressDepth( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &destOrigin, const Vector3DP &ingressDir );

/**
* 
* 
* 
*	Anti-Crossover Slot Assignment:
* 
* 
* 
**/
/**
*	@brief		Assign crowd members to formation slots minimizing total distance traveled (anti-crossover).
*	@param	memberOrigins		Current feet origins of the crowd member entities (Vector3DP).
*	@param	slots				Target formation slot definitions.
*	@param	outMemberToSlotMap	[out] Mapping from member index (0..N-1) to assigned slot index (0..N-1).
**/
void SVG_Crowd_AssignMembersToSlots( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, std::vector<int32_t> &outMemberToSlotMap );

/**
*	@brief		Assign crowd members to formation slots minimizing total distance traveled (Vector3 overload).
*	@param	memberOrigins		Current feet origins of the crowd member entities (Vector3).
*	@param	slots				Target formation slot definitions.
*	@param	outMemberToSlotMap	[out] Mapping from member index (0..N-1) to assigned slot index (0..N-1).
**/
void SVG_Crowd_AssignMembersToSlots( const std::vector<Vector3> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, std::vector<int32_t> &outMemberToSlotMap );

/**
*	@brief		Assign crowd members to formation slots with hysteresis to prevent thrashing between frames.
*	@param	memberOrigins		Current feet origins of the crowd member entities (Vector3DP).
*	@param	slots				Target formation slot definitions.
*	@param	previousSlotMap		Previous frame's slot assignments for each member (or -1 if new).
*	@param	outMemberToSlotMap	[out] Mapping from member index (0..N-1) to assigned slot index (0..N-1).
*	@param	hysteresisDist		Bonus distance threshold (default: 48.0 units) to favor holding current slot.
**/
void SVG_Crowd_AssignMembersToSlotsHysteresis( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, const std::vector<int32_t> &previousSlotMap, std::vector<int32_t> &outMemberToSlotMap, const double hysteresisDist = 48.0 );

/**
*	@brief		Assign crowd members to formation slots ordered strictly by ingress depth and approach progress.
*	@details	Ensures leading squad members take deepest slots at the back of rooms/corridors,
*				while trailing members take shallow slots at the entrance, preventing deadlocks and crossover congestion.
*	@param	memberOrigins		Current feet origins of crowd members in Vector3DP.
*	@param	slots				Target formation slot definitions.
*	@param	previousSlotMap		Previous frame's slot assignments for each member (or -1 if new).
*	@param	outMemberToSlotMap	[out] Mapping from member index (0..N-1) to assigned slot index (0..N-1).
*	@param	destOrigin			Destination center origin in Vector3DP.
*	@param	ingressDir			Normalized approach direction vector in Vector3DP (from squad towards destination).
*	@param	hysteresisDist		Bonus distance threshold to favor holding current slot.
*	@param	portalOrigin		Optional pointer to doorway / chokepoint portal origin for distance-based ranking.
*	@param	guidePath			Optional pointer to navigation route vertices for exact path-geodesic progress ranking.
**/
void SVG_Crowd_AssignMembersToSlotsIngress( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, const std::vector<int32_t> &previousSlotMap, std::vector<int32_t> &outMemberToSlotMap, const Vector3DP &destOrigin, const Vector3DP &ingressDir, const double hysteresisDist = 48.0, const Vector3DP *portalOrigin = nullptr, const std::vector<Vector3DP> *guidePath = nullptr );

