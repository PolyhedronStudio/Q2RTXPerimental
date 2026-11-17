//! CLGame Blood Entity Attachment and Basis Projection Implementation
/********************************************************************
*
*
*	ClientGame: FX: "clg_fx_blood_utils.cpp"
*
*	Implementation of orthogonal basis transformations for brush models
*	(bmodels: func_door, func_plat, func_rotating) and dynamic actors.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/fx/clg_fx_blood_utils.h"

/**
*	@brief	Compute orthogonal rotation matrix basis from Euler angles.
*	@param	angles		Pitch, yaw, roll angles in degrees.
*	@param	out_axis	[out] Three orthogonal axis vectors (forward, right, up).
**/
static void BuildEntityBasis( const Vector3 &angles, Vector3 out_axis[3] ) {
	/**
	*	Calculate forward, right, and up directions from Euler angles.
	**/
	QM_AngleVectors( angles, &out_axis[0], &out_axis[1], &out_axis[2] );
	// Invert the right vector to maintain consistent right-handed basis coordinates.
	out_axis[1] = QM_Vector3Scale( out_axis[1], -1.0f );
}

/**
*	@brief	Project a world-space point and normal into an entity's local coordinate frame.
*	@param	entity_num			Index of the struck entity (>0).
*	@param	world_pos			World-space contact point.
*	@param	world_norm			World-space contact normal.
*	@param	out_local_pos		[out] Entity-local relative position.
*	@param	out_local_norm		[out] Entity-local relative normal.
*	@return True if transformation succeeded, false if entity is invalid or dead.
**/
bool CLG_FX_WorldToEntityLocal( const int32_t entity_num, const Vector3 &world_pos, const Vector3 &world_norm, Vector3 *out_local_pos, Vector3 *out_local_norm ) {
	/**
	*	Sanity checks and parameter validation.
	**/
	// Return false if caller passed null pointers.
	if ( out_local_pos == nullptr || out_local_norm == nullptr ) {
		return false;
	}
	// Return false if entity number is out of bounds or represents static worldspawn.
	if ( entity_num <= ENTITYNUM_WORLD || entity_num >= MAX_EDICTS ) {
		return false;
	}

	/**
	*	Entity existence validation.
	**/
	const centity_t &cent = clg_entities[entity_num];
	// Verify that the slot holds the requested active entity number.
	if ( cent.current.number != entity_num ) {
		return false;
	}

	/**
	*	Calculate translation delta and construct orientation basis.
	**/
	// Vector from entity center to impact location.
	const Vector3 delta = QM_Vector3Subtract( world_pos, cent.current.origin );

	// Build orthogonal orientation frame from entity angles.
	Vector3 axis[3];
	BuildEntityBasis( cent.current.angles, axis );

	/**
	*	Project delta and normal into local axis space via dot products.
	**/
	out_local_pos->x = QM_Vector3DotProduct( delta, axis[0] );
	out_local_pos->y = QM_Vector3DotProduct( delta, axis[1] );
	out_local_pos->z = QM_Vector3DotProduct( delta, axis[2] );

	out_local_norm->x = QM_Vector3DotProduct( world_norm, axis[0] );
	out_local_norm->y = QM_Vector3DotProduct( world_norm, axis[1] );
	out_local_norm->z = QM_Vector3DotProduct( world_norm, axis[2] );

	return true;
}

/**
*	@brief	Reconstruct world-space position and normal from an entity's local coordinate frame.
*	@param	entity_num			Index of the attached entity (>0).
*	@param	local_pos			Entity-local relative position.
*	@param	local_norm			Entity-local relative normal.
*	@param	out_world_pos		[out] Reconstructed current world-space position.
*	@param	out_world_norm		[out] Reconstructed current world-space normal.
*	@return True if entity remains valid, false if entity was removed or destroyed.
**/
bool CLG_FX_EntityLocalToWorld( const int32_t entity_num, const Vector3 &local_pos, const Vector3 &local_norm, Vector3 *out_world_pos, Vector3 *out_world_norm ) {
	/**
	*	Sanity checks and parameter validation.
	**/
	// Return false if caller passed null pointers.
	if ( out_world_pos == nullptr || out_world_norm == nullptr ) {
		return false;
	}
	// Return false if entity number is out of bounds or represents static worldspawn.
	if ( entity_num <= ENTITYNUM_WORLD || entity_num >= MAX_EDICTS ) {
		return false;
	}

	/**
	*	Entity existence validation.
	**/
	const centity_t &cent = clg_entities[entity_num];
	// Verify that the slot holds the requested active entity number.
	if ( cent.current.number != entity_num ) {
		return false;
	}

	/**
	*	Construct current orientation basis from entity angles.
	**/
	Vector3 axis[3];
	BuildEntityBasis( cent.current.angles, axis );

	/**
	*	Reconstruct current world-space origin by accumulating axis projections.
	**/
	Vector3 world = cent.current.origin;
	world = QM_Vector3Add( world, QM_Vector3Scale( axis[0], local_pos.x ) );
	world = QM_Vector3Add( world, QM_Vector3Scale( axis[1], local_pos.y ) );
	world = QM_Vector3Add( world, QM_Vector3Scale( axis[2], local_pos.z ) );
	*out_world_pos = world;

	/**
	*	Reconstruct and renormalize current world-space normal vector.
	**/
	Vector3 norm = { 0.0f, 0.0f, 0.0f };
	norm = QM_Vector3Add( norm, QM_Vector3Scale( axis[0], local_norm.x ) );
	norm = QM_Vector3Add( norm, QM_Vector3Scale( axis[1], local_norm.y ) );
	norm = QM_Vector3Add( norm, QM_Vector3Scale( axis[2], local_norm.z ) );
	*out_world_norm = QM_Vector3Normalize( norm );

	return true;
}
