/********************************************************************
*
*
*	ServerGame: Crowd Formation Geometry & Slot Allocators
*	File: svg_crowd_formations.cpp
*	Description:
*		Formation pattern generators, local-to-world coordinate
*		transforms, navmesh projection/snapping, and anti-crossover
*		member-to-slot assignment algorithms using Vector3DP.
*
*
********************************************************************/
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_types.h"
#include "shared/math/qm_math_cpp.h"
#include "shared/math/qm_vector3_dp.h"
#include "svgame/nav/nav_generate.h"

#include <algorithm>
#include <cmath>
#include <limits>

/**
*
*
*
*	Formation Slot Generators (Local Coordinate Space):
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
void SVG_Crowd_GenerateLineSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double spacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : 64.0;

	// Center member at slot 0 (Leader), then alternate left and right wings.
	for ( size_t i = 0; i < memberCount; i++ ) {
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );

		if ( i == 0 ) {
			// Center leader position.
			slot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
			slot.role = crowd_member_role_t::ROLE_LEADER;
		} else {
			// Alternate sides: odd indices go left (-X), even indices go right (+X).
			const int32_t pairIdx = static_cast<int32_t>( ( i + 1 ) / 2 );
			const double side = ( ( i % 2 ) == 1 ) ? -1.0 : 1.0;
			slot.localOffset = Vector3DP{ side * pairIdx * spacing, 0.0, 0.0 };
			slot.role = ( side < 0.0 ) ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for an arrow/wedge (V-formation).
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateArrowSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double latSpacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : 64.0;
	const double longSpacing = ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : 64.0;

	for ( size_t i = 0; i < memberCount; i++ ) {
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );

		if ( i == 0 ) {
			// Spearhead / Point leader at the apex.
			slot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
			slot.role = crowd_member_role_t::ROLE_POINT;
		} else {
			// Stagger backward along -Y and outward along +/-X.
			const int32_t tier = static_cast<int32_t>( ( i + 1 ) / 2 );
			const double side = ( ( i % 2 ) == 1 ) ? -1.0 : 1.0;
			const double offsetX = side * tier * latSpacing;
			const double offsetY = -tier * longSpacing;

			slot.localOffset = Vector3DP{ offsetX, offsetY, 0.0 };
			slot.role = ( side < 0.0 ) ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for a filled concentric circle pattern.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateCircleFilledSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double radialSpacing = ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : 64.0;
	const double arcSpacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : 64.0;

	// Slot 0: Center leader.
	svg_crowd_slot_t centerSlot;
	centerSlot.slotIndex = 0;
	centerSlot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
	centerSlot.role = crowd_member_role_t::ROLE_LEADER;
	outSlots.push_back( centerSlot );

	size_t remaining = memberCount - 1;
	int32_t ringIndex = 1;
	int32_t currentSlotIdx = 1;

	// Distribute remaining members in concentric radial rings.
	while ( remaining > 0 ) {
		const double radius = ringIndex * radialSpacing;
		const double circumference = 2.0 * QM_PI * radius;
		// Estimate slots capacity for this ring.
		int32_t ringCapacity = static_cast<int32_t>( std::floor( circumference / arcSpacing ) );
		if ( ringCapacity < 4 ) {
			ringCapacity = 4;
		}

		const int32_t slotsToPlace = std::min<int32_t>( static_cast<int32_t>( remaining ), ringCapacity );
		const double angleStep = ( 2.0 * QM_PI ) / static_cast<double>( slotsToPlace );

		for ( int32_t k = 0; k < slotsToPlace; k++ ) {
			const double angle = k * angleStep;
			const double offsetX = radius * std::cos( angle );
			const double offsetY = radius * std::sin( angle );

			svg_crowd_slot_t slot;
			slot.slotIndex = currentSlotIdx++;
			slot.localOffset = Vector3DP{ offsetX, offsetY, 0.0 };
			slot.role = crowd_member_role_t::ROLE_CENTER;
			outSlots.push_back( slot );
		}

		remaining -= slotsToPlace;
		ringIndex++;
	}
}

/**
*	@brief	Generate local slot offsets for a dashed/staggered echelon line.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateDashedLineSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double latSpacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : 64.0;
	const double longSpacing = ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : 64.0;

	for ( size_t i = 0; i < memberCount; i++ ) {
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );

		if ( i == 0 ) {
			slot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
			slot.role = crowd_member_role_t::ROLE_LEADER;
		} else {
			const int32_t pairIdx = static_cast<int32_t>( ( i + 1 ) / 2 );
			const double side = ( ( i % 2 ) == 1 ) ? -1.0 : 1.0;
			// Stagger alternate columns front/back to create a checkerboard pattern.
			const double rowOffset = ( ( pairIdx % 2 ) == 1 ) ? -longSpacing : 0.0;

			slot.localOffset = Vector3DP{ side * pairIdx * latSpacing, rowOffset, 0.0 };
			slot.role = ( side < 0.0 ) ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for a surround perimeter circle.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GeneratePerimeterSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double radius = ( params.minCoverDistance > 0.0 ) ? params.minCoverDistance : 192.0;
	const double angleStep = ( 2.0 * QM_PI ) / static_cast<double>( memberCount );

	for ( size_t i = 0; i < memberCount; i++ ) {
		const double angle = i * angleStep;
		const double offsetX = radius * std::cos( angle );
		const double offsetY = radius * std::sin( angle );

		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );
		slot.localOffset = Vector3DP{ offsetX, offsetY, 0.0 };
		slot.role = ( i == 0 ) ? crowd_member_role_t::ROLE_POINT : crowd_member_role_t::ROLE_CENTER;
		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for a single-file column march.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateColumnSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double spacing = ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : 64.0;

	for ( size_t i = 0; i < memberCount; i++ ) {
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );
		// Trail members strictly backward along -Y axis.
		slot.localOffset = Vector3DP{ 0.0, -static_cast<double>( i ) * spacing, 0.0 };

		if ( i == 0 ) {
			slot.role = crowd_member_role_t::ROLE_POINT;
		} else if ( i == memberCount - 1 ) {
			slot.role = crowd_member_role_t::ROLE_REAR_GUARD;
		} else {
			slot.role = crowd_member_role_t::ROLE_CENTER;
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Master dispatcher: generate local slots for any given crowd style.
*	@param	style		Formation style identifier.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateFormationSlots( const crowd_chase_target_type_t style, const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	switch ( style ) {
		case crowd_chase_target_type_t::CROWD_STYLE_LINE:
			SVG_Crowd_GenerateLineSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_ARROW:
			SVG_Crowd_GenerateArrowSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_CIRCLE_FILLED:
			SVG_Crowd_GenerateCircleFilledSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_DASHED_LINE:
			SVG_Crowd_GenerateDashedLineSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_SURROUND_PERIMETER:
			SVG_Crowd_GeneratePerimeterSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_COLUMN_MARCH:
			SVG_Crowd_GenerateColumnSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_TACTICAL_COVER:
		default:
			// Tactical cover allocation handles slot placement independently in the manager; fallback to arrow.
			SVG_Crowd_GenerateArrowSlots( memberCount, params, outSlots );
			break;
	}
}

/**
*
*
*
*	Spatial Transformation & Navmesh Snapping:
*
*
*
**/

/**
*	@brief		Transform local formation slot offsets into world-space coordinates in Vector3DP.
*	@param	anchorOrigin	World-space center/destination in Vector3DP.
*	@param	forwardYawDeg	Heading angle in degrees for the formation forward direction.
*	@param	slots			[in/out] Slots whose world positions will be updated.
**/
void SVG_Crowd_TransformLocalSlotsToWorld( const Vector3DP &anchorOrigin, const double forwardYawDeg, std::vector<svg_crowd_slot_t> &slots ) {
	const Vector3 angles{ 0.0f, static_cast<float>( forwardYawDeg ), 0.0f };
	Vector3 forwardVec, rightVec, upVec;
	QM_AngleVectors( angles, &forwardVec, &rightVec, &upVec );

	const Vector3DP fwdDP( forwardVec );
	const Vector3DP rgtDP( rightVec );
	const Vector3DP upDP( upVec );

	for ( svg_crowd_slot_t &slot : slots ) {
		// Local X = lateral right, Local Y = forward, Local Z = up
		const Vector3DP worldPos = anchorOrigin + ( rgtDP * slot.localOffset.x ) + ( fwdDP * slot.localOffset.y ) + ( upDP * slot.localOffset.z );
		slot.worldPosition = worldPos;
		slot.isNavmeshValid = false;
	}
}

/**
*	@brief		Transform local formation slot offsets into world-space coordinates (Vector3 overload).
*	@param	anchorOrigin	World-space center/destination in Vector3.
*	@param	forwardYawDeg	Heading angle in degrees for the formation forward direction.
*	@param	slots			[in/out] Slots whose world positions will be updated.
**/
void SVG_Crowd_TransformLocalSlotsToWorld( const Vector3 &anchorOrigin, const double forwardYawDeg, std::vector<svg_crowd_slot_t> &slots ) {
	SVG_Crowd_TransformLocalSlotsToWorld( Vector3DP( anchorOrigin ), forwardYawDeg, slots );
}

/**
*	@brief		Project and snap all formation slot world positions onto valid walkable navmesh polygons.
*	@param	slots		[in/out] Formation slots to clamp/project.
*	@param	agentRadius	Radius of agents for wall standoff checking.
**/
void SVG_Crowd_SnapSlotsToNavMesh( std::vector<svg_crowd_slot_t> &slots, const double agentRadius ) {
	if ( g_nav_faces.empty() ) {
		return;
	}

	for ( svg_crowd_slot_t &slot : slots ) {
		// Attempt to locate a directly enclosing walkable navmesh polygon.
		const int32_t polyIdx = Nav_FindPolyInLeaf( slot.worldPosition );
		if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
			slot.worldPosition = g_nav_faces[ polyIdx ].center;
			slot.isNavmeshValid = true;
			continue;
		}

		// Fallback: search closest walkable polygon in KD-tree.
		const int32_t closestPolyIdx = Nav_FindClosestPolyGlobal( slot.worldPosition );
		if ( closestPolyIdx >= 0 && closestPolyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
			slot.worldPosition = g_nav_faces[ closestPolyIdx ].center;
			slot.isNavmeshValid = true;
		} else {
			// Keep computed world position with invalid navmesh flag so AI can still attempt fallback movement.
			slot.isNavmeshValid = false;
		}
	}
}

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
void SVG_Crowd_AssignMembersToSlots( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, std::vector<int32_t> &outMemberToSlotMap ) {
	const size_t count = memberOrigins.size();
	outMemberToSlotMap.clear();
	outMemberToSlotMap.resize( count, -1 );

	if ( count == 0 || slots.empty() ) {
		return;
	}

	std::vector<bool> slotClaimed( slots.size(), false );

	/**
	*	Greedy distance-squared matching: repeatedly assign the closest available slot
	*	to each unassigned member, sorting by minimum candidate distance to avoid crossover.
	**/
	struct candidate_match_t {
		int32_t memberIdx = -1;
		int32_t slotIdx = -1;
		double distSq = std::numeric_limits<double>::max();
	};

	std::vector<candidate_match_t> allPairs;
	allPairs.reserve( count * slots.size() );

	for ( size_t m = 0; m < count; m++ ) {
		for ( size_t s = 0; s < slots.size(); s++ ) {
			const double distSq = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ s ].worldPosition );
			allPairs.push_back( candidate_match_t{ static_cast<int32_t>( m ), static_cast<int32_t>( s ), distSq } );
		}
	}

	// Sort candidate pairings ascending by squared distance.
	std::sort( allPairs.begin(), allPairs.end(), []( const candidate_match_t &a, const candidate_match_t &b ) {
		return a.distSq < b.distSq;
	} );

	std::vector<bool> memberAssigned( count, false );
	size_t assignedCount = 0;

	for ( const candidate_match_t &pair : allPairs ) {
		if ( assignedCount >= count ) {
			break;
		}

		if ( !memberAssigned[ pair.memberIdx ] && !slotClaimed[ pair.slotIdx ] ) {
			memberAssigned[ pair.memberIdx ] = true;
			slotClaimed[ pair.slotIdx ] = true;
			outMemberToSlotMap[ pair.memberIdx ] = pair.slotIdx;
			assignedCount++;
		}
	}
}

/**
*	@brief		Assign crowd members to formation slots minimizing total distance traveled (Vector3 overload).
*	@param	memberOrigins		Current feet origins of the crowd member entities (Vector3).
*	@param	slots				Target formation slot definitions.
*	@param	outMemberToSlotMap	[out] Mapping from member index (0..N-1) to assigned slot index (0..N-1).
**/
void SVG_Crowd_AssignMembersToSlots( const std::vector<Vector3> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, std::vector<int32_t> &outMemberToSlotMap ) {
	std::vector<Vector3DP> memberOriginsDP;
	memberOriginsDP.reserve( memberOrigins.size() );
	for ( const Vector3 &pos : memberOrigins ) {
		memberOriginsDP.emplace_back( pos );
	}
	SVG_Crowd_AssignMembersToSlots( memberOriginsDP, slots, outMemberToSlotMap );
}
