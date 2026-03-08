/********************************************************************
*
*
*    ServerGame: Reusable NPC Sound Investigation Helpers
*
*    Shared helpers for NPCs that react to world sound slots without
*    embedding the slot-selection logic into each entity implementation.
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"

struct svg_base_edict_t;



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
svg_base_edict_t *SVG_NPC_FindFreshestAudibleSound( svg_base_edict_t *listener, const QMTime minTime, const double blendWeaponImpactDistance = 512., const bool usePHS = true, const int32_t debugLevel = 0 );
