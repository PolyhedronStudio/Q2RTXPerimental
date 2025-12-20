/********************************************************************
*
*
*   Shared Entity Functions:
*
*
********************************************************************/
#pragma once



/**
*
*
*
*   Entity State:
*
*
*
**/
/**
*	@brief	Convert a player state to entity state.
**/
void SG_PlayerStateToEntityState( const int32_t clientNumber, player_state_t *playerState, entity_state_t *entityState, const bool playerStatEventsToEntityState = true,const bool snapOrigin = false );
/**
*	@brief	Convert a player state to entity state.
*
*			Does not apply ET_PLAYER or MODELINDEX_PLAYER to the entity state.
* 			Used for special cases where we need more control over the entity state setup:
*			(e.g.: When creating pending temporary entities for PMove.)
**/
void SG_PlayerStateToMinimalEntityState( const int32_t clientNumber, player_state_t *playerState, entity_state_t *entityState, const bool snapOrigin );