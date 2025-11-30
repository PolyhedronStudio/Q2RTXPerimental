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
*   @brief  Processes the given player state event.
**/
const bool CLG_Events_FirePlayerStateEvent( centity_t *playerEntity, const player_state_t *ops, const player_state_t *ps, const int32_t playerStateEvent, const int32_t playerStateEventParm0, const Vector3 &lerpOrigin );