/********************************************************************
*
*
*	SharedGame: Player Movement
*
*
********************************************************************/
#pragma once

// PMove Constants:
//! Minimal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_STEP_MIN_SIZE = 4.f;
//! Maximal step height difference for the Z axis before marking our move as a 'stair step'.
static constexpr float PM_STEP_MAX_SIZE = 18.f;

//! Succesfully performed the move.
static constexpr int32_t PM_SLIDEMOVEFLAG_MOVED = BIT( 0 );
//! It was blocked at some point, doesn't mean it didn't slide along the blocking object.
static constexpr int32_t PM_SLIDEMOVEFLAG_BLOCKED = BIT( 1 );
//! It is trapped.
static constexpr int32_t PM_SLIDEMOVEFLAG_TRAPPED = BIT( 2 );
//! Blocked by a literal wall.
static constexpr int32_t PM_SLIDEMOVEFLAG_WALL_BLOCKED = BIT( 3 );
//! Touched at least a single plane along the way.
static constexpr int32_t PM_SLIDEMOVEFLAG_PLANE_TOUCHED = BIT( 4 );




/**
*	@brief	Shard Game Player Movement code implementation:
**/
void SG_PlayerMove( pmove_t *pmove, pmoveParams_t *params );
/**
*	@brief	Useful to in the future, configure the player move parameters to game specific scenarions.
**/
void SG_ConfigurePlayerMoveParameters( pmoveParams_t *pmp );
