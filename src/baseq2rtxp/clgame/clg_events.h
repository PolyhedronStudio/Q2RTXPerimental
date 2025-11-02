/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"
#include "shared/player_state.h"

/**
*   @brief  Processes the given entity event.
**/
//void CLG_ProcessEntityEvents( const int32_t eventValue, const Vector3 &lerpOrigin, centity_t *cent, const int32_t entityNumber, const int32_t clientNumber, clientinfo_t *clientInfo );
/**
*   @brief  Checks for entity generated events and processes them for execution.
**/
void CLG_CheckEntityEvents( centity_t *cent );

/**
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
const bool CLG_CheckPlayerStateEvent( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin );