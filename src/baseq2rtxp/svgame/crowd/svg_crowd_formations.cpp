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
#include "svgame/svg_local.h"
#include "svgame/crowd/svg_crowd_formations.h"
#include "svgame/nav/nav_path.h"
#include "svgame/nav/nav_types.h"
#include "shared/math/qm_math_cpp.h"
#include "shared/math/qm_vector3_dp.h"
#include "svgame/nav/nav_generate.h"
#include "svgame/entities/svg_base_edict.h"
#include "svgame/monsters/svg_mmove.h"
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
			// Face outward radially from circle center (360-degree outward defense)
			slot.relativeYawDeg = QM_AngleMod( angle * ( 180.0 / QM_PI ) );
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
	const double spacing = ( params.lateralSpacing > 0.0 ) ? params.lateralSpacing : ( ( params.longitudinalSpacing > 0.0 ) ? params.longitudinalSpacing : CROWD_DEFAULT_LATERAL_SPACING );
	// Dynamic circumference radius based on squad member count: perimeter = N * spacing => radius = (N * spacing) / (2 * PI)
	const double dynamicRadius = ( static_cast<double>( memberCount ) * spacing ) / ( 2.0 * QM_PI );
	const double radius = ( params.minCoverDistance > 0.0 ) ? params.minCoverDistance : std::max( CROWD_PERIMETER_MIN_RADIUS, dynamicRadius );
	const double angleStep = ( 2.0 * QM_PI ) / static_cast<double>( memberCount );

	for ( size_t i = 0; i < memberCount; i++ ) {
		const double angle = static_cast<double>( i ) * angleStep;
		const double offsetX = radius * std::cos( angle );
		const double offsetY = radius * std::sin( angle );

		svg_crowd_slot_t slot;
		slot.slotIndex = static_cast<int32_t>( i );
		slot.localOffset = Vector3DP{ offsetX, offsetY, 0.0 };
		// Map circle roles: front is Leader, flanks at +/-90, rear at 180:
		if ( i == 0 ) {
			slot.role = crowd_member_role_t::ROLE_LEADER;
		} else {
			const double deg = QM_AngleMod( angle * ( 180.0 / QM_PI ) );
			if ( deg >= CROWD_ROLE_LEFT_MIN_DEG && deg < CROWD_ROLE_LEFT_MAX_DEG ) {
				slot.role = crowd_member_role_t::ROLE_FLANK_LEFT;
			} else if ( deg >= CROWD_ROLE_RIGHT_MIN_DEG && deg < CROWD_ROLE_RIGHT_MAX_DEG ) {
				slot.role = crowd_member_role_t::ROLE_FLANK_RIGHT;
			} else if ( deg >= CROWD_ROLE_REAR_MIN_DEG && deg <= CROWD_ROLE_REAR_MAX_DEG ) {
				slot.role = crowd_member_role_t::ROLE_REAR_GUARD;
			} else {
				slot.role = crowd_member_role_t::ROLE_CENTER;
			}
		}
		// Face inward toward the encircled target
		slot.relativeYawDeg = QM_AngleMod( ( angle * ( 180.0 / QM_PI ) ) + CROWD_PERIMETER_INWARD_FACE_OFFSET_DEG );
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

	// Fallback to high-performance lateral 2D half-edge raycasting:
	nav_raycast_result_t resLeft = {};
	nav_raycast_result_t resRight = {};
	Nav_RaycastHalfEdge2D( worldOrigin, Vector3DP{ 0.0, 1.0, 0.0 }, desiredWidth * 0.5, &resLeft );
	Nav_RaycastHalfEdge2D( worldOrigin, Vector3DP{ 0.0, -1.0, 0.0 }, desiredWidth * 0.5, &resRight );
	const double measuredWidth = resLeft.hitDistance + resRight.hitDistance;
	if ( measuredWidth > 0.0 ) {
		return std::max( CROWD_DEFAULT_MIN_CORRIDOR_SPACING, std::min( desiredWidth, measuredWidth ) );
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

			// Reject slots on faces/brushes flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
			if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
				slot.isNavmeshValid = false;
				continue;
			}

			if ( std::fabs( face.normal.z ) > 0.001 ) {
				const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
				const double d = QM_Vector3DotProductDP( v0, face.normal );
				slot.worldPosition.z = ( d - ( slot.worldPosition.x * face.normal.x + slot.worldPosition.y * face.normal.y ) ) / face.normal.z;
			}

			// Project Z onto face plane.

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

			// Verify physical static hull clearance dynamically derived from agent bounding box dimensions:
			//! Lateral wall margin scale factor applied to dynamic agent radius for static clearance probe.
			static constexpr double CROWD_SNAP_CLEARANCE_RADIUS_SCALE = 0.85;
			//! Minimum absolute radius in world units for static clearance probe.
			static constexpr double CROWD_SNAP_CLEARANCE_MIN_RADIUS = 6.0;
			const float traceRadius = static_cast<float>( std::max( CROWD_SNAP_CLEARANCE_MIN_RADIUS, agentRadius * CROWD_SNAP_CLEARANCE_RADIUS_SCALE ) );

			//! Total nominal height in world units of a standard agent standup bounding box.
			static constexpr double CROWD_AGENT_STANDUP_TOTAL_HEIGHT = 72.0;
			//! Floor margin elevation in world units to prevent floor-plane contact false positives.
			static constexpr double CROWD_STATIC_CLEARANCE_FLOOR_ELEVATION = 4.0;
			//! Half-height ratio of agent standup bounds used for clearance trace probe.
			static constexpr double CROWD_STATIC_CLEARANCE_HEIGHT_SCALE = 0.90;
			const float halfProbeHeight = static_cast<float>( ( CROWD_AGENT_STANDUP_TOTAL_HEIGHT * 0.5 ) * CROWD_STATIC_CLEARANCE_HEIGHT_SCALE );
			const double probeCenterElevZ = ( CROWD_AGENT_STANDUP_TOTAL_HEIGHT * 0.5 ) + CROWD_STATIC_CLEARANCE_FLOOR_ELEVATION;

			const Vector3 probeMins{ -traceRadius, -traceRadius, -halfProbeHeight };
			const Vector3 probeMaxs{ traceRadius, traceRadius, halfProbeHeight };
			Vector3DP probeCenter = slot.worldPosition;
			probeCenter.z += probeCenterElevZ;
			// Use a tiny sweep to avoid the start==end shape position-test path, which can
			// conservatively inflate cylinder Z extents and reject valid room slots.
			Vector3DP probeEnd = probeCenter;
			probeEnd.z += 0.01;
			const svg_trace_t tr = SVG_MMove_Trace( probeCenter, probeMins, probeMaxs, probeEnd, nullptr, CM_CONTENTMASK_SOLID, MM_SHAPE_CYLINDER );
			if ( tr.startsolid || tr.allsolid ) {
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
bool SVG_Crowd_SampleGuidePathInReverse( const std::vector<Vector3DP> &path, const double targetDistBack, Vector3DP *outPos, Vector3DP *outTangent ) {
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

//! Static generation-stamp array and current generation counter for O(1) instantaneous BFS visited checks.
static std::vector<uint32_t> s_bfsFaceVisitGen;
static uint32_t s_bfsCurrentVisitGen = 0;

/**
*	@brief	Compute dynamic inflow keep-out radius from physical portal geometry and navmesh clearance.
*	@param	portalOrigin	World-space origin of the passage portal waypoint.
*	@param	minSeparation	Minimum agent separation distance.
*	@param	agentRadius		Dynamic agent collision radius in world units.
*	@return	Dynamic keep-out radius in world units (0.0 if portal is on open terrain).
**/
static inline double SVG_Crowd_ComputeDynamicPortalKeepOutRadius( const Vector3DP *portalOrigin, const double minSeparation, const double agentRadius = CROWD_DEFAULT_AGENT_RADIUS ) {
	if ( portalOrigin == nullptr ) {
		return 0.0;
	}

	int32_t polyIdx = Nav_FindFaceInLeafStrict( *portalOrigin );
	if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
		Vector3DP feetPos = *portalOrigin;
		feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		polyIdx = Nav_FindFaceInLeafStrict( feetPos );
	}

	if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
		const nav_face_t &face = g_nav_faces[ polyIdx ];
		if ( face.clearance > 0.0 ) {
			const double portalWidth = face.clearance * 2.0;
			// If the passage opening is a constrained bottleneck (e.g. doorway, narrow corridor passage):
			if ( portalWidth < CROWD_PORTAL_BOTTLENECK_MAX_WIDTH ) {
				const double effectiveRadius = ( agentRadius > 0.0 ) ? agentRadius : CROWD_DEFAULT_AGENT_RADIUS;
				const double minKeepOut = effectiveRadius + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN;
				const double maxKeepOut = ( effectiveRadius * 2.0 ) + CROWD_PORTAL_KEEPOUT_AGENT_MARGIN;
				const double dynamicRadius = std::max( portalWidth * CROWD_PORTAL_KEEPOUT_SCALE, minSeparation * CROWD_PORTAL_SEPARATION_KEEPOUT_SCALE );
				return std::clamp( dynamicRadius, minKeepOut, maxKeepOut );
			}
		}
	}

	// On open terrain or wide openings where no bottleneck constriction exists, no keep-out is applied:
	return 0.0;
}

/**
*	@brief	Perform bounded O(1) topological BFS flood-fill across adjacent NavMesh faces to pack slots into an enclosed room.
*	@note	In the topological NavMesh room-packing algorithm, a BFS Hop (Breadth-First Search Hop) is one step across a
*			shared boundary edge from a walkable polygon to its directly adjacent neighbor.
**/
bool SVG_Crowd_FloodFillRoomPackingO1( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double minPackingSepSqr, const double agentRadius, std::vector<Vector3DP> &acceptedPositions, const double maxRoomRadius, const Vector3DP *portalOrigin, const Vector3DP *ingressDir ) {
	if ( slots.empty() || g_nav_faces.empty() ) {
		return false;
	}

	// 1. Localize anchor face in O(1) via leaf link or feet offset
	int32_t startPoly = Nav_FindFaceInLeafStrict( anchorOrigin );
	if ( startPoly < 0 || startPoly >= static_cast<int32_t>( g_nav_faces.size() ) ) {
		Vector3DP feetPos = anchorOrigin;
		feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
		startPoly = Nav_FindFaceInLeafStrict( feetPos );
	}
	if ( startPoly < 0 || startPoly >= static_cast<int32_t>( g_nav_faces.size() ) ) {
		startPoly = Nav_FindClosestFaceInLeaf( anchorOrigin );
	}
	if ( startPoly < 0 || startPoly >= static_cast<int32_t>( g_nav_faces.size() ) ) {
		return false;
	}

	// Advance O(1) generation stamp to invalidate stale visited markers in 1 instruction without memset:
	if ( s_bfsFaceVisitGen.size() < g_nav_faces.size() ) {
		s_bfsFaceVisitGen.resize( g_nav_faces.size(), 0 );
	}
	s_bfsCurrentVisitGen++;
	if ( s_bfsCurrentVisitGen == 0 ) {
		// Handle 32-bit integer overflow (once every 4.29 billion queries)
		std::fill( s_bfsFaceVisitGen.begin(), s_bfsFaceVisitGen.end(), 0 );
		s_bfsCurrentVisitGen = 1;
	}

	//! Maximum number of connected room faces collected during topological BFS room exploration.
	static constexpr int32_t MAX_BFS_ROOM_FACES = 256;
	//! Maximum boundary edge traversal hops allowed during room flood fill.
	static constexpr int32_t MAX_BFS_HOPS = 32;
	//! Fraction of agent radius used for doorway/chokepoint line-of-sight clearance checks.
	static constexpr double CROWD_ROOM_PACKING_LOS_RADIUS_FRACTION = 0.20;
	//! Half-offset grid centering scale.
	static constexpr double CROWD_ROOM_PACKING_GRID_HALF_OFFSET_SCALE = 0.5;
	//! Minimum normal Z magnitude epsilon required to perform vertical plane projection.
	static constexpr double CROWD_ROOM_PACKING_NORMAL_Z_EPSILON = 0.001;
	//! Minimum number of edges required to form a valid polygon face.
	static constexpr int32_t CROWD_ROOM_PACKING_MIN_FACE_EDGES = 3;

	const double effectiveRadius = ( agentRadius > 0.0 ) ? agentRadius : CROWD_DEFAULT_AGENT_RADIUS;
	const double minPackingSep = std::sqrt( minPackingSepSqr );
	const double dynamicPortalKeepOutDist = SVG_Crowd_ComputeDynamicPortalKeepOutRadius( portalOrigin, minPackingSep, effectiveRadius );
	const double dynamicPortalKeepOutDistSq = dynamicPortalKeepOutDist * dynamicPortalKeepOutDist;

	int32_t faceQueue[ MAX_BFS_ROOM_FACES ] = {};
	int32_t hopQueue[ MAX_BFS_ROOM_FACES ] = {};
	int32_t collectedFaces[ MAX_BFS_ROOM_FACES ] = {};
	int32_t collectedFaceCount = 0;
	int32_t qHead = 0;
	int32_t qTail = 0;

	faceQueue[ qTail ] = startPoly;
	hopQueue[ qTail ] = 0;
	qTail++;
	collectedFaces[ collectedFaceCount++ ] = startPoly;
	s_bfsFaceVisitGen[ startPoly ] = s_bfsCurrentVisitGen;

	while ( qHead < qTail && collectedFaceCount < MAX_BFS_ROOM_FACES ) {
		const int32_t currFaceIdx = faceQueue[ qHead ];
		const int32_t currHops = hopQueue[ qHead ];
		qHead++;

		if ( currHops >= MAX_BFS_HOPS ) {
			continue;
		}

		const nav_face_t &currFace = g_nav_faces[ currFaceIdx ];
		for ( int32_t e = 0; e < currFace.num_edges; e++ ) {
			const nav_halfedge_t &he = g_nav_halfedges[ currFace.first_edge_idx + e ];
			if ( he.twin_idx == -1 || ( he.flags & NAV_EDGE_DISABLED ) != 0 ) {
				continue; // Solid boundary or closed door
			}
			const int32_t twinFaceIdx = g_nav_halfedges[ he.twin_idx ].face_idx;
			if ( twinFaceIdx < 0 || twinFaceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				continue;
			}

			// O(1) instantaneous generation-stamp visited check (1 CPU instruction):
			if ( s_bfsFaceVisitGen[ twinFaceIdx ] == s_bfsCurrentVisitGen ) {
				continue;
			}

			const nav_face_t &twinFace = g_nav_faces[ twinFaceIdx ];
			// Ensure twin face is walkable ground and traversable step
			if ( twinFace.normal.z < NAV_MIN_WALKABLE_Z || std::fabs( he.z_diff ) > static_cast<double>( NAV_MAX_STEP_HEIGHT ) ) {
				continue;
			}

			// Reject faces on brushes or surfaces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
			if ( ( twinFace.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
				continue;
			}

			// Calculate centroid of candidate twin face:
			const Vector3DP twinV0 = g_nav_vertices[ g_nav_halfedges[ twinFace.first_edge_idx ].vertex_idx ];
			Vector3DP twinCentroid{ 0.0, 0.0, 0.0 };
			for ( int32_t te = 0; te < twinFace.num_edges; te++ ) {
				twinCentroid += g_nav_vertices[ g_nav_halfedges[ twinFace.first_edge_idx + te ].vertex_idx ];
			}
			twinCentroid *= ( 1.0 / static_cast<double>( twinFace.num_edges ) );

			// Strictly constrain BFS expansion within the room's maximum physical radial boundary:
			if ( QM_Vector3Distance2DDP( twinCentroid, anchorOrigin ) > maxRoomRadius ) {
				continue;
			}

			s_bfsFaceVisitGen[ twinFaceIdx ] = s_bfsCurrentVisitGen;
			collectedFaces[ collectedFaceCount++ ] = twinFaceIdx;
			if ( qTail < MAX_BFS_ROOM_FACES ) {
				faceQueue[ qTail ] = twinFaceIdx;
				hopQueue[ qTail ] = currHops + 1;
				qTail++;
			}
		}
	}

	size_t invalidSlotIdx = 0;
	while ( invalidSlotIdx < slots.size() && slots[ invalidSlotIdx ].isNavmeshValid ) {
		invalidSlotIdx++;
	}
	if ( invalidSlotIdx >= slots.size() ) {
		return true; // All slots already valid
	}

	// 3. Compute aggregate 2D bounding box across all collected room faces:
	Vector3DP roomMins{ 999999.0, 999999.0, 999999.0 };
	Vector3DP roomMaxs{ -999999.0, -999999.0, -999999.0 };
	for ( int32_t f = 0; f < collectedFaceCount; f++ ) {
		const nav_face_t &face = g_nav_faces[ collectedFaces[ f ] ];
		for ( int32_t e = 0; e < face.num_edges; e++ ) {
			const Vector3DP &v = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx + e ].vertex_idx ];
			roomMins.x = std::min( roomMins.x, v.x );
			roomMins.y = std::min( roomMins.y, v.y );
			roomMins.z = std::min( roomMins.z, v.z );
			roomMaxs.x = std::max( roomMaxs.x, v.x );
			roomMaxs.y = std::max( roomMaxs.y, v.y );
			roomMaxs.z = std::max( roomMaxs.z, v.z );
		}
	}

	// Sample dense 2D cellular grid covering the entire room floor evenly (fine sub-step with Poisson-disc mutual spacing):
	//! Scale factor proportional to dynamic agent radius for grid step resolution.
	static constexpr double CROWD_ROOM_PACKING_GRID_STEP_SCALE = 0.75;
	//! Minimum absolute cellular grid sampling step in world units.
	static constexpr double CROWD_ROOM_PACKING_MIN_GRID_STEP = 10.0;
	const double gridStep = std::max( CROWD_ROOM_PACKING_MIN_GRID_STEP, effectiveRadius * CROWD_ROOM_PACKING_GRID_STEP_SCALE );

	// Multi-pass packing: Pass 1 uses nominal separation; Pass 2 uses tight separation to fill room corners:
	const double separationPasses[] = { minPackingSepSqr, minPackingSepSqr * 0.85 };

	for ( const double currentSepSqr : separationPasses ) {
		if ( invalidSlotIdx >= slots.size() ) {
			break;
		}

		for ( double gx = roomMins.x + ( gridStep * CROWD_ROOM_PACKING_GRID_HALF_OFFSET_SCALE ); gx <= roomMaxs.x && invalidSlotIdx < slots.size(); gx += gridStep ) {
			for ( double gy = roomMins.y + ( gridStep * CROWD_ROOM_PACKING_GRID_HALF_OFFSET_SCALE ); gy <= roomMaxs.y && invalidSlotIdx < slots.size(); gy += gridStep ) {
				Vector3DP candPos{ gx, gy, anchorOrigin.z };

				if ( QM_Vector3Distance2DDP( candPos, anchorOrigin ) > maxRoomRadius ) {
					continue;
				}
				// Keep the doorway portal aperture clear so incoming agents have an unobstructed ingress path:
				const double doorwayKeepOutRadius = effectiveRadius;
				if ( portalOrigin != nullptr && QM_Vector3Distance2DSqrDP( candPos, *portalOrigin ) < ( doorwayKeepOutRadius * doorwayKeepOutRadius ) ) {
					continue;
				}

				// Directly match candidate against collected room floor polygons:
				int32_t matchedFaceIdx = -1;
				for ( int32_t cf = 0; cf < collectedFaceCount; cf++ ) {
					const int32_t fIdx = collectedFaces[ cf ];
					if ( fIdx >= 0 && fIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
						const nav_face_t &face = g_nav_faces[ fIdx ];
						if ( Nav_PointInsideFace2D( candPos, face ) ) {
							matchedFaceIdx = fIdx;
							break;
						}
					}
				}
				if ( matchedFaceIdx < 0 ) {
					continue;
				}

				const nav_face_t &face = g_nav_faces[ matchedFaceIdx ];
				if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
					continue;
				}

				if ( std::fabs( face.normal.z ) > CROWD_ROOM_PACKING_NORMAL_Z_EPSILON ) {
					const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
					const double planeD = QM_Vector3DotProductDP( v0, face.normal );
					candPos.z = ( planeD - ( candPos.x * face.normal.x + candPos.y * face.normal.y ) ) / face.normal.z;
				}

				if ( std::fabs( candPos.z - anchorOrigin.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
					continue;
				}

				// Verify physical static hull clearance dynamically derived from agent bounding box dimensions:
				//! Lateral wall margin scale factor applied to dynamic agent radius for static clearance probe.
				static constexpr double CROWD_STATIC_CLEARANCE_RADIUS_SCALE = 0.85;
				//! Minimum absolute radius in world units for static clearance probe.
				static constexpr double CROWD_STATIC_CLEARANCE_MIN_RADIUS = 6.0;
				const float traceRadius = static_cast<float>( std::max( CROWD_STATIC_CLEARANCE_MIN_RADIUS, effectiveRadius * CROWD_STATIC_CLEARANCE_RADIUS_SCALE ) );

				//! Total nominal height in world units of a standard agent standup bounding box.
				static constexpr double CROWD_AGENT_STANDUP_TOTAL_HEIGHT = 72.0;
				//! Floor margin elevation in world units to prevent floor-plane contact false positives.
				static constexpr double CROWD_STATIC_CLEARANCE_FLOOR_ELEVATION = 4.0;
				//! Half-height ratio of agent standup bounds used for clearance trace probe.
				static constexpr double CROWD_STATIC_CLEARANCE_HEIGHT_SCALE = 0.90;
				const float halfProbeHeight = static_cast<float>( ( CROWD_AGENT_STANDUP_TOTAL_HEIGHT * 0.5 ) * CROWD_STATIC_CLEARANCE_HEIGHT_SCALE );
				const double probeCenterElevZ = ( CROWD_AGENT_STANDUP_TOTAL_HEIGHT * 0.5 ) + CROWD_STATIC_CLEARANCE_FLOOR_ELEVATION;

				const Vector3 probeMins{ -traceRadius, -traceRadius, -halfProbeHeight };
				const Vector3 probeMaxs{ traceRadius, traceRadius, halfProbeHeight };
				Vector3DP probeCenter = candPos;
				probeCenter.z += probeCenterElevZ;
				// Use a tiny sweep to avoid the start==end shape position-test path, which can
				// conservatively inflate cylinder Z extents and reject valid room slots.
				Vector3DP probeEnd = probeCenter;
				probeEnd.z += 0.01;
				const svg_trace_t tr = SVG_MMove_Trace( probeCenter, probeMins, probeMaxs, probeEnd, nullptr, CM_CONTENTMASK_SOLID, MM_SHAPE_CYLINDER );
				if ( tr.startsolid || tr.allsolid ) {
					continue;
				}

				// Verify mutual physical separation against all accepted positions:
				bool collides = false;
				for ( const Vector3DP &acc : acceptedPositions ) {
					if ( QM_Vector3DistanceSqrDP( candPos, acc ) < currentSepSqr ) {
						collides = true;
						break;
					}
				}

				if ( !collides ) {
					slots[ invalidSlotIdx ].worldPosition = candPos;
					slots[ invalidSlotIdx ].localOffset = candPos - anchorOrigin;
					slots[ invalidSlotIdx ].isNavmeshValid = true;
					const double angleRad = std::atan2( candPos.y - anchorOrigin.y, candPos.x - anchorOrigin.x );
					slots[ invalidSlotIdx ].relativeYawDeg = QM_AngleMod( angleRad * ( 180.0 / QM_PI ) );
					acceptedPositions.push_back( candPos );

					while ( invalidSlotIdx < slots.size() && slots[ invalidSlotIdx ].isNavmeshValid ) {
						invalidSlotIdx++;
					}
				}
			}
		}
	}

	// Sort slots so that deepest positions in the room receive lowest slot indices (Slot 0 = back of room):
	if ( portalOrigin != nullptr ) {
		std::sort( slots.begin(), slots.end(), [portalOrigin]( const svg_crowd_slot_t &a, const svg_crowd_slot_t &b ) {
			if ( a.isNavmeshValid != b.isNavmeshValid ) {
				return a.isNavmeshValid > b.isNavmeshValid;
			}
			const double distA = QM_Vector3Distance2DSqrDP( a.worldPosition, *portalOrigin );
			const double distB = QM_Vector3Distance2DSqrDP( b.worldPosition, *portalOrigin );
			return distA > distB; // Deepest in room first
		} );
		for ( size_t i = 0; i < slots.size(); i++ ) {
			slots[ i ].slotIndex = static_cast<int32_t>( i );
		}
	}

	return ( invalidSlotIdx >= slots.size() );
}

/**
*
*
*
*	Polygon-Based Room Contour Encirclement (File-Local Helpers):
*	Plots encirclement slots directly from the room's actual walkable polygons
*	instead of generating a geometric circle and repairing it with raycasts.
*
*
**/
namespace {

/**
*	@brief	One inward-eroded half-plane edge of a convex room face (2D, XY plane).
**/
struct crowd_eroded_edge_t {
	//! Unit inward (toward face interior) 2D normal in XY.
	Vector3DP inwardNormal = {};
	//! Plane offset such that inwardNormal � p >= d defines the inside half-space.
	double d = 0.0;
};

/**
*	@brief	Convex room face eroded inward by the agent radius, used for exact
*			point-in-walkable-region containment and Z plane projection.
**/
struct crowd_eroded_face_t {
	//! Eroded inward half-plane edges (convex; point is inside iff inside ALL of them).
	std::vector<crowd_eroded_edge_t> edges;
	//! Face plane normal for Z projection.
	Vector3DP planeNormal = {};
	//! Face plane offset (planeNormal � v = planeD for any vertex v of the face).
	double planeD = 0.0;
};

/**
*	@brief	Unified encirclement contour: either a circle or a rounded rectangle
*			(axis-aligned) fitted inside the eroded room bounds.
**/
struct crowd_room_contour_t {
	//! True when using the rounded-rectangle representation, false for a circle.
	bool isRect = false;
	//! Contour center (room centroid with anchor Z).
	Vector3DP center = {};
	//! Circle radius (isRect == false).
	double radius = 0.0;
	//! Rounded-rectangle half extents along X and Y (isRect == true).
	double halfX = 0.0;
	double halfY = 0.0;
	//! Rounded-rectangle corner arc radius (isRect == true).
	double cornerR = 0.0;
};

/**
*	@brief	Angular keep-out sector (radians, wrapped to [0, 2*QM_PI)) carved around
*			a portal direction from the room centroid to keep the doorway lane clear.
**/
struct crowd_keepout_sector_t {
	//! Sector start angle in radians [0, 2*PI).
	double start = 0.0;
	//! Sector end angle in radians [0, 2*PI).
	double end = 0.0;
};

/**
*	@brief	Test whether a nav room provides a portal-bounded walkable polygon set
*			suitable for analytical (eroded-face) formation plotting.
*	@details	The eroded-face grid fill only requires a closed walkable polygon union
*				bounded by solid walls and portal apertures. Enclosed rooms, alcoves and
*				flat/ramp corridors all satisfy this invariant; only unbounded open
*				exterior zones (no portals) do not.
*	@param	room	Room record to classify (may be nullptr).
*	@return	True when the room is portal-bounded and analytically fillable.
**/
static inline bool CrowdRoom_IsAnalyticallyFillable( const nav_room_t *room ) {
	// Sanity: reject null rooms outright.
	if ( room == nullptr ) {
		return false;
	}
	// A portal-bounded room always owns at least one transition portal; open spaces do not.
	if ( room->portal_indices.empty() ) {
		return false;
	}
	// Accept every zone subtype that owns a closed polygon union: enclosed rooms, alcoves,
	// and corridor passages (flat / ramp). Stair corridors change elevation mid-zone and are
	// intentionally excluded so their anchor height-band checks stay conservative.
	switch ( room->zone_type ) {
		case ZONE_TYPE_ROOM_ENCLOSED:
		case ZONE_TYPE_ALCOVE:
		case ZONE_TYPE_CORRIDOR_FLAT:
		case ZONE_TYPE_CORRIDOR_RAMP:
			return true;
		default:
			return false;
	}
}

/**
*	@brief	Resolve the portal-bounded room / corridor containing (or nearest to) the anchor.
*	@param	anchorOrigin	Formation anchor in world space.
*	@return	Pointer to the enclosing or nearest analytically fillable nav_room_t, or nullptr if none found.
**/
static const nav_room_t *CrowdRoom_ResolveEnclosedRoom( const Vector3DP &anchorOrigin ) {
	// Direct containment query first (authoritative).
	const nav_room_t *room = Nav_GetRoomForPoint( anchorOrigin );
	if ( CrowdRoom_IsAnalyticallyFillable( room ) ) {
		return room;
	}
	// Fallback: nearest portal's bounded side within a generous radius.
	double bestDistSq = 160.0 * 160.0;
	const nav_room_t *best = nullptr;
	for ( const nav_portal_t &portal : g_nav_portals ) {
		const double dSq = QM_Vector3Distance2DSqrDP( anchorOrigin, portal.center );
		if ( dSq >= bestDistSq ) {
			continue;
		}
		const int32_t rTo = portal.to_room_id;
		const int32_t rFrom = portal.from_room_id;
		if ( rTo >= 0 && rTo < static_cast<int32_t>( g_nav_rooms.size() ) &&
			 CrowdRoom_IsAnalyticallyFillable( &g_nav_rooms[ rTo ] ) ) {
			best = &g_nav_rooms[ rTo ];
			bestDistSq = dSq;
		} else if ( rFrom >= 0 && rFrom < static_cast<int32_t>( g_nav_rooms.size() ) &&
					CrowdRoom_IsAnalyticallyFillable( &g_nav_rooms[ rFrom ] ) ) {
			best = &g_nav_rooms[ rFrom ];
			bestDistSq = dSq;
		}
	}
	return best;
}

/**
*	@brief	Build inward-eroded convex face records for every face of a room.
*	@details	Each nav face is convex; shifting every edge inward by `erode` along
*				its in-plane inward normal yields the exact walkable region shrunk
*				by the agent radius. Overlapping faces naturally union via the
*				"inside at least one face" containment test.
*	@param	room		Room whose faces will be eroded.
*	@param	erode		Inward erosion distance (agentRadius + wall margin).
*	@param	outFaces	[out] Eroded face records.
**/
static void CrowdRoom_BuildErodedFaces( const nav_room_t *room, const double erode, std::vector<crowd_eroded_face_t> &outFaces ) {
	outFaces.clear();
	if ( room == nullptr ) {
		return;
	}
	outFaces.reserve( room->face_indices.size() );
	for ( const int32_t faceIdx : room->face_indices ) {
		if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			continue;
		}
		const nav_face_t &face = g_nav_faces[ faceIdx ];
		// Skip non-walkable / flagged faces entirely.
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			continue;
		}
		// Require a mostly-upward plane for sane Z projection.
		if ( std::fabs( face.normal.z ) < 0.5 ) {
			continue;
		}

		crowd_eroded_face_t eroded;
		eroded.planeNormal = face.normal;
		eroded.edges.reserve( face.num_edges );

		// Walk the face's half-edge loop. Only TRUE boundary/solid edges contribute an
		// eroded half-plane; interior edges shared with adjacent walkable faces must NOT
		// be eroded, otherwise the usable room region collapses to almost nothing and the
		// encirclement ring implodes toward the centroid.
		int32_t edgeIdx = face.first_edge_idx;
		for ( int32_t e = 0; e < face.num_edges && edgeIdx >= 0; e++ ) {
			const nav_halfedge_t &he = g_nav_halfedges[ edgeIdx ];
			const int32_t nextIdx = he.next_idx;
			if ( nextIdx < 0 || nextIdx >= static_cast<int32_t>( g_nav_halfedges.size() ) ) {
				break;
			}
			const nav_halfedge_t &heNext = g_nav_halfedges[ nextIdx ];
			if ( he.vertex_idx < 0 || he.vertex_idx >= static_cast<int32_t>( g_nav_vertices.size() ) ||
				 heNext.vertex_idx < 0 || heNext.vertex_idx >= static_cast<int32_t>( g_nav_vertices.size() ) ) {
				edgeIdx = nextIdx;
				continue;
			}

			// Classify the edge exactly like the navmesh does: an edge is a solid boundary
			// when it has no twin (outer map boundary / interior obstacle wall such as a
			// pillar), is disabled, or steps too far vertically to its twin to be
			// traversable. An edge is ALSO treated as solid when its twin face is not a
			// member of this room (interior obstacle islands are not assigned to the room,
			// so their boundary edges twin to faces with a different room_id or -1).
			// Only edges between two walkable faces of the SAME room are open transitions.
			bool isSolidBoundary = ( he.twin_idx == -1 ) ||
				( ( he.flags & NAV_EDGE_DISABLED ) != 0 ) ||
				( std::fabs( he.z_diff ) > NAV_SOLID_EDGE_MIN_Z_DIFF );
			if ( !isSolidBoundary ) {
				if ( he.twin_idx >= 0 && he.twin_idx < static_cast<int32_t>( g_nav_halfedges.size() ) ) {
					const nav_halfedge_t &twin = g_nav_halfedges[ he.twin_idx ];
					const bool twinInSameRoom = ( twin.face_idx >= 0 && twin.face_idx < static_cast<int32_t>( g_nav_faces.size() ) &&
						g_nav_faces[ twin.face_idx ].room_id == face.room_id );
					if ( !twinInSameRoom ) {
						isSolidBoundary = true;
					}
				} else {
					isSolidBoundary = true;
				}
			}
			const Vector3DP &v0 = g_nav_vertices[ he.vertex_idx ];
			const Vector3DP &v1 = g_nav_vertices[ heNext.vertex_idx ];

			// 2D edge direction (v0 -> v1).
			const double ex = v1.x - v0.x;
			const double ey = v1.y - v0.y;
			const double elen = std::sqrt( ex * ex + ey * ey );
			if ( elen > 0.001 ) {
				// Candidate inward normals are the two perpendiculars; pick the one
				// pointing toward the face centroid (guaranteed inward for convex faces).
				const double nx0 = -ey / elen;
				const double ny0 = ex / elen;
				const double toCx = face.center.x - ( ( v0.x + v1.x ) * 0.5 );
				const double toCy = face.center.y - ( ( v0.y + v1.y ) * 0.5 );
				const double sign = ( ( nx0 * toCx + ny0 * toCy ) >= 0.0 ) ? 1.0 : -1.0;

				crowd_eroded_edge_t edge;
				edge.inwardNormal = Vector3DP{ nx0 * sign, ny0 * sign, 0.0 };
				// Keep EVERY edge as a containment half-plane so the union of faces tiles
				// the room exactly; only solid boundary edges receive the inward erosion
				// offset, interior edges keep the exact polygon line (erode = 0).
				const double edgeErode = isSolidBoundary ? erode : 0.0;
				edge.d = ( edge.inwardNormal.x * v0.x + edge.inwardNormal.y * v0.y ) + edgeErode;
				eroded.edges.push_back( edge );
			}
			edgeIdx = nextIdx;
		}

		// Plane offset from the first vertex for Z projection.
		if ( face.first_edge_idx >= 0 && face.first_edge_idx < static_cast<int32_t>( g_nav_halfedges.size() ) ) {
			const nav_halfedge_t &he0 = g_nav_halfedges[ face.first_edge_idx ];
			if ( he0.vertex_idx >= 0 && he0.vertex_idx < static_cast<int32_t>( g_nav_vertices.size() ) ) {
				eroded.planeD = QM_Vector3DotProductDP( g_nav_vertices[ he0.vertex_idx ], face.normal );
			}
		}

		if ( !eroded.edges.empty() ) {
			outFaces.push_back( std::move( eroded ) );
		}
	}
}

/**
*	@brief	Test whether a 2D point lies inside at least one eroded room face,
*			and if so project it onto that face's plane to obtain the floor Z.
*	@param	erodedFaces	Eroded room faces from CrowdRoom_BuildErodedFaces.
*	@param	p			Candidate position (XY used for containment).
*	@param	outZ		[out] Projected floor Z at p when contained.
*	@return	True when p is strictly inside the eroded walkable region.
**/
static bool CrowdRoom_PointInEroded( const std::vector<crowd_eroded_face_t> &erodedFaces, const Vector3DP &p, double *outZ ) {
	for ( const crowd_eroded_face_t &face : erodedFaces ) {
		bool inside = true;
		for ( const crowd_eroded_edge_t &edge : face.edges ) {
			// Point must satisfy inwardNormal � p >= d for every edge (convex intersection).
			if ( ( edge.inwardNormal.x * p.x + edge.inwardNormal.y * p.y ) < edge.d ) {
				inside = false;
				break;
			}
		}
		if ( inside ) {
			if ( outZ != nullptr ) {
				// Project onto the face plane: n�(x,y,z) = d  =>  z = (d - nx*x - ny*y) / nz.
				const double denom = face.planeNormal.z;
				*outZ = ( std::fabs( denom ) > 0.001 )
					? ( ( face.planeD - ( face.planeNormal.x * p.x + face.planeNormal.y * p.y ) ) / denom )
					: p.z;
			}
			return true;
		}
	}
	return false;
}

/**
*	@brief	Compute the maximum inscribed radius at a point against the eroded faces.
*	@param	erodedFaces	Eroded room faces.
*	@param	p			Query point (typically the room centroid).
*	@return	Minimum distance from p to any eroded edge across all faces containing p,
*			or 0 when p is not inside the eroded region.
**/
static double CrowdRoom_InscribedRadius( const std::vector<crowd_eroded_face_t> &erodedFaces, const Vector3DP &p ) {
	double best = std::numeric_limits<double>::max();
	bool any = false;
	for ( const crowd_eroded_face_t &face : erodedFaces ) {
		bool inside = true;
		double minDist = std::numeric_limits<double>::max();
		for ( const crowd_eroded_edge_t &edge : face.edges ) {
			const double side = ( edge.inwardNormal.x * p.x + edge.inwardNormal.y * p.y ) - edge.d;
			if ( side < 0.0 ) {
				inside = false;
				break;
			}
			if ( side < minDist ) {
				minDist = side;
			}
		}
		if ( inside ) {
			any = true;
			if ( minDist < best ) {
				best = minDist;
			}
		}
	}
	return any ? best : 0.0;
}

/**
*	@brief	Fit the encirclement contour into the room using the eroded region and bounds.
*	@details	Round-ish rooms get a circle of radius min(desired, inscribed).
*				Rectangular rooms (aspect above threshold) get an axis-aligned rounded
*				rectangle inset into the room bounds, which fills long rooms without
*				corner clipping.
*	@param	room				Resolved room.
*	@param	erodedFaces			Eroded faces (for inscribed radius measurement).
*	@param	anchorZ				Anchor floor elevation used for the contour center.
*	@param	erode				Erosion distance used to inset the room bounds.
*	@param	desiredRingRadius	Requested ring radius from squad size/spacing.
*	@param	outContour			[out] Fitted contour descriptor.
**/
static void CrowdRoom_BuildContour( const nav_room_t *room, const std::vector<crowd_eroded_face_t> &erodedFaces, const double anchorZ, const double erode, const double desiredRingRadius, crowd_room_contour_t &outContour ) {
	outContour = crowd_room_contour_t{};
	outContour.center = Vector3DP{ room->centroid.x, room->centroid.y, anchorZ };

	// Inscribed radius from the centroid against eroded edges is the authoritative upper bound.
	const double inscribed = CrowdRoom_InscribedRadius( erodedFaces, outContour.center );
	const double maxRadius = ( inscribed > 0.0 ) ? inscribed : desiredRingRadius;

	// Measure the usable half-extents along +X/-X/+Y/-Y from the centroid against the
	// eroded edges directly (NOT the raw room AABB, which may include wall thickness or
	// the doorway band). Axis-aligned eroded edges give exact distances; non-axis edges
	// are handled conservatively via their perpendicular distance.
	double extPosX = maxRadius, extNegX = maxRadius, extPosY = maxRadius, extNegY = maxRadius;
	for ( const crowd_eroded_face_t &face : erodedFaces ) {
		for ( const crowd_eroded_edge_t &edge : face.edges ) {
			const double nx = edge.inwardNormal.x;
			const double ny = edge.inwardNormal.y;
			// Perpendicular distance from the centroid to this eroded edge line.
			const double side = ( nx * outContour.center.x + ny * outContour.center.y ) - edge.d;
			if ( side < 0.0 ) {
				continue; // Centroid is outside this face's half-plane; not a containing face.
			}
			// The wall lies opposite the inward normal direction.
			if ( nx < -0.9 && side < extPosX ) extPosX = side;
			if ( nx >  0.9 && side < extNegX ) extNegX = side;
			if ( ny < -0.9 && side < extPosY ) extPosY = side;
			if ( ny >  0.9 && side < extNegY ) extNegY = side;
		}
	}
	const double sizeX = std::max( 0.0, extPosX + extNegX );
	const double sizeY = std::max( 0.0, extPosY + extNegY );
	const double minSize = std::min( sizeX, sizeY );
	const double maxSize = std::max( sizeX, sizeY );
	const double aspect = ( minSize > 0.001 ) ? ( maxSize / minSize ) : 1.0;

	if ( aspect > CROWD_ROOM_RECTANGULAR_ASPECT && sizeX > 1.0 && sizeY > 1.0 ) {
		// Rounded rectangle fitted to the measured eroded extents (symmetric about centroid).
		outContour.isRect = true;
		outContour.halfX = std::min( extPosX, extNegX );
		outContour.halfY = std::min( extPosY, extNegY );
		const double cornerCap = std::min( outContour.halfX, outContour.halfY );
		outContour.cornerR = std::max( CROWD_ROOM_CONTOUR_MIN_CORNER_RADIUS, cornerCap * 0.5 );
		if ( outContour.cornerR > cornerCap ) {
			outContour.cornerR = cornerCap;
		}
	} else {
		// Circle: clamp the requested radius into the inscribed room size.
		outContour.isRect = false;
		outContour.radius = std::clamp( desiredRingRadius, CROWD_ROOM_CONTOUR_MIN_RING_RADIUS, std::max( CROWD_ROOM_CONTOUR_MIN_RING_RADIUS, maxRadius ) );
	}
}

/**
*	@brief	Compute the total arc length of a contour.
**/
static double CrowdRoom_ContourLength( const crowd_room_contour_t &c ) {
	if ( !c.isRect ) {
		return 2.0 * QM_PI * c.radius;
	}
	const double straightX = 2.0 * std::max( 0.0, ( c.halfX - c.cornerR ) ) * 2.0;
	const double straightY = 2.0 * std::max( 0.0, ( c.halfY - c.cornerR ) ) * 2.0;
	return straightX + straightY + ( 2.0 * QM_PI * c.cornerR );
}

/**
*	@brief	Sample a contour by arc length, returning the 2D position and outward yaw.
*	@param	c			Contour descriptor.
*	@param	s			Arc length parameter (wrapped into [0, length)).
*	@param	outPos		[out] 2D contour position (Z = contour center Z).
*	@param	outYawDeg	[out] Outward-facing yaw in degrees (radial or wall-normal).
**/
static void CrowdRoom_ContourSample( const crowd_room_contour_t &c, double s, Vector3DP *outPos, double *outYawDeg ) {
	const double len = CrowdRoom_ContourLength( c );
	if ( len > 0.001 ) {
		s = std::fmod( s, len );
		if ( s < 0.0 ) {
			s += len;
		}
	} else {
		s = 0.0;
	}

	if ( !c.isRect ) {
		// Circle: arc length directly maps to angle.
		const double angle = ( c.radius > 0.001 ) ? ( s / c.radius ) : 0.0;
		outPos->x = c.center.x + ( std::cos( angle ) * c.radius );
		outPos->y = c.center.y + ( std::sin( angle ) * c.radius );
		outPos->z = c.center.z;
		*outYawDeg = QM_AngleMod( angle * ( 180.0 / QM_PI ) );
		return;
	}

	// Rounded rectangle: walk right run, top-right corner, top run, top-left corner,
	// left run, bottom-left corner, bottom run, bottom-right corner (counter-clockwise).
	const double runX = std::max( 0.0, ( c.halfX - c.cornerR ) ) * 2.0;
	const double runY = std::max( 0.0, ( c.halfY - c.cornerR ) ) * 2.0;
	const double cornerArc = 0.5 * QM_PI * c.cornerR;
	const double rx = c.halfX - c.cornerR;
	const double ry = c.halfY - c.cornerR;

	struct seg_t { double len; };
	const seg_t segs[ 8 ] = { { runX }, { cornerArc }, { runY }, { cornerArc }, { runX }, { cornerArc }, { runY }, { cornerArc } };

	int32_t seg = 0;
	double remain = s;
	while ( seg < 8 && remain > segs[ seg ].len ) {
		remain -= segs[ seg ].len;
		seg++;
	}
	if ( seg >= 8 ) {
		seg = 0;
		remain = 0.0;
	}

	auto cornerPoint = [ & ]( const double cx, const double cy, const double baseAngleRad, const double t ) {
		const double a = baseAngleRad + ( t * 0.5 * QM_PI );
		outPos->x = cx + ( std::cos( a ) * c.cornerR );
		outPos->y = cy + ( std::sin( a ) * c.cornerR );
		outPos->z = c.center.z;
		*outYawDeg = QM_AngleMod( a * ( 180.0 / QM_PI ) );
	};

	switch ( seg ) {
	case 0: // bottom run, left -> right, outward -Y
		outPos->x = c.center.x - rx + remain;
		outPos->y = c.center.y - c.halfY;
		outPos->z = c.center.z;
		*outYawDeg = -90.0;
		break;
	case 1: // bottom-right corner
		cornerPoint( c.center.x + rx, c.center.y - ry, -0.5 * QM_PI, ( cornerArc > 0.001 ) ? ( remain / cornerArc ) : 0.0 );
		break;
	case 2: // right run, bottom -> top, outward +X
		outPos->x = c.center.x + c.halfX;
		outPos->y = c.center.y - ry + remain;
		outPos->z = c.center.z;
		*outYawDeg = 0.0;
		break;
	case 3: // top-right corner
		cornerPoint( c.center.x + rx, c.center.y + ry, 0.0, ( cornerArc > 0.001 ) ? ( remain / cornerArc ) : 0.0 );
		break;
	case 4: // top run, right -> left, outward +Y
		outPos->x = c.center.x + rx - remain;
		outPos->y = c.center.y + c.halfY;
		outPos->z = c.center.z;
		*outYawDeg = 90.0;
		break;
	case 5: // top-left corner
		cornerPoint( c.center.x - rx, c.center.y + ry, 0.5 * QM_PI, ( cornerArc > 0.001 ) ? ( remain / cornerArc ) : 0.0 );
		break;
	case 6: // left run, top -> bottom, outward -X
		outPos->x = c.center.x - c.halfX;
		outPos->y = c.center.y + ry - remain;
		outPos->z = c.center.z;
		*outYawDeg = 180.0;
		break;
	default: // bottom-left corner
		cornerPoint( c.center.x - rx, c.center.y - ry, QM_PI, ( cornerArc > 0.001 ) ? ( remain / cornerArc ) : 0.0 );
		break;
	}
}

/**
*	@brief	Build angular keep-out sectors around every passable portal of the room
*			so the encirclement never parks members inside a doorway lane.
*	@param	room			Resolved room.
*	@param	center			Contour center (centroid).
*	@param	ringRadius		Approximate contour radius used to convert portal width to an angle.
*	@param	agentRadius		Agent hull radius for lane margin.
*	@param	outSectors		[out] Angular sectors in radians.
**/
static void CrowdRoom_BuildPortalKeepOutSectors( const nav_room_t *room, const Vector3DP &center, const double ringRadius, const double agentRadius, std::vector<crowd_keepout_sector_t> &outSectors ) {
	outSectors.clear();
	if ( room == nullptr || ringRadius <= 1.0 ) {
		return;
	}
	for ( const int32_t pIdx : room->portal_indices ) {
		if ( pIdx < 0 || pIdx >= static_cast<int32_t>( g_nav_portals.size() ) ) {
			continue;
		}
		const nav_portal_t &portal = g_nav_portals[ pIdx ];
		// Closed/locked doors do not need a traffic lane.
		if ( !portal.is_passable ) {
			continue;
		}
		const double dx = portal.center.x - center.x;
		const double dy = portal.center.y - center.y;
		if ( ( dx * dx + dy * dy ) < 1.0 ) {
			continue;
		}
		const double portalAngle = std::atan2( dy, dx );
		// Keep-out spans the physical aperture half-width plus one lane margin only.
		// (Previously this also added a full agentRadius, which on small rings ballooned
		// the sector past +/-50 degrees and consumed most of the usable arc.)
		const double halfWidthWorld = ( portal.width * 0.5 ) + CROWD_ROOM_CONTOUR_PORTAL_LANE_MARGIN;
		const double halfAngle = std::asin( std::clamp( halfWidthWorld / ringRadius, 0.0, 1.0 ) );

		crowd_keepout_sector_t sec;
		sec.start = QM_AngleMod( ( portalAngle - halfAngle ) * ( 180.0 / QM_PI ) ) * ( QM_PI / 180.0 );
		sec.end = QM_AngleMod( ( portalAngle + halfAngle ) * ( 180.0 / QM_PI ) ) * ( QM_PI / 180.0 );
		outSectors.push_back( sec );
	}
}

/**
*	@brief	Test whether an angle (radians) lies inside any keep-out sector.
**/
static bool CrowdRoom_AngleInKeepOut( const double angleRad, const std::vector<crowd_keepout_sector_t> &sectors ) {
	const double a = QM_AngleMod( angleRad * ( 180.0 / QM_PI ) ) * ( QM_PI / 180.0 );
	for ( const crowd_keepout_sector_t &sec : sectors ) {
		if ( sec.start <= sec.end ) {
			if ( a >= sec.start && a <= sec.end ) {
				return true;
			}
		} else {
			// Wrapped sector across 0/2PI.
			if ( a >= sec.start || a <= sec.end ) {
				return true;
			}
		}
	}
	return false;
}

/**
*	@brief	Distance from a 2D point to a 2D segment (used for doorway lane carving).
**/
static double CrowdRoom_PointToSegmentDist2D( const Vector3DP &p, const Vector3DP &a, const Vector3DP &b ) {
	const double abx = b.x - a.x;
	const double aby = b.y - a.y;
	const double lenSq = ( abx * abx ) + ( aby * aby );
	double t = 0.0;
	if ( lenSq > 0.0001 ) {
		t = std::clamp( ( ( ( p.x - a.x ) * abx ) + ( ( p.y - a.y ) * aby ) ) / lenSq, 0.0, 1.0 );
	}
	const double cx = a.x + ( abx * t );
	const double cy = a.y + ( aby * t );
	const double dx = p.x - cx;
	const double dy = p.y - cy;
	return std::sqrt( ( dx * dx ) + ( dy * dy ) );
}

/**
*	@brief	Fill a room by area: rasterize the eroded walkable polygon union into a
*			spaced grid of slot positions, carve out the doorway traffic lane, and emit
*			the survivors ordered deepest-first (farthest from the primary portal).
*	@details	This is the correct primitive for small rooms: a contour ring's capacity
*				collapses with radius, whereas area fill scales with floor space. Each
*				candidate cell is validated with exact eroded containment, the anchor
*				height band, and a world-only occupancy hull trace (crowd members are
*				ignored so already-arrived agents never veto empty floor space).
*	@param	room			Resolved enclosed room / alcove.
*	@param	erodedFaces		Eroded room faces from CrowdRoom_BuildErodedFaces.
*	@param	anchorOrigin	Formation anchor (provides the reference floor Z).
*	@param	agentRadius		Agent hull radius.
*	@param	portalOrigin	Optional primary doorway portal (deepest-first ordering + lane).
*	@param	maxCount		Maximum number of cell positions to emit.
*	@param	outPositions	[out] Accepted cell centers, sorted deepest-first.
*	@return	True when at least one position was emitted.
**/
/**
*	@brief	Rasterize one spacing pass of the eroded room union into validated, lane-carved,
*			deepest-first cell centers.
*	@details	Shared inner loop of CrowdRoom_GridFillSlots so the fill can be retried at
*				progressively tighter spacing until every member has a seat.
*	@param	spacing			Grid cell pitch and greedy dedup minimum separation for this pass.
*	@param	room			Resolved room whose bounds are rasterized.
*	@param	erodedFaces		Eroded room faces for exact containment + floor Z.
*	@param	anchorOrigin	Formation anchor (reference floor Z and height band).
*	@param	agentRadius		Agent hull radius (occupancy probe half-width).
*	@param	portalOrigin	Optional primary doorway portal (deepest-first ordering + lane).
*	@param	alreadyAccepted	Positions accepted by earlier passes; new cells must clear them.
*	@param	maxCount		Maximum total positions to emit across all passes.
*	@param	outPositions	[out] Accepted cell centers (appended), sorted deepest-first per pass.
*	@return	Number of NEW positions accepted by this pass.
**/
static int32_t CrowdRoom_GridFillPass( const double spacing, const nav_room_t *room, const std::vector<crowd_eroded_face_t> &erodedFaces, const Vector3DP &anchorOrigin, const double agentRadius, const Vector3DP *portalOrigin, const std::vector<Vector3DP> &alreadyAccepted, const size_t maxCount, std::vector<Vector3DP> &outPositions ) {
	// Probe hull for world-only occupancy checks (static geometry only: CONTENTS_MONSTER
	// is excluded so crowd members standing in the room never veto empty floor cells).
	static constexpr double CROWD_AGENT_STANDUP_TOTAL_HEIGHT = 72.0;
	const float probeHalfWidth = static_cast<float>( agentRadius );
	const Vector3 probeMins{ -probeHalfWidth, -probeHalfWidth, 0.0f };
	const Vector3 probeMaxs{ probeHalfWidth, probeHalfWidth, static_cast<float>( CROWD_AGENT_STANDUP_TOTAL_HEIGHT ) };

	// Doorway throat corridor: portal center -> short interior clearance segment, kept free of plotted cells.
	const bool hasLane = ( portalOrigin != nullptr );
	const Vector3DP laneA = hasLane ? *portalOrigin : Vector3DP{};
	Vector3DP laneB = room->centroid;
	double portalHalfWidth = 0.0;
	if ( hasLane ) {
		double nearestPortalDistSq = std::numeric_limits<double>::max();
		// Resolve the finite aperture matching laneA so slot clearance includes the physical doorway span.
		for ( const int32_t portalIndex : room->portal_indices ) {
			if ( portalIndex < 0 || portalIndex >= static_cast<int32_t>( g_nav_portals.size() ) ) {
				continue;
			}
			const nav_portal_t &portal = g_nav_portals[ portalIndex ];
			const double portalDistSq = QM_Vector3Distance2DSqrDP( portal.center, laneA );
			if ( portalDistSq < nearestPortalDistSq ) {
				nearestPortalDistSq = portalDistSq;
				portalHalfWidth = portal.width * 0.5;
			}
		}

		/**
		* 	Carve only the physical doorway throat, not the entire portal-to-centroid
		* 	walkway. A full-length corridor carve removes the center of small rooms
		* 	from the packing domain, under-fills the room, and leaves exterior queue
		* 	slots masquerading as final circle seats.
		**/
		Vector3DP laneDir = room->centroid - laneA;
		laneDir.z = 0.0;
		const double laneDirLength = QM_Vector3LengthDP( laneDir );
		if ( laneDirLength > 0.001 ) {
			//! Bounded interior doorway throat depth kept clear for the active entrant's hull.
			const double throatDepth = std::max( agentRadius, CROWD_ROOM_FILL_DOOR_LANE_MARGIN );
			laneB = laneA + ( laneDir * ( throatDepth / laneDirLength ) );
		}
	}
	// Reserve the complete aperture plus one hull radius for unobstructed throughput.
	const double laneHalfWidth = hasLane
		? portalHalfWidth + agentRadius + ( CROWD_ROOM_FILL_DOOR_LANE_MARGIN * 0.25 )
		: 0.0;

	// Rasterize the eroded room bounds into spaced cell centers.
	const double minX = room->bounds.mins.x;
	const double minY = room->bounds.mins.y;
	const double maxX = room->bounds.maxs.x;
	const double maxY = room->bounds.maxs.y;
	static constexpr double CROWD_ROOM_FILL_PHASE_OFFSETS[][2] = {
		{ 0.50, 0.50 },
		{ 0.25, 0.25 },
		{ 0.25, 0.75 },
		{ 0.75, 0.25 },
		{ 0.75, 0.75 }
	};

	struct fill_cell_t {
		Vector3DP pos;
		double distToPortalSq;
	};
	std::vector<fill_cell_t> cells;

	for ( const double( &phase )[ 2 ] : CROWD_ROOM_FILL_PHASE_OFFSETS ) {
		// Offset lattices remove grid-phase aliasing where one unlucky origin misses the last physically valid seat.
		const double startX = minX + ( spacing * phase[ 0 ] );
		const double startY = minY + ( spacing * phase[ 1 ] );
		for ( double y = startY; y <= maxY; y += spacing ) {
			for ( double x = startX; x <= maxX; x += spacing ) {
				Vector3DP cell{ x, y, anchorOrigin.z };

				// Exact eroded containment keeps hulls off the walls; also yields floor Z.
				double floorZ = 0.0;
				if ( !CrowdRoom_PointInEroded( erodedFaces, cell, &floorZ ) ) {
					continue;
				}
				cell.z = floorZ;

				// Reject cells outside the anchor's traversable height band.
				if ( std::fabs( cell.z - anchorOrigin.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
					continue;
				}

				// Carve the doorway traffic lane so ingress/egress never gets parked shut.
				if ( hasLane && CrowdRoom_PointToSegmentDist2D( cell, laneA, laneB ) < laneHalfWidth ) {
					continue;
				}

				// World-only occupancy validation against static level geometry (crowd
				// members are ignored so arrived agents never veto empty floor cells).
				const Vector3DP probeStart{ cell.x, cell.y, cell.z + 1.0 };
				Vector3DP probeEnd = probeStart;
				probeEnd.z += 0.01;
				const svg_trace_t tr = SVG_MMove_Trace( probeStart, probeMins, probeMaxs, probeEnd, nullptr, CM_CONTENTMASK_SOLID, MM_SHAPE_CYLINDER );
				if ( tr.startsolid || tr.allsolid ) {
					continue;
				}

				const double dSq = hasLane ? QM_Vector3Distance2DSqrDP( cell, laneA ) : QM_Vector3Distance2DSqrDP( cell, room->centroid );
				cells.push_back( fill_cell_t{ cell, dSq } );
			}
		}
	}

	if ( cells.empty() ) {
		return 0;
	}

	// Deepest-first ordering: cells farthest from the doorway portal are emitted first
	// so the ingress-depth member->slot assignment streams leaders to the back wall.
	std::sort( cells.begin(), cells.end(), []( const fill_cell_t &a, const fill_cell_t &b ) {
		return a.distToPortalSq > b.distToPortalSq;
	} );

	// Greedy spacing dedup against this pass' cells AND every position already accepted
	// by earlier (coarser) passes, so tighter passes only ever ADD new floor area.
	const double minSepSqr = spacing * spacing;
	int32_t acceptedThisPass = 0;
	for ( const fill_cell_t &cell : cells ) {
		if ( alreadyAccepted.size() + outPositions.size() >= maxCount ) {
			break;
		}
		bool overlaps = false;
		for ( const Vector3DP &accepted : alreadyAccepted ) {
			if ( QM_Vector3Distance2DSqrDP( cell.pos, accepted ) < minSepSqr ) {
				overlaps = true;
				break;
			}
		}
		if ( !overlaps ) {
			for ( const Vector3DP &accepted : outPositions ) {
				if ( QM_Vector3Distance2DSqrDP( cell.pos, accepted ) < minSepSqr ) {
					overlaps = true;
					break;
				}
			}
		}
		if ( !overlaps ) {
			outPositions.push_back( cell.pos );
			acceptedThisPass++;
		}
	}
	return acceptedThisPass;
}

/**
*	@brief	Fill a room by area: rasterize the eroded walkable polygon union into a
*			spaced grid of slot positions, carve out the doorway traffic lane, and emit
*			the survivors ordered deepest-first (farthest from the primary portal).
*	@details	This is the correct primitive for small rooms: a contour ring's capacity
*				collapses with radius, whereas area fill scales with floor space. Runs at
*				nominal spacing first, then retries at progressively tighter compaction
*				spacings until every requested slot has a validated seat (bounded passes).
*	@param	room			Resolved enclosed room / alcove.
*	@param	erodedFaces		Eroded room faces from CrowdRoom_BuildErodedFaces.
*	@param	anchorOrigin	Formation anchor (provides the reference floor Z).
*	@param	agentRadius		Agent hull radius.
*	@param	portalOrigin	Optional primary doorway portal (deepest-first ordering + lane).
*	@param	maxCount		Maximum number of cell positions to emit.
*	@param	outPositions	[out] Accepted cell centers, sorted deepest-first.
*	@return	True when at least one position was emitted.
**/
static bool CrowdRoom_GridFillSlots( const nav_room_t *room, const std::vector<crowd_eroded_face_t> &erodedFaces, const Vector3DP &anchorOrigin, const double agentRadius, const Vector3DP *portalOrigin, const size_t maxCount, std::vector<Vector3DP> &outPositions ) {
	outPositions.clear();
	if ( room == nullptr || erodedFaces.empty() || maxCount == 0 ) {
		return false;
	}

	/**
	*	Compaction ladder: start at nominal spacing (hull diameter + buffer, which also
	*	stays outside the runtime mutual-separation trigger radius so arrived members never
	*	repel each other), then geometrically tighten toward the hard hull-diameter floor.
	*	Each pass only adds NEW floor area against everything accepted so far, so the fill
	*	densifies instead of shifting. This guarantees the room seats every member when the
	*	floor area physically allows it, instead of under-filling at nominal spacing.
	**/
	const double nominalSpacing = std::max( CROWD_ROOM_CONTOUR_MIN_ARC_SPACING, ( agentRadius * 2.0 ) + CROWD_ROOM_FILL_SPACING_BUFFER );
	//! Hard floor: never pack tighter than one hull diameter (zero overlap guarantee).
	const double minPhysicalSpacing = std::max( agentRadius * 2.0, 24.0 );
	//! Maximum number of compaction passes (bounded, allocation-light).
	static constexpr int32_t CROWD_ROOM_FILL_MAX_PASSES = 4;

	double spacing = nominalSpacing;
	std::vector<Vector3DP> passScratch;
	for ( int32_t pass = 0; pass < CROWD_ROOM_FILL_MAX_PASSES && outPositions.size() < maxCount; pass++ ) {
		passScratch.clear();
		CrowdRoom_GridFillPass( spacing, room, erodedFaces, anchorOrigin, agentRadius, portalOrigin, outPositions, maxCount, passScratch );
		// Append this pass' new deepest-first cells after existing (coarser, deeper) ones.
		for ( const Vector3DP &p : passScratch ) {
			if ( outPositions.size() >= maxCount ) {
				break;
			}
			outPositions.push_back( p );
		}
		// Geometric compaction toward the hull-diameter floor.
		spacing = std::max( minPhysicalSpacing, spacing * 0.75 );
	}
	return !outPositions.empty();
}

/**
*	@brief	Place overflow slots in a compressed queue just OUTSIDE the room's doorway,
*			stepping outward from the portal along the exit direction, each position
*			validated onto the navmesh. Keeps waiting members tight to the threshold
*			instead of parking them far back along the reverse guide path.
*	@param	portalOrigin	Doorway portal center.
*	@param	anchorOrigin	Room anchor (queue steps AWAY from this, through the portal).
*	@param	agentRadius		Agent hull radius.
*	@param	slots			[in/out] Slots; only isNavmeshValid == false entries are queued.
*	@return	Number of overflow slots successfully queued outside the doorway.
**/
static int32_t CrowdRoom_PlaceDoorwayOverflowQueue( const Vector3DP &portalOrigin, const Vector3DP &anchorOrigin, const double agentRadius, std::vector<svg_crowd_slot_t> &slots ) {
	// Exit direction: from the room anchor through the portal, continuing outward.
	Vector3DP exitDir = portalOrigin - anchorOrigin;
	exitDir.z = 0.0;
	if ( QM_Vector3LengthDP( exitDir ) < 0.001 ) {
		return 0;
	}
	exitDir = QM_Vector3NormalizeDP( exitDir );

	/**
	*	Chevron Queue Layout:
	*	A straight axial queue parks rank-1 members directly inside the ingress lane, bodily
	*	corking the aperture for the members assigned deep interior slots and producing a
	*	permanent mutual-yield deadlock at the doorway. Instead, fan overflow positions
	*	laterally off the portal corridor in alternating flanks (chevron pattern), so the
	*	lane between portal and room centroid stays physically clear for ingress traffic.
	**/
	const Vector3DP lateralDir{ -exitDir.y, exitDir.x, 0.0 };
	//! Minimum lateral offset keeping queued hulls fully clear of the ingress corridor.
	const double laneClearance = ( agentRadius * 2.0 ) + CROWD_ROOM_FILL_DOOR_LANE_MARGIN + agentRadius;

	int32_t queued = 0;
	const double step = std::max( CROWD_ROOM_OVERFLOW_QUEUE_STEP, agentRadius * 2.0 );
	for ( svg_crowd_slot_t &slot : slots ) {
		if ( slot.isNavmeshValid ) {
			continue;
		}
		bool placed = false;
		for ( int32_t rank = 1; rank <= CROWD_ROOM_OVERFLOW_QUEUE_MAX && !placed; rank++ ) {
			// Alternate flanks each rank: +lane, -lane, +lane... while stepping outward.
			const double sideSign = ( ( rank & 1 ) != 0 ) ? 1.0 : -1.0;
			const double flankOffset = laneClearance + ( step * 0.5 * static_cast<double>( rank / 2 ) );
			Vector3DP cand = portalOrigin
				+ ( exitDir * ( step * static_cast<double>( rank ) ) )
				+ ( lateralDir * ( sideSign * flankOffset ) );
			cand.z = anchorOrigin.z;

			// Snap to the navmesh face under the candidate (with a feet-level retry).
			int32_t faceIdx = Nav_FindFaceInLeafStrict( cand );
			if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				Vector3DP feet = cand;
				feet.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
				faceIdx = Nav_FindFaceInLeafStrict( feet );
			}
			if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				faceIdx = Nav_FindClosestFaceInLeaf( cand );
				if ( faceIdx >= 0 && faceIdx < static_cast<int32_t>( g_nav_faces.size() ) && !Nav_PointInsideFace2D( cand, g_nav_faces[ faceIdx ] ) ) {
					faceIdx = -1;
				}
			}
			if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				continue;
			}
			const nav_face_t &face = g_nav_faces[ faceIdx ];
			if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
				continue;
			}
			// Project onto the face plane for the true floor Z.
			if ( std::fabs( face.normal.z ) > 0.001 ) {
				const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
				const double d = QM_Vector3DotProductDP( v0, face.normal );
				cand.z = ( d - ( cand.x * face.normal.x + cand.y * face.normal.y ) ) / face.normal.z;
			}

			slot.worldPosition = cand;
			slot.localOffset = cand - anchorOrigin;
			slot.isNavmeshValid = true;
			slot.role = crowd_member_role_t::ROLE_CENTER;
			// Face back toward the doorway so queued members are ready to file in.
			slot.relativeYawDeg = QM_Vector3ToYawDP( -exitDir );
			placed = true;
			queued++;
		}
	}
	return queued;
}

} // namespace

/**
*	@brief	Adaptively scale and fit circular / perimeter defense rings inside enclosed room geometry.
*	@param	slots			[in/out] Formation slots to adaptively fit inside room walls.
*	@param	anchorOrigin	Center anchor origin in Vector3DP.
*	@param	agentRadius		Agent collision hull radius in world units.
*	@param	portalOrigin	Optional bottleneck portal origin for doorway aperture clearance.
*	@param	ingressDir		Optional ingress approach direction vector.
**/
void SVG_Crowd_FitCircularFormationToRoom( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double agentRadius, const Vector3DP *portalOrigin, const Vector3DP *ingressDir ) {
	if ( slots.empty() || g_nav_faces.empty() ) {
		return;
	}

	/**
	*	Polygon-Based Room Contour Path:
	*	When the destination anchor resolves to an enclosed room / alcove with a usable
	*	polygon set, plot the encirclement directly from the room's eroded walkable
	*	polygons (exact containment, analytical contour fit, portal keep-out lanes and
	*	step-probe validated relocation) instead of repairing a geometric circle with
	*	raycasts. Open-terrain / unclassified destinations fall through to the legacy
	*	ray-based fitting path below.
	**/
	const double contourAgentRadius = ( agentRadius > 0.0 ) ? agentRadius : CROWD_DEFAULT_AGENT_RADIUS;
	const nav_room_t *contourRoom = CrowdRoom_ResolveEnclosedRoom( anchorOrigin );
	if ( contourRoom != nullptr && !contourRoom->face_indices.empty() ) {
		// Erode every convex room face inward so plotted slots keep a full agent
		// hull plus safety margin away from the surrounding walls.
		const double erode = contourAgentRadius + CROWD_ROOM_CONTOUR_WALL_MARGIN;
		std::vector<crowd_eroded_face_t> erodedFaces;
		CrowdRoom_BuildErodedFaces( contourRoom, erode, erodedFaces );
		if ( !erodedFaces.empty() ) {
			/**
			*	Area-Fill Placement:
			*	Rasterize the room's eroded walkable polygon union into a spaced grid of
			*	slot cells, carve the doorway traffic lane, validate each cell against
			*	world geometry (ignoring crowd members), and emit survivors deepest-first
			*	so ingress assignment streams leading members to the back wall. This is
			*	the structurally correct primitive for small rooms: capacity scales with
			*	floor area instead of collapsing with ring radius.
			**/
			std::vector<Vector3DP> cellPositions;
			CrowdRoom_GridFillSlots( contourRoom, erodedFaces, anchorOrigin, contourAgentRadius, portalOrigin, slots.size(), cellPositions );

			int32_t numPlaced = 0;
			int32_t numFailed = 0;

			// Compute each slot's outward-facing yaw (away from the room centroid) so
			// filled members still present a 360-degree defensive posture.
			const Vector3DP roomCenter = contourRoom->centroid;
			for ( size_t i = 0; i < slots.size(); i++ ) {
				svg_crowd_slot_t &slot = slots[ i ];
				slot.slotIndex = static_cast<int32_t>( i );

				if ( i < cellPositions.size() ) {
					const Vector3DP &pos = cellPositions[ i ];
					slot.worldPosition = pos;
					slot.localOffset = pos - anchorOrigin;
					slot.isNavmeshValid = true;
					// Deepest cell (index 0) leads the room interior.
					slot.role = ( i == 0 ) ? crowd_member_role_t::ROLE_LEADER : crowd_member_role_t::ROLE_CENTER;
					Vector3DP outDir = pos - roomCenter;
					outDir.z = 0.0;
					slot.relativeYawDeg = ( QM_Vector3LengthDP( outDir ) > 0.001 )
						? QM_Vector3ToYawDP( QM_Vector3NormalizeDP( outDir ) )
						: ( ( ingressDir != nullptr && QM_Vector3LengthDP( *ingressDir ) > 0.001 ) ? QM_Vector3ToYawDP( *ingressDir ) : 0.0 );
					numPlaced++;
				} else {
					// Genuine overflow: room floor area is saturated.
					slot.isNavmeshValid = false;
					numFailed++;
				}
			}

			// Queue genuine overflow members in a compressed line just outside the doorway
			// threshold (on-mesh), so waiters stay tight to the entrance instead of being
			// parked far back along the reverse path where they idle without a sane route.
			int32_t numQueued = 0;
			if ( numFailed > 0 && portalOrigin != nullptr ) {
				numQueued = CrowdRoom_PlaceDoorwayOverflowQueue( *portalOrigin, anchorOrigin, contourAgentRadius, slots );
				numFailed -= numQueued;
			}

			gi.dprintf( "%s: room %d area-fill placed %d/%d slots (doorway queue %d, overflow %d, cells=%zu)\n",
				__func__, contourRoom->room_id, numPlaced, static_cast<int32_t>( slots.size() ), numQueued, numFailed, cellPositions.size() );
			return;
		}
	}

	//! Doorway portal clearance keep-out radius in world units guaranteeing unobstructed entrance passage.
	static constexpr double CROWD_ROOM_DOORWAY_KEEPOUT_RADIUS = 64.0;
	//! Static clearance probe radius scale applied to dynamic agent radius.
	static constexpr double CROWD_ROOM_CIRCLE_PROBE_RADIUS_SCALE = 0.85;
	//! Minimum absolute radius in world units for static clearance probe.
	static constexpr double CROWD_ROOM_CIRCLE_MIN_PROBE_RADIUS = 6.0;

	const double effectiveRadius = ( agentRadius > 0.0 ) ? agentRadius : CROWD_DEFAULT_AGENT_RADIUS;
	const float traceRadius = static_cast<float>( std::max( CROWD_ROOM_CIRCLE_MIN_PROBE_RADIUS, effectiveRadius * CROWD_ROOM_CIRCLE_PROBE_RADIUS_SCALE ) );
	//! Half-height ratio of agent standup bounds used for clearance trace probe.
	static constexpr double CROWD_AGENT_STANDUP_TOTAL_HEIGHT = 72.0;
	static constexpr double CROWD_STATIC_CLEARANCE_FLOOR_ELEVATION = 4.0;
	static constexpr double CROWD_STATIC_CLEARANCE_HEIGHT_SCALE = 0.90;
	const float halfHeight = static_cast<float>( ( CROWD_AGENT_STANDUP_TOTAL_HEIGHT * 0.5 ) * CROWD_STATIC_CLEARANCE_HEIGHT_SCALE );
	const double probeCenterElevZ = ( CROWD_AGENT_STANDUP_TOTAL_HEIGHT * 0.5 ) + CROWD_STATIC_CLEARANCE_FLOOR_ELEVATION;

	const Vector3 probeMins{ -traceRadius, -traceRadius, -halfHeight };
	const Vector3 probeMaxs{ traceRadius, traceRadius, halfHeight };

	//! Multiplier on agent radius to compute nominal circular formation radius.
	static constexpr double CROWD_ROOM_CIRCLE_NOMINAL_RADIUS_SCALE = 2.5;
	//! Minimum nominal radius for room circular formations.
	static constexpr double CROWD_ROOM_CIRCLE_MIN_NOMINAL_RADIUS = 96.0;
	//! Minimum physical ring radius inside tightly bounded alcoves or small rooms.
	static constexpr double CROWD_ROOM_CIRCLE_MIN_RING_RADIUS = 32.0;
	//! Wall standoff margin subtracted from traced wall distance.
	static constexpr double CROWD_ROOM_CIRCLE_WALL_MARGIN = 20.0;
	//! Radial expansion scale per ring member in circular formations.
	static constexpr double CROWD_ROOM_CIRCLE_PER_MEMBER_RADIUS_SCALE = 9.0;
	//! Minimum number of ring members required to subdivide into multi-tier concentric protective rings.
	static constexpr size_t CROWD_ROOM_CIRCLE_MIN_CONCENTRIC_MEMBERS = 8;
	//! Radius scaling factor applied to the inner concentric protective ring.
	static constexpr double CROWD_ROOM_CIRCLE_INNER_RING_SCALE = 0.52;
	//! Half-angle of doorway aperture keep-out cone in degrees.
	static constexpr double CROWD_DOORWAY_SECTOR_HALF_ANGLE_DEG = 70.0;
	//! Arc span in degrees across interior back and flank walls for perimeter slots.
	static constexpr double CROWD_DOORWAY_PERIMETER_ARC_DEG = 220.0;
	//! Strict keep-out distance in world units from doorway portal center to prevent stationary slot placement.
	static constexpr double CROWD_ROOM_CIRCLE_DOOR_KEEP_OUT_DIST = 80.0;
	//! Radius compression scaling factor used when pushing encroaching doorway slots to room flanks.
	static constexpr double CROWD_ROOM_CIRCLE_DOOR_PUSH_SCALE = 0.6;
	//! Extra distance for radial hull sweeps beyond nominal radius.
	static constexpr double CROWD_ROOM_CIRCLE_WALL_TRACE_EXTRA = 48.0;
	//! Vertical trace elevation offset above floor for wall margin sweeps.
	static constexpr double CROWD_ROOM_CIRCLE_TRACE_OFFSET_Z = 16.0;
	//! Number of radial probe rays used to scan room geometry.
	static constexpr int32_t CROWD_ROOM_SCANNER_NUM_RAYS = 16;
	//! Maximum trace distance for omnidirectional room profile scanner.
	static constexpr double CROWD_ROOM_SCANNER_MAX_DIST = 350.0;

	// 1. Determine the room center anchor using spatial decomposition or direct geometry:
	const nav_room_t *room = Nav_GetRoomForPoint( anchorOrigin );
	if ( !CrowdRoom_IsAnalyticallyFillable( room ) ) {
		double bestPortalDistSq = 160.0 * 160.0;
		for ( const nav_portal_t &portal : g_nav_portals ) {
			const double dSq = QM_Vector3Distance2DSqrDP( anchorOrigin, portal.center );
			if ( dSq < bestPortalDistSq ) {
				const int32_t rTo = portal.to_room_id;
				const int32_t rFrom = portal.from_room_id;
				if ( rTo >= 0 && rTo < static_cast<int32_t>( g_nav_rooms.size() ) &&
					 CrowdRoom_IsAnalyticallyFillable( &g_nav_rooms[ rTo ] ) ) {
					room = &g_nav_rooms[ rTo ];
					bestPortalDistSq = dSq;
				} else if ( rFrom >= 0 && rFrom < static_cast<int32_t>( g_nav_rooms.size() ) &&
							CrowdRoom_IsAnalyticallyFillable( &g_nav_rooms[ rFrom ] ) ) {
					room = &g_nav_rooms[ rFrom ];
					bestPortalDistSq = dSq;
				}
			}
		}
	}

	// Recenter the anchor onto the resolved room's centroid for every portal-bounded zone
	// (enclosed rooms, alcoves, AND corridors). Without this, a destination order issued on
	// the porch/lawn outside a corridor-class room keeps the radial ring centered off-mesh,
	// so the strict room-containment and LOS checks below reject every candidate and the
	// whole formation collapses to invalid slots piling up at the doorway.
	Vector3DP effectiveAnchor = anchorOrigin;
	if ( CrowdRoom_IsAnalyticallyFillable( room ) ) {
		effectiveAnchor = room->centroid;
	}

	// 2. High-performance 2D half-edge topological room profile scanner:
	// Automatically measures physical wall distances and discovers the primary doorway aperture in < 5 microseconds:
	double minWallDist = CROWD_ROOM_SCANNER_MAX_DIST;
	double avgWallDist = CROWD_ROOM_SCANNER_MAX_DIST;
	double detectedDoorYaw = 0.0;
	bool hasDetectedDoor = false;
	nav_zone_type_t detectedZone = ZONE_TYPE_OPEN_SPACE;

	Nav_RaycastRoomBoundary2D( effectiveAnchor, CROWD_ROOM_SCANNER_MAX_DIST, CROWD_ROOM_SCANNER_NUM_RAYS, &detectedDoorYaw, &hasDetectedDoor, &minWallDist, &avgWallDist, &detectedZone, effectiveRadius );

	const bool isEnclosedRoomGeometry = ( minWallDist < 250.0 );

	// 3. Compute doorway orientation:
	bool hasDoorwayDirection = false;
	double doorYaw = 0.0;
	if ( portalOrigin != nullptr ) {
		Vector3DP toDoor = *portalOrigin - effectiveAnchor;
		toDoor.z = 0.0;
		if ( QM_Vector3LengthDP( toDoor ) > 0.001 ) {
			doorYaw = QM_Vector3ToYawDP( toDoor );
			hasDoorwayDirection = true;
		}
	} else if ( room != nullptr && !room->portal_indices.empty() ) {
		const int32_t pIdx = room->portal_indices[ 0 ];
		if ( pIdx >= 0 && pIdx < static_cast<int32_t>( g_nav_portals.size() ) ) {
			Vector3DP toDoor = g_nav_portals[ pIdx ].center - effectiveAnchor;
			toDoor.z = 0.0;
			if ( QM_Vector3LengthDP( toDoor ) > 0.001 ) {
				doorYaw = QM_Vector3ToYawDP( toDoor );
				hasDoorwayDirection = true;
			}
		}
	} else if ( hasDetectedDoor ) {
		doorYaw = detectedDoorYaw;
		hasDoorwayDirection = true;
	} else if ( ingressDir != nullptr && QM_Vector3LengthDP( *ingressDir ) > 0.001 ) {
		doorYaw = QM_Vector3ToYawDP( -( *ingressDir ) );
		hasDoorwayDirection = true;
	}

	// 4. Generate true symmetrical circular formation slots (Center + concentric radial perimeter rings):
	const size_t totalMembers = slots.size();
	const size_t ringMembers = ( totalMembers > 1 ) ? ( totalMembers - 1 ) : 0;
	// Allow expansion up to the maximum room scan distance to fill the room area in enclosed spaces.
	const double maxAllowedRoomRadius = CROWD_ROOM_SCANNER_MAX_DIST;
	// Scale by agent radius to ensure large squads have enough physical space.
	const double baseNominalRadius = std::max( effectiveRadius * CROWD_ROOM_CIRCLE_NOMINAL_RADIUS_SCALE, std::max( CROWD_ROOM_CIRCLE_MIN_NOMINAL_RADIUS, static_cast<double>( ringMembers ) * effectiveRadius ) );
	const double nominalRadius = std::min( baseNominalRadius, maxAllowedRoomRadius );

	// For large squads (N >= 8), distribute members across two concentric protective rings (inner + outer perimeter):
	const size_t innerRingCount = ( ringMembers >= CROWD_ROOM_CIRCLE_MIN_CONCENTRIC_MEMBERS ) ? ( ringMembers / 2 ) : 0;
	const size_t outerRingCount = ringMembers - innerRingCount;

	for ( size_t i = 0; i < slots.size(); i++ ) {
		svg_crowd_slot_t &slot = slots[ i ];
		slot.slotIndex = static_cast<int32_t>( i );

		if ( i == 0 ) {
			// Center slot: Protected VIP / Leader positioned at exact room center
			Vector3DP slot0Pos = effectiveAnchor;
			if ( portalOrigin != nullptr ) {
				const double distToDoor = QM_Vector3Distance2DDP( slot0Pos, *portalOrigin );
				if ( distToDoor < CROWD_ROOM_CIRCLE_DOOR_KEEP_OUT_DIST ) {
					Vector3DP awayFromDoor = slot0Pos - *portalOrigin;
					awayFromDoor.z = 0.0;
					if ( QM_Vector3LengthDP( awayFromDoor ) > 0.001 ) {
						slot0Pos = *portalOrigin + ( QM_Vector3NormalizeDP( awayFromDoor ) * CROWD_ROOM_CIRCLE_DOOR_KEEP_OUT_DIST );
					}
				}
			}
			slot.worldPosition = slot0Pos;
			slot.localOffset = slot0Pos - anchorOrigin;
			slot.isNavmeshValid = true;
			slot.role = crowd_member_role_t::ROLE_LEADER;
			slot.relativeYawDeg = hasDoorwayDirection ? doorYaw : ( ( ingressDir != nullptr && QM_Vector3LengthDP( *ingressDir ) > 0.001 ) ? QM_Vector3ToYawDP( *ingressDir ) : 0.0 );
			continue;
		}

		// Determine whether slot belongs to inner ring or outer ring:
		const size_t ringSlotIdx = i - 1;
		double targetRadius = nominalRadius;
		double angleDeg = 0.0;
		bool isInnerRing = false;

		if ( innerRingCount > 0 && ringSlotIdx < innerRingCount ) {
			// Inner protective ring:
			targetRadius = nominalRadius * CROWD_ROOM_CIRCLE_INNER_RING_SCALE;
			isInnerRing = true;
			if ( hasDoorwayDirection ) {
				const double arcStep = ( innerRingCount > 1 ) ? ( CROWD_DOORWAY_PERIMETER_ARC_DEG / static_cast<double>( innerRingCount - 1 ) ) : 0.0;
				angleDeg = QM_AngleMod( doorYaw + CROWD_DOORWAY_SECTOR_HALF_ANGLE_DEG + ( static_cast<double>( ringSlotIdx ) * arcStep ) );
			} else {
				angleDeg = static_cast<double>( ringSlotIdx ) * ( 360.0 / static_cast<double>( innerRingCount ) );
			}
		} else {
			// Outer protective ring:
			const size_t outerIdx = ( innerRingCount > 0 ) ? ( ringSlotIdx - innerRingCount ) : ringSlotIdx;
			targetRadius = nominalRadius;
			if ( hasDoorwayDirection ) {
				const double arcStep = ( outerRingCount > 1 ) ? ( CROWD_DOORWAY_PERIMETER_ARC_DEG / static_cast<double>( outerRingCount - 1 ) ) : 0.0;
				angleDeg = QM_AngleMod( doorYaw + CROWD_DOORWAY_SECTOR_HALF_ANGLE_DEG + ( static_cast<double>( outerIdx ) * arcStep ) );
			} else {
				angleDeg = static_cast<double>( outerIdx ) * ( 360.0 / static_cast<double>( outerRingCount ) );
				if ( innerRingCount > 0 ) {
					angleDeg += ( 180.0 / static_cast<double>( outerRingCount ) );
				}
			}
		}

		const double angle = angleDeg * ( QM_PI / 180.0 );
		const Vector3DP radialDir{ std::cos( angle ), std::sin( angle ), 0.0 };

		// Half-edge topological circle-cast against navmesh boundary geometry taking agent capsule radius into account:
		nav_raycast_result_t rayRes = {};
		Nav_CircleCastHalfEdge2D( effectiveAnchor, radialDir, CROWD_ROOM_SCANNER_MAX_DIST, effectiveRadius, &rayRes );

		const double wallDist = rayRes.hitSolidWall ? rayRes.hitDistance : CROWD_ROOM_SCANNER_MAX_DIST;
		const double maxAvailableRad = std::max( 0.0, wallDist - CROWD_ROOM_CIRCLE_WALL_MARGIN );

		// Scale the ring radius based on whether it is inner or outer, ensuring it never exceeds the room boundaries.
		double ringRadius = 0.0;
		if ( isInnerRing ) {
			const double maxInnerRad = maxAvailableRad * CROWD_ROOM_CIRCLE_INNER_RING_SCALE;
			ringRadius = std::min( targetRadius, maxInnerRad );
		} else {
			ringRadius = std::min( targetRadius, maxAvailableRad );
		}

		const double minRingRad = std::min( CROWD_ROOM_CIRCLE_MIN_RING_RADIUS, targetRadius );
		ringRadius = std::max( minRingRad, ringRadius );

		Vector3DP candidatePos = effectiveAnchor + ( radialDir * ringRadius );

		// Geometric Portal Plane Clipping (O(1)):
		// Prevent slots from leaking out of the room by mathematically clipping them against the doorway plane.
		if ( portalOrigin != nullptr && ingressDir != nullptr && QM_Vector3LengthSqrDP( *ingressDir ) > 0.01 ) {
			const double rayDot = QM_Vector3DotProductDP( radialDir, *ingressDir );
			if ( rayDot < -0.001 ) {
				const double anchorDot = QM_Vector3DotProductDP( effectiveAnchor - *portalOrigin, *ingressDir );
				const double maxRad = ( CROWD_ROOM_CIRCLE_WALL_MARGIN - anchorDot ) / rayDot;
				if ( maxRad > 0.0 && maxRad < ringRadius ) {
					ringRadius = std::max( minRingRad, maxRad );
					candidatePos = effectiveAnchor + ( radialDir * ringRadius );
				}
			}
		}

		// Geometric Doorway Corridor Keep-Out:
		// Push any slot that encroaches within doorway keep-out distance of the portal center out to the room flanks:
		if ( portalOrigin != nullptr ) {
			const double distToDoor = QM_Vector3Distance2DDP( candidatePos, *portalOrigin );
			if ( distToDoor < CROWD_ROOM_CIRCLE_DOOR_KEEP_OUT_DIST ) {
				const double minPush = std::min( CROWD_ROOM_CIRCLE_MIN_RING_RADIUS, ringRadius );
				const double pushRadius = std::clamp( ringRadius * CROWD_ROOM_CIRCLE_DOOR_PUSH_SCALE, minPush, ringRadius );
				candidatePos = effectiveAnchor + ( radialDir * pushRadius );
			}
		}

		// Topological Room Containment Verification:
		// Ensure candidate position lies strictly within the designated room boundaries:
		if ( room != nullptr ) {
			const nav_room_t *slotRoom = Nav_GetRoomForPoint( candidatePos );
			const double minContainmentRadius = std::min( CROWD_ROOM_CIRCLE_MIN_RING_RADIUS, targetRadius );
			while ( ( slotRoom == nullptr || slotRoom->room_id != room->room_id ) && ringRadius > minContainmentRadius ) {
				ringRadius -= 8.0;
				candidatePos = effectiveAnchor + ( radialDir * ringRadius );
				slotRoom = Nav_GetRoomForPoint( candidatePos );
			}
		}

		/**
		*	Validate the candidate against walkable navmesh topology and static hull clearance
		*	before exposing it as a reachable slot destination.
		**/
		int32_t faceIdx = Nav_FindFaceInLeafStrict( candidatePos );
		if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			Vector3DP feetPos = candidatePos;
			feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
			faceIdx = Nav_FindFaceInLeafStrict( feetPos );
		}
		if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			faceIdx = Nav_FindClosestFaceInLeaf( candidatePos );
			if ( faceIdx >= 0 && faceIdx < static_cast<int32_t>( g_nav_faces.size() ) && !Nav_PointInsideFace2D( candidatePos, g_nav_faces[ faceIdx ] ) ) {
				faceIdx = -1;
			}
		}
		if ( faceIdx < 0 || faceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
			slot.isNavmeshValid = false;
			continue;
		}

		const nav_face_t &face = g_nav_faces[ faceIdx ];
		if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
			slot.isNavmeshValid = false;
			continue;
		}
		if ( std::fabs( face.normal.z ) > 0.001 ) {
			const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
			const double d = QM_Vector3DotProductDP( v0, face.normal );
			candidatePos.z = ( d - ( candidatePos.x * face.normal.x + candidatePos.y * face.normal.y ) ) / face.normal.z;
		}

		// Keep radial defensive slots on the same traversable height band as the room anchor.
		if ( std::fabs( candidatePos.z - effectiveAnchor.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
			slot.isNavmeshValid = false;
			continue;
		}

		// Reject candidates that would require crossing interior wall geometry from the room anchor.
		if ( !Nav_HasGeometricLineOfSight2D( effectiveAnchor, candidatePos, effectiveRadius ) ) {
			slot.isNavmeshValid = false;
			continue;
		}

		// Validate static hull occupancy at the candidate center-space probe volume.
		Vector3DP probeCenter = candidatePos;
		probeCenter.z += probeCenterElevZ;
		// Use a tiny sweep to avoid the start==end shape position-test path, which can
		// conservatively inflate cylinder Z extents and reject valid room slots.
		Vector3DP probeEnd = probeCenter;
		probeEnd.z += 0.01;
		const svg_trace_t tr = SVG_MMove_Trace( probeCenter, probeMins, probeMaxs, probeEnd, nullptr, CM_CONTENTMASK_SOLID, MM_SHAPE_CYLINDER );
		if ( tr.startsolid || tr.allsolid ) {
			slot.isNavmeshValid = false;
			continue;
		}

		slot.worldPosition = candidatePos;
		slot.localOffset = candidatePos - anchorOrigin;
		slot.isNavmeshValid = true;
		slot.role = crowd_member_role_t::ROLE_CENTER;

		// 360-degree outward defensive coverage yaw: each perimeter agent faces outward along radial vector:
		slot.relativeYawDeg = QM_Vector3ToYawDP( radialDir );
	}
}

/**
*	@brief	Detect and resolve slot-to-slot spatial collisions and invalid/off-mesh slot positions.
*	@param	slots			[in/out] Formation slots to validate and space out.
*	@param	anchorOrigin	Formation center anchor in Vector3DP.
*	@param	headingYawDeg	Forward movement heading in degrees.
*	@param	minSeparation	Minimum physical separation distance required between distinct slot centers.
*	@param	agentRadius		Agent hull radius for boundary clearance.
*	@param	guidePath		Optional navigation guide path from squad approach to destination used
*							to curve trailing column slots along curved corridors, ramps, and staircases.
*	@param	tacticalFlags	Configurable tactical behavior and fallback bitflags (see crowd_tactical_flags_t).
*	@param	explicitPortalOrigin	Optional bottleneck portal waypoint to enforce ingress keep-out.
**/
void SVG_Crowd_ResolveSlotCollisionsAndInvalidSlots( std::vector<svg_crowd_slot_t> &slots, const Vector3DP &anchorOrigin, const double headingYawDeg, const double minSeparation, const double agentRadius, const std::vector<Vector3DP> *guidePath, const uint32_t tacticalFlags, const Vector3DP *explicitPortalOrigin ) {
	/**
	*	Sanity checks: ensure slots array and navmesh are valid.
	**/
	if ( slots.empty() || g_nav_faces.empty() ) {
		return;
	}

	const double effectiveRadius = ( agentRadius > 0.0 ) ? agentRadius : CROWD_DEFAULT_AGENT_RADIUS;
	//! Minimum physical slot-to-slot separation distance guaranteeing zero overlap between agent collision hulls.
	static constexpr double CROWD_MIN_PACKING_SEPARATION = 32.0;
	const double minPackingSep = std::max( effectiveRadius * 2.0, CROWD_MIN_PACKING_SEPARATION );
	const double minPackingSepSqr = minPackingSep * minPackingSep;
	const double minSepSqr = minSeparation * minSeparation;
	s_acceptedPositions.clear();
	s_acceptedPositions.reserve( slots.size() );

	// Calculate forward unit vector along formation heading and reverse step direction.
	const double headingRad = headingYawDeg * QM_DEG2RAD;
	const Vector3DP fwdDir = { std::cos( headingRad ), std::sin( headingRad ), 0.0 };
	const Vector3DP rightDir = { fwdDir.y, -fwdDir.x, 0.0 };

	// Extract optional doorway portal waypoint and compute dynamic ingress keep-out radius from NavMesh clearance:
	const Vector3DP *portalOrigin = explicitPortalOrigin;
	if ( portalOrigin == nullptr && guidePath != nullptr && guidePath->size() >= 2 ) {
		portalOrigin = &( ( *guidePath )[ guidePath->size() - 2 ] );
	}
	const double dynamicPortalKeepOutDist = SVG_Crowd_ComputeDynamicPortalKeepOutRadius( portalOrigin, minPackingSep, effectiveRadius );
	const double dynamicPortalKeepOutDistSq = dynamicPortalKeepOutDist * dynamicPortalKeepOutDist;

	// Measure physical room boundary around anchorOrigin using 2D NavMesh half-edge raycasting:
	double nearestWallDist = 999999.0;
	double avgWallDist = 999999.0;
	double detectedDoorYaw = 0.0;
	bool hasDetectedDoor = false;
	nav_zone_type_t detectedZone = ZONE_TYPE_OPEN_SPACE;

	Nav_RaycastRoomBoundary2D( anchorOrigin, 350.0, 16, &detectedDoorYaw, &hasDetectedDoor, &nearestWallDist, &avgWallDist, &detectedZone );

	// If the route passes through a doorway bottleneck portal, or the zone is an enclosed room/alcove, or boundary walls are near:
	const bool isInEnclosedRoom = ( portalOrigin != nullptr ) || ( detectedZone == ZONE_TYPE_ROOM_ENCLOSED || detectedZone == ZONE_TYPE_ALCOVE ) || ( nearestWallDist < CROWD_ROOM_PROBE_ENCLOSED_WALL_DIST );
	//! Maximum search radius across the interior room space for flood-fill slot packing.
	static constexpr double CROWD_ROOM_PACKING_MAX_ROOM_RADIUS = 600.0;
	const double maxRoomRadius = isInEnclosedRoom ? CROWD_ROOM_PACKING_MAX_ROOM_RADIUS : 999999.0;

	// Detect if slots were generated as a circular / defensive perimeter ring (slot 0 at origin, slots 1..N arranged radially):
	const bool isCircularFormation = ( slots.size() > 1 &&
		( std::fabs( slots[ 0 ].localOffset.x ) < 0.01 && std::fabs( slots[ 0 ].localOffset.y ) < 0.01 ) &&
		( std::fabs( slots[ 1 ].localOffset.x ) > 1.0 || std::fabs( slots[ 1 ].localOffset.y ) > 1.0 ) );

	/**
	*	Phase 1: In open terrain, accept valid slots of the requested formation style.
	*	For circular formations in an enclosed room, adaptively fit radial slots to room walls.
	*	For non-circular formations in an enclosed room, generate 2D cellular room packing.
	**/
	if ( isCircularFormation || isInEnclosedRoom || portalOrigin != nullptr ) {
		Vector3DP ingressDir = fwdDir;
		if ( portalOrigin != nullptr && guidePath != nullptr && guidePath->size() >= 2 ) {
			ingressDir = QM_Vector3NormalizeDP( anchorOrigin - *portalOrigin );
		}
		SVG_Crowd_FitCircularFormationToRoom( slots, anchorOrigin, effectiveRadius, portalOrigin, &ingressDir );
		for ( svg_crowd_slot_t &slot : slots ) {
			if ( slot.isNavmeshValid ) {
				s_acceptedPositions.push_back( slot.worldPosition );
			}
		}
	} else {
		for ( svg_crowd_slot_t &slot : slots ) {
			if ( !slot.isNavmeshValid ) {
				continue;
			}

			bool collides = false;
			for ( const Vector3DP &acceptedPos : s_acceptedPositions ) {
				if ( QM_Vector3DistanceSqrDP( slot.worldPosition, acceptedPos ) < minPackingSepSqr ) {
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
	}

	/**
	*	Phase 1B: Adaptive Interior Room Packing & Topological BFS Flood Fill.
	*	Executes in strictly bounded O(1) time across adjacent room faces to pack invalid slots
	*	within room boundaries without dynamic memory allocations or framerate hitches.
	**/
	const bool allowCellularRoomPack = ( ( tacticalFlags & CROWD_TACTICAL_FLAG_CELLULAR_ROOM_PACK ) != 0 );
	if ( allowCellularRoomPack ) {
		Vector3DP ingressDir = fwdDir;
		if ( portalOrigin != nullptr && guidePath != nullptr && guidePath->size() >= 2 ) {
			ingressDir = QM_Vector3NormalizeDP( anchorOrigin - *portalOrigin );
		}

		/**
		*	Always run bounded room packing for unresolved slots (including enclosed portal rooms)
		*	so available interior area is consumed before spilling members back onto reverse-path queues.
		**/
		SVG_Crowd_FloodFillRoomPackingO1( slots, anchorOrigin, minPackingSepSqr, effectiveRadius, s_acceptedPositions, maxRoomRadius, portalOrigin, &ingressDir );
	}

	/**
	*	Phase 2: If the destination room is physically packed to capacity, step remaining
	*	excess slots backwards along the reverse heading axis or guide path into sequential ranks
	*	outside the room in the open corridor or courtyard.
	*	CRITICAL: Strictly skip narrow doorway thresholds (< 96 units width) so trailing squad
	*	members form an orderly waiting queue outside rather than parking stationary inside doorways.
	**/
	double totalPathLen = 0.0;
	if ( guidePath != nullptr && guidePath->size() >= 2 ) {
		for ( size_t p = 0; p + 1 < guidePath->size(); p++ ) {
			totalPathLen += QM_Vector3DistanceDP( ( *guidePath )[ p ], ( *guidePath )[ p + 1 ] );
		}
	}
	const double pathBoundDepth = ( totalPathLen > 0.0 ) ? ( totalPathLen * CROWD_COLUMN_PATH_MAX_FRACTION ) : CROWD_DESTINATION_COLUMN_MAX_DEPTH;
	const double maxBackOffset = std::max( minSeparation * static_cast<double>( slots.size() + 4 ), std::min( CROWD_DESTINATION_COLUMN_MAX_DEPTH, pathBoundDepth ) );
	const int32_t maxColumnRanks = std::max( 4, static_cast<int32_t>( maxBackOffset / minSeparation ) + static_cast<int32_t>( slots.size() ) );

	int32_t columnRank = 1;
	for ( svg_crowd_slot_t &slot : slots ) {
		if ( slot.isNavmeshValid ) {
			continue;
		}

		/**
		*	Even when targeting enclosed rooms and doorway ingress, unresolved slots must spill
		*	outward along the guide path so followers queue outside instead of collapsing at one point.
		**/
		bool foundValidPlacement = false;

		while ( columnRank <= maxColumnRanks && !foundValidPlacement ) {
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

			// Check if the current sampleBase along the guide path lies on a narrow corridor,
			// plank, catwalk, or staircase (< 48 units clearance radius). On narrow or elevated
			// passages, lateral side slots (+/- 1.0) would step off the edges or into walls;
			// constrain placement strictly to the centerline (0.0).
			int32_t basePolyIdx = Nav_FindFaceInLeafStrict( sampleBase );
			if ( basePolyIdx < 0 || basePolyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				Vector3DP feetBase = sampleBase;
				feetBase.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
				basePolyIdx = Nav_FindFaceInLeafStrict( feetBase );
			}
			const bool isConstrainedBase = ( basePolyIdx >= 0 && basePolyIdx < static_cast<int32_t>( g_nav_faces.size() ) &&
			                                 g_nav_faces[ basePolyIdx ].clearance > 0.0 &&
			                                 g_nav_faces[ basePolyIdx ].clearance < CROWD_MIN_TWO_AGENT_ABREAST_CLEARANCE );

			// Test center of column first; only test left and right flanks if corridor clearance permits:
			const std::vector<double> lateralSigns = isConstrainedBase ? std::vector<double>{ 0.0 } : std::vector<double>{ 0.0, 1.0, -1.0 };
			for ( const double lateralSign : lateralSigns ) {
				const double lateralOffset = lateralSign * ( minSeparation * CROWD_COLUMN_LATERAL_OFFSET_RATIO );
				Vector3DP candPos = sampleBase + ( localRight * lateralOffset );

				int32_t polyIdx = Nav_FindFaceInLeafStrict( candPos );
				if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
					Vector3DP feetPos = candPos;
					feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
					polyIdx = Nav_FindFaceInLeafStrict( feetPos );
				}
				if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
					polyIdx = Nav_FindClosestFaceInLeaf( candPos );
					if ( polyIdx >= 0 && !Nav_PointInsideFace2D( candPos, g_nav_faces[ polyIdx ] ) ) {
						polyIdx = -1;
					}
				}

				if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
					const nav_face_t &face = g_nav_faces[ polyIdx ];

					// Reject faces on brushes or surfaces flagged with CM_SURFACE_NO_NAVMESH or CONTENTS_NO_NAVMESH:
					if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
						continue;
					}

					// Reject candidate locations within the dynamic portal keep-out radius:
					if ( portalOrigin != nullptr && dynamicPortalKeepOutDistSq > 0.0 && QM_Vector3Distance2DSqrDP( candPos, *portalOrigin ) < dynamicPortalKeepOutDistSq ) {
						continue;
					}

					// Lateral side slots require wide corridor clearance to prevent embedding in walls:
					if ( lateralSign != 0.0 && face.clearance > 0.0 && face.clearance < ( CROWD_DEFAULT_AGENT_RADIUS * 1.75 ) ) {
						continue;
					}

					// Centerline slots on stairs or corridors only require minimum passable agent clearance:
					if ( lateralSign == 0.0 && face.clearance > 0.0 && face.clearance < ( CROWD_DEFAULT_AGENT_RADIUS * 0.70 ) ) {
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

					// Verify physical swept reachability and hull clearance using the monster's native analytical shape:
					const Vector3 probeMins{ static_cast<float>( -effectiveRadius ), static_cast<float>( -effectiveRadius ), PHYS_DEFAULT_BBOX_STANDUP_MINS.z };
					const Vector3 probeMaxs{ static_cast<float>( effectiveRadius ), static_cast<float>( effectiveRadius ), PHYS_DEFAULT_BBOX_STANDUP_MAXS.z };
					Vector3DP probeStart = sampleBase;
					probeStart.z -= static_cast<double>( probeMins.z );
					Vector3DP probeEnd = candPos;
					probeEnd.z -= static_cast<double>( probeMins.z );
					Vector3DP probeGround;
					if ( !SVG_MMove_StepProbe( probeStart, probeMins, probeMaxs, probeEnd, nullptr, &probeGround ) ) {
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

		// Fallback if corridor is extremely constrained: sample open exterior ground around guide path or anchor (only in open terrain)
		if ( !foundValidPlacement && !isInEnclosedRoom && portalOrigin == nullptr ) {
			for ( double r = maxRoomRadius + 48.0; r <= maxRoomRadius + 384.0 && !foundValidPlacement; r += minSeparation ) {
				for ( int32_t a = 0; a < 16; a++ ) {
					const double ang = static_cast<double>( a ) * ( ( 2.0 * QM_PI ) / 16.0 );
					Vector3DP candPos = anchorOrigin + Vector3DP{ r * std::cos( ang ), r * std::sin( ang ), 0.0 };

					int32_t polyIdx = Nav_FindFaceInLeafStrict( candPos );
					if ( polyIdx < 0 || polyIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
						Vector3DP feetPos = candPos;
						feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
						polyIdx = Nav_FindFaceInLeafStrict( feetPos );
					}
					if ( polyIdx >= 0 && polyIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
						const nav_face_t &face = g_nav_faces[ polyIdx ];
						if ( ( face.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) != 0 ) {
							continue;
						}
						if ( portalOrigin != nullptr && dynamicPortalKeepOutDistSq > 0.0 && QM_Vector3Distance2DSqrDP( candPos, *portalOrigin ) < dynamicPortalKeepOutDistSq ) {
							continue;
						}
						if ( std::fabs( face.normal.z ) > 0.001 ) {
							const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ face.first_edge_idx ].vertex_idx ];
							const double d = QM_Vector3DotProductDP( v0, face.normal );
							candPos.z = ( d - ( candPos.x * face.normal.x + candPos.y * face.normal.y ) ) / face.normal.z;
						}
						if ( std::fabs( candPos.z - anchorOrigin.z ) > CROWD_ARRIVAL_MAX_Z_DIFF ) {
							continue;
						}

						// Verify physical swept reachability and hull clearance using the monster's native analytical shape:
						const Vector3 probeMins{ static_cast<float>( -effectiveRadius ), static_cast<float>( -effectiveRadius ), PHYS_DEFAULT_BBOX_STANDUP_MINS.z };
						const Vector3 probeMaxs{ static_cast<float>( effectiveRadius ), static_cast<float>( effectiveRadius ), PHYS_DEFAULT_BBOX_STANDUP_MAXS.z };
						Vector3DP probeStart = anchorOrigin;
						probeStart.z -= static_cast<double>( probeMins.z );
						Vector3DP probeEnd = candPos;
						probeEnd.z -= static_cast<double>( probeMins.z );
						Vector3DP probeGround;
						if ( !SVG_MMove_StepProbe( probeStart, probeMins, probeMaxs, probeEnd, nullptr, &probeGround ) ) {
							continue;
						}

						bool collides = false;
						for ( const Vector3DP &acc : s_acceptedPositions ) {
							if ( QM_Vector3DistanceSqrDP( candPos, acc ) < minPackingSepSqr ) {
								collides = true;
								break;
							}
						}
						if ( !collides ) {
							slot.worldPosition = candPos;
							slot.localOffset = candPos - anchorOrigin;
							slot.isNavmeshValid = true;
							s_acceptedPositions.push_back( candPos );
							foundValidPlacement = true;
							break;
						}
					}
				}
			}
		}

		if ( !foundValidPlacement ) {
			/**
			*	Synthesize a deterministic overflow queue position instead of collapsing
			*	multiple unresolved slots onto the same fallback anchor.
			**/
			const int32_t fallbackRank = std::max( 1, columnRank );
			columnRank++;

			const double targetDistBack = static_cast<double>( fallbackRank ) * minSeparation;
			Vector3DP fallbackPos = anchorOrigin - ( fwdDir * targetDistBack );

			// Prefer reverse guide-path sampling to keep overflow slots aligned with corridor geometry.
			if ( guidePath != nullptr && guidePath->size() >= 2 ) {
				Vector3DP pathPos = {};
				Vector3DP pathTangent = {};
				if ( SVG_Crowd_SampleGuidePathInReverse( *guidePath, targetDistBack, &pathPos, &pathTangent ) ) {
					fallbackPos = pathPos;
					if ( targetDistBack > totalPathLen ) {
						const double extraBackDist = targetDistBack - totalPathLen;
						fallbackPos = pathPos - ( pathTangent * extraBackDist );
					}
				}
			}

			// Snap fallback to a nearby walk face and project to plane when available.
			int32_t fallbackFaceIdx = Nav_FindFaceInLeafStrict( fallbackPos );
			if ( fallbackFaceIdx < 0 || fallbackFaceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				Vector3DP feetPos = fallbackPos;
				feetPos.z -= CROWD_SLOT_FEET_SNAP_OFFSET_Z;
				fallbackFaceIdx = Nav_FindFaceInLeafStrict( feetPos );
			}
			if ( fallbackFaceIdx < 0 || fallbackFaceIdx >= static_cast<int32_t>( g_nav_faces.size() ) ) {
				fallbackFaceIdx = Nav_FindClosestFaceInLeaf( fallbackPos );
				if ( fallbackFaceIdx >= 0 && fallbackFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) && !Nav_PointInsideFace2D( fallbackPos, g_nav_faces[ fallbackFaceIdx ] ) ) {
					fallbackFaceIdx = -1;
				}
			}
			if ( fallbackFaceIdx >= 0 && fallbackFaceIdx < static_cast<int32_t>( g_nav_faces.size() ) ) {
				const nav_face_t &fallbackFace = g_nav_faces[ fallbackFaceIdx ];
				if ( ( fallbackFace.surface_flags & ( CM_SURFACE_NO_NAVMESH | CONTENTS_NO_NAVMESH ) ) == 0 && std::fabs( fallbackFace.normal.z ) > 0.001 ) {
					const Vector3DP v0 = g_nav_vertices[ g_nav_halfedges[ fallbackFace.first_edge_idx ].vertex_idx ];
					const double d = QM_Vector3DotProductDP( v0, fallbackFace.normal );
					fallbackPos.z = ( d - ( fallbackPos.x * fallbackFace.normal.x + fallbackPos.y * fallbackFace.normal.y ) ) / fallbackFace.normal.z;
				}
			}

			// Keep overflow fallbacks unique by pushing farther backward if separation still collides.
			for ( int32_t attempt = 0; attempt < 16; attempt++ ) {
				bool collides = false;
				for ( const Vector3DP &acc : s_acceptedPositions ) {
					if ( QM_Vector3DistanceSqrDP( fallbackPos, acc ) < minSepSqr ) {
						collides = true;
						break;
					}
				}
				if ( !collides ) {
					break;
				}
				fallbackPos = fallbackPos - ( fwdDir * minSeparation );
			}

			slot.worldPosition = fallbackPos;
			slot.localOffset = slot.worldPosition - anchorOrigin;
			slot.isNavmeshValid = true;
			s_acceptedPositions.push_back( fallbackPos );
		}
	}

	// Guarantee 360-degree outward defensive facing for all perimeter slots around the center anchor:
	for ( size_t i = 1; i < slots.size(); i++ ) {
		if ( slots[ i ].isNavmeshValid ) {
			const double dx = slots[ i ].worldPosition.x - anchorOrigin.x;
			const double dy = slots[ i ].worldPosition.y - anchorOrigin.y;
			if ( ( dx * dx + dy * dy ) > 1.0 ) {
				const double outwardYawDeg = std::atan2( dy, dx ) * ( 180.0 / QM_PI );
				slots[ i ].relativeYawDeg = QM_AngleMod( outwardYawDeg - headingYawDeg );
			}
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

//! Topological wall obstruction penalty squared added to Euclidean pairings that cross solid world architecture.
static constexpr double CROWD_SLOT_MATCH_WALL_PENALTY_SQ = 400.0 * 400.0;

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
			double distSq = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ s ].worldPosition );
			// If line of sight is obstructed by solid geometry (such as an exterior wall separating agent from slot),
			// apply topological detour penalty so direct pathing through solid geometry is rejected:
			if ( !Nav_HasGeometricLineOfSight2D( memberOrigins[ m ], slots[ s ].worldPosition, 8.0 ) ) {
				distSq += CROWD_SLOT_MATCH_WALL_PENALTY_SQ;
			}
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

	auto GetInitialPairCost = [&]( size_t m, int32_t sIdx ) -> double {
		double dSq = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ sIdx ].worldPosition );
		if ( !Nav_HasGeometricLineOfSight2D( memberOrigins[ m ], slots[ sIdx ].worldPosition, 8.0 ) ) {
			dSq += CROWD_SLOT_MATCH_WALL_PENALTY_SQ;
		}
		return dSq;
	};

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

				const double currDistSq = GetInitialPairCost( i, slotI ) + GetInitialPairCost( j, slotJ );
				const double swapDistSq = GetInitialPairCost( i, slotJ ) + GetInitialPairCost( j, slotI );

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
			// If line of sight is obstructed by solid geometry (such as an exterior wall separating agent from slot),
			// apply topological detour penalty so direct pathing through solid geometry is rejected:
			if ( !Nav_HasGeometricLineOfSight2D( memberOrigins[ m ], slots[ s ].worldPosition, 8.0 ) ) {
				distSq += CROWD_SLOT_MATCH_WALL_PENALTY_SQ;
			}
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

	auto GetHystPairCost = [&]( size_t m, int32_t sIdx ) -> double {
		double dSq = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ sIdx ].worldPosition );
		if ( !Nav_HasGeometricLineOfSight2D( memberOrigins[ m ], slots[ sIdx ].worldPosition, 8.0 ) ) {
			dSq += CROWD_SLOT_MATCH_WALL_PENALTY_SQ;
		}
		const int32_t prevSlot = ( m < previousSlotMap.size() ) ? previousSlotMap[ m ] : -1;
		if ( sIdx == prevSlot ) {
			dSq = ( dSq > hystBonusSq ) ? ( dSq - hystBonusSq ) : 0.0;
		}
		return dSq;
	};

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

				const double currDistSq = GetHystPairCost( i, slotI ) + GetHystPairCost( j, slotJ );
				const double swapDistSq = GetHystPairCost( i, slotJ ) + GetHystPairCost( j, slotI );

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
void SVG_Crowd_AssignMembersToSlotsIngress( const std::vector<Vector3DP> &memberOrigins, const std::vector<svg_crowd_slot_t> &slots, const std::vector<int32_t> &previousSlotMap, std::vector<int32_t> &outMemberToSlotMap, const Vector3DP &destOrigin, const Vector3DP &ingressDir, const double hysteresisDist, const Vector3DP *portalOrigin, const std::vector<Vector3DP> *guidePath ) {
	const size_t count = memberOrigins.size();
	outMemberToSlotMap.clear();
	outMemberToSlotMap.resize( count, -1 );

	if ( count == 0 || slots.empty() ) {
		return;
	}

	// If ingress direction is negligible, no portal is designated, and no guide path is present, fallback to standard hysteresis:
	const double ingressLen = QM_Vector3LengthDP( ingressDir );
	if ( ingressLen < 0.001 && portalOrigin == nullptr && ( guidePath == nullptr || guidePath->size() < 2 ) ) {
		SVG_Crowd_AssignMembersToSlotsHysteresis( memberOrigins, slots, previousSlotMap, outMemberToSlotMap, hysteresisDist );
		return;
	}

	const Vector3DP fwdNorm = ( ingressLen > 0.001 ) ? ( ingressDir * ( 1.0 / ingressLen ) ) : Vector3DP{ 1.0, 0.0, 0.0 };
	Vector3DP portalInwardNorm = fwdNorm;
	if ( portalOrigin != nullptr ) {
		portalInwardNorm = destOrigin - *portalOrigin;
		portalInwardNorm.z = 0.0;
		if ( QM_Vector3LengthSqrDP( portalInwardNorm ) > 0.0001 ) {
			portalInwardNorm = QM_Vector3NormalizeDP( portalInwardNorm );
		} else {
			portalInwardNorm = fwdNorm;
		}
	}
	const Vector3DP rankingForward = ( portalOrigin != nullptr ) ? portalInwardNorm : fwdNorm;
	const Vector3DP rightNorm{ rankingForward.y, -rankingForward.x, 0.0 };

	// Calculate squad centroid.
	Vector3DP centroid = { 0.0, 0.0, 0.0 };
	for ( const Vector3DP &pos : memberOrigins ) {
		centroid = centroid + pos;
	}
	centroid = centroid * ( 1.0 / static_cast<double>( count ) );

	// 1. Rank members by true geodesic progress towards destination along the navigation route.
	// When guidePath is provided, member progress is computed as the remaining arc-length along the route,
	// so the forward agent closest to the exit doorway receives Rank 0 (leader of the outflow).
	s_rankedMembers.clear();
	s_rankedMembers.reserve( count );

	if ( portalOrigin != nullptr ) {
		/**
		*	Use signed portal-plane progress so crossing the threshold remains monotonic.
		*	Unsigned portal distance folds the exterior and interior half-spaces together.
		**/
		for ( size_t m = 0; m < count; m++ ) {
			const Vector3DP delta = memberOrigins[ m ] - *portalOrigin;
			const double progress = QM_Vector3DotProductDP( delta, portalInwardNorm );
			const double lateral = QM_Vector3DotProductDP( delta, rightNorm );
			s_rankedMembers.push_back( svg_crowd_member_rank_t{ static_cast<int32_t>( m ), progress, lateral } );
		}
	} else if ( guidePath != nullptr && guidePath->size() >= 2 ) {
		const size_t numPoints = guidePath->size();
		std::vector<double> distToGoalAtPoint( numPoints, 0.0 );
		for ( size_t p = numPoints - 1; p > 0; p-- ) {
			distToGoalAtPoint[ p - 1 ] = distToGoalAtPoint[ p ] + QM_Vector3DistanceDP( ( *guidePath )[ p - 1 ], ( *guidePath )[ p ] );
		}

		for ( size_t m = 0; m < count; m++ ) {
			const Vector3DP &mPos = memberOrigins[ m ];
			double bestDistAlongPath = std::numeric_limits<double>::max();
			double bestOrthogDistSq = std::numeric_limits<double>::max();

			for ( size_t p = 0; p + 1 < numPoints; p++ ) {
				const Vector3DP &segA = ( *guidePath )[ p ];
				const Vector3DP &segB = ( *guidePath )[ p + 1 ];
				const Vector3DP segVec = segB - segA;
				const double segLenSq = QM_Vector3LengthSqrDP( segVec );
				double t = 0.0;
				if ( segLenSq > 0.0001 ) {
					t = std::clamp( QM_Vector3DotProductDP( mPos - segA, segVec ) / segLenSq, 0.0, 1.0 );
				}
				const Vector3DP projPoint = segA + ( segVec * t );
				const double orthogDistSq = QM_Vector3DistanceSqrDP( mPos, projPoint );

				if ( orthogDistSq < bestOrthogDistSq ) {
					bestOrthogDistSq = orthogDistSq;
					const double segLen = std::sqrt( segLenSq );
					const double remDistOnSeg = ( 1.0 - t ) * segLen;
					bestDistAlongPath = remDistOnSeg + distToGoalAtPoint[ p + 1 ];
				}
			}

			if ( bestDistAlongPath == std::numeric_limits<double>::max() ) {
				bestDistAlongPath = QM_Vector3DistanceDP( mPos, destOrigin );
			}

			// Negative distance along path so smaller remaining distance = higher progress:
			const double prog = -bestDistAlongPath;
			const Vector3DP delta = memberOrigins[ m ] - centroid;
			const double lat = QM_Vector3DotProductDP( delta, rightNorm );
			s_rankedMembers.push_back( svg_crowd_member_rank_t{ static_cast<int32_t>( m ), prog, lat } );
		}
	} else {
		for ( size_t m = 0; m < count; m++ ) {
			double prog = 0.0;
			if ( portalOrigin != nullptr ) {
				// Negative Euclidean distance: closest to portal has highest progress (least negative).
				prog = -QM_Vector3DistanceDP( memberOrigins[ m ], *portalOrigin );
			} else {
				const Vector3DP delta = memberOrigins[ m ] - centroid;
				prog = QM_Vector3DotProductDP( delta, fwdNorm );
			}
			const Vector3DP delta = memberOrigins[ m ] - centroid;
			const double lat = QM_Vector3DotProductDP( delta, rightNorm );
			s_rankedMembers.push_back( svg_crowd_member_rank_t{ static_cast<int32_t>( m ), prog, lat } );
		}
	}

	// Sort members descending by progress (leader / front members first):
	std::sort( s_rankedMembers.begin(), s_rankedMembers.end(), []( const svg_crowd_member_rank_t &a, const svg_crowd_member_rank_t &b ) {
		if ( std::fabs( a.progress - b.progress ) > 0.001 ) {
			return a.progress > b.progress;
		}
		if ( std::fabs( a.lateral - b.lateral ) > 0.001 ) {
			return a.lateral < b.lateral;
		}
		return a.memberIdx < b.memberIdx;
	} );

	// 2. Rank slots by longitudinal depth along fwdNorm or distance from entrance portal.
	// Deepest slots in the room/formation have highest depth.
	s_rankedSlots.clear();
	s_rankedSlots.reserve( slots.size() );

	for ( size_t s = 0; s < slots.size(); s++ ) {
		double depth = 0.0;
		if ( portalOrigin != nullptr ) {
			// Signed depth keeps exterior overflow slots behind every interior room slot.
			depth = QM_Vector3DotProductDP( slots[ s ].worldPosition - *portalOrigin, portalInwardNorm );
		} else {
			const Vector3DP delta = slots[ s ].worldPosition - destOrigin;
			depth = QM_Vector3DotProductDP( delta, fwdNorm );
		}
		const Vector3DP delta = slots[ s ].worldPosition - destOrigin;
		const double lat = QM_Vector3DotProductDP( delta, rightNorm );
		s_rankedSlots.push_back( svg_crowd_slot_rank_t{ static_cast<int32_t>( s ), depth, lat } );
	}

	// Sort slots descending by depth (deepest first):
	std::sort( s_rankedSlots.begin(), s_rankedSlots.end(), []( const svg_crowd_slot_rank_t &a, const svg_crowd_slot_rank_t &b ) {
		if ( std::fabs( a.depth - b.depth ) > 0.001 ) {
			return a.depth > b.depth;
		}
		if ( std::fabs( a.lateral - b.lateral ) > 0.001 ) {
			return a.lateral < b.lateral;
		}
		return a.slotIdx < b.slotIdx;
	} );

	// 3. Assignment matching member ranks to slot ranks:
	// When entering an enclosed room with a doorway (portalOrigin != nullptr), strictly enforce monotonic depth assignment
	// so the forward-most leading members are guaranteed the deepest back-wall slots, streaming all the way to the rear
	// without halting in the doorway. For adjacent pairs in the same depth tier, pair matching lateral sides (left to left, right to right):
	s_slotClaimed.clear();
	s_slotClaimed.resize( slots.size(), false );

	if ( portalOrigin != nullptr ) {
		/**
		*	Portal ingress = minimum-cost bipartite assignment (Hungarian algorithm).
		*
		*	Rank-metric pairing (member progress vs. slot depth, both measured as portal distance)
		*	is fundamentally unstable at the aperture: portal distance is non-monotonic across the
		*	threshold (it decreases while approaching and increases after crossing), so the member
		*	straddling the doorway flips rank every re-order and gets retargeted 180 degrees,
		*	dead-locking the queue. A global minimum-cost matching with a geometrically coherent
		*	cost and previous-slot hysteresis eliminates the oscillation class entirely: only a
		*	strictly better TOTAL assignment can ever retarget a member.
		*
		*	Cost(m, s) = d^2(member, slot)						// travel
		*			   + WALL_PENALTY  (if no 2D LOS)			// reachability through doorway
		*			   + sideMismatch * SIDE_PENALTY			// portal-side depth coherence
		*			   - HYSTERESIS_BONUS (if s == prevSlot)	// assignment stability
		**/
		/**
		*	Signed portal-side classification.
		*	The room centroid lies on the INTERIOR side of the portal; the ingress normal points
		*	from the portal toward that centroid (inward). The signed projection of (point-portal)
		*	onto this inward normal classifies every point as INTERIOR (positive, same side as the
		*	room centroid) or EXTERIOR (negative, the approach/queue side). This is the correct
		*	geometric test — an UNSIGNED distance threshold is degenerate here because it folds
		*	far-outside members and deep-interior slots into the same "far" class, actively
		*	crossing assignments through the doorway queue.
		**/
		//! Inward unit normal of the portal plane, derived once from the room anchor.
		const Vector3DP inwardNorm = portalInwardNorm;

		//! Penalty for an EXTERIOR member assigned an INTERIOR slot: this is the crossing
		//! pairing that forces an outsider to path through the whole doorway queue.
		static constexpr double CROWD_PORTAL_CROSS_IN_PENALTY_SQ = 8000.0 * 8000.0;
		//! Penalty for an INTERIOR member (already inside) pulled back out to an EXTERIOR slot:
		//! forces a reversal through the doorway against the ingress flow.
		static constexpr double CROWD_PORTAL_PULLBACK_PENALTY_SQ = 8000.0 * 8000.0;
		//! Previous-slot hysteresis: 160u equivalent — only strongly better slots retarget.
		static constexpr double CROWD_PORTAL_HYSTERESIS_DIST = 160.0;
		static constexpr double CROWD_PORTAL_HYSTERESIS_BONUS_SQ = CROWD_PORTAL_HYSTERESIS_DIST * CROWD_PORTAL_HYSTERESIS_DIST;

		const size_t n = std::max( count, slots.size() );

		/**
		*	Build O(1) reverse rank lookups for the monotone portal assignment constraint.
		**/
		static std::vector<int32_t> s_memberRankByIndex;
		static std::vector<int32_t> s_slotRankByIndex;
		s_memberRankByIndex.assign( count, -1 );
		s_slotRankByIndex.assign( slots.size(), -1 );
		for ( size_t rank = 0; rank < s_rankedMembers.size(); rank++ ) {
			s_memberRankByIndex[ s_rankedMembers[ rank ].memberIdx ] = static_cast<int32_t>( rank );
		}
		for ( size_t rank = 0; rank < s_rankedSlots.size(); rank++ ) {
			s_slotRankByIndex[ s_rankedSlots[ rank ].slotIdx ] = static_cast<int32_t>( rank );
		}

		//! Convex order penalty enforcing front-member to deep-slot monotonicity.
		static constexpr double CROWD_PORTAL_RANK_PENALTY_SQ = 16384.0 * 16384.0;

		/**
		*	Build the n x n cost matrix (O(n^2) build, Hungarian solves O(n^3); n <= ~32 so
		*	this is trivially bounded and allocation-free via static scratch buffers).
		**/
		static std::vector<double> s_costMatrix;
		s_costMatrix.assign( n * n, 0.0 );
		auto CostAt = [&]( size_t row, size_t col ) -> double & { return s_costMatrix[ ( row * n ) + col ]; };

		for ( size_t m = 0; m < n; m++ ) {
			for ( size_t s = 0; s < n; s++ ) {
				// Dummy rows/columns (m >= count or s >= slots.size()) receive zero cost.
				if ( m >= count || s >= slots.size() ) {
					CostAt( m, s ) = 0.0;
					continue;
				}

				// Base travel cost: squared Euclidean distance.
				double c = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ s ].worldPosition );

				// Penalize rank inversions quadratically so lateral/travel optimization occurs only
				// inside the monotone front-to-back ingress ordering.
				const int32_t rankDelta = s_memberRankByIndex[ m ] - s_slotRankByIndex[ s ];
				c += static_cast<double>( rankDelta * rankDelta ) * CROWD_PORTAL_RANK_PENALTY_SQ;

				// Reachability: penalize slots with no geometric line-of-sight (through walls).
				if ( !Nav_HasGeometricLineOfSight2D( memberOrigins[ m ], slots[ s ].worldPosition, 8.0 ) ) {
					c += CROWD_SLOT_MATCH_WALL_PENALTY_SQ;
				}

				// Portal-side depth coherence via SIGNED side classification:
				// - EXTERIOR member -> INTERIOR slot: crossing the queue (heavy penalty).
				// - INTERIOR member -> EXTERIOR slot: pulled back out against flow (heavy penalty).
				// Coherent pairings (exterior->exterior queue, interior->interior fill) cost nothing
				// extra, so the matching naturally streams outsiders to queue slots and insiders to
				// interior slots without ever crossing the doorway against itself.
				const double memberSide = QM_Vector3DotProductDP( memberOrigins[ m ] - *portalOrigin, inwardNorm );
				const double slotSide = QM_Vector3DotProductDP( slots[ s ].worldPosition - *portalOrigin, inwardNorm );
				const bool memberInside = ( memberSide > 0.0 );
				const bool slotInside = ( slotSide > 0.0 );
				if ( !memberInside && slotInside ) {
					// Outsider assigned an interior slot: only acceptable for the members that
					// will actually cross (there are exactly as many interior slots as crossers),
					// but still discouraged so the NEAREST outsiders are the ones chosen to cross.
					c += CROWD_PORTAL_CROSS_IN_PENALTY_SQ;
				} else if ( memberInside && !slotInside ) {
					// Insider pulled back out to the queue: strongly discouraged.
					c += CROWD_PORTAL_PULLBACK_PENALTY_SQ;
				}

				// Hysteresis: strongly prefer holding the previous slot.
				const int32_t prevSlot = ( m < previousSlotMap.size() ) ? previousSlotMap[ m ] : -1;
				if ( static_cast<int32_t>( s ) == prevSlot ) {
					c = ( c > CROWD_PORTAL_HYSTERESIS_BONUS_SQ ) ? ( c - CROWD_PORTAL_HYSTERESIS_BONUS_SQ ) : 0.0;
				}

				CostAt( m, s ) = c;
			}
		}

		/**
		*	Hungarian algorithm (rectangular-safe via square padding) for minimization.
		*	Standard O(n^3) potential-based implementation (Jonker-style dual updates).
		**/
		static std::vector<double> s_u, s_v, s_minV;
		static std::vector<int32_t> s_p, s_way;
		s_u.assign( n + 1, 0.0 );
		s_v.assign( n + 1, 0.0 );
		s_p.assign( n + 1, 0 );
		s_way.assign( n + 1, 0 );
		s_minV.assign( n + 1, 0.0 );

		for ( size_t i = 1; i <= n; i++ ) {
			s_p[ 0 ] = static_cast<int32_t>( i );
			size_t j0 = 0;
			std::fill( s_minV.begin(), s_minV.end(), std::numeric_limits<double>::max() );
			static std::vector<char> s_used;
			s_used.assign( n + 1, 0 );

			do {
				s_used[ j0 ] = 1;
				const size_t i0 = static_cast<size_t>( s_p[ j0 ] );
				double delta = std::numeric_limits<double>::max();
				size_t j1 = 0;
				for ( size_t j = 1; j <= n; j++ ) {
					if ( s_used[ j ] ) {
						continue;
					}
					const double cur = CostAt( i0 - 1, j - 1 ) - s_u[ i0 ] - s_v[ j ];
					if ( cur < s_minV[ j ] ) {
						s_minV[ j ] = cur;
						s_way[ j ] = static_cast<int32_t>( j0 );
					}
					if ( s_minV[ j ] < delta ) {
						delta = s_minV[ j ];
						j1 = j;
					}
				}
				for ( size_t j = 0; j <= n; j++ ) {
					if ( s_used[ j ] ) {
						s_u[ s_p[ j ] ] += delta;
						s_v[ j ] -= delta;
					} else {
						s_minV[ j ] -= delta;
					}
				}
				j0 = j1;
			} while ( s_p[ j0 ] != 0 );

			// Augment along the alternating path.
			do {
				const size_t j1 = static_cast<size_t>( s_way[ j0 ] );
				s_p[ j0 ] = s_p[ j1 ];
				j0 = j1;
			} while ( j0 != 0 );
		}

		// Extract assignment: s_p[j] = row i matched to column j (1-based).
		for ( size_t j = 1; j <= n; j++ ) {
			const int32_t mIdx = s_p[ j ] - 1;
			const int32_t sIdx = static_cast<int32_t>( j ) - 1;
			if ( mIdx >= 0 && mIdx < static_cast<int32_t>( count ) &&
				 sIdx >= 0 && sIdx < static_cast<int32_t>( slots.size() ) ) {
				outMemberToSlotMap[ mIdx ] = sIdx;
				s_slotClaimed[ sIdx ] = true;
			}
		}
	} else {
		for ( size_t r = 0; r < count; r++ ) {
			const int32_t mIdx = s_rankedMembers[ r ].memberIdx;
			const double mLat = s_rankedMembers[ r ].lateral;

			int32_t bestSlotIdx = -1;
			double bestScore = std::numeric_limits<double>::max();

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
	}

	// 4. Ingress-preserving 2-Opt refinement (non-portal path only).
	// The portal path above already produced a globally minimum-cost assignment via the
	// Hungarian algorithm under its full cost model (travel + LOS + side coherence +
	// hysteresis); running a differently-costed 2-Opt pass over it would only degrade that
	// optimum. Restrict the refinement to the rank-window path.
	bool improved = ( portalOrigin == nullptr );
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
				const double progI = ( portalOrigin != nullptr ) ? -QM_Vector3DistanceDP( memberOrigins[ i ], *portalOrigin ) : QM_Vector3DotProductDP( memberOrigins[ i ] - centroid, fwdNorm );
				const double progJ = ( portalOrigin != nullptr ) ? -QM_Vector3DistanceDP( memberOrigins[ j ], *portalOrigin ) : QM_Vector3DotProductDP( memberOrigins[ j ] - centroid, fwdNorm );

				// Measure slot depth of slotI and slotJ using the same metric as ingress ranking.
				const double depthI = ( portalOrigin != nullptr ) ?
					QM_Vector3DistanceDP( slots[ slotI ].worldPosition, *portalOrigin ) :
					QM_Vector3DotProductDP( slots[ slotI ].worldPosition - destOrigin, fwdNorm );
				const double depthJ = ( portalOrigin != nullptr ) ?
					QM_Vector3DistanceDP( slots[ slotJ ].worldPosition, *portalOrigin ) :
					QM_Vector3DotProductDP( slots[ slotJ ].worldPosition - destOrigin, fwdNorm );

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

				auto GetIngressPairCost = [&]( size_t m, int32_t sIdx ) -> double {
					double dSq = QM_Vector3DistanceSqrDP( memberOrigins[ m ], slots[ sIdx ].worldPosition );
					if ( !Nav_HasGeometricLineOfSight2D( memberOrigins[ m ], slots[ sIdx ].worldPosition, 8.0 ) ) {
						dSq += CROWD_SLOT_MATCH_WALL_PENALTY_SQ;
					}
					// Stability hysteresis: holding the member's previous slot receives a squared-distance
					// bonus, so only meaningfully better swaps (not sub-inch oscillations at the doorway
					// threshold) can retarget the member. This is the exact anti-thrash guard for the
					// portal blocker that otherwise flip-flops goals every re-order and yaw-jitters in place.
					const int32_t prevSlot = ( m < previousSlotMap.size() ) ? previousSlotMap[ m ] : -1;
					if ( sIdx == prevSlot ) {
						static constexpr double CROWD_INGRESS_HYSTERESIS_DIST = 96.0;
						static constexpr double hystBonusSq = CROWD_INGRESS_HYSTERESIS_DIST * CROWD_INGRESS_HYSTERESIS_DIST;
						dSq = ( dSq > hystBonusSq ) ? ( dSq - hystBonusSq ) : 0.0;
					}
					return dSq;
				};

				const double currDistSq = GetIngressPairCost( i, slotI ) + GetIngressPairCost( j, slotJ );
				const double swapDistSq = GetIngressPairCost( i, slotJ ) + GetIngressPairCost( j, slotI );

				if ( swapDistSq + 0.001 < currDistSq ) {
					std::swap( outMemberToSlotMap[ i ], outMemberToSlotMap[ j ] );
					improved = true;
				}
			}
		}
	}
}


