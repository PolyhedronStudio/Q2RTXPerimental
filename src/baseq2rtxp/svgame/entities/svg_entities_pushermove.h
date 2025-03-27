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
/**
*   @brief
**/
void SVG_PushMove_MoveDone( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveFinal( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveBegin( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_PushMove_MoveCalculate( svg_entity_t *ent, const Vector3 &destination, svg_pushmove_endcallback endMoveCallback );



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
void SVG_PushMove_AngleMoveDone( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveFinal( svg_entity_t *ent );
/**
*   @brief
**/
void SVG_PushMove_AngleMoveBegin( svg_entity_t *ent );
/**
*   @brief	
**/
void SVG_PushMove_AngleMoveCalculate( svg_entity_t *ent, svg_pushmove_endcallback endMoveCallback );
/**
*   @brief  Begins an angular move with its default direction multiplied by the sign(+/- 1).
**/
void SVG_PushMove_AngleMoveCalculateSign( svg_entity_t *ent, const float sign, svg_pushmove_endcallback endMoveCallback );



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
void SVG_PushMove_Think_AccelerateMove( svg_entity_t *ent );
void SVG_PushMove_Think_AccelerateMoveNew( svg_entity_t *ent );
/**
*	@brief	Readjust speeds so that teamed movers start/end synchronized.
**/
void SVG_PushMove_Think_CalculateMoveSpeed( svg_entity_t *self );



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