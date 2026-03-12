/********************************************************************
*
*
*    ServerGame: Reusable NPC Sound Investigation Helpers
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/nav/svg_nav.h"
#include "svgame/nav/svg_nav_clusters.h"
#include "svgame/nav/svg_nav_traversal.h"

#include "svgame/player/svg_player_weapon.h"

#include "svgame/entities/svg_npc_sound_helper.h"

#include <algorithm>
#include <cmath>

/**
*    @brief	Project a sound origin onto the nearest walkable nav Z while preserving XY.
*    @param	origin		World-space sound origin to normalize.
*    @param	out_origin	[out] Projected origin when a walkable layer is found.
*    @return	True when the sound origin was projected onto a walkable nav layer.
*    @note	This keeps sound-follow goals on reachable floors without inventing a new XY target.
**/
static bool SVG_NPCSound_TryProjectOriginToWalkableZ( const Vector3 &origin, Vector3 *out_origin ) {
	/**
	*    Sanity checks: require navmesh storage and an output buffer.
	**/
	const nav_mesh_t *mesh = g_nav_mesh.get();
	if ( !mesh || !out_origin ) {
		return false;
	}

	/**
	*    Resolve the agent hull used for nav-center conversion.
	**/
	Vector3 agent_mins = mesh->agent_mins;
	Vector3 agent_maxs = mesh->agent_maxs;
	const bool mesh_agent_valid = ( agent_maxs.x > agent_mins.x ) && ( agent_maxs.y > agent_mins.y ) && ( agent_maxs.z > agent_mins.z );
	if ( !mesh_agent_valid ) {
		const nav_agent_profile_t agent_profile = SVG_Nav_BuildAgentProfileFromCvars();
		if ( !( agent_profile.maxs.x > agent_profile.mins.x ) || !( agent_profile.maxs.y > agent_profile.mins.y ) || !( agent_profile.maxs.z > agent_profile.mins.z ) ) {
			return false;
		}
		agent_mins = agent_profile.mins;
		agent_maxs = agent_profile.maxs;
	}

	/**
	*    Convert the sound origin into nav-center space and inspect the current XY cell.
	**/
    const float center_offset_z = ( agent_mins.z + agent_maxs.z ) * 0.5f;
	Vector3 center_origin = origin;
	center_origin.z += center_offset_z;
	const nav_tile_cluster_key_t tile_key = SVG_Nav_GetTileKeyForPosition( mesh, center_origin );
	const nav_world_tile_key_t world_tile_key = { .tile_x = tile_key.tile_x, .tile_y = tile_key.tile_y };
	const auto tile_it = mesh->world_tile_id_of.find( world_tile_key );
	if ( tile_it == mesh->world_tile_id_of.end() ) {
		return false;
	}

	const nav_tile_t *tile = &mesh->world_tiles[ tile_it->second ];
	const auto cells_view = SVG_Nav_Tile_GetCells( mesh, tile );
	const nav_xy_cell_t *cells = cells_view.first;
	const int32_t cell_count = cells_view.second;
	if ( !cells || cell_count <= 0 ) {
		return false;
	}

	const double tile_world_size = ( double )mesh->tile_size * mesh->cell_size_xy;
	const double tile_origin_x = ( double )tile->tile_x * tile_world_size;
	const double tile_origin_y = ( double )tile->tile_y * tile_world_size;
	const double local_x = center_origin.x - tile_origin_x;
	const double local_y = center_origin.y - tile_origin_y;
	if ( local_x < 0.0 || local_y < 0.0 ) {
		return false;
	}

	const int32_t cell_x = std::clamp( ( int32_t )std::floor( local_x / mesh->cell_size_xy ), 0, mesh->tile_size - 1 );
	const int32_t cell_y = std::clamp( ( int32_t )std::floor( local_y / mesh->cell_size_xy ), 0, mesh->tile_size - 1 );
	const int32_t cell_index = ( cell_y * mesh->tile_size ) + cell_x;
	if ( cell_index < 0 || cell_index >= cell_count ) {
		return false;
	}

	const nav_xy_cell_t *cell = &cells[ cell_index ];
	if ( !cell || cell->num_layers <= 0 || !cell->layers ) {
		return false;
	}

	int32_t layer_index = -1;
	if ( !Nav_SelectLayerIndex( mesh, cell, center_origin.z, &layer_index ) || layer_index < 0 || layer_index >= cell->num_layers ) {
		return false;
	}

	/**
	*    Convert the chosen walkable layer height back into the caller's feet-origin space.
	**/
	const double projected_center_z = ( double )cell->layers[ layer_index ].z_quantized * mesh->z_quant;
	*out_origin = origin;
	out_origin->z = ( float )( projected_center_z - center_offset_z );
	return true;
}


/**
*	@brief	Find the freshest audible sound entity newer than `minTime`.
*	@param	listener	NPC evaluating whether a world sound is worth reacting to.
*	@param	minTime	Lower timestamp bound used to ignore already-consumed sound events.
*	@param	blendWeaponImpactDistance	`Weapon Use Sounds` and their `Weapon Impact` sounds, may occure in the same frame.
*			When blendWeaponImpactDistance is set (` > 0. `) a distance check is performed and makes
*			impact sounds audible over weapon fire sounds if the listener is within the specified distance to the impact.
*			This allows NPCs to react to weapon impacts even if they are occluded by the weapon fire sound,
*			as long as they are close enough to the impact.
*			These sounds can occure in the same frame, even if they would normally be occluded.
*			Set to 0 to disable this feature.
*	@param	usePHS	When true, require the sound source to be audible via the PHS helper.
*	@param	debugLevel	Optional debug verbosity forwarded into the audibility helper.
*	@return	The freshest eligible sound entity, or `nullptr` when no usable sound exists.
**/
svg_base_edict_t *SVG_NPC_FindFreshestAudibleSound( svg_base_edict_t *listener, const QMTime minTime, const double blendWeaponImpactDistance, const bool usePHS, const int32_t debugLevel ) {
	/**
	*    Sanity-check the listener pointer first.
	**/
	// The caller must provide a valid listener entity so audibility checks have context.
	if ( !listener ) {
		return nullptr;
	}
	#if 0
	// This is the first entity to prefer by default. ( No weapon/impact sound blending. )
	svg_base_edict_t *firstAudibleEntity = level.weapon_sound_entity;
	// This is the second entity to prefer by default. ( No weapon/impact sound blending. )
	svg_base_edict_t *secondAudibleEntity = level.impact_sound_entity;

	// Distance check for the listener to the impact sound, used to determine whether to prefer the impact sound over the weapon sound when both occur in the same frame and blending is enabled.
	double distanceToImpact = 0.;
	if ( SVG_PlayerNoise_IsEntityAlive( level.weapon_sound_entity ) && SVG_PlayerNoise_IsEntityAlive( level.impact_sound_entity ) ) {
		distanceToImpact = QM_Vector3LengthSqr( listener->currentOrigin - level.impact_sound_entity->currentOrigin );
		if ( distanceToImpact <= blendWeaponImpactDistance ) {
			firstAudibleEntity = level.impact_sound_entity;
			secondAudibleEntity = level.weapon_sound_entity;
		} else {
			firstAudibleEntity = level.weapon_sound_entity;
			secondAudibleEntity = level.impact_sound_entity;
		}
	}
	#else
	// This is the first entity to prefer by default. ( No weapon/impact sound blending. )
	svg_base_edict_t *firstAudibleEntity = level.impact_sound_entity;
	// This is the second entity to prefer by default. ( No weapon/impact sound blending. )
	svg_base_edict_t *secondAudibleEntity = level.weapon_sound_entity;
	#endif
	/**
	*    Select the freshest raw sound-slot candidate newer than the caller's lower bound.
	**/
	// Stores the weapon or impact sound entity based on the distance between the listener and the impact point. 
	// This allows NPCs to react to weapon impacts even if they are occluded by the weapon fire sound, 
	// as long as they are close enough to the impact.
	// Track the newest sound entity before optional audibility filtering.
	svg_base_edict_t *freshestSound = nullptr;

		// If blending is disabled we just prefer the weapon sound first, impact second and personal last, 
	// based on the assumption that weapon sounds are more likely to be occluded by their impact sounds than the other way around, 
	// and that personal sounds are generally less important to react to than world sounds.
	// Prefer the primary world sound slot when it contains a newer event.
	if ( SVG_PlayerNoise_IsEntityAlive( level.impact_sound_entity )
		&& level.impact_sound_entity->last_sound_time > minTime ) {
		freshestSound = ( usePHS == true
			&& SVG_Util_IsEntityAudibleByPHS( listener, level.impact_sound_entity, true, debugLevel ) )
			? level.impact_sound_entity : nullptr;
	}
	// Replace the candidate when the secondary world sound slot is even newer.
	if ( SVG_PlayerNoise_IsEntityAlive( level.weapon_sound_entity )
		&& level.weapon_sound_entity->last_sound_time > minTime ) {
		if ( !freshestSound || level.weapon_sound_entity->last_sound_time > freshestSound->last_sound_time ) {
			freshestSound = ( usePHS == true
				&& SVG_Util_IsEntityAudibleByPHS( listener, level.weapon_sound_entity, true, debugLevel ) )
				? level.weapon_sound_entity : nullptr;
		}
	}
	// Replace the candidate when personal noise sound slot contains an even newer event.
	if ( SVG_PlayerNoise_IsEntityAlive( level.personal_sound_entity )
		&& level.personal_sound_entity->last_sound_time > minTime ) {
		if ( !freshestSound || level.personal_sound_entity->last_sound_time > freshestSound->last_sound_time ) {
			freshestSound = level.personal_sound_entity;
		}
	}

	#if 0
	/**
	*	We prefer the first audible entity candidate when it contains a newer sound event than the caller's provided lower bound.
	*	This is either the weapon sound or the impact sound, depending on the distance of the listener to the impact point when 
	*	both events occur in the same frame and `distance blending` is enabled.
	* 
	*	
	**/
	if ( SVG_PlayerNoise_IsEntityAlive( firstAudibleEntity )
		&& firstAudibleEntity->last_sound_time > minTime ) 
	{
			freshestSound = firstAudibleEntity;
	}
	// Replace the candidate when the secondary world sound slot is even newer.
	if ( SVG_PlayerNoise_IsEntityAlive( secondAudibleEntity )
		&& secondAudibleEntity->last_sound_time > minTime ) 
	{
		if ( !freshestSound || secondAudibleEntity->last_sound_time > freshestSound->last_sound_time ) {
			freshestSound = secondAudibleEntity;
		}
	}
	// Replace the candidate when personal noise sound slot contains an even newer event.
	if ( SVG_PlayerNoise_IsEntityAlive( level.personal_sound_entity )
		&& level.personal_sound_entity->last_sound_time > minTime ) 
	{
		if ( !freshestSound || level.personal_sound_entity->last_sound_time > freshestSound->last_sound_time ) {
			freshestSound = level.personal_sound_entity;
		}
	}
	#endif

	#if 0
	// If blending is disabled we just prefer the weapon sound first, impact second and personal last, 
	// based on the assumption that weapon sounds are more likely to be occluded by their impact sounds than the other way around, 
	// and that personal sounds are generally less important to react to than world sounds.
	// Prefer the primary world sound slot when it contains a newer event.
	if ( SVG_PlayerNoise_IsEntityAlive( level.weapon_sound_entity ) 
		&& level.weapon_sound_entity->last_sound_time > minTime )
	{
		freshestSound = ( usePHS == true 
			&& SVG_Util_IsEntityAudibleByPHS( listener, level.weapon_sound_entity, true, debugLevel ) )
			? level.weapon_sound_entity : nullptr;
	}
	// Replace the candidate when the secondary world sound slot is even newer.
	if ( SVG_PlayerNoise_IsEntityAlive( level.impact_sound_entity ) 
		&& level.impact_sound_entity->last_sound_time > minTime ) 
	{
		if ( !freshestSound || level.impact_sound_entity->last_sound_time > freshestSound->last_sound_time ) {
			freshestSound = level.impact_sound_entity;
		}
	}
	// Replace the candidate when personal noise sound slot contains an even newer event.
	if ( SVG_PlayerNoise_IsEntityAlive( level.personal_sound_entity ) 
		&& level.personal_sound_entity->last_sound_time > minTime ) 
	{
		if ( !freshestSound || level.personal_sound_entity->last_sound_time > freshestSound->last_sound_time ) {
			freshestSound = level.personal_sound_entity;
		}
	}
	#endif

	/**
	*    Optionally require that the chosen sound is actually audible to the listener.
	**/
	// Return immediately when there is no fresh sound candidate at all.
	if ( !freshestSound ) {
		return nullptr;
	}

	/**
	*    Normalize the chosen sound origin onto a walkable nav layer before audibility checks.
	**/
	Vector3 snapped_sound_origin = freshestSound->currentOrigin;
	if ( SVG_NPCSound_TryProjectOriginToWalkableZ( freshestSound->currentOrigin, &snapped_sound_origin )
		&& std::fabs( snapped_sound_origin.z - freshestSound->currentOrigin.z ) > 0.001f ) {
		SVG_Util_SetEntityOrigin( freshestSound, snapped_sound_origin, true );
		VectorSubtract( snapped_sound_origin, freshestSound->maxs, freshestSound->absMin );
		VectorAdd( snapped_sound_origin, freshestSound->maxs, freshestSound->absMax );
		gi.linkentity( freshestSound );
	}

	// Reject the candidate when the caller requested a PHS audibility gate and the sound fails it.
	if ( usePHS && !SVG_Util_IsEntityAudibleByPHS( listener, freshestSound, true, debugLevel ) ) {
		return nullptr;
	}

	// Return the validated freshest sound source.
	return freshestSound;
}


