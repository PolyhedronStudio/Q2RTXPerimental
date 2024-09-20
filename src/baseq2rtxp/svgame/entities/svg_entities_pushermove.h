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
static constexpr int32_t STATE_TOP = 0;
static constexpr int32_t STATE_BOTTOM = 1;
static constexpr int32_t STATE_MOVING_UP = 2;
static constexpr int32_t STATE_MOVING_DOWN = 3;

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
void Move_Done( edict_t *ent );
/**
*   @brief
**/
void Move_Final( edict_t *ent );
/**
*   @brief
**/
void Move_Begin( edict_t *ent );
/**
*   @brief
**/
void Think_AccelMove( edict_t *ent );
/**
*   @brief
**/
void Move_Calc( edict_t *ent, const Vector3 &dest, void( *func )( edict_t * ) );



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
void AngleMove_Done( edict_t *ent );
/**
*   @brief
**/
void AngleMove_Final( edict_t *ent );
/**
*   @brief
**/
void AngleMove_Begin( edict_t *ent );
/**
*   @brief
**/
void AngleMove_Calc( edict_t *ent, void( *func )( edict_t * ) );



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
*   @brief
**/
constexpr float AccelerationDistance( float target, float rate );
/**
*   @brief
**/
void plat_CalcAcceleratedMove( g_pusher_moveinfo_t *moveinfo );
/**
*   @brief
**/
void plat_Accelerate( g_pusher_moveinfo_t *moveinfo );
/**
*   @brief  The team has completed a frame of movement, so
*           change the speed for the next frame.
**/
void Think_AccelMove( edict_t *ent );