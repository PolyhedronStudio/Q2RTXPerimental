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
*	Shared Entity ConstExprs Etc:
* 
* 
* 
**/
/**
*	Primary Fire Animation Types for EV_WEAPON_PRIMARY_FIRE event.
**/
static constexpr int32_t EVARG0_PRIMARY_FIRE_ANIMATIONTYPE_STAND = 0;
static constexpr int32_t EVARG0_PRIMARY_FIRE_ANIMATIONTYPE_CROUCH = 1;
static constexpr int32_t EVARG0_PRIMARY_FIRE_ANIMATIONTYPE_WALK = 2;
static constexpr int32_t EVARG0_PRIMARY_FIRE_ANIMATIONTYPE_RUN = 3;

//!  Health threshold for gib death.
static constexpr int32_t GIB_DEATH_HEALTH = -40; // Health threshold for gib death.



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