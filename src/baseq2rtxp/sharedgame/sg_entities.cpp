/********************************************************************
*
*
*	SharedGame: Shared Entity Functions.
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_entity_types.h"
#include "sharedgame/sg_entities.h"



/**
*
*
* 
*
*   Entity State:
*
* 
*
*
**/
/**
*	@brief	Convert a player state to entity state.
* 
*			This is done after each set of usercmd_t on the server,
*			and after local prediction on the client.
**/
void SG_PlayerStateToEntityState( const int32_t clientNumber, player_state_t *playerState, entity_state_t *entityState, const bool snapOrigin ) {
	/**
	*	Entity Type, Model Index, etc:
	**/
	entityState->entityType = ET_PLAYER;
	entityState->modelindex = MODELINDEX_PLAYER;

	/**
	*	Skin and Weapon Model Index:
	**/
	//! Setup the skin number.
	sg_player_skinnum_t encodedSkin;
	//! Set the client number.
	encodedSkin.clientNumber = (uint8_t)clientNumber;
	//! Set the view weapon index.
	encodedSkin.viewWeaponIndex = (uint8_t)playerState->gun.modelIndex;
	// Set the entity state values.
	entityState->skinnum = encodedSkin.skinnum;

	/**
	*	Origin:
	**/
	entityState->origin = ( snapOrigin ? QM_Vector3Snap( playerState->pmove.origin ) : playerState->pmove.origin );

	/**
	*	Angles:
	**/
	double pitch = playerState->viewangles[ PITCH ];
	if ( pitch > 180. ) {
		pitch -= 360.;
	}
	entityState->angles[ PITCH ] = pitch / 3.;
	entityState->angles[ YAW ] = playerState->viewangles[ YAW ];
	entityState->angles[ ROLL ] = 0.;

	/**
	*	Check for Events:
	**/
	// Check for an external event first.
	if ( playerState->externalEvent ) {
		entityState->event = playerState->externalEvent;
		entityState->eventParm = playerState->externalEventParm;
	// Otherwise, check for PMove generated events.
	} else if ( playerState->entityEventSequence < playerState->eventSequence ) {
		// Don't allow events to be generated if they are too old.
		if ( playerState->entityEventSequence < playerState->eventSequence - MAX_PS_EVENTS ) {
			playerState->entityEventSequence = playerState->eventSequence - MAX_PS_EVENTS;
		}
		
		// Calculate the event index.
		const int64_t eventSequence = playerState->entityEventSequence & ( MAX_PS_EVENTS - 1 );
		// Set the event.
		entityState->event = playerState->events[ eventSequence ] | ( ( playerState->entityEventSequence & 3 ) << 8 );
		entityState->eventParm = playerState->eventParms[ eventSequence ];

		// Increment event sequence.
		playerState->entityEventSequence++;
	}
}