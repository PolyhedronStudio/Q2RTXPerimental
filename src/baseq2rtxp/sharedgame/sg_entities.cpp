/********************************************************************
*
*
*	SharedGame: Shared Entity Functions.
*
*
********************************************************************/
#include "shared/shared.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_entities.h"



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
* 
*			This is done after each set of usercmd_t on the server,
*			and after local prediction on the client.
**/
void SG_PlayerStateToEntityState( const int32_t clientNumber, player_state_t *playerState, entity_state_t *entityState, const bool snapOrigin ) {
	#if 0
	//! Setup the skin number.
	sg_player_skinnum_t skinnum;
	//! Set the client number.
	skinnum.clientNumber = (uint8_t)clientNumber;
	//! Set the view weapon index.
	skinnum.viewWeaponIndex = playerState->gun.modelIndex;
	// Set the entity state values.
	entityState->skinnum = skinnum.skinnum;
	#endif

	#if 0
		// Copy over the playerState values.
		entityState->number = clientNumber;
		entityState->entityType = ET_PLAYER;
		entityState->modelindex = MODELINDEX_PLAYER;
		entityState->modelindex2 = playerState->gun.modelIndex; // View weapon model.
		entityState->frame = playerState->gun.animationID;
		//entityState->frame = playerState->gun.animationID & ~GUN_ANIMATION_TOGGLE_BIT;
		entityState->angles = playerState->viewangles; // VectorCopy( playerState->viewangles, entityState->angles );
		// Copy over the origin.
		if ( snapOrigin ) {
			// Snap to integer coordinates for network transmission.
			entityState->origin[ 0 ] = (int32_t)playerState->pmove.origin[ 0 ];
			entityState->origin[ 1 ] = (int32_t)playerState->pmove.origin[ 1 ];
			entityState->origin[ 2 ] = (int32_t)playerState->pmove.origin[ 2 ];
		} else {
			// Copy without snapping.
			entityState->origin[ 0 ] = playerState->pmove.origin[ 0 ];
			entityState->origin[ 1 ] = playerState->pmove.origin[ 1 ];
			entityState->origin[ 2 ] = playerState->pmove.origin[ 2 ];
		}
	#else
		// Copy without snapping.
		VectorCopy( playerState->pmove.origin, entityState->origin );

		double pitch = playerState->viewangles[ PITCH ];
		if ( pitch > 180. ) {
			pitch -= 360.;
		}
		entityState->angles[ PITCH ] = pitch / 3.;
		entityState->angles[ YAW ] = playerState->viewangles[ YAW ];
		entityState->angles[ ROLL ] = 0.;
	#endif

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