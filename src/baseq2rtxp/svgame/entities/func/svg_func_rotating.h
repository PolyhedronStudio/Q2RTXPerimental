/********************************************************************
*
*
*	ServerGame: Pusher Entity 'func_rotating'.
*
*
********************************************************************/
#pragma once


/**
*   Spawnflags:
**/
//! Starts ON.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_START_ON			= BIT( 0 );
//! Moves into reverse direction..
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_START_REVERSE		= BIT( 1 );
//! Accelerate/Decelerate into velocity desired speed.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_ACCELERATED		= BIT( 2 );
//! Rotate around X axis.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_ROTATE_X			= BIT( 3 );
//! Rotate around Y axis.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_ROTATE_Y			= BIT( 4 );
//! Pain on Touch.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_PAIN_ON_TOUCH		= BIT( 5 );
//! Blocking object stops movement.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_BLOCK_STOPS		= BIT( 6 );
//! Brush has animations.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_ANIMATE_ALL		= BIT( 7 );
//! Brush has animations.
static constexpr int32_t FUNC_ROTATING_SPAWNFLAG_ANIMATE_ALL_FAST	= BIT( 8 );