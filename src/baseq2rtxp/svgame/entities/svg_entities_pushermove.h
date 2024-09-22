/********************************************************************
*
*
*	ServerGame: Support Routines for implementing brush entity 
*				movement.
* 
*				(Changes in origin using velocity.)
*
*
********************************************************************/
#pragma once


/**
*	State Constants:
**/
static constexpr int32_t PUSHMOVE_STATE_TOP = 0;
static constexpr int32_t PUSHMOVE_STATE_BOTTOM = 1;
static constexpr int32_t PUSHMOVE_STATE_MOVING_UP = 2;
static constexpr int32_t PUSHMOVE_STATE_MOVING_DOWN = 3;

/**
*
*
*
*   Support routines for movement (changes in origin using velocity)
*
*
*
**/
/**
*   @brief
**/
void SVG_PushMove_MoveDone( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveFinal( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveBegin( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveCalculate( edict_t *ent, const Vector3 &dest, void( *func )( edict_t * ) );



/**
*
*
*
*   Support routines for angular movement (changes in angle using avelocity)
*
*
*
**/
/**
*   @brief
**/
void SVG_PushMove_AngleMoveDone( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveFinal( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveBegin( edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveCalculate( edict_t *ent, void( *func )( edict_t * ) );



/**
*
*
*
*   Move Acceleration:
*
*
*
**/
/**
*   @brief  The team has completed a frame of movement, so calculate
*			the speed required for a move during the next game frame.
**/
void SVG_PushMove_Think_AccelerateMove( edict_t *ent );