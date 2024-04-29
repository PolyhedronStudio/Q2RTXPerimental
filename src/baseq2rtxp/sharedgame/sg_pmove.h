/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#pragma once

// PMove Constants:
static constexpr float PM_MIN_STEP_SIZE = 4.f;
static constexpr float PM_MAX_STEP_SIZE = 18.f;

/**
*	@brief	Shard Game Player Movement code implementation:
**/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params );
/**
*	@brief	Useful to in the future, configure the player move parameters to game specific scenarions.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp );
