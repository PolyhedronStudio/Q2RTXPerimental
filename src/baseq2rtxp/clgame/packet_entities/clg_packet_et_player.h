/********************************************************************
*
*
*	ClientGame: ET_PLAYER Header
* 
*	Some of these methods are used by the prediction code system.
*
*
********************************************************************/
#pragma once



/**
*   @brief  Calculate desired yaw derived player state move direction, and recored time of change.
**/
const bool CLG_ETPlayer_CalculateDesiredYaw( centity_t *packetEntity, const player_state_t *playerState, const player_state_t *oldPlayerState, const sg_time_t &currentTime );