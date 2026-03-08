/********************************************************************
*
*
*    ServerGame: Reusable NPC Sound Investigation Helpers
*
*
********************************************************************/
#include "svgame/svg_local.h"
#include "svgame/svg_utils.h"

#include "svgame/player/svg_player_weapon.h"

#include "svgame/entities/svg_npc_sound_helper.h"


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

	/**
	*    Select the freshest raw sound-slot candidate newer than the caller's lower bound.
	**/
	// Stores the weapon or impact sound entity based on the distance between the listener and the impact point. 
	// This allows NPCs to react to weapon impacts even if they are occluded by the weapon fire sound, 
	// as long as they are close enough to the impact.
	// Track the newest sound entity before optional audibility filtering.
	svg_base_edict_t *freshestSound = nullptr;

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
	// Reject the candidate when the caller requested a PHS audibility gate and the sound fails it.
	if ( usePHS && !SVG_Util_IsEntityAudibleByPHS( listener, freshestSound, true, debugLevel ) ) {
		return nullptr;
	}

	// Return the validated freshest sound source.
	return freshestSound;
}
