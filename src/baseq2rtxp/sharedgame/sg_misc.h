/********************************************************************
*
*
*	SharedGame: Misc Functions:
*
*
********************************************************************/

/**
*
*
*	Events: WID: TODO: Should probably move to their own file sooner or later.
*
*
**/
/**
*	@brief	Adds a player movement predictable event to the player move state.
**/
void SG_PMoveState_AddPredictableEvent( const uint8_t newEvent, const uint8_t eventParm, player_state_t *playerState );



/**
*
*
*	(Client-)Player: WID: TODO: Move to their own file sometime.
* 
* 
**/
/**
*   @brief  Returns a string stating the determined 'Base' animation, and sets the FrameTime value for frameTime pointer.
**/
const std::string SG_Player_GetClientBaseAnimation( const player_state_t *oldPlayerState, const player_state_t *playerState, double *frameTime );
