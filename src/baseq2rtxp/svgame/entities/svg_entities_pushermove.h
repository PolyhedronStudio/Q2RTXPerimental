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


struct svg_pushmove_edict_t;
/**
*	@brief	State Constants, top and down are synonymous to open and close, up to opening, down to closing.
**/
//static constexpr int32_t PUSHMOVE_STATE_TOP = 0;
//static constexpr int32_t PUSHMOVE_STATE_BOTTOM = 1;
//static constexpr int32_t PUSHMOVE_STATE_MOVING_UP = 2;
//static constexpr int32_t PUSHMOVE_STATE_MOVING_DOWN = 3;

/**
*
*
*
*   Support routines for movement (changes in origin using velocity)
*
*
*
**/
//! <Q2RTXP>: WID: Moved into svg_pushmove_edict_T as member functions. 
#if 0
/**
*   @brief
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_MoveDone );
/**
*   @brief
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_MoveFinal );
/**
*   @brief
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_MoveBegin );
/**
*   @brief
**/
void SVG_PushMove_MoveCalculate( svg_pushmove_edict_t *ent, const Vector3 &destination, svg_pushmove_endcallback endMoveCallback );
#endif // #if 0

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
void SVG_PushMove_AngleMoveDone( svg_pushmove_edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveFinal( svg_pushmove_edict_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveBegin( svg_pushmove_edict_t *ent );
/**
*   @brief	
**/
void SVG_PushMove_AngleMoveCalculate( svg_pushmove_edict_t *ent, svg_pushmove_endcallback endMoveCallback );
/**
*   @brief  Begins an angular move with its default direction multiplied by the sign(+/- 1).
**/
void SVG_PushMove_AngleMoveCalculateSign( svg_pushmove_edict_t *ent, const float sign, svg_pushmove_endcallback endMoveCallback );



/**
*
*
*
*   Move Acceleration:
*
*
*
**/
//! <Q2RTXP>: WID: Moved into svg_pushmove_edict_T as member functions. 
#if 0
/**
*   @brief  The team has completed a frame of movement, so calculate
*			the speed required for a move during the next game frame.
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_AccelerateMove );
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_AccelerateMoveNew );
/**
*	@brief	Readjust speeds so that teamed movers start/end synchronized.
**/
DECLARE_GLOBAL_CLASSNAME_CALLBACK_THINK( svg_pushmove_edict_t, SVG_PushMove_Think_CalculateMoveSpeed );
#endif // #if 0


/**
*
*
*
*   MoveWith Entities:
*
*
**/
/**
*   @brief  Reposition 'moveWith' entities to their parent entity its made movement.
**/
void SVG_PushMove_UpdateMoveWithEntities();