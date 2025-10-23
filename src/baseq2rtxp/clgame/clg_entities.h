/********************************************************************
*
*
*	ClientGame:	General Entities Header
*
*
********************************************************************/
// Use a static entity ID on some things because the renderer relies on eid to match between meshes
// on the current and previous frames.
static constexpr int32_t RENTITIY_RESERVED_GUN = 1;
static constexpr int32_t RENTITIY_RESERVED_TESTMODEL = 2;
static constexpr int32_t RENTITIY_RESERVED_COUNT = 3;


static constexpr int32_t RENTITIY_OFFSET_LOCALENTITIES = MAX_MODELS / 2; // TODO: Increase MAX_MODELS * 2 for refresh and all that.

/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what );
#endif
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void PF_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org );



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
/**
*	@brief	Called when a new frame has been received that contains an entity
*			which was not present in the previous frame.
**/
void CLG_EntityState_FrameEnter( centity_t *ent, const entity_state_t *state, const Vector3 *origin );
/**
*	@brief	Called when a new frame has been received that contains an entity 
*			already present in the previous frame.
**/
void CLG_EntityState_FrameUpdate( centity_t *ent, const entity_state_t *state, const Vector3 *origin );



/**
*
*
*
*	PlayerState Updating:
*
*
*
**/
/**
*   Determine whether the player state has to lerp between the current and old frame,
*   or snap 'to'.
**/
void CLG_PlayerState_LerpOrSnap( server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv );



/**
*
*
*
*	Entity Identification:
*
*
*
**/
/**
*	@brief	Returns true if the entityNumber matches to our local connection received entity number.
**/
static inline const bool CLG_IsLocalClientEntity( const int32_t entityNumber ) {
	if ( entityNumber == clgi.client->clientNumber + 1 ) {
		return true;
	}
	return false;
}
/**
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
static inline const bool CLG_IsLocalClientEntity( const entity_state_t *state ) {
	if ( state && state->number > 0 ) {
		return CLG_IsLocalClientEntity( state->number );
	}
	return false;
}



/**
*
*
*
*	Entity Chase and View Bindings:
*
*
*
**/
/**
*	@return		A pointer into clg_entities that matches to the client we're currently chasing with our view.
*               If we're not chasing anyone, it'll return a nullptr.
**/
static inline centity_t *CLG_GetChaseBoundEntity( void ) {
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] > 0 ) {
		return &clg_entities[ clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS - 1 ];
	} else {
		return nullptr;
	}
}
/**
*	@return		A pointer to the entity bound to the received server frame's view(index of clientNumber was sent during connect.)
*               Unless STAT_CHASE is set to specific client number, this'll point to the local client player himself.
**/
static inline centity_t *CLG_GetViewBoundEntity( void ) {
	// Sanity check.
	if ( clgi.client->clientNumber == -1 ) {
		clgi.Print( PRINT_DEVELOPER, "CLG_GetViewBoundEntity: No client number set yet(Value is -1).\n" );
		// Return a nullptr.
		return nullptr;
	}
	// Get chase entity.
	centity_t *viewBoundEntity = CLG_GetChaseBoundEntity();
	// If not chasing anyone, assign it the local client entity.
	if ( !viewBoundEntity ) {
		viewBoundEntity = &clg_entities[ clgi.client->clientNumber + 1 ];
	}
	// Return the view bound entity.
	return viewBoundEntity;
}

/**
*	@brief	Returns true if the entity state's number matches to the view entity number.
**/
static inline const bool CLG_IsCurrentViewEntity( const centity_t *cent ) {
	if ( !cent ) {
		clgi.Print( PRINT_DEVELOPER, "%s: (nullptr) entity pointer.\n", __func__ );
		return false;
	}

	// Get view entity.
	centity_t *viewEntity = CLG_GetViewBoundEntity();
	// Get chase entity.
	centity_t *chaseEntity = CLG_GetChaseBoundEntity();

	// Check if we match with the chase entity first.
	if ( chaseEntity && cent->current.number == chaseEntity->current.number ) {
		return true;
	// Check if we match with the view entity.
	} else if ( viewEntity && cent->current.number == viewEntity->current.number ) {
		return true;
	// No match.
	} else {
		return false;
	}

	#if 0
	// The entity we're chasing with our view.
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] > 0
		/* Account for the entity number.( Do a - 1 ) */
		&& ( state->number == clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS - 1 )
		// State matches the localclient entity.
		) {
		return true;
	} else if ( state->number == clgi.client->frame.clientNum + 1 ) {
		return true;
	}
	// No match.
	return false;
	#endif
}