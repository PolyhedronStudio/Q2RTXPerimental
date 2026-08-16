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
*	Formation Slot Generation Functions:
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
*	@brief	Master dispatcher: generate local slots for any given crowd style.
*	@param	style		Formation style identifier.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateFormationSlots( const crowd_chase_target_type_t style, const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots );

/**
*	Spatial Transformation & Navmesh Snapping:
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
*	@param	slots		[in/out] Formation slots to clamp/project.
*	@param	agentRadius	Radius of agents for wall standoff checking.
**/
void SVG_Crowd_SnapSlotsToNavMesh( std::vector<svg_crowd_slot_t> &slots, const double agentRadius = 16.0 );

/**
*	Anti-Crossover Slot Assignment:
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
