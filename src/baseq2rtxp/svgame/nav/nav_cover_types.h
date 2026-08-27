#pragma once

#include "svgame/svg_local.h"
#include "svgame/svg_pushmove_info.h"
#include <algorithm>

/**
*	@brief	Cover posture classification.
**/
enum nav_cover_type_t : uint8_t {
	//! Low obstacle (crouch cover, e.g. ~32 to 52 units tall).
	NAV_COVER_LOW = 0,
	//! Full-height obstacle (standing cover, e.g. > 56 units tall).
	NAV_COVER_HIGH = 1,
	//! Any posture / no posture restriction.
	NAV_COVER_NONE = 2
};

/**
*	@brief	Peeking capability flags for lean/step-out attacks.
**/
enum nav_cover_peek_flags_t : uint8_t {
	NAV_COVER_PEEK_NONE   = 0,
	//! Peeking or stepping out around the left corner is possible.
	NAV_COVER_PEEK_LEFT   = 1 << 0,
	//! Peeking or stepping out around the right corner is possible.
	NAV_COVER_PEEK_RIGHT  = 1 << 1,
	//! Peeking/firing over the top of the obstacle is possible (low cover).
	NAV_COVER_PEEK_OVER   = 1 << 2,
	//! Cover point is positioned at a corner vertex bend with multi-angle sightlines.
	NAV_COVER_PEEK_CORNER = 1 << 3
};

/**
*	@brief	Dynamic behavior and attachment flags for mover-bound cover points.
**/
enum nav_cover_flags_t : uint16_t {
	NAV_COVER_FLAG_NONE                 = 0,
	//! Attached to a MOVETYPE_PUSH / MOVETYPE_STOP mover (position/normal relative to mover).
	NAV_COVER_FLAG_MOVER_BOUND          = 1 << 0,
	//! Occlusion is gated by a door entity; valid only when door is closed (e.g. PUSHMOVE_STATE_BOTTOM).
	NAV_COVER_FLAG_REQUIRES_DOOR_CLOSED = 1 << 1,
	//! Invalid for non-riders while parent entity is actively moving (velocity != 0).
	NAV_COVER_FLAG_REJECT_WHILE_MOVING  = 1 << 2
};

/**
*	@brief	Precalculated tactical cover point on the navmesh.
*	@details	Stores entity-local coordinates and dynamic mover linkages so cover
*				positions remain accurate on moving platforms, elevators, and doors.
**/
struct nav_cover_point_t {
	//! Local position relative to parent entity origin/orientation (World position if parent is ENTITYNUM_NONE).
	Vector3 local_position = { 0.0f, 0.0f, 0.0f };
	//! Local outward normal pointing away from the wall/obstacle (into the open area).
	Vector3 local_normal = { 0.0f, 0.0f, 0.0f };
	//! Local tangent vector along the wall edge (for stepping out / peeking).
	Vector3 local_tangent = { 0.0f, 0.0f, 0.0f };
	//! Navmesh face ID where this point is located.
	int32_t face_idx = -1;
	//! Entity ID this cover point is physically attached to (ENTITYNUM_NONE if static world).
	int32_t parent_entity_id = ENTITYNUM_NONE;
	//! Optional door/transition entity gating this cover's validity.
	int32_t transition_entity_id = ENTITYNUM_NONE;
	//! Height of the protective obstacle behind this point.
	float wall_height = 0.0f;
	//! Posture type (Low vs High).
	nav_cover_type_t cover_type = NAV_COVER_LOW;
	//! Peeking flags.
	uint8_t peek_flags = NAV_COVER_PEEK_NONE;
	//! Dynamic mover behavior flags.
	uint16_t cover_flags = NAV_COVER_FLAG_NONE;

	/**
	*	Runtime transient state (not serialized to .nav7):
	**/
	//! Entity ID currently claiming this cover point (ENTITYNUM_NONE if available).
	mutable int32_t claimed_by_ent = ENTITYNUM_NONE;
	//! Game timestamp until which reservation holds.
	mutable QMTime claim_expiration = 0_ms;
	//! Game timestamp until which this cover point is on cooldown (e.g. after compromise/abandonment).
	mutable QMTime cooldown_until = 0_ms;
};

/**
*	@brief		Resolve a cover point's current world-space position, normal, and tangent.
*	@param	cover			Cover point record to resolve.
*	@param	out_pos			[out] Transformed world-space feet position.
*	@param	out_normal		[out] Transformed world-space outward normal.
*	@param	out_tangent		[out] Optional transformed world-space edge tangent.
*	@return	True when the parent entity is alive/valid and coordinates were resolved.
**/
inline const bool Nav_GetCoverPointWorld( const nav_cover_point_t &cover, Vector3 *out_pos, Vector3 *out_normal, Vector3 *out_tangent = nullptr ) {
	/**
	*	Validate output pointers.
	**/
	// Check if caller provided valid destination pointers for position and normal.
	if ( !out_pos || !out_normal ) {
		return false;
	}

	/**
	*	Static world geometry: local coordinates match world coordinates directly.
	**/
	// If the cover point belongs to the static worldspawn, copy directly without transforms.
	if ( cover.parent_entity_id == ENTITYNUM_NONE || cover.parent_entity_id == ENTITYNUM_WORLD ) {
		*out_pos = cover.local_position;
		*out_normal = cover.local_normal;
		if ( out_tangent ) {
			*out_tangent = cover.local_tangent;
		}
		return true;
	}

	/**
	*	Dynamic parent mover: transform local coordinates by parent edict's origin and angles.
	**/
	// Bounds check parent entity index against active edict pool.
	if ( cover.parent_entity_id <= 0 || cover.parent_entity_id >= g_edict_pool.num_edicts ) {
		return false;
	}

	// Fetch parent mover edict.
	const svg_base_edict_t *parent_ent = g_edicts[ cover.parent_entity_id ];
	// If parent is missing or freed, this dynamic cover point is invalid.
	if ( !parent_ent || !parent_ent->inUse ) {
		return false;
	}

	// Calculate rotation basis vectors from parent mover angles.
	Vector3 forward = {}, right = {}, up = {};
	QM_AngleVectors( parent_ent->currentAngles, &forward, &right, &up );

	// Transform local position to world space: Origin + (Forward * x + Right * y + Up * z).
	*out_pos = QM_Vector3Add( parent_ent->currentOrigin,
		QM_Vector3Add(
			QM_Vector3Scale( forward, cover.local_position.x ),
			QM_Vector3Add(
				QM_Vector3Scale( right, cover.local_position.y ),
				QM_Vector3Scale( up, cover.local_position.z )
			)
		)
	);

	// Rotate local normal by parent basis.
	*out_normal = QM_Vector3Normalize(
		QM_Vector3Add(
			QM_Vector3Scale( forward, cover.local_normal.x ),
			QM_Vector3Add(
				QM_Vector3Scale( right, cover.local_normal.y ),
				QM_Vector3Scale( up, cover.local_normal.z )
			)
		)
	);

	// Rotate local tangent if caller requested tangent output.
	if ( out_tangent ) {
		*out_tangent = QM_Vector3Normalize(
			QM_Vector3Add(
				QM_Vector3Scale( forward, cover.local_tangent.x ),
				QM_Vector3Add(
					QM_Vector3Scale( right, cover.local_tangent.y ),
					QM_Vector3Scale( up, cover.local_tangent.z )
				)
			)
		);
	}

	return true;
}

/**
*	@brief		Verify whether a cover point is currently usable given dynamic mover states.
*	@param	cover		Cover point to test.
*	@param	requester	Optional entity requesting cover (to verify rider status on moving platforms).
*	@return	True when the cover point is active, unblocked, and safely accessible.
**/
inline const bool Nav_IsCoverPointUsable( const nav_cover_point_t &cover, const svg_base_edict_t *requester = nullptr ) {
	/**
	*	Check door transition gating (e.g. blast doors that must be closed to provide cover).
	**/
	// If this cover is associated with a dynamic door/mover, verify the door's state.
	if ( cover.transition_entity_id > 0 && cover.transition_entity_id < g_edict_pool.num_edicts ) {
		const svg_base_edict_t *door_ent = g_edicts[ cover.transition_entity_id ];
		if ( door_ent && door_ent->inUse ) {
			// If cover requires the door to be closed, invalidate if door is open or opening.
			if ( ( cover.cover_flags & NAV_COVER_FLAG_REQUIRES_DOOR_CLOSED ) != 0 && door_ent->pushMoveInfo.state != PUSHMOVE_STATE_BOTTOM ) {
				return false;
			}
		}
	}

	/**
	*	Check parent mover motion (e.g. trains, elevators in flight).
	**/
	// If attached to a moving parent entity, check motion constraints.
	if ( cover.parent_entity_id > 0 && cover.parent_entity_id < g_edict_pool.num_edicts ) {
		const svg_base_edict_t *parent_ent = g_edicts[ cover.parent_entity_id ];
		if ( !parent_ent || !parent_ent->inUse ) {
			return false;
		}

		// Check if parent is actively translating or rotating.
		const bool is_moving = ( parent_ent->movetype == MOVETYPE_PUSH ) &&
			( ( QM_Vector3DotProduct( parent_ent->velocity, parent_ent->velocity ) > 0.01f ) ||
			  ( QM_Vector3DotProduct( parent_ent->avelocity, parent_ent->avelocity ) > 0.01f ) );

		// If motion rejection flag is set and entity is in motion:
		if ( is_moving && ( ( cover.cover_flags & NAV_COVER_FLAG_REJECT_WHILE_MOVING ) != 0 ) ) {
			// If requester is already riding this mover, it is safe to use; non-riders cannot safely path to it.
			if ( requester && requester->groundInfo.entityNumber != parent_ent->s.number ) {
				return false;
			}
		}
	}

	return true;
}
