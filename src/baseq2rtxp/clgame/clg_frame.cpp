/********************************************************************
*
*
*	ClientGame: Entity Management.
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_client.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_events.h"
#include "clgame/clg_packet_entities.h"
#include "clgame/clg_parse.h"
#include "clgame/clg_playerstate.h"
#include "clgame/clg_predict.h"
#include "clgame/clg_screen.h"
#include "sharedgame/sg_entity_events.h"

#include "sharedgame/pmove/sg_pmove.h"
#include "sharedgame/sg_entities.h"
#include "sharedgame/sg_entity_types.h"



/**
*
*
*
*	Utility Functions:
*
*
*
**/
/**
*	@brief	Setup the entity numbers for the server received its 'local client' and
*			the predicted client entity.
**/
static void SetupLocalClientEntities( void ) {
	// Ensure to assign.
	if ( clgi.client->clientNumber > -1 ) {
		// Calculate local client entity number.
		const int32_t localClientEntityNumber = clgi.client->clientNumber + 1;
		// Setup the local client entity for this player.
		game.localClientEntity = &clg_entities[ localClientEntityNumber ];
		// Setup local entity number and skinnum(for having the proper clientNumber attached.).
		game.localClientEntity->current = {
			.number = localClientEntityNumber,
			.skinnum = encoded_skinnum_t{
				.clientNumber = ( int16_t )clgi.client->clientNumber
			}.skinnum
		};
		// Copy specific player state data to the server received local client entity.
		SG_PlayerStateToEntityState( clgi.client->clientNumber, &clgi.client->frame.ps, &game.localClientEntity->current, true, false );
		// Copy current state to previous state.
		game.localClientEntity->prev = game.localClientEntity->current;
		// Ensure lerp origins/angles are correct for the initial frame.
		game.localClientEntity->lerpOrigin = game.localClientEntity->current.origin;
		game.localClientEntity->lerpAngles = game.localClientEntity->current.angles;

		/**
		*	Sets up the 'predicted' client entity.
		**/
		// Calculate predicted client entity number.
		const int32_t predictedClientEntityNumber = clgi.client->clientNumber + 1;
		// Setup local entity number and skinnum(for having the proper clientNumber attached.).
		game.predictedEntity.current = {
			.number = predictedClientEntityNumber,
			.skinnum = encoded_skinnum_t{
				.clientNumber = ( int16_t )clgi.client->clientNumber
			}.skinnum
		};
		// Setup predicted player entity's current entity state.
		SG_PlayerStateToEntityState( clgi.client->clientNumber, &game.predictedState.currentPs, &game.predictedEntity.current, true, false );
		// Copy current state to previous state.
		game.predictedEntity.prev = game.predictedEntity.current;
		// Ensure lerp origins/angles are correct for the initial frame.
		game.predictedEntity.lerpOrigin = game.predictedEntity.current.origin;
		game.predictedEntity.lerpAngles = game.predictedEntity.current.angles;

		/**
		*	Setup the predicted player entity's refresh entity.
		**/
		game.predictedEntity.refreshEntity = {
			// Store the frame of the previous refresh entity iteration so it'll default to that.
			//.frame = game.predictedEntity.refreshEntity.frame,
			//.oldframe = packetEntity->refreshEntity.oldframe,
			.id = REFRESHENTITIY_RESERVED_PREDICTED_PLAYER,
			// Restore bone poses cache pointer.
			.bonePoses = game.predictedEntity.refreshEntity.bonePoses,
		};
	} else {
		clgi.Error( ERR_FATAL, "%s: Invalid client number received: %d\n", __func__, clgi.client->clientNumber );
	}
}

/**
*   @brief
**/
static inline void ResetPlayerEntity( centity_t *cent ) {
	// Ensure to assign.
	if ( clgi.client->clientNumber > -1 ) {
		//cent->current.number = clgi.client->clientNumber + 1;
	}
}

/**
*   @return True if origin / angles update has been optimized out to the player state.
**/
static inline bool CheckLocalPlayerStateEntity( const entity_state_t *state ) {
	if ( state->number != clgi.client->frame.ps.clientNumber + 1 ) {
		return false;
	}

	if ( clgi.client->frame.ps.pmove.pm_type >= PM_DEAD ) {
		return false;
	}
	
	if ( state->number == clgi.client->clientNumber + 1 ) {
		return true;
	}

	return false;
}

/**
*   @return True if the SAME entity was NOT IN the PREVIOUS frame.
**/
static inline bool CheckEntityNewForFrame( const centity_t *ent ) {
	// Last received frame was invalid, so this entity is new to the current frame.
	if ( !clgi.client->oldframe.valid ) {
		return true;
	}
	// Wasn't in last received frame, so this entity is new to the current frame.
	if ( ent->serverframe != clgi.client->oldframe.number ) {
		return true;
	}
	// Developer option, always new.
	if ( cl_nolerp->integer == 2 ) {
		return true;
	}
	//! Developer option, lerp from last received frame.
	if ( cl_nolerp->integer == 3 ) {
		return false;
	}
	//! Previous server frame was dropped, so we're new
	//! if ( clgi.client->oldframe.number != clgi.client->frame.number - 1 ) { <-- OLD ORIGINAL.
	if ( clgi.client->oldframe.number != clgi.client->frame.number - clgi.client->serverdelta ) {
		return true;
	}

	return false;
}



/**
*
*
*
*	Solid Entity List:
*		- Deals with building up a list of solid entities, which are used for collision detection.
*		- This list is built up on receiving a new frame from the server.
*
*
*
**/
/**
*   @brief  Builds up a list of 'solid' entities which are residing inside the received frame.
**/
static inline void BuildSolidEntityList( void ) {
	// Reset the count.
	game.frameEntities.numSolids = 0;

	// Iterate over the current frame entity states and add solid entities to the solid list.
	for ( int32_t i = 0; i < clgi.client->frame.numEntities; i++ ) {
		// Fetch next state from states array..
		entity_state_t *nextState = &clgi.client->entityStates[ ( clgi.client->frame.firstEntity + i ) & PARSE_ENTITIES_MASK ];
		// Get entity with the matching number  this state.
		centity_t *ent = &clg_entities[ nextState->number ];

		// Update its next state.
		//ent->next = *nextState;

		// This'll technically never happen, but should it still happen, error out.
		if ( game.frameEntities.numSolids >= MAX_PACKET_ENTITIES ) {
			Com_Error( ERR_DROP, "BuildSolidEntityList: Exceeded MAX_PACKET_ENTITIES limit when building solid entity list.\n" );
			return;
		}

		// If entity is solid, and not the frame's client entity, add it to the solid entity list.
		// (We don't want to add the client's own entity, to prevent issues with self-collision).
		if ( nextState->solid != SOLID_NOT && nextState->solid != SOLID_TRIGGER ) {
			// Do NOT add the frame client entity.
			if ( nextState->number != ( clgi.client->frame.ps.clientNumber + 1 )
				// Only add if we have room.
				&& game.frameEntities.numSolids < MAX_PACKET_ENTITIES
				) {
					// If not a brush model, acquire the bounds from the state. (It will use the clip brush node its bounds otherwise.)
				if ( nextState->solid != BOUNDS_BRUSHMODEL ) {
					// If not a brush bsp entity, decode its mins and maxs.
					MSG_UnpackBoundsUint32(
						bounds_packed_t{
							.u = nextState->bounds
						},
						ent->mins, ent->maxs
					);
					// Clear out the mins and maxs. (Will use the clip brush node its bounds otherwise.)
				} else {
					ent->mins = QM_Vector3Zero();
					ent->maxs = QM_Vector3Zero();
				}

				// Add it to the solids entity list.
				game.frameEntities.solids[ game.frameEntities.numSolids++ ] = ent;
			}
		}
	}
}



/**
*
*
*
*	Entity Receive and Update:
*
*	Callbacks for the client game when it receives entity states from the server,
*	to update them for when entering from new frame or updating from a deltaframe
*
*
*
**/
static inline void EntityState_ResetState( centity_t *cent, const entity_state_t *nextState ) {
    // If the previous snapshot this entity was updated in is at least
    // an event window back in time then we can reset the previous event
    if ( cent->snapShotTime < clgi.client->time - EVENT_VALID_MSEC ) {
        cent->previousEvent = 0;
    }
    // For diminishing rocket / grenade trails.
    cent->trailCount = 1024;

	// Set the lerp origin to the current origin by default.
    cent->lerpOrigin = nextState->origin;
	cent->lerpAngles = nextState->angles;
;
    if ( nextState->entityType == ET_PLAYER ) {
        ResetPlayerEntity( cent );
    }
}

/**
*	@brief	Called when a new frame has been received that contains an entity
*			which was not present in the previous frame.
**/
static inline void EntityState_FrameEnter( centity_t *ent, const entity_state_t *nextState, const Vector3 &newIntendOrigin ) {
    // Assign a unique id to each entity for tracking it over multiple frames.
    static int64_t entity_ctr = 0;
    ent->id = ++entity_ctr;

		// Reset if needed any specific entity state properties.
	EntityState_ResetState( ent, nextState );
	// Duplicate the current state into the previous state so "lerping doesn't hurt anything."
	ent->prev = *nextState;

    #if 1
    ent->current_frame = ent->last_frame = nextState->frame;
    ent->frame_servertime = clgi.client->servertime;
    ent->frame_realtime = clgi.GetRealTime();
    #endif

	// Off the next and current entity event values.
    const int32_t nextEntityEvent = EV_GetEntityEventValue( nextState->event );
    const int32_t currentEntityEvent = EV_GetEntityEventValue( ent->current.event );

	// Check if it is a temporary event entity.
    const bool isTempEventEntity = ( nextState->entityType >= ET_TEMP_EVENT_ENTITY );
	// Check if it is a teleport event.
    const bool isTeleportEvent =
        ( nextEntityEvent != currentEntityEvent
            && ( nextEntityEvent == EV_PLAYER_TELEPORT || nextEntityEvent == EV_OTHER_TELEPORT ) );

	if (// If it isa teleport event, do not lerp. 
		isTeleportEvent
		// If it is a beam entity, do not lerp.
        || ( nextState->entityType == ET_BEAM || nextState->renderfx & RF_BEAM )
		// If it is a temporary event entity, no lerping.
        || isTempEventEntity ) 
	{
        ent->lerpOrigin = newIntendOrigin;
        return;
    }

    ent->prev.origin = nextState->old_origin;
    ent->lerpOrigin = nextState->old_origin;
}
/**
*	@brief	Called when a new frame has been received that contains an entity
*			already present in the previous frame.
**/
static inline void EntityState_FrameUpdate( centity_t *ent, const entity_state_t *state, const Vector3 &newIntendOrigin ) {
    // <Q2RTXP>: WID: TODO: Do we still want/need this per se?
    // Handle proper lerping for animated entities by Hz.    
    #if 1
    if ( ent->current_frame != state->frame ) {
        if ( state->renderfx & RF_OLD_FRAME_LERP ) {
            ent->last_frame = ent->current.old_frame;
        } else {
            ent->last_frame = ent->current.frame;
        }
        ent->current_frame = state->frame;
        ent->frame_servertime = clgi.client->servertime;
        ent->frame_realtime = clgi.GetRealTime();
    }
    #endif
    #if 1
    // Set step height, and server time, if caught stair stepping.
    if ( state->renderfx & RF_STAIR_STEP ) {
        ent->step_height = state->origin[ 2 ] - ent->current.origin[ 2 ];
        ent->step_servertime = clgi.client->servertime;
        ent->step_realtime = clgi.GetRealTime();
    }
    #endif

	// Get absolute difference in origin.
	const Vector3 originDifference = QM_Vector3Fabs( newIntendOrigin - ent->current.origin );
	const double originDifferenceMax = std::max( { originDifference.x, originDifference.y, originDifference.z } );

	// Off the next and current entity event values.
	const int32_t nextEntityEvent = EV_GetEntityEventValue( state->event );
	const int32_t currentEntityEvent = EV_GetEntityEventValue( ent->current.event );

	// Is it a new entity type?
	const bool isNewEntityType = ( state->entityType != ent->current.entityType );
	// Has a new model set.
	const bool hasNewModelSet = ( ent->current.modelindex != state->modelindex || ent->current.modelindex2 != state->modelindex2 || ent->current.modelindex3 != state->modelindex3 || ent->current.modelindex4 != state->modelindex4 );

	// Check if it is a temporary event entity.
	const bool isTempEventEntity = ( state->entityType >= ET_TEMP_EVENT_ENTITY );
	// Check if it is a teleport event.
	const bool isTeleportEvent = ( nextEntityEvent != currentEntityEvent && ( nextEntityEvent == EV_PLAYER_TELEPORT || nextEntityEvent == EV_OTHER_TELEPORT ) );

    /**
	*	Some data changes will force no lerping
	**/
    if (// A new entity type is set. 
		isNewEntityType
		// A new model was set.
        || hasNewModelSet
		// Teleporting events do not lerp.
		|| isTeleportEvent
		// A new temporary event entity, so no lerping.
		|| isTempEventEntity
		// If it exceed 64, one of its axis is responsible for that.
        || ( originDifferenceMax > 64. )
		// Lerping is disabled.
        || ( cl_nolerp->integer == 1 ) )
    {
        // Reset if needed any specific entity state properties.
        EntityState_ResetState( ent, state );
        // Duplicate the current state into the previous state so "lerping doesn't hurt anything."
        ent->prev = *state;

        // No lerping if Teleported or Morphed, so use the passed in current origin.
        ent->lerpOrigin = newIntendOrigin;

        // <Q2RTXP>: WID: TODO: Do we still want/need this per se?
        // Handle proper lerping for animated entities by Hz.    
        #if 1// WID: 40hz
        ent->last_frame = state->frame;
        #endif// WID: 40hz
        return;
    }

    // All checks passed, this entity can lerp properly.
    // Shuffle the last 'current' state to previous
    ent->prev = ent->current;
}



/**
*
*
*
*	Frame Transitioning:
*
*
*
**/
/**
*   @brief  Prepares the client state for 'activation', also setting the predicted values
*           to those of the initially received first valid frame.
**/
void CLG_Frame_SetInitialServerFrame( void ) {
	/**
	*	Make sure to setup the predicted entity and local client entity states immediately.
	**/
	// Setup entity numbers for the local client entity and predicted client entity.
	SetupLocalClientEntities();

    // Rebuild the solid entity list for this frame.
    BuildSolidEntityList();

	#if 0
	// Player States.
	player_state_t *playerState = &clgi.client->frame.ps;
	// Get the client entity for this frame's player state..
	centity_t *frameClientEntity = &clg_entities[ playerState->clientNumber + 1 ];
    // Update the player entity state from the playerstate.
    // This will allow effects and such to be properly placed.
    SG_PlayerStateToEntityState( playerState->clientNumber, playerState, &frameClientEntity->current );
	SG_PlayerStateToEntityState( clgi.client->clientNumber, playerState, &game.predictedEntity.current );
	#endif

	// Iterate through all received entity states for the first frame and update them.
    for ( int32_t i = 0; i < clgi.client->frame.numEntities; i++ ) {
        // Fetch next state from states array..
        entity_state_t *nextState = &clgi.client->entityStates[ ( clgi.client->frame.firstEntity + i ) & PARSE_ENTITIES_MASK ];
        // Get entity with the matching number  this state.
        centity_t *cent = &clg_entities[ nextState->number ];
        
        // Assign new state.
        cent->current = *nextState;

		#if 0
		// If this is the local (client/player -)entity whose origin/angles are optimized out of packet entities,
		// patch in the entity_state_t, the authoritative player_state_t origin/angles before any event processing.
		// Including patching in the entity events that might be present.
		if ( CheckLocalPlayerStateEntity( nextState ) ) {
			SG_PlayerStateToEntityState(
				clgi.client->frame.ps.clientNumber,
				&clgi.client->frame.ps,
				&cent->current,
				true,
				false 
			);
		}

		// Make sure previous matches current for the first valid frame so lerp/origin
		// math (and thus event effect origins) start from a sane value.
		cent->prev = cent->current;
		#endif
		cent->lerpOrigin = cent->current.origin;
		cent->lerpAngles = cent->current.angles;

		// Update serverframe and snapShotTime.
		cent->serverframe = clgi.client->frame.number;
		cent->snapShotTime = clgi.client->servertime;

        // Reset the needed entity state properties.
        EntityState_ResetState( cent, nextState );

        // Fire any needed entity events.
        CLG_Events_CheckForEntity( cent );
    }
    //// Work around Q2PRO server bandwidth optimization.
    //if ( CheckLocalPlayerStateEntity( nextState ) ) {
    //    //Com_PlayerToEntityState( /*&clgi.client->frame.ps*/ &clgi.client->predictedFrame.ps, &ent->current );
    //    SG_PlayerStateToEntityState(
    //        clgi.client->frame.ps.clientNumber,
    //        &clgi.client->frame.ps, &clg_entities[ clgi.client->frame.ps.clientNumber + 1 ].current
    //    );
    //}

    // Call upon client begin if this is not a demo playback.
    if ( !clgi.IsDemoPlayback() ) {
		// Begin the client.
        CLG_ClientBegin();
    } else {
        // Initialize some demo things.
        clgi.InitialDemoFrame();
    }
}

/**
*   @brief  Called after finished parsing the frame data. All entity states and the
*           player state will be transitioned and/or reset as needed, to make sure
*           the client has a proper view of the current server frame. It does the following:
*               - Rebuilds the solid entity list for this frame.
*               - Updates all entity states for this frame.
*               - Fires any entity events for this frame.
*               - Updates the predicted frame.
*               - Initializes the player's own entity position from the received frame's playerstate.
*               - Lerps or snaps the playerstate between the old and current frame.
*               
*				- If demo playback, handles demo frame recording and demo freelook hack.
*
*				- Grabs mouse if player move type changed between frames.
*				- Notifies screen about the delta frame.
*
*               - Checks for prediction errors.
**/
void CLG_Frame_TransitionToNext( void ) {
    // Demo playback?
    const bool isDemoPlayback = clgi.IsDemoPlayback();
    // Demo recording?
    const bool isDemoRecording = clgi.IsDemoRecording();

	// Player States.
	player_state_t *playerState = &clgi.client->frame.ps;
	player_state_t *oldPlayerState = &clgi.client->oldframe.ps;

	/**
	*	Update predicted state frame if acceptable:
	*	  - If the frame is valid.
	*	  - If it's a non-positive frame number (meaning first frame/fully non-delta compressed).
	**/
	if ( clgi.client->frame.number <= 0 || clgi.client->frame.delta >= 0 || clgi.client->frame.valid ) {
		game.predictedState.frame = clgi.client->frame;
		//game.predictedState.currentPs = clgi.client->frame.ps;
	}
    // Predicted Player State.
    player_state_t *predictedPlayerState = &game.predictedState.currentPs;

	/**
	*	Initialize position of the player's own entity from playerstate.
	*	this is needed in situations when player entity is invisible, but
	*	server sends an effect referencing it's origin (such as MZ_LOGIN, etc).
	**/
    // Get the client entity for this frame's player state..
    centity_t *frameClientEntity = &clg_entities[ playerState->clientNumber + 1 ];
    // Update the player entity state from the playerstate.
    // This will allow effects and such to be properly placed.
    SG_PlayerStateToEntityState( playerState->clientNumber, playerState, &frameClientEntity->current, true, false );

    // Iterate over the current frame entity states and add solid entities to the solid list.
    for ( int32_t i = 0; i < clgi.client->frame.numEntities; i++ ) {
        // Fetch next state from states array..
        entity_state_t *nextState = &clgi.client->entityStates[ ( clgi.client->frame.firstEntity + i ) & PARSE_ENTITIES_MASK ];
        // Get entity with the matching number  this state.
        centity_t *cent = &clg_entities[ nextState->number ];
        // Copy it in as the next state.
		//cent->next = *nextState;

		// Is this entity the local server received player entity?
		const bool isLocalPlayerEntity = CheckLocalPlayerStateEntity( nextState );
		// Is this entity new for this frame?
		const bool isNewFrameEntity = CheckEntityNewForFrame( cent );

		// Default either the predicted player state origin, or the next state origin.
        const Vector3 nextOrigin = ( isLocalPlayerEntity ? predictedPlayerState->pmove.origin : nextState->origin );

        /**
		*	See if the entity is new for the current serverframe or not and base our next move on that.
		**/
		// Wasn't in last update, so initialize some things:
        if ( isNewFrameEntity ) {
            EntityState_FrameEnter( cent, nextState, nextOrigin );
		// The entity isn't 'new' for our frame, backup 'current' into 'previous', 
		// and transition its state from 'current' to 'next'. 
		} else {
            EntityState_FrameUpdate( cent, nextState, nextOrigin );
        }

        // Assign new state.
        cent->current = *nextState;

		// Now calculate the default final lerped origin and angles for this entity.
		cent->lerpOrigin = CLG_GetEntityLerpOrigin( cent->prev.origin, cent->current.origin, clgi.client->lerpfrac );
		cent->lerpAngles = CLG_GetEntityLerpAngles( cent->prev.angles, cent->current.angles, clgi.client->lerpfrac );

        // Assign last received server frame number.
        cent->serverframe = clgi.client->frame.number;
		// Assign snapshot time.
        cent->snapShotTime = clgi.client->servertime;

		// Update the player entity state from the playerstate.
        // This will allow effects and such to be properly placed.
        if ( isLocalPlayerEntity ) {
            SG_PlayerStateToMinimalEntityState(
                clgi.client->frame.ps.clientNumber,
                &clgi.client->frame.ps, 
                &game.localClientEntity->current, 
                false
            );
        }

		// Fire any needed entity events.
		CLG_Events_CheckForEntity( cent );
    }

    // If we're recording a demo, make sure to store this frame into the demo data.
    if ( isDemoPlayback ) {
        // Emit the frame into the demo recording.
        clgi.EmitDemoFrame();
        // This delta has nothing to do with local viewangles, CLEAR it to avoid interfering with demo freelook hack.
        clgi.client->frame.ps.pmove.delta_angles = {};
    }

    // Grab mouse in case of player move type changes between frames.
    if ( clgi.client->oldframe.ps.pmove.pm_type != clgi.client->frame.ps.pmove.pm_type ) {
        clgi.ActivateInput();
    }

    // Rebuild the solid entity list for this frame.
    BuildSolidEntityList();

    // if we are not doing client side movement prediction for any
    // reason, then the client events and view changes will be issued now between the last and previous received frames.
    if ( isDemoPlayback || ( clgi.client->clientNumber != clgi.client->frame.ps.clientNumber ) /*|| ( ps.pmove.pm_flags & PMF_FOLLOW ) */ 
		|| !cl_predict->integer /*|| cg_synchronousClients.integer*/ ) {
        //CG_TransitionPlayerState( ps, ops );
        CLG_PlayerState_Transition( &clg_entities[ clgi.client->frame.ps.clientNumber + 1 ], &clgi.client->oldframe, &clgi.client->frame, clgi.client->serverdelta );
        // Otherwise, we are predicting thus expect to check for a prediction error instad.
    } else {
        // See if we had any prediction errors.
        CLG_CheckPredictionError();
    }

    // Notiy screen about the delta frame, so it may adjust some data if needed.
    CLG_SCR_DeltaFrame();
}