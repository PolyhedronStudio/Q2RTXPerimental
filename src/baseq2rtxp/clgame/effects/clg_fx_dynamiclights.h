/********************************************************************
*
*
*	ClientGame: 'Dynamic Lights' FX Implementation:
*
*
********************************************************************/
#pragma once



/**
*   @brief
**/
void CLG_ClearDynamicLights( void );
/**
*   @brief
**/
clg_dlight_t *CLG_AllocateDynamicLight( const int32_t key );
/**
*   @brief
**/
void CLG_AddDynamicLights( void );