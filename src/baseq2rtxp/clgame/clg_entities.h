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
void CLG_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org );






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
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
static inline const bool CLG_IsLocalClientEntity( const server_frame_t *frame ) {
	//if ( state && state->number > 0 ) {
	//	return CLG_IsLocalClientEntity( state->number );
	//}
	if ( frame ) {
		// Check for client number match.
		if ( frame->ps.clientNumber == clgi.client->clientNumber ) {
			return true;
		}
	}
	return false;
}
/**
*	@brief	Returns true if the entity state's number matches to our local client entity number,
*			received at initial time of connection.
**/
static inline const bool CLG_IsLocalClientEntity( const entity_state_t *state ) {
	//if ( state && state->number > 0 ) {
	//	return CLG_IsLocalClientEntity( state->number );
	//}
	if ( state ) {
		// Decode skinnum.
		const encoded_skinnum_t encodedSkin = static_cast<const encoded_skinnum_t>( state->skinnum );
		// Check for client number match.
		if ( encodedSkin.clientNumber == clgi.client->clientNumber ) {
			return true;
		}
	}
	return false;
}
/**
*	@brief	Returns true if the entityNumber matches to our local connection received entity number.
**/
static inline const bool CLG_IsLocalClientEntity( const int32_t entityNumber ) {
	if ( entityNumber < 0 ) {
		clgi.Print( PRINT_DEVELOPER, "%s: Invalid entity number (#%d) < (#0).\n", __func__, entityNumber );
		return false;
	}
	if ( entityNumber >= MAX_EDICTS ) {
		clgi.Print( PRINT_DEVELOPER, "%s: Entity number (#%d) exceeds MAX_EDICTS(#%d).\n", __func__, entityNumber, MAX_EDICTS );
		return false;
	}
	// Get the entity state.
	return CLG_IsLocalClientEntity( &clg_entities[ entityNumber ].current );
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
*	@return		A pointer into clg_entities that matches to the client we're currently chasing. nullptr if not chasing anyone.
**/
static inline centity_t *CLG_GetChaseBoundEntity( void ) {
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] > 0 ) {
		return &clg_entities[ clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS - 1 ];
	} else {
		return nullptr;
	}
}
/**
*	@return		The local client entity pointer, which is a match with the entity for the client number which we received at initial time of connection.
**/
static inline centity_t *CLG_GetLocalClientEntity( void ) {
	// Sanity check.
	if ( clgi.client->clientNumber == -1 ) {
		clgi.Print( PRINT_DEVELOPER, "CLG_GetLocalClientEntity: No client number set yet(Value is CLIENTNUM_NONE(%d)).\n", CLIENTNUM_NONE );
		// Return a nullptr.
		return nullptr;
	}
	// Return the local client entity.
	return &clg_entities[ clgi.client->clientNumber + 1 ];
}

/**
*	@return		A pointer to the entity which our view has to be bound to. If STAT_CHASE is set, it'll point to the chased entity.
* 				Otherwise, it'll point to the local client entity.
* 
*				Exception: If no client number is set yet, it'll return a nullptr and print a developer warning.
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
		viewBoundEntity = CLG_GetLocalClientEntity();
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
	const centity_t *viewEntity = CLG_GetViewBoundEntity();
	// Get chase entity.
	const centity_t *chaseEntity = CLG_GetChaseBoundEntity();

	// Check if we match with the chase entity first.
	if ( chaseEntity && cent == chaseEntity ) {
		return true;
		// Check if we match with the view entity.
	} else if ( viewEntity && cent == viewEntity ) {
		return true;
		// No match.
	} else {
		return false;
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
/**
*	@brief	Called when a new frame has been received that contains an entity
*			which was not present in the previous frame.
**/
void CLG_EntityState_FrameEnter( centity_t *ent, const entity_state_t *state, const Vector3 &origin );
/**
*	@brief	Called when a new frame has been received that contains an entity 
*			already present in the previous frame.
**/
void CLG_EntityState_FrameUpdate( centity_t *ent, const entity_state_t *state, const Vector3 &origin );



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
*   @brief  Handles player state transition between old and new server frames. Duplicates old state into current state
*           in case of no lerping conditions being met.
**/
void CLG_PlayerState_Transition( server_frame_t *oldframe, server_frame_t *frame, const int32_t framediv );
