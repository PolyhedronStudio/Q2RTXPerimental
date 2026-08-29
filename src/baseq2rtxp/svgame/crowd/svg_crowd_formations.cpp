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
			slot.relativeYawDeg = 0.0;
		} else {
			// Alternate sides: odd indices go left (-X), even indices go right (+X).
			const int32_t pairIdx = static_cast<int32_t>( ( i + 1 ) / 2 );
			const double side = ( ( i % 2 ) == 1 ) ? -1.0 : 1.0;
			slot.localOffset = Vector3DP{ side * pairIdx * spacing, 0.0, 0.0 };
			slot.role = ( side < 0.0 ) ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
			slot.relativeYawDeg = ( side < 0.0 ) ? -15.0 : 15.0;
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
			slot.relativeYawDeg = 0.0;
		} else {
			// Stagger backward along -Y and outward along +/-X.
			const int32_t tier = static_cast<int32_t>( ( i + 1 ) / 2 );
			const double side = ( ( i % 2 ) == 1 ) ? -1.0 : 1.0;
			const double offsetX = side * tier * latSpacing;
			const double offsetY = -tier * longSpacing;

			slot.localOffset = Vector3DP{ offsetX, offsetY, 0.0 };
			slot.role = ( side < 0.0 ) ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
			slot.relativeYawDeg = ( side < 0.0 ) ? -30.0 : 30.0;
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
	centerSlot.relativeYawDeg = 0.0;
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
			// Face outward radially from circle center
			slot.relativeYawDeg = QM_AngleMod( angle * ( 180.0 / QM_PI ) - 90.0 );
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
			slot.relativeYawDeg = 0.0;
		} else {
			const int32_t pairIdx = static_cast<int32_t>( ( i + 1 ) / 2 );
			const double side = ( ( i % 2 ) == 1 ) ? -1.0 : 1.0;
			// Stagger alternate columns front/back to create a checkerboard pattern.
			const double rowOffset = ( ( pairIdx % 2 ) == 1 ) ? -longSpacing : 0.0;

			slot.localOffset = Vector3DP{ side * pairIdx * latSpacing, rowOffset, 0.0 };
			slot.role = ( side < 0.0 ) ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
			slot.relativeYawDeg = ( side < 0.0 ) ? -20.0 : 20.0;
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
		// Face inward toward the encircled target
		slot.relativeYawDeg = QM_AngleMod( ( angle * ( 180.0 / QM_PI ) ) + 90.0 );
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
			slot.relativeYawDeg = 0.0;
		} else if ( i == memberCount - 1 ) {
			slot.role = crowd_member_role_t::ROLE_REAR_GUARD;
			slot.relativeYawDeg = 180.0;
		} else {
			slot.role = crowd_member_role_t::ROLE_CENTER;
			slot.relativeYawDeg = ( ( i % 2 ) == 1 ) ? -15.0 : 15.0;
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for a double-column staggered patrol march.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateStaggeredColumnSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double latSpacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : 48.0;
	const double longSpacing = ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : 64.0;

	for ( size_t i = 0; i < memberCount; i++ ) {
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );

		if ( i == 0 ) {
			// Spearhead point-man.
			slot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
			slot.role = crowd_member_role_t::ROLE_POINT;
			slot.relativeYawDeg = 0.0;
		} else {
			const int32_t row = static_cast<int32_t>( ( i + 1 ) / 2 );
			const bool isLeft = ( ( i % 2 ) == 1 );
			const double side = isLeft ? -1.0 : 1.0;
			// Stagger right column half a row behind left column
			const double staggerOffset = isLeft ? 0.0 : ( -0.5 * longSpacing );
			const double offsetX = side * 0.5 * latSpacing;
			const double offsetY = ( -row * longSpacing ) + staggerOffset;

			slot.localOffset = Vector3DP{ offsetX, offsetY, 0.0 };
			if ( i == memberCount - 1 ) {
				slot.role = crowd_member_role_t::ROLE_REAR_GUARD;
				slot.relativeYawDeg = 180.0;
			} else {
				slot.role = isLeft ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
				slot.relativeYawDeg = isLeft ? -25.0 : 25.0;
			}
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for a 4-point diamond / 5-point box formation.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateBoxDiamondSlots( const size_t memberCount, const svg_crowd_params_t &params, std::vector<svg_crowd_slot_t> &outSlots ) {
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

		switch ( i ) {
			case 0:
				// Point / Apex forward
				slot.localOffset = Vector3DP{ 0.0, longSpacing, 0.0 };
				slot.role = crowd_member_role_t::ROLE_POINT;
				slot.relativeYawDeg = 0.0;
				break;
			case 1:
				// Left wing
				slot.localOffset = Vector3DP{ -latSpacing, 0.0, 0.0 };
				slot.role = crowd_member_role_t::ROLE_FLANK_LEFT;
				slot.relativeYawDeg = -90.0;
				break;
			case 2:
				// Right wing
				slot.localOffset = Vector3DP{ latSpacing, 0.0, 0.0 };
				slot.role = crowd_member_role_t::ROLE_FLANK_RIGHT;
				slot.relativeYawDeg = 90.0;
				break;
			case 3:
				// Rear guard
				slot.localOffset = Vector3DP{ 0.0, -longSpacing, 0.0 };
				slot.role = crowd_member_role_t::ROLE_REAR_GUARD;
				slot.relativeYawDeg = 180.0;
				break;
			case 4:
				// Center anchor / Leader
				slot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
				slot.role = crowd_member_role_t::ROLE_LEADER;
				slot.relativeYawDeg = 0.0;
				break;
			default: {
				// Additional members expand outer perimeter
				const int32_t extraIdx = static_cast<int32_t>( i - 4 );
				const double tier = 1.0 + ( extraIdx * 0.4 );
				const double side = ( ( extraIdx % 2 ) == 1 ) ? -1.0 : 1.0;
				slot.localOffset = Vector3DP{ side * latSpacing * tier, -longSpacing * ( 0.5 * tier ), 0.0 };
				slot.role = crowd_member_role_t::ROLE_CENTER;
				slot.relativeYawDeg = side * 45.0;
				break;
			}
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Generate local slot offsets for a slanted echelon formation.
*	@param	memberCount	Number of squad members to place.
*	@param	params		Formation spacing parameters.
*	@param	leftFlank	True for echelon left, false for echelon right.
*	@param	outSlots	[out] Array of computed local slots.
**/
void SVG_Crowd_GenerateEchelonSlots( const size_t memberCount, const svg_crowd_params_t &params, const bool leftFlank, std::vector<svg_crowd_slot_t> &outSlots ) {
	outSlots.clear();
	if ( memberCount == 0 ) {
		return;
	}

	outSlots.reserve( memberCount );
	const double latSpacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : 64.0;
	const double longSpacing = ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : 64.0;
	const double side = leftFlank ? -1.0 : 1.0;

	for ( size_t i = 0; i < memberCount; i++ ) {
		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );

		if ( i == 0 ) {
			slot.localOffset = Vector3DP{ 0.0, 0.0, 0.0 };
			slot.role = crowd_member_role_t::ROLE_POINT;
			slot.relativeYawDeg = 0.0;
		} else {
			slot.localOffset = Vector3DP{ side * static_cast<double>( i ) * latSpacing, -static_cast<double>( i ) * longSpacing, 0.0 };
			if ( i == memberCount - 1 ) {
				slot.role = crowd_member_role_t::ROLE_REAR_GUARD;
				slot.relativeYawDeg = 180.0;
			} else {
				slot.role = leftFlank ? crowd_member_role_t::ROLE_FLANK_LEFT : crowd_member_role_t::ROLE_FLANK_RIGHT;
				slot.relativeYawDeg = side * 35.0;
			}
		}

		outSlots.push_back( slot );
	}
}

/**
*	@brief	Compute walkable corridor clearance width around a world position on the navmesh.
*	@param	worldOrigin		Query position in world space.
*	@param	desiredWidth	Default unconstrained formation width.
*	@return	Constrained width allowed by navmesh boundaries (at least 24 units).
**/
double SVG_Crowd_ComputeCorridorClearance( const Vector3DP &worldOrigin, const double desiredWidth ) {
	if ( g_nav_faces.empty() ) {
		return desiredWidth;
	}

	// Locate the navmesh face enclosing or closest to worldOrigin.
	// Prefer quick strict KD-leaf test at origin, falling back to feet offset if query position is elevated.
	int32_t polyIdx = Nav_FindFaceInLeafStrict( worldOrigin );
	if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
		Vector3DP feetOrigin = worldOrigin;
		feetOrigin.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		polyIdx = Nav_FindFaceInLeafStrict( feetOrigin );
	}

	// If face located within local KD-leaf, query clearance corridor.
	if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
		const nav_face_t &face = g_nav_faces[ polyIdx ];
		if ( face.clearance > 0.0 ) {
			// Approximate traversable corridor diameter is clearance * 2.0
			const double availableWidth = face.clearance * 2.0;
			return std::max( CROWD_DEFAULT_MIN_CORRIDOR_SPACING, std::min( desiredWidth, availableWidth ) );
		}
	}

	return desiredWidth;
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
		case crowd_chase_target_type_t::CROWD_STYLE_STAGGERED_COLUMN:
			SVG_Crowd_GenerateStaggeredColumnSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_BOX_DIAMOND:
			SVG_Crowd_GenerateBoxDiamondSlots( memberCount, params, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_ECHELON_LEFT:
			SVG_Crowd_GenerateEchelonSlots( memberCount, params, true, outSlots );
			break;
		case crowd_chase_target_type_t::CROWD_STYLE_ECHELON_RIGHT:
			SVG_Crowd_GenerateEchelonSlots( memberCount, params, false, outSlots );
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
*	@param	slots			[in/out] Formation slots to clamp/project.
*	@param	anchorOrigin	World-space formation center used for geometric line-of-sight rejection
*							of slots that project through solid brush walls.
*	@param	agentRadius		Radius of agents for wall standoff checking.
**/
void SVG_Crowd_SnapSlotsToNavMesh( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double agentRadius ) {
	if ( g_nav_faces.empty() ) {
		return;
	}

	for ( svg_crowd_slot_t &slot : slots ) {
		// Attempt to locate a directly enclosing walkable navmesh polygon within the local KD-leaf.
		int32_t polyIdx = Nav_FindFaceInLeafStrict( slot.worldPosition );
		if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			// Fallback: test with feet elevation offset in case slot is slightly above walkable surface.
			Vector3DP feetPos = slot.worldPosition;
			feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
			polyIdx = Nav_FindFaceInLeafStrict( feetPos );
		}

		// If a valid enclosing face was found in the local KD-leaf, project Z onto face plane.
		if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
			const nav_face_t &face = g_nav_faces[ polyIdx ];
			if ( std::fabs( face.normal.z ) > 0.001 ) {
				const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
				const double d = QM_Vector3DotProductDP( v0, face.normal );
				slot.worldPosition.z = ( d - ( slot.worldPosition.x * face.normal.x + slot.worldPosition.y * face.normal.y ) ) / face.normal.z;
			}

			// Geometric line-of-sight from formation anchor to slot:
			// Reject slots that project through solid brush walls even though they land on valid navmesh.
			// This prevents radial formations (circle_filled, perimeter, etc.) from placing goals behind
			// walls that are only reachable via long detours through narrow doorways.
			if ( !Nav_HasGeometricLineOfSight2D( anchorOrigin, slot.worldPosition, agentRadius ) ) {
				slot.isNavmeshValid = false;
				continue;
			}

			slot.isNavmeshValid = true;
			continue;
		}

		// Slot lands outside walkable mesh: mark invalid so caller clamps to anchor origin.
		// Never call Nav_FindClosestPolyGlobal here to avoid expensive O(num_faces) full-level searches.
		slot.isNavmeshValid = false;
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

/**
*	@brief		Assign crowd members to formation slots with hysteresis to prevent thrashing between frames.
*	@param	memberOrigins		Current feet origins of the crowd member entities (Vector3DP).
*	@param	slots				Target formation slot definitions.
*	@param	previousSlotMap		Previous frame's slot assignments for each member (or -1 if new).
*	@param	outMemberToSlotMap	[out] Mapping from member index (0..N-1) to assigned slot index (0..N-1).
*	@param	hysteresisDist		Bonus distance threshold (default: 48.0 units) to favor holding current slot.
**/
void SVG_Crowd_AssignMembersToSlotsHysteresis( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, const std::vector<int32_t> &previousSlotMap, std::vector<int32_t> &outMemberToSlotMap, const double hysteresisDist ) {
	const size_t count = memberOrigins.size();
	outMemberToSlotMap.clear();
	outMemberToSlotMap.resize( count, -1 );

	if ( count == 0 || slots.empty() ) {
		return;
	}

	std::vector<bool> slotClaimed( slots.size(), false );
	const double hystBonusSq = hysteresisDist * hysteresisDist;

	struct candidate_match_t {
		int32_t memberIdx = -1;
		int32_t slotIdx = -1;
		double effectiveDistSq = std::numeric_limits<double>::max();
	};

	std::vector<candidate_match_t> allPairs;
	allPairs.reserve( count * slots.size() );

	for ( size_t m = 0; m < count; m++ ) {
		const int32_t prevSlot = ( m < previousSlotMap.size() ) ? previousSlotMap[ m ] : -1;
		for ( size_t s = 0; s < slots.size(); s++ ) {
			double distSq = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ s ].worldPosition );
			// Apply hysteresis: if the member previously held this slot, discount the effective distance
			// so the assignment algorithm is sticky and does not thrash on small orientation changes.
			if ( static_cast<int32_t>( s ) == prevSlot ) {
				distSq = ( distSq > hystBonusSq ) ? ( distSq - hystBonusSq ) : 0.0;
			}
			allPairs.push_back( candidate_match_t{ static_cast<int32_t>( m ), static_cast<int32_t>( s ), distSq } );
		}
	}

	// Sort candidate pairings ascending by effective squared distance.
	std::sort( allPairs.begin(), allPairs.end(), []( const candidate_match_t &a, const candidate_match_t &b ) {
		return a.effectiveDistSq < b.effectiveDistSq;
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

