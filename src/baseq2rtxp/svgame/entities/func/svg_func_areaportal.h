/********************************************************************
*
*
*	ServerGame: Entity 'func_areaportal'.
*
*
********************************************************************/
#pragma once


/**
*   Button SpawnFlags:
**/
//! Button starts pressed.
static constexpr int32_t BUTTON_SPAWNFLAG_START_PRESSED = BIT( 0 );
//! Button can't be triggered by monsters.
static constexpr int32_t BUTTON_SPAWNFLAG_NO_MONSTERS = BIT( 1 );
//! Button fires targets when touched.
static constexpr int32_t BUTTON_SPAWNFLAG_TOUCH_ACTIVATES = BIT( 2 );
//! Button fires targets when damaged.
static constexpr int32_t BUTTON_SPAWNFLAG_DAMAGE_ACTIVATES = BIT( 3 );
//! Button is locked from spawn, so it can't be used.
static constexpr int32_t BUTTON_SPAWNFLAG_TOGGLEABLE = BIT( 4 );
//! Button is locked from spawn, so it can't be used.
static constexpr int32_t BUTTON_SPAWNFLAG_LOCKED = BIT( 5 );


/**
*   For readability's sake:
**/
static constexpr svg_pushmove_state_t BUTTON_STATE_PRESSED = PUSHMOVE_STATE_TOP;
static constexpr svg_pushmove_state_t BUTTON_STATE_UNPRESSED = PUSHMOVE_STATE_BOTTOM;
static constexpr svg_pushmove_state_t BUTTON_STATE_MOVING_TO_PRESSED_STATE = PUSHMOVE_STATE_MOVING_UP;
static constexpr svg_pushmove_state_t BUTTON_STATE_MOVING_TO_UNPRESSED_STATE = PUSHMOVE_STATE_MOVING_DOWN;
