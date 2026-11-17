//! CLGame Blood Entity Attachment and Basis Projection Utilities
/********************************************************************
*
*
*	ClientGame: FX: "clg_fx_blood_utils.h"
*
*	Mathematical utility functions to project world-space blood impacts
*	into entity-local coordinate frames for moving brush models (doors,
*	plats, trains) and actors, and reproject them back each frame.
*
*
********************************************************************/
#pragma once

#include "sharedgame/sg_shared.h"
#include <cstdint>

/**
*	@brief	Project a world-space point and normal into an entity's local coordinate frame.
*	@param	entity_num			Index of the struck entity (>0).
*	@param	world_pos			World-space contact point.
*	@param	world_norm			World-space contact normal.
*	@param	out_local_pos		[out] Entity-local relative position.
*	@param	out_local_norm		[out] Entity-local relative normal.
*	@return True if transformation succeeded, false if entity is invalid or dead.
**/
bool CLG_FX_WorldToEntityLocal( const int32_t entity_num, const Vector3 &world_pos, const Vector3 &world_norm, Vector3 *out_local_pos, Vector3 *out_local_norm );

/**
*	@brief	Reconstruct world-space position and normal from an entity's local coordinate frame.
*	@param	entity_num			Index of the attached entity (>0).
*	@param	local_pos			Entity-local relative position.
*	@param	local_norm			Entity-local relative normal.
*	@param	out_world_pos		[out] Reconstructed current world-space position.
*	@param	out_world_norm		[out] Reconstructed current world-space normal.
*	@return True if entity remains valid, false if entity was removed or destroyed.
**/
bool CLG_FX_EntityLocalToWorld( const int32_t entity_num, const Vector3 &local_pos, const Vector3 &local_norm, Vector3 *out_world_pos, Vector3 *out_world_norm );
