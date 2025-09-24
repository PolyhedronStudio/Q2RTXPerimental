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
*	@brief	Returns true if the entity state's number matches to the view entity number.
**/
static inline const bool CLG_IsCurrentViewEntity( const entity_state_t *state ) {
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
}

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

	// Default to clgi.client->clientNumberl.
	int32_t entityIndex = clgi.client->clientNumber + 1;
	// Possibility of us chasing an entity.
	if ( clgi.client->frame.ps.stats[ STAT_CHASE ] > 0 ) {
		/* Account for the entity number( -1 ).*/
		entityIndex = clgi.client->frame.ps.stats[ STAT_CHASE ] - CS_PLAYERSKINS - 1;
	}
	// Return the entity pointer.
	return &clg_entities[ entityIndex ];
}
