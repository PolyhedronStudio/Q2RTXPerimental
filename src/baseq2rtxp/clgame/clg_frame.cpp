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
*   @brief
**/
static inline void ResetPlayerEntity( centity_t *cent ) {
    
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
    cent->trailcount = 1024;

	// Set the lerp origin to the current origin by default.
    cent->lerp_origin = cent->current.origin;
    //cent->lerp_angles = cent->current.angles;
    if ( cent->current.entityType == ET_PLAYER ) {
        ResetPlayerEntity( cent );
    }
    //ent->lerp_angles = ent->current.angles;
}

/**
*	@brief	Called when a new frame has been received that contains an entity
*			which was not present in the previous frame.
**/
static inline void EntityState_FrameEnter( centity_t *ent, const entity_state_t *nextState, const Vector3 &newIntendOrigin ) {
    // Assign a unique id to each entity for tracking it over multiple frames.
    static int64_t entity_ctr = 0;
    ent->id = ++entity_ctr;

    // Reset the needed entity state properties.
    EntityState_ResetState( ent, nextState );

    // Duplicate the current state so lerping doesn't hurt anything.
    ent->prev = *nextState;

    // <Q2RTXP>: WID: TODO: Do we still want/need this per se?
    // Handle proper lerping for animated entities by Hz.    
    #if 1
    // WID: 40hz
    // Update the animation frames and time.
    ent->current_frame = ent->last_frame = nextState->frame;
    ent->frame_servertime = clgi.client->servertime;
    ent->frame_realtime = clgi.GetRealTime();
    // WID: 40hz
    #endif

    // Off the entity event value.
    const int32_t entityEvent = EV_GetEntityEventValue( nextState->event );

    // No lerping if teleported, or a BEAM effect entity.
    if ( ( nextState->event != ent->current.event && ( entityEvent == EV_PLAYER_TELEPORT || entityEvent == EV_OTHER_TELEPORT ) ) 
		|| ( nextState->entityType == ET_BEAM || nextState->renderfx & RF_BEAM )
		// It is a temporary event entity, so no lerping needed.
		//|| ( nextState->entityType - ET_TEMP_EVENT_ENTITY > 0 )
    ) {
        ent->lerp_origin = newIntendOrigin;
        // We also thus do not want to mess about with assigning the state's
        // old_origin for lerping. Hence, we just return here.
        return;
    }

    // old_origin is valid for new entities,
    // so use it as starting point for interpolating between
    ent->prev.origin = nextState->old_origin;
    ent->lerp_origin = nextState->old_origin;
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
    // Off the entity event value.
    const int32_t entityEvent = EV_GetEntityEventValue( state->event );
    const int32_t entityEventOld = EV_GetEntityEventValue( ent->prev.event );

	// Get absolute difference in origin.
    const Vector3 originDifference = QM_Vector3Fabs( newIntendOrigin - ent->current.origin );
	const double differenceMax = std::max( { originDifference.x, originDifference.y, originDifference.z } );

    // Some data changes will force no lerping
    if ( state->entityType != ent->current.entityType
        || ( ent->current.modelindex != state->modelindex || ent->current.modelindex2 != state->modelindex2 || ent->current.modelindex2 != state->modelindex3 || ent->current.modelindex4 != state->modelindex4 )
        || ( ent->current.event != state->event && ( entityEvent == EV_PLAYER_TELEPORT || entityEvent == EV_OTHER_TELEPORT ) )
        || differenceMax > 64 // If it exceed 64, one of its axis is responsible for that.
        || cl_nolerp->integer == 1 )
    {
        // Duplicate the state to previous state, and reset if needed any specific entity state properties.
        EntityState_ResetState( ent, state );

        // Duplicate the current state so lerping doesn't hurt anything.
        ent->prev = *state;

        // No lerping if Teleported or Morphed, so use the passed in current origin.
        ent->lerp_origin = newIntendOrigin;

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
*   @return True if origin / angles update has been optimized out to the player state.
**/
static inline bool CheckPlayerStateEntity( const entity_state_t *state ) {
 //   if ( state->number == clgi.client->clientNumber + 1 ) {
 //       return true;
	//}
    if ( state->number != clgi.client->frame.ps.clientNumber + 1 ) {
        return false;
    }

    if ( clgi.client->frame.ps.pmove.pm_type >= PM_DEAD ) {
        return false;
    }

    return true;
}

/**
*   @return True if the SAME entity was NOT IN the PREVIOUS frame.
**/
static inline bool CheckNewFrameEntity( const centity_t *ent ) {
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
*   @brief  Prepares the client state for 'activation', also setting the predicted values
*           to those of the initially received first valid frame.
**/
void CLG_Frame_SetInitialServerFrame( void ) {
    // Set predicted frame to the current frame.
    game.predictedState.frame = clgi.client->frame;

	// Player States.
	player_state_t *playerState = &clgi.client->frame.ps;
    // Get the client entity for this frame's player state..
    centity_t *frameClientEntity = &clg_entities[ playerState->clientNumber + 1 ];

    // Rebuild the solid entity list for this frame.
    BuildSolidEntityList();

    // Update the player entity state from the playerstate.
    // This will allow effects and such to be properly placed.
    SG_PlayerStateToEntityState( playerState->clientNumber, playerState, &frameClientEntity->current );
	SG_PlayerStateToEntityState( clgi.client->clientNumber, playerState, &game.predictedEntity.current );

    // 
    for ( int32_t i = 0; i < clgi.client->frame.numEntities; i++ ) {
        // Fetch next state from states array..
        entity_state_t *nextState = &clgi.client->entityStates[ ( clgi.client->frame.firstEntity + i ) & PARSE_ENTITIES_MASK ];
        // Get entity with the matching number  this state.
        centity_t *cent = &clg_entities[ nextState->number ];
        
        // Assign new state.
        cent->current = *nextState;

        // Update serverframe and snapShotTime.
        cent->serverframe = clgi.client->frame.number;
        cent->snapShotTime = clgi.client->servertime;

        // Reset the needed entity state properties.
        EntityState_ResetState( cent, nextState );

        // Fire any needed entity events.
        CLG_Events_CheckForEntity( cent );
    }
    //// Work around Q2PRO server bandwidth optimization.
    //if ( CheckPlayerStateEntity( nextState ) ) {
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

    // Initialize position of the player's own entity from playerstate.
    // this is needed in situations when player entity is invisible, but
    // server sends an effect referencing it's origin (such as MZ_LOGIN, etc).
    game.predictedState.frame = clgi.client->frame;

    // Player States.
    player_state_t *playerState = &clgi.client->frame.ps;
    player_state_t *oldPlayerState = &clgi.client->oldframe.ps;
    // Predicted Player States.
    player_state_t *predictedPlayerState = &game.predictedState.frame.ps;

    // Get the client entity for this frame's player state..
    centity_t *frameClientEntity = &clg_entities[ playerState->clientNumber + 1 ];

    // Update the player entity state from the playerstate.
    // This will allow effects and such to be properly placed.
    SG_PlayerStateToEntityState( playerState->clientNumber, playerState, &frameClientEntity->current );

    /////////////////////
    //
    //
    //
    // Waking Up Tomorrow:
    //
    //
    //  The entity to be updated should like in Q3 be a separate independent 
	//  predictedEntity from the playerstate, so that effects and such can be properly
	//  placed without interfering with the predicted player entity.
    //
    //  
    //
    //////////////////////


    // Iterate over the current frame entity states and add solid entities to the solid list.
    for ( int32_t i = 0; i < clgi.client->frame.numEntities; i++ ) {
        // Fetch next state from states array..
        entity_state_t *nextState = &clgi.client->entityStates[ ( clgi.client->frame.firstEntity + i ) & PARSE_ENTITIES_MASK ];
        // Get entity with the matching number  this state.
        centity_t *cent = &clg_entities[ nextState->number ];
        // Copy it in as the next state.

        // Default to the next entity state origin.
        Vector3 nextOrigin = nextState->origin;

		// However, if it is a frame's player state matching entity, we take the origin from the playerstate.
        if ( CheckPlayerStateEntity( nextState ) ) {
            nextOrigin = predictedPlayerState->pmove.origin;
        }

        // See if the entity is new for the current serverframe or not and base our next move on that.
        if ( CheckNewFrameEntity( cent ) ) {
            // Wasn't in last update, so initialize some things.
            EntityState_FrameEnter( cent, nextState, nextOrigin );
        } else {
            // Was around, sp update some things.
            EntityState_FrameUpdate( cent, nextState, nextOrigin );
        }

        // Assign new state.
        cent->current = *nextState;

        // Assign last received server frame.
        cent->serverframe = clgi.client->frame.number;
        cent->snapShotTime = clgi.client->servertime;

        // Fire any needed entity events.
        CLG_Events_CheckForEntity( cent );

        // Update the player entity state from the playerstate.
        // This will allow effects and such to be properly placed.
        if ( CheckPlayerStateEntity( nextState ) ) {
            SG_PlayerStateToMinimalEntityState(
                clgi.client->frame.ps.clientNumber,
                &clgi.client->frame.ps, 
                &clg_entities[ clgi.client->frame.ps.clientNumber + 1 ].current, 
                false
            );
        }
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