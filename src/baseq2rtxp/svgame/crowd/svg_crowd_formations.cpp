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
#include "svgame/entities/svg_base_edict.h"
#include "svgame/svg_utils.h"

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

		// Calculate maximum capacity using minimum acceptable separation (e.g. 48 units)
		// to avoid pushing only 1 or 2 leftover members into a giant, solitary outer ring.
		const double minAcceptableSpacing = std::max( ( CROWD_DEFAULT_AGENT_RADIUS * 2.0 ) + 4.0, ( params.minCorridorSpacing > 0.0 ) ? params.minCorridorSpacing : 48.0 );
		const int32_t maxRingCapacity = std::max( ringCapacity, static_cast<int32_t>( std::floor( circumference / minAcceptableSpacing ) ) );

		int32_t slotsToPlace = 0;
		if ( static_cast<int32_t>( remaining ) <= maxRingCapacity ) {
			// All remaining members fit in this ring with safe physical separation
			slotsToPlace = static_cast<int32_t>( remaining );
		} else {
			slotsToPlace = std::min<int32_t>( static_cast<int32_t>( remaining ), ringCapacity );
		}

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

			// Reject slots that land on a drastically different vertical level (e.g. overhead catwalks, roof tops, or pits).
			static constexpr double MAX_FORMATION_Z_DIFF = CROWD_ARRIVAL_MAX_Z_DIFF;
			if ( std::fabs( slot.worldPosition.z - anchorOrigin.z ) > MAX_FORMATION_Z_DIFF ) {
				slot.isNavmeshValid = false;
				continue;
			}

			// Geometric line-of-sight from formation anchor to slot:
			// Reject slots that project through solid brush walls even though they land on valid navmesh.
			// This prevents radial formations (circle_filled, perimeter, etc.) from placing goals behind
			// walls that are only reachable via long detours through narrow doorways.
			if ( !Nav_HasGeometricLineOfSight2D( anchorOrigin, slot.worldPosition, agentRadius ) ) {
				slot.isNavmeshValid = false;
				continue;
			}

			// Physical world trace check to confirm the slot origin is not embedded inside a brush wall:
			const Vector3 trStart = QM_Vector3FromDP( anchorOrigin + Vector3DP{ 0.0, 0.0, 18.0 } );
			const Vector3 trEnd = QM_Vector3FromDP( slot.worldPosition + Vector3DP{ 0.0, 0.0, 18.0 } );
			const svg_trace_t tr = SVG_Trace( trStart, vec3_origin, vec3_origin, trEnd, nullptr, CM_CONTENTMASK_SOLID );
			if ( tr.fraction < 0.99f ) {
				slot.isNavmeshValid = false;
				continue;
			}

			slot.isNavmeshValid = true;
			continue;
		}

		// Slot lands outside walkable mesh.
		slot.isNavmeshValid = false;
	}
}

/**
*	@brief		Resolve off-mesh, in-wall, and mutually colliding formation slots into distinct column ranks.
*	@details	Guarantees that every slot is on a valid walkable navmesh surface and no two slots share
*				the same coordinate or violate mutual separation distance.
*	@param	slots			[in/out] Formation slots to validate and space out.
/**
*	@brief	Interpolate a point along a navigation guide path in reverse from destination by target distance.
*	@param	path			Navigation path vertices from start to destination.
*	@param	targetDistBack	Distance to traverse backwards from path.back().
*	@param	outPos			[out] Interpolated 3D position along the path.
*	@param	outTangent		[out] Forward corridor tangent vector at the sample point.
*	@return	True if a sample was obtained from the path.
**/
static bool SVG_Crowd_SampleGuidePathInReverse( const std::vector<Vector3DP> &path, const double targetDistBack, Vector3DP *outPos, Vector3DP *outTangent ) {
	if ( path.size() < 2 || targetDistBack <= 0.0 ) {
		if ( !path.empty() && outPos ) {
			*outPos = path.back();
		}
		if ( outTangent && path.size() >= 2 ) {
			Vector3DP fwd = path.back() - path[ path.size() - 2 ];
			fwd.z = 0.0;
			const double fwdLen = QM_Vector3LengthDP( fwd );
			*outTangent = ( fwdLen > 0.001 ) ? ( fwd * ( 1.0 / fwdLen ) ) : Vector3DP{ 1.0, 0.0, 0.0 };
		}
		return false;
	}

	double accumulated = 0.0;
	for ( size_t i = path.size() - 1; i > 0; --i ) {
		const Vector3DP &pCurr = path[ i ];
		const Vector3DP &pPrev = path[ i - 1 ];
		Vector3DP seg = pPrev - pCurr;
		const double segLen = QM_Vector3LengthDP( seg );
		if ( segLen <= 0.001 ) {
			continue;
		}

		if ( accumulated + segLen >= targetDistBack ) {
			const double frac = ( targetDistBack - accumulated ) / segLen;
			if ( outPos ) {
				*outPos = pCurr + ( seg * frac );
			}
			if ( outTangent ) {
				Vector3DP forwardSeg = pCurr - pPrev;
				forwardSeg.z = 0.0;
				const double fwdLen = QM_Vector3LengthDP( forwardSeg );
				*outTangent = ( fwdLen > 0.001 ) ? ( forwardSeg * ( 1.0 / fwdLen ) ) : Vector3DP{ 1.0, 0.0, 0.0 };
			}
			return true;
		}
		accumulated += segLen;
	}

	// Reached the start of the path: use the first waypoint
	if ( outPos ) {
		*outPos = path.front();
	}
	if ( outTangent && path.size() >= 2 ) {
		Vector3DP forwardSeg = path[ 1 ] - path[ 0 ];
		forwardSeg.z = 0.0;
		const double fwdLen = QM_Vector3LengthDP( forwardSeg );
		*outTangent = ( fwdLen > 0.001 ) ? ( forwardSeg * ( 1.0 / fwdLen ) ) : Vector3DP{ 1.0, 0.0, 0.0 };
	}
	return true;
}

//! Scratch ranking record for crowd member progress sorting.
struct svg_crowd_member_rank_t {
	int32_t memberIdx = -1;
	double progress = 0.0;
	double lateral = 0.0;
};

//! Scratch ranking record for formation slot depth sorting.
struct svg_crowd_slot_rank_t {
	int32_t slotIdx = -1;
	double depth = 0.0;
	double lateral = 0.0;
};

//! Static reusable scratch buffer for crowd member rank sorting.
static std::vector<svg_crowd_member_rank_t> s_rankedMembers;
//! Static reusable scratch buffer for crowd slot rank sorting.
static std::vector<svg_crowd_slot_rank_t> s_rankedSlots;
//! Static reusable scratch buffer for slot claim tracking.
static std::vector<bool> s_slotClaimed;
//! Static reusable scratch buffer for accepted slot positions to eliminate heap allocations.
static std::vector<Vector3DP> s_acceptedPositions;

/**
*	@brief	Detect and resolve slot-to-slot spatial collisions and invalid/off-mesh slot positions.
*	@param	slots			[in/out] Formation slots to validate and space out.
*	@param	anchorOrigin	Formation center anchor in Vector3DP.
*	@param	headingYawDeg	Forward movement heading in degrees.
*	@param	minSeparation	Minimum physical separation distance required between distinct slot centers.
*	@param	agentRadius		Agent hull radius for boundary clearance.
*	@param	guidePath		Optional navigation guide path from squad approach to destination used
*							to curve trailing column slots along curved corridors, ramps, and staircases.
**/
void SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double headingYawDeg, const double minSeparation, const double agentRadius, const std::vector<Vector3DP> *guidePath ) {
	/**
	*	Sanity checks: ensure slots array and navmesh are valid.
	**/
	if ( slots.empty() || g_nav_faces.empty() ) {
		return;
	}

	const double minSepSqr = minSeparation * minSeparation;
	s_acceptedPositions.clear();
	s_acceptedPositions.reserve( slots.size() );

	// Calculate forward unit vector along formation heading and reverse step direction.
	const double headingRad = headingYawDeg * QM_DEG2RAD;
	const Vector3DP fwdDir = { std::cos( headingRad ), std::sin( headingRad ), 0.0 };
	const Vector3DP rightDir = { fwdDir.y, -fwdDir.x, 0.0 };

	/**
	*	Phase 1: Accept existing valid slots that maintain mutual separation from already-accepted slots.
	**/
	for ( svg_crowd_slot_t &slot : slots ) {
		if ( !slot.isNavmeshValid ) {
			continue;
		}

		bool collides = false;
		for ( const Vector3DP &acceptedPos : s_acceptedPositions ) {
			if ( QM_Vector3DistanceSqrDP( slot.worldPosition, acceptedPos ) < minSepSqr ) {
				collides = true;
				break;
			}
		}

		if ( !collides ) {
			s_acceptedPositions.push_back( slot.worldPosition );
		} else {
			slot.isNavmeshValid = false;
		}
	}

	/**
	*	Phase 1B: Adaptive Interior Room Packing.
	*	For slots that collided or were placed off-mesh (e.g. against walls in an enclosed bunker),
	*	search concentric candidate rings at increasing radii inside the room (with unobstructed
	*	geometric line-of-sight to anchorOrigin). This packs all members inside the destination room
	*	without spilling into doorways or exterior corridors.
	**/
	static constexpr double INTERIOR_PACKING_RADII[] = { 36.0, 48.0, 64.0, 80.0, 96.0, 112.0, 128.0 };
	for ( svg_crowd_slot_t &slot : slots ) {
		if ( slot.isNavmeshValid ) {
			continue;
		}

		bool foundInterior = false;
		for ( const double r : INTERIOR_PACKING_RADII ) {
			if ( foundInterior ) {
				break;
			}
			for ( int32_t a = 0; a < CROWD_INTERIOR_PACKING_ANGLES; a++ ) {
				const double ang = static_cast<double>( a ) * CROWD_INTERIOR_PACKING_ANGLE_STEP;
				Vector3DP candPos = anchorOrigin + Vector3DP{ r * std::cos( ang ), r * std::sin( ang ), 0.0 };

				int32_t polyIdx = Nav_FindFaceInLeafStrict( candPos );
				if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
					Vector3DP feetPos = candPos;
					feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
					polyIdx = Nav_FindFaceInLeafStrict( feetPos );
				}

				if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
					const nav_face_t &face = g_nav_faces[ polyIdx ];
					if ( std::fabs( face.normal.z ) > 0.001 ) {
						const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
						const double d = QM_Vector3DotProductDP( v0, face.normal );
						candPos.z = ( d - ( candPos.x * face.normal.x + candPos.y * face.normal.y ) ) / face.normal.z;
					}

					if ( std::fabs( candPos.z - anchorOrigin.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
						continue;
					}

					// Verify geometric line-of-sight from anchorOrigin to candidate (must be in same room)
					if ( !Nav_HasGeometricLineOfSight2D( anchorOrigin, candPos, agentRadius ) ) {
						continue;
					}

					// Physical world trace check
					const Vector3 trStart = QM_Vector3FromDP( anchorOrigin + Vector3DP{ 0.0, 0.0, CROWD_SLOT_TRACE_CLEARANCE_OFFSET_Z } );
					const Vector3 trEnd = QM_Vector3FromDP( candPos + Vector3DP{ 0.0, 0.0, CROWD_SLOT_TRACE_CLEARANCE_OFFSET_Z } );
					const svg_trace_t tr = SVG_Trace( trStart, vec3_origin, vec3_origin, trEnd, nullptr, CM_CONTENTMASK_SOLID );
					if ( tr.fraction < CROWD_SLOT_TRACE_FRACTION_THRESHOLD ) {
						continue;
					}

					// Check separation against accepted positions
					bool collides = false;
					for ( const Vector3DP &acc : s_acceptedPositions ) {
						if ( QM_Vector3DistanceSqrDP( candPos, acc ) < minSepSqr ) {
							collides = true;
							break;
						}
					}

					if ( !collides ) {
						slot.worldPosition = candPos;
						slot.localOffset = candPos - anchorOrigin;
						slot.isNavmeshValid = true;
						s_acceptedPositions.push_back( candPos );
						foundInterior = true;
						break;
					}
				}
			}
		}
	}

	/**
	*	Phase 2: If the destination room is physically packed to capacity, step remaining
	*	excess slots backwards along the reverse heading axis or guide path into sequential ranks.
	**/
	int32_t columnRank = 1;
	for ( svg_crowd_slot_t &slot : slots ) {
		if ( slot.isNavmeshValid ) {
			continue;
		}

		bool foundValidPlacement = false;

		while ( columnRank < CROWD_MAX_COLUMN_SEARCH_RANKS && !foundValidPlacement ) {
			const double backOffset = static_cast<double>( columnRank ) * minSeparation;

			Vector3DP sampleBase = anchorOrigin - ( fwdDir * backOffset );
			Vector3DP localFwd = fwdDir;
			Vector3DP localRight = rightDir;

			if ( guidePath != nullptr && guidePath->size() >= 2 ) {
				Vector3DP pathPos = {};
				Vector3DP pathTangent = {};
				if ( SVG_Crowd_SampleGuidePathInReverse( *guidePath, backOffset, &pathPos, &pathTangent ) ) {
					sampleBase = pathPos;
					localFwd = pathTangent;
					localRight = Vector3DP{ localFwd.y, -localFwd.x, 0.0 };
				}
			}

			// Test center of column, then slightly left, then slightly right:
			for ( const double lateralSign : { 0.0, 1.0, -1.0 } ) {
				const double lateralOffset = lateralSign * ( minSeparation * CROWD_COLUMN_LATERAL_OFFSET_RATIO );
				Vector3DP candPos = sampleBase + ( localRight * lateralOffset );

				int32_t polyIdx = Nav_FindFaceInLeafStrict( candPos );
				if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
					Vector3DP feetPos = candPos;
					feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
					polyIdx = Nav_FindFaceInLeafStrict( feetPos );
				}

				if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
					const nav_face_t &face = g_nav_faces[ polyIdx ];

					// Do not place parking slots directly inside narrow doorway thresholds or chokepoints:
					if ( face.clearance > 0.0 && face.clearance < ( CROWD_DEFAULT_AGENT_RADIUS * 1.75 ) ) {
						continue;
					}

					if ( std::fabs( face.normal.z ) > 0.001 ) {
						const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
						const double d = QM_Vector3DotProductDP( v0, face.normal );
						candPos.z = ( d - ( candPos.x * face.normal.x + candPos.y * face.normal.y ) ) / face.normal.z;
					}

					// Verify vertical elevation sanity relative to sample base.
					if ( std::fabs( candPos.z - sampleBase.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
						continue;
					}

					// Verify geometric clearance from sample base to candidate.
					if ( !Nav_HasGeometricLineOfSight2D( sampleBase, candPos, agentRadius ) ) {
						continue;
					}

					// Verify physical trace check.
					const Vector3 trStart = QM_Vector3FromDP( sampleBase + Vector3DP{ 0.0, 0.0, CROWD_SLOT_TRACE_CLEARANCE_OFFSET_Z } );
					const Vector3 trEnd = QM_Vector3FromDP( candPos + Vector3DP{ 0.0, 0.0, CROWD_SLOT_TRACE_CLEARANCE_OFFSET_Z } );
					const svg_trace_t tr = SVG_Trace( trStart, vec3_origin, vec3_origin, trEnd, nullptr, CM_CONTENTMASK_SOLID );
					if ( tr.fraction < CROWD_SLOT_TRACE_FRACTION_THRESHOLD ) {
						continue;
					}

					// Verify separation against all already-accepted slot positions.
					bool collidesWithAccepted = false;
					for ( const Vector3DP &acc : s_acceptedPositions ) {
						if ( QM_Vector3DistanceSqrDP( candPos, acc ) < minSepSqr ) {
							collidesWithAccepted = true;
							break;
						}
					}

					if ( !collidesWithAccepted ) {
						slot.worldPosition = candPos;
						slot.localOffset = candPos - anchorOrigin;
						slot.isNavmeshValid = true;
						s_acceptedPositions.push_back( candPos );
						foundValidPlacement = true;
						break;
					}
				}
			}

			columnRank++;
		}

		// Fallback if corridor is extremely constrained: clamp to the last valid accepted position.
		if ( !foundValidPlacement ) {
			const Vector3DP fallbackAnchor = s_acceptedPositions.empty() ? anchorOrigin : s_acceptedPositions.back();
			slot.worldPosition = fallbackAnchor;
			slot.localOffset = slot.worldPosition - anchorOrigin;
			slot.isNavmeshValid = true;
		}
	}
}

/**
*	@brief		Sort formation slots so that slots deepest along the ingress vector are indexed first.
*	@details	Enforces that the earliest-arriving agents navigate to the back of an enclosed area
*				(bunker, room, corridor) so they never block subsequent incoming agents.
*	@param	slots			[in/out] Formation slots to order by ingress depth.
*	@param	destOrigin		Destination center origin in Vector3DP.
*	@param	ingressDir		Normalized approach direction vector in Vector3DP (from squad towards destination).
**/
void SVG_Crowd_SortSlotsByIngressDepth( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &destOrigin, const Vector3DP &ingressDir ) {
	if ( slots.size() <= 1 ) {
		return;
	}

	// Compute projection along ingressDir for each slot:
	// Slots with large positive projection are deepest into the room (farthest from entrance).
	// Slots with negative/small projection are near the entrance.
	std::sort( slots.begin(), slots.end(), [&]( const svg_crowd_slot_t &a, const svg_crowd_slot_t &b ) {
		const Vector3DP deltaA = a.worldPosition - destOrigin;
		const Vector3DP deltaB = b.worldPosition - destOrigin;
		const double depthA = QM_Vector3DotProductDP( deltaA, ingressDir );
		const double depthB = QM_Vector3DotProductDP( deltaB, ingressDir );
		return depthA > depthB; // Deepest first
	} );

	// Reassign contiguous slotIndex values.
	for ( size_t i = 0; i < slots.size(); i++ ) {
		slots[ i ].slotIndex = static_cast<int32_t>( i );
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

	/**
	*	2-Opt anti-crossover refinement pass:
	*	Repeatedly swap any pair of slot assignments where the swapped Euclidean distance sum
	*	is strictly less than the current distance sum, eliminating all trajectory crossings.
	**/
	bool improved = true;
	int32_t iter = 0;
	static constexpr int32_t MAX_2OPT_INITIAL_ITERS = 32;

	while ( improved && iter < MAX_2OPT_INITIAL_ITERS ) {
		improved = false;
		iter++;

		for ( size_t i = 0; i < count; i++ ) {
			const int32_t slotI = outMemberToSlotMap[ i ];
			if ( slotI < 0 || slotI >= static_cast<int32_t>( slots.size() ) ) {
				continue;
			}

			for ( size_t j = i + 1; j < count; j++ ) {
				const int32_t slotJ = outMemberToSlotMap[ j ];
				if ( slotJ < 0 || slotJ >= static_cast<int32_t>( slots.size() ) ) {
					continue;
				}

				const double currDistSq = QM_Vector3DistanceSqrDP( memberOrigins[ i ], slots[ slotI ].worldPosition ) +
				                          QM_Vector3DistanceSqrDP( memberOrigins[ j ], slots[ slotJ ].worldPosition );
				const double swapDistSq = QM_Vector3DistanceSqrDP( memberOrigins[ i ], slots[ slotJ ].worldPosition ) +
				                          QM_Vector3DistanceSqrDP( memberOrigins[ j ], slots[ slotI ].worldPosition );

				if ( swapDistSq + 0.001 < currDistSq ) {
					std::swap( outMemberToSlotMap[ i ], outMemberToSlotMap[ j ] );
					improved = true;
				}
			}
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

	/**
	*	2-Opt anti-crossover refinement pass with hysteresis:
	*	Repeatedly swap any pair of slot assignments where the swapped Euclidean distance sum
	*	is strictly less than the current distance sum, eliminating all trajectory crossings.
	**/
	bool improved = true;
	int32_t iter = 0;
	static constexpr int32_t MAX_2OPT_HYST_ITERS = 32;

	while ( improved && iter < MAX_2OPT_HYST_ITERS ) {
		improved = false;
		iter++;

		for ( size_t i = 0; i < count; i++ ) {
			const int32_t slotI = outMemberToSlotMap[ i ];
			if ( slotI < 0 || slotI >= static_cast<int32_t>( slots.size() ) ) {
				continue;
			}

			for ( size_t j = i + 1; j < count; j++ ) {
				const int32_t slotJ = outMemberToSlotMap[ j ];
				if ( slotJ < 0 || slotJ >= static_cast<int32_t>( slots.size() ) ) {
					continue;
				}

				const double currDistSq = QM_Vector3DistanceSqrDP( memberOrigins[ i ], slots[ slotI ].worldPosition ) +
				                          QM_Vector3DistanceSqrDP( memberOrigins[ j ], slots[ slotJ ].worldPosition );
				const double swapDistSq = QM_Vector3DistanceSqrDP( memberOrigins[ i ], slots[ slotJ ].worldPosition ) +
				                          QM_Vector3DistanceSqrDP( memberOrigins[ j ], slots[ slotI ].worldPosition );

				if ( swapDistSq + 0.001 < currDistSq ) {
					std::swap( outMemberToSlotMap[ i ], outMemberToSlotMap[ j ] );
					improved = true;
				}
			}
		}
	}
}

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
**/
void SVG_Crowd_AssignMembersToSlotsIngress( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, const std::vector<int32_t> &previousSlotMap, std::vector<int32_t> &outMemberToSlotMap, const Vector3DP &destOrigin, const Vector3DP &ingressDir, const double hysteresisDist ) {
	const size_t count = memberOrigins.size();
	outMemberToSlotMap.clear();
	outMemberToSlotMap.resize( count, -1 );

	if ( count == 0 || slots.empty() ) {
		return;
	}

	// If ingress direction is negligible, fallback to standard hysteresis assignment.
	const double ingressLen = QM_Vector3LengthDP( ingressDir );
	if ( ingressLen < 0.001 ) {
		SVG_Crowd_AssignMembersToSlotsHysteresis( memberOrigins, slots, previousSlotMap, outMemberToSlotMap, hysteresisDist );
		return;
	}

	const Vector3DP fwdNorm = ingressDir * ( 1.0 / ingressLen );
	const Vector3DP rightNorm{ fwdNorm.y, -fwdNorm.x, 0.0 };

	// Calculate squad centroid.
	Vector3DP centroid = { 0.0, 0.0, 0.0 };
	for ( const Vector3DP &pos : memberOrigins ) {
		centroid = centroid + pos;
	}
	centroid = centroid * ( 1.0 / static_cast<double>( count ) );

	// 1. Rank members by longitudinal progress along fwdNorm towards destination.
	// Members most advanced (closest to destination / entering room first) have highest progress.
	s_rankedMembers.clear();
	s_rankedMembers.reserve( count );

	for ( size_t m = 0; m < count; m++ ) {
		const Vector3DP delta = memberOrigins[ m ] - centroid;
		const double prog = QM_Vector3DotProductDP( delta, fwdNorm );
		const double lat = QM_Vector3DotProductDP( delta, rightNorm );
		s_rankedMembers.push_back( svg_crowd_member_rank_t{ static_cast<int32_t>( m ), prog, lat } );
	}

	// Sort members descending by progress (leader / front members first):
	std::sort( s_rankedMembers.begin(), s_rankedMembers.end(), []( const svg_crowd_member_rank_t &a, const svg_crowd_member_rank_t &b ) {
		return a.progress > b.progress;
	} );

	// 2. Rank slots by longitudinal depth along fwdNorm.
	// Deepest slots in the room/formation have highest depth.
	s_rankedSlots.clear();
	s_rankedSlots.reserve( slots.size() );

	for ( size_t s = 0; s < slots.size(); s++ ) {
		const Vector3DP delta = slots[ s ].worldPosition - destOrigin;
		const double depth = QM_Vector3DotProductDP( delta, fwdNorm );
		const double lat = QM_Vector3DotProductDP( delta, rightNorm );
		s_rankedSlots.push_back( svg_crowd_slot_rank_t{ static_cast<int32_t>( s ), depth, lat } );
	}

	// Sort slots descending by depth (deepest first):
	std::sort( s_rankedSlots.begin(), s_rankedSlots.end(), []( const svg_crowd_slot_rank_t &a, const svg_crowd_slot_rank_t &b ) {
		return a.depth > b.depth;
	} );

	// 3. Greedy assignment matching member ranks to slot ranks:
	// Leader / forward members get deepest slots; trailing members get entrance slots.
	// For members in the same progress tier, pair matching lateral sides (left to left, right to right).
	s_slotClaimed.clear();
	s_slotClaimed.resize( slots.size(), false );

	for ( size_t r = 0; r < count; r++ ) {
		const int32_t mIdx = s_rankedMembers[ r ].memberIdx;
		const double mLat = s_rankedMembers[ r ].lateral;

		// Select the best available slot among candidate depth ranks.
		// Prefer the slot closest in rank that matches lateral side:
		int32_t bestSlotIdx = -1;
		double bestScore = std::numeric_limits<double>::max();

		// Check window of candidate slots around rank r
		const size_t startRank = ( r >= CROWD_INGRESS_RANK_WINDOW ) ? ( r - CROWD_INGRESS_RANK_WINDOW ) : 0;
		const size_t endRank = std::min( slots.size(), r + CROWD_INGRESS_RANK_WINDOW + 1 );

		for ( size_t sr = startRank; sr < endRank; sr++ ) {
			const int32_t candSlot = s_rankedSlots[ sr ].slotIdx;
			if ( s_slotClaimed[ candSlot ] ) {
				continue;
			}

			const double rankDiff = std::fabs( static_cast<double>( sr ) - static_cast<double>( r ) );
			const double latDiff = std::fabs( s_rankedSlots[ sr ].lateral - mLat );
			const double score = ( rankDiff * CROWD_INGRESS_RANK_WEIGHT ) + latDiff;

			if ( score < bestScore ) {
				bestScore = score;
				bestSlotIdx = candSlot;
			}
		}

		// Fallback to first unclaimed slot in s_rankedSlots if window was exhausted
		if ( bestSlotIdx < 0 ) {
			for ( size_t sr = 0; sr < s_rankedSlots.size(); sr++ ) {
				const int32_t candSlot = s_rankedSlots[ sr ].slotIdx;
				if ( !s_slotClaimed[ candSlot ] ) {
					bestSlotIdx = candSlot;
					break;
				}
			}
		}

		if ( bestSlotIdx >= 0 ) {
			s_slotClaimed[ bestSlotIdx ] = true;
			outMemberToSlotMap[ mIdx ] = bestSlotIdx;
		}
	}

	// 4. Ingress-preserving 2-Opt refinement:
	// Allow swaps that reduce distance ONLY if they do not invert ingress depth monotonicity!
	bool improved = true;
	int32_t iter = 0;
	while ( improved && iter < CROWD_MAX_INGRESS_2OPT_ITERS ) {
		improved = false;
		iter++;

		for ( size_t i = 0; i < count; i++ ) {
			const int32_t slotI = outMemberToSlotMap[ i ];
			if ( slotI < 0 || slotI >= static_cast<int32_t>( slots.size() ) ) {
				continue;
			}

			for ( size_t j = i + 1; j < count; j++ ) {
				const int32_t slotJ = outMemberToSlotMap[ j ];
				if ( slotJ < 0 || slotJ >= static_cast<int32_t>( slots.size() ) ) {
					continue;
				}

				// Measure ingress progress of members i and j:
				const double progI = QM_Vector3DotProductDP( memberOrigins[ i ] - centroid, fwdNorm );
				const double progJ = QM_Vector3DotProductDP( memberOrigins[ j ] - centroid, fwdNorm );

				// Measure slot depth of slotI and slotJ:
				const double depthI = QM_Vector3DotProductDP( slots[ slotI ].worldPosition - destOrigin, fwdNorm );
				const double depthJ = QM_Vector3DotProductDP( slots[ slotJ ].worldPosition - destOrigin, fwdNorm );

				// Ingress monotonicity constraint:
				// If member i is clearly ahead of member j, slot for i must be at least as deep as slot for j.
				// After swap, member i receives slotJ (depthJ) and member j receives slotI (depthI).
				// A swap is invalid if it would give the trailing member a deeper slot than the leading member.
				if ( progI > ( progJ + CROWD_INGRESS_ORDER_TOLERANCE ) && depthJ < ( depthI - CROWD_INGRESS_ORDER_TOLERANCE ) ) {
					continue;
				}
				if ( progJ > ( progI + CROWD_INGRESS_ORDER_TOLERANCE ) && depthI < ( depthJ - CROWD_INGRESS_ORDER_TOLERANCE ) ) {
					continue;
				}

				const double currDistSq = QM_Vector3DistanceSqrDP( memberOrigins[ i ], slots[ slotI ].worldPosition ) +
				                          QM_Vector3DistanceSqrDP( memberOrigins[ j ], slots[ slotJ ].worldPosition );
				const double swapDistSq = QM_Vector3DistanceSqrDP( memberOrigins[ i ], slots[ slotJ ].worldPosition ) +
				                          QM_Vector3DistanceSqrDP( memberOrigins[ j ], slots[ slotI ].worldPosition );

				if ( swapDistSq + 0.001 < currDistSq ) {
					std::swap( outMemberToSlotMap[ i ], outMemberToSlotMap[ j ] );
					improved = true;
				}
			}
		}
	}
}


