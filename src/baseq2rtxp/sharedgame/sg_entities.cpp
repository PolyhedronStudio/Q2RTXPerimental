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
static void PlayerStateEventsToEntityStateEvent( player_state_t *playerState, entity_state_t *entityState ) {
	// Check for an external event first.
	if ( playerState->externalEvent ) {
		entityState->event = playerState->externalEvent;
		entityState->eventParm0 = playerState->externalEventParm0;
		entityState->eventParm1 = playerState->externalEventParm1;
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
		entityState->eventParm0 = playerState->eventParms[ eventSequence ];
		entityState->eventParm1 = 0; // playerState->eventParms1[ eventSequence ];

		// Increment event sequence.
		playerState->entityEventSequence++;
	}
}
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
	encoded_skinnum_t encodedSkin;
	//! Set the client number.
	encodedSkin.clientNumber = (uint8_t)clientNumber;
	//! Set the view weapon index.
	encodedSkin.viewWeaponIndex = (uint8_t)playerState->gun.modelIndex;
	//! Set the view height.
	encodedSkin.viewHeight = (int8_t)playerState->pmove.viewheight;
	//encodedSkin.viewHeight = 0; // Unused for now.
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
	PlayerStateEventsToEntityStateEvent( playerState, entityState );
}

/**
*	@brief	Convert a player state to entity state.
*
*			Does not apply ET_PLAYER or MODELINDEX_PLAYER to the entity state.
* 			Used for special cases where we need more control over the entity state setup:
*			(e.g.: When creating pending temporary entities for PMove.)
**/
void SG_PlayerStateToMinimalEntityState( const int32_t clientNumber, player_state_t *playerState, entity_state_t *entityState, const bool snapOrigin ) {
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
	PlayerStateEventsToEntityStateEvent( playerState, entityState );
}