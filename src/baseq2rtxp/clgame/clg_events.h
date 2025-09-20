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
*   @brief  Checks for player state generated events(usually by PMove) and processed them for execution.
**/
void CLG_FirePlayerStateEvent( const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const Vector3 &lerpOrigin );