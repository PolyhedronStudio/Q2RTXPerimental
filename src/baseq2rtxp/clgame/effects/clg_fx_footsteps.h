/********************************************************************
*
*
*	ClientGame: 'FootSteps' FX Implementation:
*
*
********************************************************************/
#pragma once


/**
*
*
*
*
*	FootSteps Core:
*
*
*
*
**/
/**
*	@brief  Precaches footstep audio for all material "kinds".
**/
void CLG_PrecacheFootsteps( void );
/**
*   @brief  Will play an appropriate footstep sound effect depending on the material that we're currently
*           standing on.
**/
void CLG_FX_FootStepSound( const int32_t entityNumber, const Vector3 &lerpOrigin, const bool isLadder = false, const bool isLocalClient = false );



/**
*
*
* 
*
*	FootStep Wrappers:
*
* 
*
*
**/
/**
*   @brief  The (LOCAL PLAYER) footstep sound event handler.
**/
void CLG_FX_LocalFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );

/**
*   @brief  The generic (PLAYER) footstep sound event handler.
**/
void CLG_FX_PlayerFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );
/**
*   @brief  The generic (OTHER entity) footstep sound event handler.
**/
void CLG_FX_OtherFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );

/**
*   @brief  Passes on to CLG_FX_FootStepSound with isLadder beign true. Used by EV_FOOTSTEP_LADDER.
**/
void CLG_FX_LadderFootStep( const int32_t entityNumber, const Vector3 &lerpOrigin );